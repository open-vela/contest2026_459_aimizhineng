#ifndef __EXAMPLES_HIFOSS_OLED_H
#define __EXAMPLES_HIFOSS_OLED_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

int  oled_init(void);               /* 打开设备并完成 SSD1306 初始化+显示 */
void oled_clear(void);              /* 清屏 */
void oled_set_pos(uint8_t x, uint8_t y);
void oled_show_char(uint8_t x, uint8_t y, uint8_t chr);
void oled_show_string(uint8_t x, uint8_t y, const char *chr);

#endif /* __EXAMPLES_HIFOSS_OLED_H */
