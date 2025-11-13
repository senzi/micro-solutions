#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "drivers/lcd_pcf8576.h"
#include "drivers/board.h"

int main() {
    stdio_init_all();   // 如果不需要串口打印，留着也无所谓
    sleep_ms(200);      // 上电稍微等一会

    lcd_pcf8576_init();

    while (1) {
        lcd_pcf8576_demo();
        // demo 里已经有 while 循环感很强的效果了，这里可以再加点自己的逻辑
        // 比如只跑一遍 demo，然后改成固定显示
        // lcd_pcf8576_display_digits(...);
    }

    return 0;
}