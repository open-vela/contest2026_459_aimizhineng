/****************************************************************************
 * examples/hifoss/esp8266_at.h
 *
 * 语音模块串口驱动
 *
 * 移植自裸机 bsp_wifi.c/h
 * 串口: /dev/ttyS1 (UART3 PC10/PC11, 115200 8N1)
 *
 ****************************************************************************/

#ifndef __EXAMPLES_HIFOSS_ESP8266_AT_H
#define __EXAMPLES_HIFOSS_ESP8266_AT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* DWT 周期计数器：微秒级精确计时（Cortex-M4 DWT->CYCCNT，CPU 240MHz）
 * 系统 tick 为 10ms 粒度，测不出微秒级执行耗时，故用 DWT。
 */

#define HIFOSS_DWT_DEMCR    (*(volatile uint32_t *)0xe000edfc)
#define HIFOSS_DWT_CTRL     (*(volatile uint32_t *)0xe0001000)
#define HIFOSS_DWT_CYCCNT   (*(volatile uint32_t *)0xe0001004)
#define HIFOSS_CPU_HZ       240000000u

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/* 使能 DWT CYCCNT（上电调用一次） */

static inline void hifoss_timer_init(void)
{
  HIFOSS_DWT_DEMCR  |= (1u << 24);   /* TRCENA */
  HIFOSS_DWT_CYCCNT  = 0;
  HIFOSS_DWT_CTRL   |= 1u;           /* CYCCNTENA */
}

/* 当前时刻（us，由 DWT 周期数换算；约 17.9s 回绕一次，测差值足够） */

static inline uint64_t hifoss_now_us(void)
{
  return (uint64_t)HIFOSS_DWT_CYCCNT * 1000000ull / HIFOSS_CPU_HZ;
}

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* 接收缓冲，对应裸机 g_recv_buff[4096] */

extern char g_recv_buff[4096];

/* 本帧最后一个 '\n' 到达时刻（CLOCK_MONOTONIC, us）及有效标志 */

extern uint64_t g_rx_newline_us;
extern int      g_rx_newline_valid;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/* 打开 /dev/ttyS1 并配置 115200 8N1，返回 0 成功 */

int wifi_uart_open(void);

/* 发送字符串（对应裸机 wifi_send_string） */

void wifi_send_string(const char *str);

/* AT 初始化序列：复位 -> STA -> 连 WiFi -> 华为云 MQTT，对应裸机 wifi_init */

void wifi_init(void);

/* 轮询接收：30ms 空闲判一帧（替代裸机 IDLE 中断），返回本帧字节数，0 表示无数据 */

int wifi_poll_rx(void);

#endif /* __EXAMPLES_HIFOSS_ESP8266_AT_H */
