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

// EC11 旋转编码器
#define ENCODER_EC11_PIN_A   6   // OTA
#define ENCODER_EC11_PIN_B   8   // OTB
#define ENCODER_EC11_PIN_C   7   // OTC (按键)

#define encoder_none  0
#define cw            1   // 顺时针
#define ccw           2   // 逆时针
#define key           3   // 按键
