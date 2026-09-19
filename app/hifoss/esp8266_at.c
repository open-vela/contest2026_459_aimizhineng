/****************************************************************************
 * examples/hifoss/esp8266_at.c
 *
 * 语音模块串口驱动（UART3 / /dev/ttyS1, 115200 8N1, TX=PC10 RX=PC11）
 * 移植自裸机 Hardware/wifi/bsp_wifi.c：
 *   - 发送：逐字节等 TBE  ->  write() 一次发出（驱动内部处理）
 *   - 接收：RBNE + IDLE 中断判帧  ->  poll() + 30ms 空闲判帧
 *   - 裸机 wifi_gpio_config() 中的 AT/MQTT 序列已注释，此处同样不发送
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <termios.h>
#include <time.h>
#include <sys/ioctl.h>

#include "esp8266_at.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define WIFI_DEV        "/dev/ttyS1"   /* UART3: TX=PC10, RX=PC11 */

/* 对应裸机 delay_1ms() */

#define WIFI_DELAY_MS(ms)   usleep((useconds_t)(ms) * 1000)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static int g_wifi_fd = -1;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* 对应裸机 g_recv_buff[4096] */

char g_recv_buff[4096];

/* 本帧最后一个 '\n' 到达的时刻（CLOCK_MONOTONIC, us），用于执行耗时统计 */

uint64_t g_rx_newline_us;
int      g_rx_newline_valid;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: wifi_uart_open
 *
 * Description:
 *   打开 UART3 并配置为 115200 8N1（对应裸机 wifi_gpio_config）。
 *
 ****************************************************************************/

int wifi_uart_open(void)
{
  struct termios tio;
  int ret;

  if (g_wifi_fd >= 0)
    {
      return 0;
    }

  g_wifi_fd = open(WIFI_DEV, O_RDWR);
  if (g_wifi_fd < 0)
    {
      printf("open %s failed: %d\n", WIFI_DEV, errno);
      return -1;
    }

  ret = tcgetattr(g_wifi_fd, &tio);
  if (ret < 0)
    {
      printf("tcgetattr failed: %d\n", errno);
      close(g_wifi_fd);
      g_wifi_fd = -1;
      return -1;
    }

  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);

  tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
  tio.c_cflag |= CS8 | CREAD | CLOCAL;

  /* 原始模式：不做任何输入/输出加工，关闭回显与信号 */

  tio.c_iflag = 0;
  tio.c_oflag = 0;
  tio.c_lflag = 0;

  tio.c_cc[VMIN]  = 0;
  tio.c_cc[VTIME] = 0;

  ret = tcsetattr(g_wifi_fd, TCSANOW, &tio);
  if (ret < 0)
    {
      printf("tcsetattr failed: %d\n", errno);
      close(g_wifi_fd);
      g_wifi_fd = -1;
      return -1;
    }

  /* 清空驱动里可能残留的数据 */

  tcflush(g_wifi_fd, TCIOFLUSH);

  return 0;
}

/****************************************************************************
 * Name: wifi_send_string
 *
 * Description:
 *   通过 UART3 发送字符串（对应裸机 wifi_send_string，
 *   裸机逐字节等待 TBE，这里由串口驱动完成）。
 *
 ****************************************************************************/

void wifi_send_string(const char *str)
{
  size_t len = strlen(str);
  size_t sent = 0;

  if (g_wifi_fd < 0)
    {
      return;
    }

  while (sent < len)
    {
      ssize_t n = write(g_wifi_fd, str + sent, len - sent);
      if (n < 0)
        {
          if (errno == EINTR)
            {
              continue;
            }

          printf("wifi write failed: %d\n", errno);
          break;
        }

      sent += (size_t)n;
    }
}

/****************************************************************************
 * Name: wifi_poll_rx
 *
 * Description:
 *   轮询 UART3 接收数据。读到数据后持续接收，直到连续 30ms 无数据
 *   视为一帧结束（替代裸机的 USART IDLE 中断 + g_recv_complete_flag）。
 *
 * Return:
 *   本帧接收到的字节数；0 表示当前无数据。
 *
 ****************************************************************************/

int wifi_poll_rx(void)
{
  struct pollfd pfd;
  size_t total = 0;

  if (g_wifi_fd < 0)
    {
      return 0;
    }

  pfd.fd     = g_wifi_fd;
  pfd.events = POLLIN;

  g_rx_newline_valid = 0;

  for (; ; )
    {
      int ret = poll(&pfd, 1, 30);   /* 30ms 空闲超时判帧 */
      if (ret <= 0)
        {
          break;                     /* 超时或出错：帧结束 */
        }

      if (pfd.revents & POLLIN)
        {
          ssize_t n = read(g_wifi_fd, g_recv_buff + total,
                           sizeof(g_recv_buff) - 1 - total);
          if (n <= 0)
            {
              break;
            }

          /* 若本块数据含 '\n'（URC 帧尾）：立即打微秒级时间戳，
           * 并直接判帧返回（URC 以 \n 结尾，无需再等 30ms 空闲）。
           * 一帧可能含多条 URC，最后一条的时间戳覆盖前者。
           */

          if (memchr(g_recv_buff + total, '\n', n) != NULL)
            {
              g_rx_newline_us    = hifoss_now_us();
              g_rx_newline_valid = 1;

              total += (size_t)n;
              break;               /* \n 结尾，帧完整，立即返回 */
            }

          total += (size_t)n;
          if (total >= sizeof(g_recv_buff) - 1)
            {
              break;                 /* 缓冲满，强制结束 */
            }
        }
      else
        {
          break;
        }
    }

  g_recv_buff[total] = '\0';
  return (int)total;
}

/****************************************************************************
 * Name: wifi_init
 *
 * Description:
 *   语音模块串口初始化（对应裸机 wifi_gpio_config）：仅完成 UART 初始化。
 *   裸机中的 AT/MQTT 发送序列已全部注释——该外挂模块为语音模块，
 *   上电后通过 +WAKEUP/+VAD:START/+VAD:END/EXIT_WAKEUP 事件主动上报，
 *   无需 MCU 发送任何指令。
 *
 ****************************************************************************/

void wifi_init(void)
{
  if (wifi_uart_open() < 0)
    {
      printf("voice uart open failed!\n");
      return;
    }

  printf("voice uart ready!\n");
}
