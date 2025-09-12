# CH347-I2C-TOOLS

本项目将 `i2c-tools` 中的 `i2cdetect`、`i2cdump` 和 `i2cset` 三个常用工具移植至 Windows 平台，用于通过 CH347 芯片进行 I2C 设备的调试。

## 项目简介

`CH347-I2C-TOOLS` 提供了在 Windows 环境下使用命令行工具调试 I2C 设备的能力，特别适用于嵌入式开发、单片机（如 STM32）及硬件工程等场景。编译后生成 `i2cdetect.exe`、`i2cdump.exe` 和 `i2cset.exe` 三个可执行文件，支持设备扫描、寄存器读取与写入等操作。

## 使用说明

详细使用方法、编译步骤及示例请参考：

👉 [CH347应用 USB转I2C功能之：开源项目i2c-tools工具的使用](https://blog.csdn.net/qq_43010294/article/details/151619825)

该文章涵盖了以下内容：
- 如何编译项目
- 使用 `i2cdetect.exe` 扫描 I2C 总线上的设备
- 使用 `i2cdump.exe` 读取 I2C 设备寄存器内容
- 使用 `i2cset.exe` 向 I2C 设备写入数据并支持回读校验
