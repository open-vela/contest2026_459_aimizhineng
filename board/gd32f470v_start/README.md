# GD32F470V-START

[ English | [简体中文](README_zh-cn.md) ]

The [GD32F470V-START](https://www.gigadevice.com/product/mcu/high-performance-mcus/gd32f4xx-series/gd32f470)
is an entry-level Starter Kit from GigaDevice featuring the
**GD32F470VKT6** Arm Cortex-M4F MCU running at up to 240 MHz with
3 MB on-die Flash, 256 KB SRAM, single-precision FPU and DSP. The
board pairs the MCU with on-board user LEDs, user keys, an on-board
GD-Link debugger and Arduino-compatible header pins, making it a
canonical bring-up and peripheral-prototyping target in openvela.

## Features

  - GD32F470VKT6 (Arm Cortex-M4F + DSP, LQFP100, 240 MHz)
  - 3 MB on-die Flash (zero-wait-state at 240 MHz), 256 KB SRAM,
    64 KB TCMSRAM
  - Up to 82 GPIO routed to Arduino-compatible + 2.54 mm headers
  - On-board GD-Link debugger (CMSIS-DAP, SWD)
  - USB Mini-B port (powers the board and exposes GD-Link)
  - User LEDs, User Key, Reset Key
  - 25 MHz HSE crystal, 32.768 kHz LSE crystal

For board hardware details and schematics see the GigaDevice
documentation:

- [GD32F470 series product page](https://www.gigadevice.com/product/mcu/high-performance-mcus/gd32f4xx-series/gd32f470)
- [GD32F470VKT6 part page](https://www.gigadevice.com/product/mcu/mcus-product-selector/gd32f470vkt6)
- [GD32F470xx Datasheet Rev1.4](https://www.gd32mcu.com/data/documents/datasheet/GD32F470xx_Datasheet_Rev1.4.pdf)
- [AN056 — GD32F4xx Hardware Development Guide Rev1.2](https://www.gd32mcu.com/data/documents/applicationNote/AN056%20GD32F4xx%20Hardware%20Development%20Guide_Rev1.2.pdf)

## Directory Structure

```
vendor/gigadevice/boards/gd32f4/gd32f470v_start/
├── Kconfig                  board-level Kconfig
├── CMakeLists.txt           CMake glue
├── README.md / README_zh-cn.md
├── include/board.h          clock + LED + GPIO board definitions
├── scripts/
│   ├── ld.script            flat-build linker script
│   ├── memory.ld            protected/kernel-build memory regions
│   ├── kernel-space.ld
│   ├── user-space.ld
│   ├── gnu-elf.ld
│   └── Make.defs
├── configs/
│   └── nsh/defconfig
└── src/                     board bring-up + per-peripheral hooks
    ├── gd32f470v_start.h
    ├── gd32f4xx_boot.c, gd32f4xx_appinit.c, gd32f4xx_bringup.c
    ├── gd32f4xx_autoleds.c, gd32f4xx_userleds.c
    ├── gd32f4xx_buttons.c, gd32f4xx_gpio.c, gd32f4xx_reset.c
    ├── gd32f4xx_spi.c, gd32f4xx_sdio.c
    ├── gd32f4xx_at24.c, gd32f4xx_gd25.c
    └── gd32f4xx_romfs.{c,h}, etc.
```

## Memory Layout

| Region   | Address      | Size     | Notes                                  |
|----------|--------------|----------|----------------------------------------|
| Flash    | `0x08000000` | 3072 KB  | On-die, zero-wait-state at 240 MHz     |
| SRAM0    | `0x20000000` | 112 KB   | Main heap                              |
| SRAM1    | `0x2001C000` | 16 KB    | Available via `MM_REGIONS`             |
| SRAM2    | `0x20020000` | 64 KB    | Available via `MM_REGIONS`             |
| TCMSRAM  | `0x10000000` | 64 KB    | Tightly-coupled, accessible from CPU   |
| BKP SRAM | `0x40024000` | 4 KB     | Battery-backed                         |

## Clock Tree

The default configuration drives SYSCLK from the 25 MHz HSE crystal
through the main PLL:

```
HSE = 25 MHz                       Kconfig: GD32F470V_START_HXTAL_VALUE
        │
        ▼
    ┌── PLLP ──→ SYSCLK (200 MHz)  Kconfig: GD32F470V_START_{168,200,240}MHZ
    └── PLLQ ──→ 48 MHz            USB / SDIO / RNG
        │
        ▼
    HCLK  = SYSCLK / 1
    APB2  = HCLK   / 2
    APB1  = HCLK   / 4
```

Selecting `GD32F470V_START_USE_IRC16` switches the clock source to
the on-die 16 MHz IRC oscillator, useful when the HSE crystal is not
populated.

## Configurations

All configurations below are built using openvela's
`ARCH_BOARD_CUSTOM` mechanism, since the board folder lives outside
`nuttx/boards/`. From the `nuttx/` directory:

```
$ ./tools/configure.sh -E -l \
    ../vendor/gigadevice/boards/gd32f4/gd32f470v_start/configs/<config_name>
$ make -j$(nproc)
```

Where `<config_name>` is the name of the configuration. Build artefacts
land in `nuttx/`:

| File          | Use                                          |
|---------------|----------------------------------------------|
| `nuttx.elf`   | Full ELF with debug info, for GDB            |
| `nuttx.bin`   | Raw binary, flash to `0x08000000`            |
| `nuttx.hex`   | Intel HEX for tools that prefer HEX          |
| `nuttx.map`   | Linker map                                   |

### nsh

Basic NuttShell over **USART0 @ 115200 8N1** (TX = PA9, RX = PA10).
No on-board peripherals are mounted at boot; additional peripherals
are wired through the headers and enabled via `menuconfig`.

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

## Flashing

The on-board GD-Link debugger exposes a CMSIS-DAP SWD interface over
the USB Mini-B port. OpenOCD ≥ 0.11 is the recommended flashing tool:

```
$ openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg \
          -c "program nuttx.bin verify reset exit 0x08000000"
```

`stm32f4x.cfg` is used because GD32F470 is binary-compatible with the
STM32F407/F427 SWD flash programming protocol. An external SEGGER
J-Link works equivalently:

```
$ JLinkExe -device GD32F470VK -if SWD -speed 4000 -autoconnect 1 \
           -CommanderScript flash.jlink
```

with a minimal `flash.jlink`:

```
loadbin nuttx.bin 0x08000000
r
g
qc
```

## Serial Console

The default `nsh` configuration routes the console to **USART0 at
115200 8N1**. The board has no on-board USB-to-UART bridge, so a
USB-TTL adapter must be wired to the user header:

| Signal | MCU pin | Direction        |
|--------|---------|------------------|
| TX     | PA9     | board → host     |
| RX     | PA10    | host → board     |
| GND    | GND     | shared           |

Open the host terminal with `picocom -b 115200 /dev/ttyUSB0` (or
`minicom`, `screen`, etc.) and reset the board to see the NSH banner.

## Customising the Configuration

```
$ make menuconfig
```

Useful entry points:

- `Board Selection → Custom Board Configuration`
  selects the console pin group (USART0 on PA9/PA10 or USART3),
  the HSE/IRC16 clock source and the target CPU frequency
  (168 / 200 / 240 MHz).
- `System Type → GD32F4 Peripheral Selection`
  enables individual peripherals (USART1..7, SPI0..5, I2C0..2,
  SDIO, GPIO ports, DMA channels, ADC, DAC, ENET, USB FS/HS) before
  wiring breakout boards through the headers.
- `Application Configuration → NSH Library`
  selects the built-in apps that ship with NSH.
- `RTOS Features` — schedulers, IPC, file systems.

## Debugging

On-chip debug uses SWD via the on-board GD-Link. OpenOCD opens a
GDB target at `localhost:3333`:

```
# Terminal 1 — start OpenOCD
$ openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg

# Terminal 2 — connect GDB
$ arm-none-eabi-gdb nuttx.elf
(gdb) target extended-remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) continue
```

For text crash logs (panic / hardfault dumps) without an attached
debugger, enable `CONFIG_DEBUG_HARDFAULT=y` and
`CONFIG_ARCH_STACKDUMP=y` and capture the dump from USART0.

## Hardware Notes

- **Console UART has no on-board USB bridge**. The V-START board does
  not include a USB-to-UART converter; USART0 (PA9/PA10) is exposed
  on the user headers and requires an external USB-TTL adapter to
  reach the host. Selecting `Console wiring → Virtual COM Port` in
  Kconfig switches the console to USART3 if PA9/PA10 are needed for
  another use.
- **No external SDRAM, LCD or camera connector**. The V-START board
  routes only basic I/O to the headers; the EXMC SDRAM controller,
  TLI LCD controller and DCI parallel camera interface present in
  the GD32F470 SoC have no on-board target. Configurations that
  rely on these (for example LVGL on a 480×272 TFT) are not provided
  on this board — see the sibling `gd32f470i_eval` board for those
  workloads.
- **Chip variant**. The `nsh` defconfig selects
  `CONFIG_ARCH_CHIP_GD32F470IK=y` because the GD32F4 chip Kconfig in
  upstream NuttX does not yet expose a `GD32F470VK` variant. The two
  variants share the same SoC die; the LQFP100 V-START package
  exposes ports A–G (vs. A–I on the LQFP176 IK package), and writes
  to the absent ports H/I behave as no-ops per the GD32F470xx
  datasheet §3.2.

## Known Limitations

These items are intentionally **not** wired up by the shipped
configuration; they are tracked as future enhancements:

- **Ethernet (RMII)** — not enabled. The MAC is present in the SoC
  but no PHY is on-board; an external PHY module on the headers can
  drive `STM32-style` RMII through the existing pinmap.
- **USB Device (FS / HS)** — not enabled in the `nsh` defconfig.
  `CONFIG_GD32F4_USBFS_DEVICE` (or HS) wires the device stack to
  USART0's place once configured.
- **External SPI Flash (GD25)** and **EEPROM (AT24)** — driver hooks
  are retained in `src/gd32f4xx_gd25.c` and `src/gd32f4xx_at24.c` for
  users who attach the chips through the SPI / I²C headers, but no
  device is on-board and the auto-mount Kconfigs default to off.
- **SDIO** — driver is present (`src/gd32f4xx_sdio.c`) but no SD slot
  is on-board; an SD breakout on the headers can be enabled through
  `CONFIG_MMCSD_SDIO`.
- **DCI camera, EXMC SDRAM, TLI TFT-LCD** — the on-die controllers
  are not exercised on this board because the V-START hardware does
  not route the signals to the headers. The `gd32f470i_eval` sibling
  board covers these workloads.

## License

Released under the **Apache License, Version 2.0** — same as the
upstream GigaDevice contribution. See the top-level `LICENSE` file
in the `vendor_gigadevice` repository.
