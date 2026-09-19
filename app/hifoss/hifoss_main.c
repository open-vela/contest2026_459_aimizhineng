/****************************************************************************
 * examples/hifoss/hifoss_main.c
 *
 * 语音模块 + OLED 演示主逻辑，移植自裸机 User/main.c：
 *   裸机流程：systick_config -> usart_gpio_config(9600)
 *             -> wifi_gpio_config(115200) -> LCD_Init()
 *             -> OLED_ShowString(12,2,"Hello OpenVela")
 *             -> oled_update_state(STATE_IDLE)
 *             -> while(1){ 收完一帧 -> 打印 -> 解析更新状态 -> 清缓冲;
 *                          delay 10ms; }
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <nuttx/ioexpander/gpio.h>

#include "oled.h"
#include "esp8266_at.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* 逐字移植自裸机 main.c */

typedef enum
{
  STATE_IDLE = 0,      /* 空闲 */
  STATE_WAKEUP,        /* 已唤醒 */
  STATE_LISTENING,     /* 拾音中 */
  STATE_RESPONDING,    /* 应答中 */
} voice_state_t;

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* 设备控制节点：/dev/gpio4=PA8(LED) /dev/gpio5=PD4(FAN)
 * (gpio 编号规则：先输入后输出再中断，OUT4/OUT5 为板级新增输出，
 *  原 PB2 中断顺延至 /dev/gpio6，见板级 gd32f4xx_gpio.c)
 */

#define LED_DEV   "/dev/gpio4"
#define FAN_DEV   "/dev/gpio5"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static voice_state_t g_voice_state = STATE_IDLE;

static int g_led_fd = -1;
static int g_fan_fd = -1;

/* 本帧接收完成时刻（ms），用于测量 接收->执行 链路延迟 */

static unsigned long g_rx_ms;

/* VAD 事件时刻（ms），用于测量 说话->执行 端到端延迟 */

static unsigned long g_vad_start_ms;
static unsigned long g_vad_end_ms;

/****************************************************************************
 * Name: now_ms
 *
 * Description:
 *   返回单调时钟毫秒数，用于延迟测量。
 *
 ****************************************************************************/

static unsigned long now_ms(void)
{
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)(ts.tv_sec * 1000ul + ts.tv_nsec / 1000000ul);
}

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: oled_update_state
 *
 * Description:
 *   刷新 OLED 显示：标题行 + 当前语音状态（逐字移植自裸机 main.c）。
 *
 ****************************************************************************/

static void oled_update_state(voice_state_t state)
{
  /* 标题行 */

  oled_show_string(12, 2, "Hello OpenVela  ");

  /* 状态行 */

  switch (state)
    {
      case STATE_IDLE:
        oled_show_string(5, 5, "      IDLE      ");
        break;
      case STATE_WAKEUP:
        oled_show_string(5, 5, "     WAKEUP   ");
        break;
      case STATE_LISTENING:
        oled_show_string(5, 5, "   LISTENING ");
        break;
      case STATE_RESPONDING:
        oled_show_string(5, 5, "   RESPONDING   ");
        break;
      default:
        break;
    }
}

/****************************************************************************
 * Name: device_init
 *
 * Description:
 *   打开 LED(PA8) / 风扇(PD4) 的 GPIO 设备节点。
 *
 ****************************************************************************/

static void device_init(void)
{
  g_led_fd = open(LED_DEV, O_RDWR);
  g_fan_fd = open(FAN_DEV, O_RDWR);
  if (g_led_fd < 0 || g_fan_fd < 0)
    {
      printf("device: open gpio failed led=%d fan=%d\n", g_led_fd, g_fan_fd);
    }
}

/* 执行耗时统计（us）：min/max/sum/count */

static uint64_t      g_exec_min = UINT64_MAX;
static uint64_t      g_exec_max;
static uint64_t      g_exec_sum;
static unsigned int  g_exec_cnt;

/* 当前时刻（us，DWT 周期计数器，见 esp8266_at.h） */

static uint64_t now_us(void)
{
  return hifoss_now_us();
}

/* GPIO 执行完成后打印耗时统计：[URC_TIMING] exec=N us 含 min/max/avg
 * 起点 = 本帧最后一个 '\n' 到达时刻（esp8266_at.c 记录）
 */

static void urc_timing_print(const char *dev, bool on)
{
  printf("%s: %s (vad_end=%lums vad_start=%lums)\n",
         dev, on ? "on" : "off",
         now_ms() - g_vad_end_ms, now_ms() - g_vad_start_ms);

  if (g_rx_newline_valid)
    {
      uint64_t exec = now_us() - g_rx_newline_us;

      if (exec < g_exec_min) g_exec_min = exec;
      if (exec > g_exec_max) g_exec_max = exec;
      g_exec_sum += exec;
      g_exec_cnt++;

      printf("[URC_TIMING] exec=%lluus min=%lluus max=%lluus "
             "avg=%lluus n=%u\n",
             (unsigned long long)exec,
             (unsigned long long)g_exec_min,
             (unsigned long long)g_exec_max,
             (unsigned long long)(g_exec_sum / g_exec_cnt),
             g_exec_cnt);
    }
}

/****************************************************************************
 * Name: device_led_set / device_fan_set
 *
 * Description:
 *   控制 LED(PA8) / 风扇(PD4) 通断。
 *
 ****************************************************************************/

static void device_led_set(bool on)
{
  if (g_led_fd >= 0)
    {
      ioctl(g_led_fd, GPIOC_WRITE, (unsigned long)on);
      urc_timing_print("led", on);
    }
}

static void device_fan_set(bool on)
{
  if (g_fan_fd >= 0)
    {
      /* 风扇模块低电平有效，逻辑取反 */

      ioctl(g_fan_fd, GPIOC_WRITE, (unsigned long)!on);
      urc_timing_print("fan", on);
    }
}

/****************************************************************************
 * Name: parse_and_update_state
 *
 * Description:
 *   在接收缓冲中匹配语音模块事件串并更新状态（逐字移植自裸机 main.c），
 *   并解析 +LED:ON/+LED:OFF/+FAN:ON/+FAN:OFF 设备控制指令。
 *
 ****************************************************************************/

static void parse_and_update_state(const char *buf, uint32_t len)
{
  voice_state_t new_state = g_voice_state;

  if (strstr(buf, "+WAKEUP"))
    {
      new_state = STATE_WAKEUP;
    }
  else if (strstr(buf, "+VAD:START"))
    {
      new_state = STATE_LISTENING;
      g_vad_start_ms = g_rx_ms;
    }
  else if (strstr(buf, "+VAD:END"))
    {
      new_state = STATE_RESPONDING;
      g_vad_end_ms = g_rx_ms;
    }
  else if (strstr(buf, "EXIT_WAKEUP"))
    {
      new_state = STATE_IDLE;
    }

  /* 状态发生变化时才刷新 OLED */

  if (new_state != g_voice_state)
    {
      g_voice_state = new_state;
      oled_update_state(g_voice_state);
    }

  /* 设备控制指令 */

  if (strstr(buf, "+LED:ON"))
    {
      device_led_set(true);
    }
  else if (strstr(buf, "+LED:OFF"))
    {
      device_led_set(false);
    }
  else if (strstr(buf, "+FAN:ON"))
    {
      device_fan_set(true);
    }
  else if (strstr(buf, "+FAN:OFF"))
    {
      device_fan_set(false);
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hifoss_main
 ****************************************************************************/

int hifoss_main(int argc, char *argv[])
{
  int n;

  /* OLED 初始化并显示开机画面（对应裸机 LCD_Init()） */

  oled_init();

  /* 语音模块串口初始化（对应裸机 wifi_gpio_config(115200)，
   * UART3 PC10/PC11；裸机 AT/MQTT 序列已注释，仅打开串口）
   */

  wifi_init();

  /* 使能 DWT 周期计数器（微秒级耗时统计用） */

  hifoss_timer_init();

  /* 打开 LED / 风扇 GPIO 设备 */

  device_init();

  /* 对应裸机 LCD_Init 后的两行显示调用 */

  oled_show_string(12, 2, "Hello OpenVela");
  oled_update_state(STATE_IDLE);

  while (1)
    {
      /* 收完一帧（对应裸机 g_recv_complete_flag 分支） */

      n = wifi_poll_rx();
      if (n > 0)
        {
          /* 记录本帧接收完成时刻 */

          g_rx_ms = now_ms();

          /* 打印接收数据 */

          printf("g_recv_length:%d ", n);
          printf("g_recv_buff:%s\r\n", g_recv_buff);

          /* 解析并更新状态 */

          parse_and_update_state(g_recv_buff, n);

          /* 清空接收缓冲 */

          memset(g_recv_buff, 0, sizeof(g_recv_buff));
        }

      /* 轮询间隔 2ms（URC 以 \n 判帧即时返回后，间隔决定响应下限） */

      usleep(2 * 1000);
    }

  return 0;
}
