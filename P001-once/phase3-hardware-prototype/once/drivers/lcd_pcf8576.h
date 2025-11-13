// lcd_pcf8576.h
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lcd_pcf8576_init(void);
void lcd_pcf8576_display_all(uint8_t value);
void lcd_pcf8576_display_single(uint8_t addr, uint8_t value);
void lcd_pcf8576_display_digits(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4);

// 背光相关
void lcd_backlight_init(void);
void lcd_backlight_on(void);
void lcd_backlight_off(void);

// 可选：跑一遍原厂 demo 的测试动画
void lcd_pcf8576_demo(void);

#ifdef __cplusplus
}
#endif
