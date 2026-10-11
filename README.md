# VantaDesk Taskbar Host

Código-fonte do `VantaDeskTaskbarHost.exe`, o componente do VantaDesk que aplica a aparência
da barra de tarefas do Windows 11. Ele carrega a `VantaDeskExplorerTAP.dll`, compilada a partir
do componente ExplorerTAP do [TranslucentTB](https://github.com/TranslucentTB/TranslucentTB)
(GPL-3.0), e por isso é distribuído sob a mesma licença: **GNU GPL versão 3** (arquivo `LICENSE`).

Este repositório contém apenas este componente. O restante do VantaDesk é um programa separado,
que conversa com ele por entrada e saída padrão, e não está incluído aqui.

## Código-fonte da DLL

`VantaDeskExplorerTAP.dll` corresponde ao componente `ExplorerTAP` do TranslucentTB, branch
`release`, revisão
[`322e2b7395a51975150126276308b415970e080b`](https://github.com/TranslucentTB/TranslucentTB/tree/322e2b7395a51975150126276308b415970e080b).

## Compilar

Requer o Visual Studio Build Tools 2022 com C++:

```powershell
.\build-host.ps1
```

## Uso

```text
VantaDeskTaskbarHost.exe --server                 linhas "<id> <modo> <cor> <desfoque>" no stdin;
                                                  responde "READY" e depois "<id> OK|ERROR <código>"
VantaDeskTaskbarHost.exe <modo> [cor] [desfoque]  aplica uma vez e sai
```

Modos: `normal`, `clear`, `blur`, `acrylic`. Cor em `0xAABBGGRR`, em decimal.

---

# VantaDesk Taskbar Host (English)

Source code of `VantaDeskTaskbarHost.exe`, the VantaDesk component that applies the Windows 11
taskbar appearance. It loads `VantaDeskExplorerTAP.dll`, built from the ExplorerTAP component of
[TranslucentTB](https://github.com/TranslucentTB/TranslucentTB) (GPL-3.0), and is therefore
distributed under the same license: **GNU GPL version 3** (see `LICENSE`).

This repository contains only this component. The rest of VantaDesk is a separate program that
talks to it through standard input/output and is not included here.

The DLL corresponds to TranslucentTB's `ExplorerTAP`, branch `release`, revision
`322e2b7395a51975150126276308b415970e080b`. Build with Visual Studio Build Tools 2022 (C++):
`.\build-host.ps1`.
