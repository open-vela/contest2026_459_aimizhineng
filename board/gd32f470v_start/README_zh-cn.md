# GD32F470V-START

[ [English](README.md) | 简体中文 ]

[GD32F470V-START](https://www.gigadevice.com/product/mcu/high-performance-mcus/gd32f4xx-series/gd32f470)
是兆易创新推出的入门级 Starter Kit，主控为 **GD32F470VKT6**，
基于 Arm Cortex-M4F 内核，最高主频 240 MHz，3 MB 片上 Flash、
256 KB SRAM，带单精度 FPU 和 DSP 指令。板上集成用户 LED、用户按键、
板载 GD-Link 调试器以及 Arduino 兼容排针，是 openvela 上典型的
**板级 bring-up 与外设原型验证**目标板。

## 特性

  - GD32F470VKT6（Arm Cortex-M4F + DSP，LQFP100 封装，240 MHz）
  - 3 MB 片上 Flash（240 MHz 下零等待状态），256 KB SRAM，
    64 KB TCMSRAM
  - 最多 82 路 GPIO 引出到 Arduino 兼容 + 2.54 mm 排针
  - 板载 GD-Link 调试器（CMSIS-DAP，SWD）
  - USB Mini-B 口（供电 + GD-Link 通信）
  - 用户 LED、用户按键、复位键
  - 25 MHz HSE 晶振，32.768 kHz LSE 晶振

板级硬件细节与原理图请参考兆易创新官方文档：

- [GD32F470 系列产品页](https://www.gigadevice.com/product/mcu/high-performance-mcus/gd32f4xx-series/gd32f470)
- [GD32F470VKT6 型号详情](https://www.gigadevice.com/product/mcu/mcus-product-selector/gd32f470vkt6)
- [GD32F470xx 数据手册 Rev1.4](https://www.gd32mcu.com/data/documents/datasheet/GD32F470xx_Datasheet_Rev1.4.pdf)
- [AN056 — GD32F4xx 硬件开发指南 Rev1.2](https://www.gd32mcu.com/data/documents/applicationNote/AN056%20GD32F4xx%20Hardware%20Development%20Guide_Rev1.2.pdf)

## 目录结构

```
vendor/gigadevice/boards/gd32f4/gd32f470v_start/
├── Kconfig                  板级 Kconfig
├── CMakeLists.txt           CMake 集成
├── README.md / README_zh-cn.md
├── include/board.h          时钟 + LED + GPIO 板级定义
├── scripts/
│   ├── ld.script            Flat 构建链接脚本
│   ├── memory.ld            Protected/Kernel 构建内存分区
│   ├── kernel-space.ld
│   ├── user-space.ld
│   ├── gnu-elf.ld
│   └── Make.defs
├── configs/
│   └── nsh/defconfig
└── src/                     板级 bring-up + 各外设 hook
    ├── gd32f470v_start.h
    ├── gd32f4xx_boot.c, gd32f4xx_appinit.c, gd32f4xx_bringup.c
    ├── gd32f4xx_autoleds.c, gd32f4xx_userleds.c
    ├── gd32f4xx_buttons.c, gd32f4xx_gpio.c, gd32f4xx_reset.c
    ├── gd32f4xx_spi.c, gd32f4xx_sdio.c
    ├── gd32f4xx_at24.c, gd32f4xx_gd25.c
    └── gd32f4xx_romfs.{c,h} 等
```

## 内存布局

| 区域     | 起始地址     | 大小     | 备注                                    |
|----------|--------------|----------|-----------------------------------------|
| Flash    | `0x08000000` | 3072 KB  | 片上，240 MHz 下零等待状态              |
| SRAM0    | `0x20000000` | 112 KB   | 主堆区                                  |
| SRAM1    | `0x2001C000` | 16 KB    | 通过 `MM_REGIONS` 接入                  |
| SRAM2    | `0x20020000` | 64 KB    | 通过 `MM_REGIONS` 接入                  |
| TCMSRAM  | `0x10000000` | 64 KB    | 紧耦合 SRAM，CPU 直接访问               |
| BKP SRAM | `0x40024000` | 4 KB     | 电池供电备份                            |

## 时钟树

默认配置下 SYSCLK 由 25 MHz HSE 晶振经主 PLL 生成：

```
HSE = 25 MHz                       Kconfig：GD32F470V_START_HXTAL_VALUE
        │
        ▼
    ┌── PLLP ──→ SYSCLK (200 MHz)  Kconfig：GD32F470V_START_{168,200,240}MHZ
    └── PLLQ ──→ 48 MHz            USB / SDIO / RNG
        │
        ▼
    HCLK  = SYSCLK / 1
    APB2  = HCLK   / 2
    APB1  = HCLK   / 4
```

选择 `GD32F470V_START_USE_IRC16` 可将时钟源切换到片内 16 MHz IRC
振荡器，适用于 HSE 晶振未贴片的场景。

## 配置 (Configurations)

下列所有配置均通过 openvela 的 `ARCH_BOARD_CUSTOM` 机制构建（板目录
位于 `nuttx/boards/` 之外）。在 `nuttx/` 目录下：

```
$ ./tools/configure.sh -E -l \
    ../vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/<config_name>
$ make -j$(nproc)
```

`<config_name>` 为目标配置名。构建产物落在 `nuttx/`：

| 文件          | 用途                                            |
|---------------|-------------------------------------------------|
| `nuttx.elf`   | 完整 ELF（含调试信息），用于 GDB                |
| `nuttx.bin`   | 裸二进制，烧到 `0x08000000`                     |
| `nuttx.hex`   | Intel HEX，给偏好 HEX 格式的工具用              |
| `nuttx.map`   | 链接 map                                        |

### nsh

最小 NuttShell，console 走 **USART0 @ 115200 8N1**（TX = PA9，
RX = PA10）。启动时不挂载任何板载外设；额外外设通过排针外接，
经 `menuconfig` 解锁。

```
nsh> ?
help usage:  help [-v] [<cmd>]

    .         break     dd        exit      ls        ps        source    umount
    [         cat       df        false     mkdir     pwd       test      unset
    ?         cd        dmesg     free      mkrd      rm        time      uptime
    alias     cp        echo      help      mount     rmdir     true      usleep
    ...

Builtin Apps:
    nsh  sh
nsh> uname -a
NuttX  arm gd32f4 gd32f470v_start
nsh>
```

## 烧录 (Flashing)

板载 GD-Link 调试器通过 USB Mini-B 口提供 CMSIS-DAP SWD 接口。
推荐使用 OpenOCD ≥ 0.11：

```
$ openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
          -c "program nuttx.bin verify reset exit 0x08000000"
```

这里使用 `stm32f4x.cfg` 是因为 GD32F470 与 STM32F407/F427 的 SWD
Flash 编程协议二进制兼容。外接 SEGGER J-Link 等价：

```
$ JLinkExe -device GD32F470VK -if SWD -speed 4000 -autoconnect 1 \
           -CommanderScript flash.jlink
```

最小化的 `flash.jlink`：

```
loadbin nuttx.bin 0x08000000
r
g
qc
```

## 串口控制台 (Serial Console)

默认 `nsh` 配置将 console 路由到 **USART0 @ 115200 8N1**。板上没有
USB-to-UART 桥接芯片，需要使用外部 USB-TTL 适配器接到用户排针：

| 信号   | MCU 引脚 | 方向                |
|--------|---------|---------------------|
| TX     | PA9     | 板 → 主机           |
| RX     | PA10    | 主机 → 板           |
| GND    | GND     | 共地                |

主机端用 `picocom -b 115200 /dev/ttyUSB0`（或 `minicom`、`screen`
等）打开终端，复位板子即可看到 NSH banner。

## 定制配置

```
$ make menuconfig
```

常用切入点：

- `Board Selection → Custom Board Configuration`
  选择 console 引脚组（USART0 走 PA9/PA10 或 USART3）、HSE/IRC16
  时钟源、目标 CPU 频率（168 / 200 / 240 MHz）。
- `System Type → GD32F4 Peripheral Selection`
  在外接 breakout 板前启用具体外设（USART1..7、SPI0..5、I2C0..2、
  SDIO、GPIO 端口、DMA 通道、ADC、DAC、ENET、USB FS/HS）。
- `Application Configuration → NSH Library`
  选择 NSH 内置应用。
- `RTOS Features` — 调度器、IPC、文件系统。

## 调试 (Debugging)

板载 GD-Link 通过 SWD 完成片上调试。OpenOCD 在 `localhost:3333`
打开 GDB target：

```
# 终端 1 —— 启动 OpenOCD
$ openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg

# 终端 2 —— 连接 GDB
$ arm-none-eabi-gdb nuttx.elf
(gdb) target extended-remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) continue
```

如果不挂调试器、只想抓 panic / hardfault 文本 dump，编译前打开
`CONFIG_DEBUG_HARDFAULT=y` 和 `CONFIG_ARCH_STACKDUMP=y`，从
USART0 抓取即可。

## 硬件说明 (Hardware Notes)

- **Console UART 无板载 USB 桥**。V-START 板上没有 USB-to-UART
  转接芯片；USART0（PA9/PA10）从用户排针引出，需要外接 USB-TTL
  适配器才能连到主机。如果 PA9/PA10 被占用，可在 Kconfig
  `Console wiring → Virtual COM Port` 把 console 切到 USART3。
- **板上不带外置 SDRAM、LCD、摄像头接口**。V-START 仅把基础 I/O
  引到排针；GD32F470 SoC 内置的 EXMC SDRAM 控制器、TLI LCD
  控制器和 DCI 并行摄像头接口在板上没有走线。依赖这些控制器
  的配置（例如 480×272 TFT 上跑 LVGL）不在本板提供——这类负载
  请改用兄弟板 `gd32f470i_eval`。
- **芯片型号**。`nsh` defconfig 中 `CONFIG_ARCH_CHIP_GD32F470IK=y`，
  因为上游 NuttX 的 GD32F4 chip Kconfig 暂未提供 `GD32F470VK`
  入口。两个型号共用同一颗 SoC 裸片；V-START 的 LQFP100 封装
  引出端口 A–G（IK 的 LQFP176 封装为 A–I），写入未引出的 H/I
  端口寄存器在 GD32F470xx 数据手册 §3.2 中定义为 no-op。

## 已知限制 (Known Limitations)

下列特性在出厂配置中**有意未启用**，作为后续增强项追踪：

- **以太网 (RMII)** — 未启用。SoC 中存在 MAC 但板上没有 PHY；
  排针外接 PHY 模块后可通过现有 pinmap 驱动 RMII。
- **USB 设备 (FS / HS)** — `nsh` defconfig 未启用。配置
  `CONFIG_GD32F4_USBFS_DEVICE`（或 HS）后即可挂入设备栈。
- **外接 SPI Flash (GD25)** 与 **EEPROM (AT24)** — 驱动 hook 在
  `src/gd32f4xx_gd25.c` 和 `src/gd32f4xx_at24.c` 中保留，供用户
  通过 SPI / I²C 排针接器件后启用；板上不焊这些器件，相应的
  auto-mount Kconfig 默认关闭。
- **SDIO** — 驱动文件 (`src/gd32f4xx_sdio.c`) 已就位，但板上没有
  SD 卡座；通过 `CONFIG_MMCSD_SDIO` 可启用排针外接的 SD breakout。
- **DCI 摄像头、EXMC SDRAM、TLI TFT-LCD** — 这些片上控制器在本板上
  不参与构建，因为 V-START 硬件没有把信号引到排针。兄弟板
  `gd32f470i_eval` 覆盖这些场景。

## License

采用 **Apache License, Version 2.0**——与上游 GigaDevice 贡献保持
一致。详情见 `vendor_gigadevice` 仓库顶层的 `LICENSE` 文件。
