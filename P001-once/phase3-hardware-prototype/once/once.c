#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "drivers/lcd_pcf8576.h"
#include "drivers/board.h"
#include "drivers/encoder_ec11.h"

// 小工具：根据总秒数显示 MM:SS
static void show_time_from_total_sec(uint16_t total_sec) {
    if (total_sec > 59 * 60 + 59) {
        total_sec = 59 * 60 + 59;
    }
    uint8_t mm = total_sec / 60;
    uint8_t ss = total_sec % 60;
    lcd_pcf8576_show_time_mmss(mm, ss);
}

typedef enum {
    TIMER_STATE_SET = 0,      // 设定目标时间
    TIMER_STATE_RUNNING,      // 正在计时
    TIMER_STATE_PAUSED,       // 暂停在中途某个时间
    TIMER_STATE_DONE          // 计时完成，闪烁背光
} timer_state_t;

int main() {
    stdio_init_all();
    sleep_ms(200);

    lcd_pcf8576_init();
    lcd_backlight_on();

    Encoder_Init();

    // GP9 输出高电平，给模块供电
    gpio_init(9);
    gpio_set_dir(9, GPIO_OUT);
    gpio_put(9, 1);

    uint16_t target_total_sec  = 0;   // 目标时间（秒）
    uint16_t elapsed_total_sec = 0;   // 已经计时（秒）
    show_time_from_total_sec(target_total_sec);

    printf("ENC debug start.\r\n");
    printf("A=%d B=%d C=%d\r\n",
           gpio_get(ENCODER_EC11_PIN_A),
           gpio_get(ENCODER_EC11_PIN_B),
           gpio_get(ENCODER_EC11_PIN_C));

    timer_state_t state = TIMER_STATE_SET;

    // 速度相关状态：当前步长、上一次旋转时间、方向
    int32_t  step_seconds = 1;
    int8_t   last_dir     = 0;        // +1(逆时针)、-1(顺时针)
    uint64_t last_rot_us  = 0;        // 上一次旋转事件时间戳

    const int32_t STEP_TAB[]    = {1, 2, 5, 8, 10, 20};
    const int     STEP_TAB_LEN  = sizeof(STEP_TAB) / sizeof(STEP_TAB[0]);
    int           step_idx      = 0;  // 当前步长索引

    const uint32_t FAST_MS = 80;      // < 80ms 认为快速旋转（同向）
    const uint32_t SLOW_MS = 300;     // > 300ms 认为停顿

    // 计时 & 闪烁相关
    uint64_t last_sec_tick_us = 0;
    uint64_t last_blink_us    = 0;
    bool     backlight_on     = true;

    while (true) {
        uint64_t now = time_us_64();

        // 1 秒计时逻辑：只在 RUNNING 时工作
        if (state == TIMER_STATE_RUNNING && target_total_sec > 0) {
            if (last_sec_tick_us == 0) {
                last_sec_tick_us = now;
            }

            if (now - last_sec_tick_us >= 1000000) {
                last_sec_tick_us += 1000000;

                if (elapsed_total_sec < target_total_sec) {
                    elapsed_total_sec++;
                    show_time_from_total_sec(elapsed_total_sec);

                    if (elapsed_total_sec >= target_total_sec) {
                        // 达到目标时间，进入 DONE 状态并开始闪烁
                        elapsed_total_sec = target_total_sec;
                        state = TIMER_STATE_DONE;
                        last_blink_us = now;
                        backlight_on = true;
                        lcd_backlight_on();
                    }
                }
            }
        } else {
            // 非 RUNNING 状态，不继续用旧的 tick
            last_sec_tick_us = 0;
        }

        // 背光闪烁逻辑：只在 DONE 时闪烁，其它状态保持常亮
        if (state == TIMER_STATE_DONE) {
            const uint32_t BLINK_PERIOD_US = 300000; // 0.3s
            if (last_blink_us == 0) {
                last_blink_us = now;
            }
            if (now - last_blink_us >= BLINK_PERIOD_US) {
                last_blink_us += BLINK_PERIOD_US;
                backlight_on = !backlight_on;
                if (backlight_on) {
                    lcd_backlight_on();
                } else {
                    lcd_backlight_off();
                }
            }
        } else {
            // 非 DONE：确保背光常亮
            if (!backlight_on) {
                lcd_backlight_on();
                backlight_on = true;
            }
            last_blink_us = 0;
        }

        // 处理编码器事件
        uint8_t ev = Encoder_ReadData();

        if (ev != encoder_none) {
            const char *ev_name = "none";
            int32_t delta = 0;

            // 先处理按键：根据状态切换计时状态
            if (ev == key) {
                ev_name = "key";

                if (state == TIMER_STATE_SET) {
                    // 从设定模式开始计时：从 0 计到 target_total_sec
                    elapsed_total_sec = 0;
                    show_time_from_total_sec(elapsed_total_sec);
                    state = TIMER_STATE_RUNNING;
                    last_sec_tick_us = now;
                    // 背光保持常亮
                    backlight_on = true;
                    lcd_backlight_on();
                } else if (state == TIMER_STATE_RUNNING) {
                    // 计时中按键：暂停
                    state = TIMER_STATE_PAUSED;
                    // 显示保持在当前 elapsed_total_sec
                    show_time_from_total_sec(elapsed_total_sec);
                    backlight_on = true;
                    lcd_backlight_on();
                } else if (state == TIMER_STATE_PAUSED) {
                    // 暂停状态按键：继续计时
                    state = TIMER_STATE_RUNNING;
                    last_sec_tick_us = now;
                    show_time_from_total_sec(elapsed_total_sec);
                    backlight_on = true;
                    lcd_backlight_on();
                } else if (state == TIMER_STATE_DONE) {
                    // 完成状态按键：停止闪烁，回到设定模式，时间归零，显示目标时间
                    state = TIMER_STATE_SET;
                    elapsed_total_sec = 0;
                    backlight_on = true;
                    lcd_backlight_on();
                    show_time_from_total_sec(target_total_sec);
                }

                // 按键不改目标时间，不需要下面的步长逻辑
            }

            // 旋转：根据速度调整步长，然后在合适的状态下修改 target_total_sec
            if (ev == cw || ev == ccw) {
                int8_t dir = (ev == ccw) ? +1 : -1;  // 逆时针增加，顺时针减少
                ev_name = (ev == ccw) ? "ccw" : "cw";

                // 如果当前是 DONE，旋钮一动就认为进入重新设定
                if (state == TIMER_STATE_DONE) {
                    state = TIMER_STATE_SET;
                    elapsed_total_sec = 0;
                    backlight_on = true;
                    lcd_backlight_on();
                    show_time_from_total_sec(target_total_sec);
                }

                // 如果当前是 PAUSED，旋钮一动就回到设定模式，基于原来的目标时间调整
                if (state == TIMER_STATE_PAUSED) {
                    state = TIMER_STATE_SET;
                    elapsed_total_sec = 0;
                    backlight_on = true;
                    lcd_backlight_on();
                    show_time_from_total_sec(target_total_sec);
                }

                // RUNNING 状态下旋钮不改目标时间，直接忽略（防误触）
                if (state == TIMER_STATE_RUNNING) {
                    delta = 0;
                } else {
                    // SET 状态下才真正修改 target_total_sec（包括从 PAUSED/DONE 刚刚切回来的情况）

                    uint64_t diff_ms = 0;
                    if (last_rot_us != 0) {
                        uint64_t diff = now - last_rot_us;
                        diff_ms = diff / 1000;
                    }

                    if (last_rot_us == 0) {
                        // 第一次旋转，步长先用最小值
                        step_idx = 0;
                    } else {
                        if (dir == last_dir && diff_ms > 0 && diff_ms < FAST_MS) {
                            // 同向且快速：尝试升档
                            if (step_idx < STEP_TAB_LEN - 1) {
                                step_idx++;
                            }
                        } else if (diff_ms > SLOW_MS || dir != last_dir) {
                            // 停顿较久或换方向：回到精细模式
                            step_idx = 0;
                        }
                    }

                    step_seconds = STEP_TAB[step_idx];
                    last_dir     = dir;
                    last_rot_us  = now;

                    int32_t before_target = target_total_sec;
                    int32_t t = before_target + dir * step_seconds;
                    if (t < 0) t = 0;
                    if (t > 59 * 60 + 59) t = 59 * 60 + 59;
                    target_total_sec = (uint16_t)t;
                    delta = dir * step_seconds;

                    // 设定模式下显示目标时间
                    if (state == TIMER_STATE_SET) {
                        show_time_from_total_sec(target_total_sec);
                    }

                    // 打印：基于目标时间的变化
                    printf("[ENC] target_before=%4d  ev=%s(%d)  dir=%+d  step=%2d  delta=%+4d  target_after=%4d  A=%d B=%d\n",
                           before_target,
                           ev_name,
                           ev,
                           dir,
                           step_seconds,
                           delta,
                           target_total_sec,
                           gpio_get(ENCODER_EC11_PIN_A),
                           gpio_get(ENCODER_EC11_PIN_B));
                }
            }

            // 按键事件也打印一行，方便调试
            if (ev == key) {
                printf("[KEY] state=%d  target=%4u  elapsed=%4u\n",
                       state, target_total_sec, elapsed_total_sec);
            }
        }

        tight_loop_contents();
    }

    return 0;
}
