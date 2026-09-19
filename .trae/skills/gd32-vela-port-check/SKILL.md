---
name: gd32-vela-port-check
description: GD32F470 平台 openvela 适配检查清单。新增或修改 GD32 板级配置、串口波特率、GPIO 注册表、Flash 分区、时钟配置时使用，也可用于排查设备节点未注册、OLED 无显示、串口乱码等移植故障。不用于应用层业务逻辑开发。
---

# GD32 openvela 平台适配检查

在修改 GD32F470 板级配置或排查移植故障时，按以下顺序逐项检查。本清单沉淀自 GD32F470ZE 实际移植踩坑记录。

## 1. Flash 分区与芯片型号匹配（最高优先级，隐蔽故障源）

- 确认芯片后缀：ZE=512K Flash，ZK=3MB Flash，两者分区配置完全不同。
- 若分区按 ZK 配置而实际为 ZE，littlefs 格式化越界会导致 bringup 提前 return，SPI/GPIO 等设备节点全部静默缺失，且无明显报错日志。
- 检查项：`.config` 中 `CONFIG_GD32F4_PROGMEM` 相关分区大小是否超出实际 Flash 容量；不确定时先关闭该配置验证。
- 验证手段：对固件反汇编，确认 bringup 函数尾部仍包含 spidev/gpio 注册调用。

## 2. 时钟树

- 默认 IRC16M，需经 PLL 倍频至目标主频（如 240MHz）。
- 检查 board.h 中 PLLM（=16 对应 16MHz 输入）与倍频/分频参数是否匹配目标频率。
- 时钟错误会表现为串口波特率整体偏移（乱码）而非完全无输出。

## 3. GPIO 字符设备注册表

- 设备节点编号规则：`gd32_gpio_initialize()` 按 **输入→输出→中断** 顺序递增分配 pincount，与数组顺序严格对应。
- 修改 `gd32f470v_start.h` 中 `BOARD_NGPIOIN/NGPIOOUT/NGPIOINT` 计数时，必须同步修改板级 `gd32f4xx_gpio.c` 中对应数组元素，两者不一致会导致节点错位或缺失。
- 新增输出引脚定义格式：`GPIO_CFG_MODE_OUTPUT | GPIO_CFG_OUTPUT_SET | GPIO_CFG_SPEED_50MHZ | GPIO_CFG_PORT_x | GPIO_CFG_PIN_n`，其中 `OUTPUT_SET` 决定上电初始电平。
- 变更后先列出期望的 /dev/gpioN 映射表，再逐一核对应用层 open 的节点号。

## 4. 引脚复用冲突

- 新增 GPIO 前核对 board.h 中已占用引脚：USART0(PA9/PA10)、SPI0(PA4/PA5/PA7)、UART3(PC10/PC11)、OLED DC/RES(PA6/PA1)。
- 注意复用脚背景：如 PD4 为 EXMC_NOE 复用脚，需确认对应外设驱动（EXMC）未启用。

## 5. 串口

- 控制台波特率需同时修改 `.config` 与板级 defconfig 中 `CONFIG_USART0_BAUD`，两处不同步会在下次 olddefconfig 后被覆盖。
- 修改后提醒用户同步调整串口工具波特率。

## 6. 构建与验证基准

- 构建成功标志：日志尾部出现 `LD: nuttx` / `CP: nuttx.hex` / `CP: nuttx.bin`。
- grep "error" 会误匹配 LD 链接行中的文件名，需人工甄别。
- 烧录前确认 nuttx.bin 时间戳为本次构建产物（跨 WSL/Windows 文件时间戳可能有延迟）。
