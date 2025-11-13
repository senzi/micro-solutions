// board.h
#pragma once
#include <stdint.h>

// I2C / LCD
#define LCD_SDA_PIN   2
#define LCD_SCL_PIN   3

// 背光控制：BL- 通过这个脚拉低/拉高
#define LCD_BL_PIN    4          // 替换成你实际接的 GPIO

// 极性：1 表示输出 1 = 背光点亮，0 表示输出 0 = 背光点亮
#define LCD_BL_ACTIVE_HIGH  1
