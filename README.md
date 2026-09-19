# VelaGate — 基于 openvela 的语音控制智能网关

## 一、作品简介

VelaGate 是一个基于 openvela（NuttX 系 RTOS）的语音控制智能网关，运行在 GD32F470ZE MCU 开发板（512K Flash / 256K RAM，Cortex-M4F）上。

系统采用多级协同架构：

```text
CI130x 语音前端（本地唤醒/识别）
    → WS63 网关（WiFi/云端转发）
    → 云端大模型（语义理解）
    → WS63 → UART(115200)
    → GD32F470 + openvela（核心主控：指令解析、外设控制、状态显示）
    → 外设（SSD1306 OLED / LED / 风扇继电器等）
```

openvela 主控侧亮点：

- **UART 指令成帧与 GPIO 控制**：`\n` 即时成帧解析 `+LED`/`-LED`/`+FAN`/`-FAN` 等指令，DWT 周期级计时，节点执行延迟实测平均 337µs
- **SSD1306 OLED 状态面板**：SPI0 驱动，实时显示系统与外设状态
- **端到端语音控制链路**：唤醒 → 云端理解 → 设备动作，端到端平均 1540ms

## 二、选题方向

- **新硬件适配**：完成 GD32F470ZE MCU 开发板的 openvela 板级适配（时钟 240MHz、USART/SPI/GPIO 驱动接入、NSH 控制台、littlefs）。
- **AI 硬件产品创新**：在适配好的硬件之上构建语音控制网关这一 AIoT 场景。

## 三、目录结构

- `app/hifoss/`          — 主控应用：UART 指令解析、ESP8266 AT、OLED 显示（映射到 `packages/demos/contest2026_459_hifoss`）
- `board/gd32f470v_start/` — GD32F470ZE MCU 板级适配：defconfig、链接脚本、板级初始化与 GPIO 注册（映射到 `vendor/openvela/boards/contest2026_459_gd32f470v_start`）
- `logs/`                — AI Coding 日志（见 logs/README.md）
- `.trae/skills/`        — 项目沉淀的 AI 技能：`gd32-vela-port-check`（GD32 移植检查清单）、`word-report-generator`（Word 报告生成）
- `contest2026_459_aimizhineng.xml` — repo manifest（含上述 linkfile 映射）

## 四、运行方式

### 1. 拉取工程

```bash
repo init -u https://github.com/open-vela/contest2026_459_aimizhineng \
  -b dev-ai-contest-2026 -m contest2026_459_aimizhineng.xml
repo sync -c -j8
```

### 2. 编译

在 openvela 工作区根目录执行：

```bash
./build.sh vendor/openvela/boards/contest2026_459_gd32f470v_start/configs/hifoss -j8
```

产物为 `nuttx/nuttx.bin`（及 `nuttx.hex`）。如需纯 NSH 基线，可使用同目录下 `configs/nsh`。

### 3. 烧录与运行

使用 J-Link 将 `nuttx.bin` 烧录至 GD32F470V-START（烧录工具与接线见板卡手册）。

接线：

| 信号 | 引脚 | 说明 |
| ---- | ---- | ---- |
| 控制台 USART0 | PA9(TX) / PA10(RX) | 115200 8N1，NSH Shell |
| 指令 UART3 | PC10(TX) / PC11(RX) | 115200，接 WS63 网关 |
| OLED SPI0 | PA5(SCK) PA7(MOSI) PA4(CS) PA6(DC) PA1(RES) | SSD1306 |

上电后 OLED 显示系统状态，NSH 控制台输入 `hifoss` 启动应用（defconfig 已配置为内置应用）；语音唤醒后云端下发指令，网关执行对应外设动作并在 OLED 上刷新状态。

### 4. 串口运行日志（实测）

以下为一次完整语音控制会话的 USART0 控制台实测输出：

```text
NuttShell (NSH)
nsh> hifoss
voice uart ready!
g_recv_length:9 g_recv_buff:+WAKEUP
g_recv_length:12 g_recv_buff:+VAD:START
g_recv_length:10 g_recv_buff:+VAD:END
g_recv_length:9 g_recv_buff:+LED:ON
led: on (vad_end=1560ms vad_start=2370ms)
[URC_TIMING] exec=330us min=330us max=330us avg=330us n=1
g_recv_length:12 g_recv_buff:+VAD:START
g_recv_length:10 g_recv_buff:+VAD:END
g_recv_length:9 g_recv_buff:+FAN:ON
fan: on (vad_end=1480ms vad_start=2350ms)
[URC_TIMING] exec=334us min=330us max=334us avg=332us n=2
g_recv_length:12 g_recv_buff:+VAD:START
g_recv_length:10 g_recv_buff:+VAD:END
g_recv_length:10 g_recv_buff:+LED:OFF
led: off (vad_end=1530ms vad_start=2290ms)
[URC_TIMING] exec=340us min=330us max=340us avg=334us n=3
g_recv_length:12 g_recv_buff:+VAD:START
g_recv_length:12 g_recv_buff:+VAD:START
g_recv_length:10 g_recv_buff:+VAD:END
g_recv_length:10 g_recv_buff:+FAN:OFF
fan: off (vad_end=1590ms vad_start=2570ms)
[URC_TIMING] exec=345us min=330us max=345us avg=337us n=4
g_recv_length:14 g_recv_buff:+EXIT_WAKEUP
g_recv_length:9 g_recv_buff:+WAKEUP
g_recv_length:12 g_recv_buff:+VAD:START
g_recv_length:10 g_recv_buff:+VAD:END
g_recv_length:14 g_recv_buff:+EXIT_WAKEUP
```

日志关键行说明：

- `+WAKEUP` / `+EXIT_WAKEUP`：CI130x 语音前端的唤醒 / 退出唤醒事件。
- `+VAD:START` / `+VAD:END`：语音端点检测（说话开始 / 结束），`vad_start`/`vad_end` 为相对唤醒时刻的毫秒时间戳。
- `+LED:ON` 等：云端理解后经 WS63 由 UART3 下发的控制指令，hifoss 以 `\n` 即时成帧解析并执行 GPIO 动作（`led: on` 等回显）。
- `[URC_TIMING]`：基于 DWT 周期计数器的指令执行耗时统计，实测 4 次指令执行延迟 330~345µs，平均 337µs。

## 五、AI Coding 使用说明

本项目全程采用 AI 结对开发：

- **方案设计**：与 AI 讨论确定"CI130x → WS63 → 大模型 → GD32(openvela)"的多级架构与 UART 指令协议（`+LED` 等即时成帧格式）。
- **BSP 适配**：AI 辅助完成 GD32F470V 板级时钟（25MHz HXTAL → 240MHz PLL）、USART0 控制台、SPI0/GPIO 设备注册代码的编写与调试。
- **应用开发**：hifoss 应用的指令解析状态机、OLED 驱动、DWT 计时埋点（`[URC_TIMING]` 日志）由 AI 生成初版，人工评审后迭代。
- **性能优化**：AI 协助分析端到端延迟构成，将链路优化至平均 1540ms（优化幅度 99.2%）。

完整对话日志见 `logs/` 目录。
