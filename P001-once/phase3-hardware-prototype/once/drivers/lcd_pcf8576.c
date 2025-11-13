#include "lcd_pcf8576.h"
#include "board.h"           // ← 就这一句，让它接管 PIN 定义
#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>
#include "hardware/gpio.h"


// 这里沿用原例程里的常量
#define IC_ADDR        0x70      // 这里是 8bit 总线值，照搬原例程
#define LCD_MODE_SET   0xC9      // 模式设置命令

// 段码表：0~9 和 "-"
static const uint8_t LCD_Digit[11] = {
    0x7e,  // "0"
    0x12,  // "1"
    0xbc,  // "2"
    0xb6,  // "3"
    0xd2,  // "4"
    0xe6,  // "5"
    0xee,  // "6"
    0x32,  // "7"
    0xfe,  // "8"
    0xf6,  // "9"
    0x80   // "-"
};

#define DOT_ON   0x01
#define COL_ON   0x01

#define LCD_ON   0xff
#define LCD_OFF  0x00

#define ADDR_NUM1 0x00
#define ADDR_NUM2 0x08
#define ADDR_NUM3 0x10
#define ADDR_NUM4 0x18

// 内部函数声明
static void iic_start(void);
static void iic_stop(void);
static void iic_delay(void);
static void iic_send_byte(uint8_t data);

// 对原 SendNBitToRAM 的等价实现
static inline void SendNBitToRAM(uint8_t data) {
    iic_send_byte(data);
}

static void iic_delay(void) {
    // I²C 低速足够，给个几微秒的空隙就行
    sleep_us(4);
}

static void iic_start(void) {
    gpio_set_dir(LCD_SDA_PIN, GPIO_OUT);
    gpio_put(LCD_SDA_PIN, 1);
    gpio_put(LCD_SCL_PIN, 1);
    iic_delay();
    gpio_put(LCD_SDA_PIN, 0);
    iic_delay();
    gpio_put(LCD_SCL_PIN, 0);
    iic_delay();
}

static void iic_stop(void) {
    gpio_set_dir(LCD_SDA_PIN, GPIO_OUT);
    gpio_put(LCD_SDA_PIN, 0);
    iic_delay();
    gpio_put(LCD_SCL_PIN, 1);
    iic_delay();
    gpio_put(LCD_SDA_PIN, 1);
    iic_delay();
}

// 发送 1 字节并简单处理 ACK
static void iic_send_byte(uint8_t data) {
    for (int i = 0; i < 8; ++i) {
        gpio_set_dir(LCD_SDA_PIN, GPIO_OUT);
        gpio_put(LCD_SDA_PIN, (data & 0x80) != 0);
        iic_delay();
        gpio_put(LCD_SCL_PIN, 1);
        iic_delay();
        gpio_put(LCD_SCL_PIN, 0);
        iic_delay();
        data <<= 1;
    }

    // 释放 SDA，读 ACK（低有效），为了安全不 busy-wait
    gpio_put(LCD_SDA_PIN, 1);
    gpio_set_dir(LCD_SDA_PIN, GPIO_IN);
    iic_delay();
    gpio_put(LCD_SCL_PIN, 1);
    iic_delay();
    (void)gpio_get(LCD_SDA_PIN);  // 如需检查 ACK，可在此读
    gpio_put(LCD_SCL_PIN, 0);
    iic_delay();
    gpio_set_dir(LCD_SDA_PIN, GPIO_OUT);
}

// ========== 对外接口 ==========

void lcd_pcf8576_init(void) {
    // I2C 相关初始化
    gpio_init(LCD_SDA_PIN);
    gpio_init(LCD_SCL_PIN);
    gpio_set_dir(LCD_SDA_PIN, GPIO_OUT);
    gpio_set_dir(LCD_SCL_PIN, GPIO_OUT);
    gpio_put(LCD_SDA_PIN, 1);
    gpio_put(LCD_SCL_PIN, 1);

    // 背光
    lcd_backlight_init();
    lcd_backlight_on();

    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(LCD_MODE_SET);
    iic_stop();
}


void lcd_pcf8576_display_all(uint8_t value) {
    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(0x00);          // 起始地址
    for (int i = 0; i < 4; ++i) { // 四个“数字位”
        SendNBitToRAM(value);
    }
    iic_stop();
}

void lcd_pcf8576_display_single(uint8_t addr, uint8_t value) {
    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(addr);
    SendNBitToRAM(value);
    iic_stop();
}

void lcd_pcf8576_display_digits(uint8_t d1, uint8_t d2, uint8_t d3, uint8_t d4) {
    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(0x00);

    // 保持和原例程一致：前三位加小数点，最后一位加 COL
    SendNBitToRAM(d1 + DOT_ON);
    SendNBitToRAM(d2 + DOT_ON);
    SendNBitToRAM(d3 + DOT_ON);
    SendNBitToRAM(d4 + COL_ON);

    iic_stop();
}

static int bl_step = 0;
static bool bl_on = true;

static void toggle_bl_every_2(void) {
    bl_step++;
    if ((bl_step % 2) == 0) {
        bl_on = !bl_on;
        if (bl_on) {
            lcd_backlight_on();
        } else {
            lcd_backlight_off();
        }
    }
}

void lcd_pcf8576_demo(void) {
    // 全亮
    lcd_pcf8576_display_all(LCD_ON);
    sleep_ms(1000);
    toggle_bl_every_2();

    lcd_pcf8576_display_all(LCD_OFF);
    sleep_ms(500);
    toggle_bl_every_2();

    // 5 → 5. → 5.1 → 5.1. → 5.1.9 → 5.1.9.0 → 5.1.:9.0
    lcd_pcf8576_display_single(ADDR_NUM1, LCD_Digit[5]);
    sleep_ms(500);
    toggle_bl_every_2();

    lcd_pcf8576_display_single(ADDR_NUM1, LCD_Digit[5] + DOT_ON);
    sleep_ms(500);
    toggle_bl_every_2();

    lcd_pcf8576_display_single(ADDR_NUM2, LCD_Digit[1]);
    sleep_ms(500);
    toggle_bl_every_2();

    lcd_pcf8576_display_single(ADDR_NUM2, LCD_Digit[1] + DOT_ON);
    sleep_ms(500);
    toggle_bl_every_2();

    lcd_pcf8576_display_single(ADDR_NUM3, LCD_Digit[9]);
    sleep_ms(500);
    toggle_bl_every_2();

    lcd_pcf8576_display_single(ADDR_NUM3, LCD_Digit[9] + DOT_ON);
    sleep_ms(500);
    toggle_bl_every_2();

    lcd_pcf8576_display_single(ADDR_NUM4, LCD_Digit[0]);
    sleep_ms(500);
    toggle_bl_every_2();

    lcd_pcf8576_display_single(ADDR_NUM4, LCD_Digit[0] + COL_ON);
    sleep_ms(1500);
    toggle_bl_every_2();

    // 显示 2.9.-7.8
    lcd_pcf8576_display_digits(
        LCD_Digit[2],
        LCD_Digit[9],
        LCD_Digit[7],
        LCD_Digit[8]
    );
    sleep_ms(1500);
    toggle_bl_every_2();

    // 循环 -.1.-9.0~9
    for (int i = 0; i < 10; ++i) {
        lcd_pcf8576_display_digits(
            LCD_Digit[10], // "-"
            LCD_Digit[1],
            LCD_Digit[9],
            LCD_Digit[i]
        );
        sleep_ms(500);
        toggle_bl_every_2();
    }
}

void lcd_backlight_init(void) {
    gpio_init(LCD_BL_PIN);
    gpio_set_dir(LCD_BL_PIN, GPIO_OUT);

#if LCD_BL_ACTIVE_HIGH
    gpio_put(LCD_BL_PIN, 0);    // 默认关灯
#else
    gpio_put(LCD_BL_PIN, 1);
#endif
}

void lcd_backlight_on(void) {
#if LCD_BL_ACTIVE_HIGH
    gpio_put(LCD_BL_PIN, 1);
#else
    gpio_put(LCD_BL_PIN, 0);
#endif
}

void lcd_backlight_off(void) {
#if LCD_BL_ACTIVE_HIGH
    gpio_put(LCD_BL_PIN, 0);
#else
    gpio_put(LCD_BL_PIN, 1);
#endif
}

// 显示 MM:SS，分钟和秒都限定在 0~59
void lcd_pcf8576_show_time_mmss(uint8_t minutes, uint8_t seconds) {
    if (minutes > 59) minutes = 59;
    if (seconds > 59) seconds = 59;

    uint8_t m_t = minutes / 10;
    uint8_t m_u = minutes % 10;
    uint8_t s_t = seconds / 10;
    uint8_t s_u = seconds % 10;

    // 这里用原来那套单独写寄存器的方式，保持灵活度
    // 你可以根据实物的实际段映射，决定哪些位要加 DOT/COL
    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(ADDR_NUM1);
    SendNBitToRAM(LCD_Digit[m_t]);             // 第 1 位：分钟十位
    iic_stop();

    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(ADDR_NUM2);
    SendNBitToRAM(LCD_Digit[m_u]);             // 第 2 位：分钟个位
    iic_stop();

    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(ADDR_NUM3);
    SendNBitToRAM(LCD_Digit[s_t]);             // 第 3 位：秒钟十位
    iic_stop();

    iic_start();
    SendNBitToRAM(IC_ADDR);
    SendNBitToRAM(ADDR_NUM4);
    SendNBitToRAM(LCD_Digit[s_u] + COL_ON);    // 第 4 位：秒钟个位，同时点亮冒号
    iic_stop();
}
