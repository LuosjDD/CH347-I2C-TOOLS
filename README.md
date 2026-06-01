# CH347-I2C-TOOLS

本项目将 `i2c-tools` 移植至 Windows 平台，用于通过 CH347 芯片进行 I2C 设备的调试。

## 项目简介

`CH347-I2C-TOOLS` 提供了在 Windows 环境下使用命令行工具调试 I2C 设备的能力，特别适用于嵌入式开发、单片机（如 STM32）及硬件工程等场景。目前提供以下五个工具：

| 工具 | 说明 |
|------|------|
| `i2cdetect` | 扫描 I2C 总线上的设备地址 |
| `i2cdump` | Dump 设备全部寄存器内容（兼容 SMBus） |
| `i2cget` | 读取单个或连续寄存器值（支持 byte/word/block/SMBus 模式） |
| `i2cset` | 向 I2C 设备写入数据并回读校验 |
| `i2ctransfer` | 发送拼接的 I2C 消息，支持写+读组合与重复 START |

## 编译步骤

```bash
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

编译后生成 `.exe` 和 `CH347DLLA64.DLL`（64位）到项目根目录。

## 使用说明

详细使用方法及示例请参考：

👉 [CH347应用 USB转I2C功能之：开源项目i2c-tools工具的使用](https://blog.csdn.net/qq_43010294/article/details/151619825)
