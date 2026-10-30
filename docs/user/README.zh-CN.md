[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | **简体中文** | [Русский](README.ru.md)

# 简介

将可执行文件自动移植到 Linux 和 Windows 的工具。

仅 relinker 的构建可在 macOS 上运行，包括 Apple silicon。

[**网页转换器**](https://boykopovar.github.io/AnyPS5/) - 文件保留在你的设备上。

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

包含一个 [relinker](../../core/relinker)，可将可执行文件转换为目标系统的原生格式，以及适用于动态链接的[系统 prx 库](../../core/libs/prx)实现。无需模拟，也没有独立的运行时进程。

[**Discord**](https://discord.gg/BHFztBPUe)、[使用说明](USAGE.md)、[构建说明](../dev/BUILD.md)、[架构](../dev/ARCHITECTURE.md)、[项目技术债务](../dev/TechnicalDebt.md)、[代码风格约定](../dev/CONVENTIONS.md)、[贡献指南](../../CONTRIBUTING.md)

## 状态

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shaders](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[进度历史](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* 系统库：项目目前已知函数（在 [core/libs/prx](../../core/libs/prx) 中声明）的百分比，而非 PS5 全部系统函数的百分比。随着声明的函数增多，总数会增长。</sub>

不支持或意外的状态一律抛出 `std::runtime_error`。`what()` 会输出到 stderr，然后进程终止。

[着色器重编译器](../../core/shader/recompiler/Recompiler.cpp)可成功生成 SPIR-V（使用 `ANYPS5_ENABLE_SPIRV_TOOLS` 构建时通过 [Spirv-Tools](../../3rdparty/SPIRV-Tools) 验证）。

## 兼容性

每个游戏的状态和测试报告都在 [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up) 讨论区，每个游戏一个帖子。

## 输入映射

支持 SDL 映射的游戏手柄，包括摇杆和扳机。键盘和鼠标操作可通过 `anyps5-input.ini` 文件配置。支持的设备和配置格式请参阅[输入映射](INPUT_MAPPING.md)。

## 免责声明

本项目旨在用于互操作性、研究、保存和兼容性目的。本项目不包含、不分发也不需要任何受版权保护的软件、固件、加密密钥或专有库。用户有责任确保与本项目一起使用的任何二进制文件均按照适用法律及其各自的许可条款获取和使用。

AnyPS5 是一个独立项目，与 Sony Interactive Entertainment 没有关联，也未获得其授权或认可。“PlayStation”和“PS5”是 Sony Interactive Entertainment Inc. 的商标。

## 许可证

本项目仅依据 GNU General Public License 第 2 版授权。
