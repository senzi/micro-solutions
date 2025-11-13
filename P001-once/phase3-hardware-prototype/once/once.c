#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "drivers/lcd_pcf8576.h"
#include "drivers/board.h"

int main() {
    stdio_init_all();
    sleep_ms(200);

    lcd_pcf8576_init();
    lcd_backlight_off();  // 默认关背光

    uint8_t minutes = 0;
    uint8_t seconds = 0;

    lcd_pcf8576_show_time_mmss(minutes, seconds);

    uint64_t now = time_us_64();
    uint64_t next_tick_us = now + 1000000;     // 下一次“整秒”
    bool blink_active = false;
    bool bl_on = false;
    uint64_t blink_end_us = 0;
    uint64_t blink_next_toggle_us = 0;

    while (true) {
        now = time_us_64();

        // 计时节拍：每 1 秒更新一次 MM:SS
        if (now >= next_tick_us) {
            next_tick_us += 1000000;

            seconds++;
            if (seconds >= 60) {
                seconds = 0;
                minutes++;
                if (minutes >= 60) {
                    minutes = 0;
                }
            }

            lcd_pcf8576_show_time_mmss(minutes, seconds);

            // 每到 15 分钟倍数的整分钟（15:00, 30:00, 45:00, 00:00 不提醒，避免上电就闪）
            if (seconds == 0 && minutes > 0 && (minutes % 15 == 0)) {
                blink_active = true;
                bl_on = false;
                lcd_backlight_off();

                // 闪烁 3 秒，200ms 切换一次背光 => 7~8 次闪烁
                blink_end_us = now + 3000000;
                blink_next_toggle_us = now;
            }
        }

        // 背光闪烁状态机：不影响 1Hz 计时
        if (blink_active) {
            if (now >= blink_end_us) {
                blink_active = false;
                bl_on = false;
                lcd_backlight_off();
            } else if (now >= blink_next_toggle_us) {
                bl_on = !bl_on;
                if (bl_on) {
                    lcd_backlight_on();
                } else {
                    lcd_backlight_off();
                }
                blink_next_toggle_us += 200000;  // 每 200ms 翻转一次
            }
        }

        tight_loop_contents();   // 给 SDK 一点点喘息空间
    }

    return 0;
}
