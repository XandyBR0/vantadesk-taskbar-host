#include <Windows.h>
#include <Unknwn.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>

enum TaskbarBrush
{
    Acrylic,
    SolidColor,
};

MIDL_INTERFACE("5bcf9150-c28a-4ef2-913c-4c3ea2f5ead0")
ITaskbarAppearanceService : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetTaskbarAppearance(HWND taskbar, TaskbarBrush brush, UINT color) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetTaskbarBlur(HWND taskbar, UINT color, FLOAT blurAmount) = 0;
    virtual HRESULT STDMETHODCALLTYPE ReturnTaskbarToDefaultAppearance(HWND taskbar) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetTaskbarBorderVisibility(HWND taskbar, BOOL visible) = 0;
    virtual HRESULT STDMETHODCALLTYPE RestoreAllTaskbarsToDefault() = 0;
    virtual HRESULT STDMETHODCALLTYPE RestoreAllTaskbarsToDefaultWhenProcessDies(DWORD pid) = 0;
    virtual HRESULT STDMETHODCALLTYPE KillExplorerWhenPackageUninstalls(LPCWSTR packageFullName) = 0;
};

using InjectExplorerTap = HRESULT (*)(HWND window, REFIID riid, void** object);

namespace
{
    constexpr wchar_t TapDllName[] = L"VantaDeskExplorerTAP.dll";

    struct ComScope
    {
        HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        ~ComScope()
        {
            if (SUCCEEDED(result))
            {
                CoUninitialize();
            }
        }
    };

    struct ModuleScope
    {
        HMODULE module = nullptr;

        ~ModuleScope()
        {
            if (module)
            {
                FreeLibrary(module);
            }
        }
    };

    struct ServiceScope
    {
        ITaskbarAppearanceService* service = nullptr;

        ~ServiceScope()
        {
            if (service)
            {
                service->Release();
            }
        }
    };

    std::filesystem::path GetTapPath()
    {
        wchar_t executablePath[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, executablePath, ARRAYSIZE(executablePath));
        if (!length || length >= ARRAYSIZE(executablePath))
        {
            return {};
        }

        return std::filesystem::path(executablePath).parent_path() / TapDllName;
    }

    bool ParseUnsigned(const wchar_t* value, UINT& output)
    {
        if (!value)
        {
            return false;
        }

        wchar_t* end = nullptr;
        const unsigned long parsed = wcstoul(value, &end, 10);
        if (end == value || *end != L'\0')
        {
            return false;
        }

        output = static_cast<UINT>(parsed);
        return true;
    }

    HRESULT ApplyToTaskbar(
        ITaskbarAppearanceService* service,
        HWND taskbar,
        std::wstring_view mode,
        UINT color,
        float blurAmount
    )
    {
        if (!taskbar)
        {
            return S_FALSE;
        }

        HRESULT result = S_OK;
        if (mode == L"normal")
        {
            result = service->ReturnTaskbarToDefaultAppearance(taskbar);
            if (SUCCEEDED(result))
            {
                result = service->SetTaskbarBorderVisibility(taskbar, TRUE);
            }
        }
        else if (mode == L"blur")
        {
            result = service->SetTaskbarBlur(taskbar, color, blurAmount);
            if (SUCCEEDED(result))
            {
                result = service->SetTaskbarBorderVisibility(taskbar, FALSE);
            }
        }
        else
        {
            const TaskbarBrush brush = mode == L"acrylic" ? Acrylic : SolidColor;
            result = service->SetTaskbarAppearance(taskbar, brush, color);
            if (SUCCEEDED(result))
            {
                result = service->SetTaskbarBorderVisibility(taskbar, FALSE);
            }
        }

        return result;
    }

    HRESULT ApplyToAllTaskbars(
        ITaskbarAppearanceService* service,
        std::wstring_view mode,
        UINT color,
        float blurAmount
    )
    {
        if (mode == L"normal")
        {
            return service->RestoreAllTaskbarsToDefault();
        }

        const HWND mainTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!mainTaskbar)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        }

        HRESULT result = ApplyToTaskbar(service, mainTaskbar, mode, color, blurAmount);
        if (FAILED(result))
        {
            return result;
        }

        HWND secondary = nullptr;
        do
        {
            secondary = FindWindowExW(nullptr, secondary, L"Shell_SecondaryTrayWnd", nullptr);
            if (secondary)
            {
                result = ApplyToTaskbar(service, secondary, mode, color, blurAmount);
                if (FAILED(result))
                {
                    return result;
                }
            }
        } while (secondary);

        return result;
    }

    bool WriteProtocolLine(const std::string& line)
    {
        const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!output || output == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        const std::string message = line + "\n";
        DWORD written = 0;
        return WriteFile(output, message.data(), static_cast<DWORD>(message.size()), &written, nullptr) &&
            written == message.size();
    }

    int RunServer(ITaskbarAppearanceService* service)
    {
        const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (!input || input == INVALID_HANDLE_VALUE)
        {
            return 40;
        }

        // O registro inicial da árvore XAML pode terminar logo depois da injeção.
        // A conexão permanece viva, como no TranslucentTB, e evita reinjeções por comando.
        Sleep(350);
        if (!WriteProtocolLine("READY"))
        {
            return 41;
        }

        std::string pending;
        char chunk[512] = {};
        DWORD bytesRead = 0;
        while (ReadFile(input, chunk, sizeof(chunk), &bytesRead, nullptr) && bytesRead > 0)
        {
            pending.append(chunk, bytesRead);

            size_t newline = std::string::npos;
            while ((newline = pending.find('\n')) != std::string::npos)
            {
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }

                unsigned long requestId = 0;
                std::string modeText;
                unsigned long colorValue = 0;
                float blurAmount = 12.0f;
                std::istringstream command(line);
                if (!(command >> requestId >> modeText >> colorValue >> blurAmount))
                {
                    WriteProtocolLine("0 ERROR 42");
                    continue;
                }

                const std::wstring mode(modeText.begin(), modeText.end());
                if (mode != L"normal" && mode != L"clear" && mode != L"blur" && mode != L"acrylic")
                {
                    WriteProtocolLine(std::to_string(requestId) + " ERROR 43");
                    continue;
                }

                HRESULT result = S_OK;
                for (int attempt = 0; attempt < 5; ++attempt)
                {
                    result = ApplyToAllTaskbars(
                        service,
                        mode,
                        static_cast<UINT>(colorValue),
                        blurAmount
                    );
                    if (FAILED(result))
                    {
                        break;
                    }

                    if (attempt < 4)
                    {
                        Sleep(80);
                    }
                }

                if (SUCCEEDED(result))
                {
                    WriteProtocolLine(std::to_string(requestId) + " OK");
                }
                else
                {
                    WriteProtocolLine(
                        std::to_string(requestId) + " ERROR " +
                        std::to_string(static_cast<unsigned long>(result))
                    );
                }
            }
        }

        return 0;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int argumentCount = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments)
    {
        return 10;
    }

    const auto freeArguments = [&]() { LocalFree(arguments); };
    if (argumentCount < 2)
    {
        freeArguments();
        return 11;
    }

    const std::wstring_view mode(arguments[1]);
    const bool serverMode = mode == L"--server";
    if (!serverMode && mode != L"normal" && mode != L"clear" && mode != L"blur" && mode != L"acrylic")
    {
        freeArguments();
        return 12;
    }

    UINT color = 0;
    if (!serverMode && mode != L"normal" && (argumentCount < 3 || !ParseUnsigned(arguments[2], color)))
    {
        freeArguments();
        return 13;
    }

    float blurAmount = 12.0f;
    if (argumentCount >= 4)
    {
        blurAmount = static_cast<float>(_wtof(arguments[3]));
    }

    freeArguments();

    ComScope com;
    if (FAILED(com.result))
    {
        return 20;
    }

    const std::filesystem::path tapPath = GetTapPath();
    if (tapPath.empty())
    {
        return 21;
    }

    ModuleScope tap;
    tap.module = LoadLibraryW(tapPath.c_str());
    if (!tap.module)
    {
        return 22;
    }

    const auto inject = reinterpret_cast<InjectExplorerTap>(GetProcAddress(tap.module, "InjectExplorerTAP"));
    if (!inject)
    {
        return 23;
    }

    const HWND mainTaskbar = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!mainTaskbar)
    {
        return 24;
    }

    ServiceScope appearance;
    HRESULT result = inject(mainTaskbar, __uuidof(ITaskbarAppearanceService), reinterpret_cast<void**>(&appearance.service));
    if (FAILED(result) || !appearance.service)
    {
        return 25;
    }

    if (serverMode)
    {
        return RunServer(appearance.service);
    }

    if (mode == L"normal")
    {
        for (int attempt = 0; attempt < 3; ++attempt)
        {
        result = ApplyToAllTaskbars(appearance.service, mode, color, blurAmount);
            if (attempt < 2)
            {
                Sleep(70);
            }
        }
        return SUCCEEDED(result) ? 0 : 30;
    }

    for (int attempt = 0; attempt < 3; ++attempt)
    {
        result = ApplyToAllTaskbars(appearance.service, mode, color, blurAmount);
        if (FAILED(result))
        {
            return 31;
        }

        if (attempt < 2)
        {
            Sleep(70);
        }
    }

    return 0;
}
