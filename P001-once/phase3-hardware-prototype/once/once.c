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

    // 速度相关状态
    int32_t  step_seconds = 1;
    int8_t   last_dir     = 0;        // +1(逆时针)、-1(顺时针) —— 逻辑方向
    uint64_t last_rot_us  = 0;

    // 你手感好的这套
    const int32_t STEP_TAB[]    = {1, 2, 5, 8, 10, 20};
    const int     STEP_TAB_LEN  = sizeof(STEP_TAB) / sizeof(STEP_TAB[0]);
    int           step_idx      = 0;

    // 调得更“迟钝”一点
    const uint32_t FAST_MS = 50;      // < 50ms 才算真快
    const uint32_t SLOW_MS = 400;     // > 400ms 算停顿

    // 计时 & 闪烁
    uint64_t last_sec_tick_us = 0;
    uint64_t last_blink_us    = 0;
    bool     backlight_is_on  = true;

    while (true) {
        uint64_t now = time_us_64();

        // 计时：只有 RUNNING 状态才走表
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
                        elapsed_total_sec = target_total_sec;
                        state = TIMER_STATE_DONE;
                        last_blink_us = now;
                        backlight_is_on = true;
                        lcd_backlight_on();
                    }
                }
            }
        } else {
            last_sec_tick_us = 0;
        }

        // 背光闪烁：DONE 状态
        if (state == TIMER_STATE_DONE) {
            const uint32_t BLINK_PERIOD_US = 300000; // 0.3s
            if (last_blink_us == 0) {
                last_blink_us = now;
            }
            if (now - last_blink_us >= BLINK_PERIOD_US) {
                last_blink_us += BLINK_PERIOD_US;
                backlight_is_on = !backlight_is_on;
                if (backlight_is_on) {
                    lcd_backlight_on();
                } else {
                    lcd_backlight_off();
                }
            }
        } else {
            if (!backlight_is_on) {
                lcd_backlight_on();
                backlight_is_on = true;
            }
            last_blink_us = 0;
        }

        // 处理编码器事件
        uint8_t ev = Encoder_ReadData();

        if (ev != encoder_none) {
            int32_t delta = 0;
            const char *ev_name = "none";

            // 按键：切换状态，不改目标时间
            if (ev == key) {
                ev_name = "key";

                if (state == TIMER_STATE_SET) {
                    // 从 SET → RUNNING
                    elapsed_total_sec = 0;
                    show_time_from_total_sec(elapsed_total_sec);
                    state = TIMER_STATE_RUNNING;
                    last_sec_tick_us = now;
                    backlight_is_on = true;
                    lcd_backlight_on();
                } else if (state == TIMER_STATE_RUNNING) {
                    // RUNNING → PAUSED
                    state = TIMER_STATE_PAUSED;
                    show_time_from_total_sec(elapsed_total_sec);
                    backlight_is_on = true;
                    lcd_backlight_on();
                } else if (state == TIMER_STATE_PAUSED) {
                    // PAUSED → RUNNING
                    state = TIMER_STATE_RUNNING;
                    last_sec_tick_us = now;
                    show_time_from_total_sec(elapsed_total_sec);
                    backlight_is_on = true;
                    lcd_backlight_on();
                } else if (state == TIMER_STATE_DONE) {
                    // DONE → SET（停止闪烁，计时归零）
                    state = TIMER_STATE_SET;
                    elapsed_total_sec = 0;
                    backlight_is_on = true;
                    lcd_backlight_on();
                    show_time_from_total_sec(target_total_sec);
                }

                printf("[KEY]   state=%d  target=%4u  elapsed=%4u\n",
                       state, target_total_sec, elapsed_total_sec);
            }

            // 旋转：设定 / 重设目标时间
            if (ev == cw || ev == ccw) {
                // 注意：这里的 cw/ccw 是“驱动眼中的方向”，和你手上顺/逆时针，
                // 目前因为接线关系是反的，但逻辑是稳定的。
                int8_t dir = (ev == ccw) ? +1 : -1;
                ev_name = (ev == ccw) ? "ccw" : "cw";

                // DONE 状态下旋钮一动：回到 SET 模式，停掉闪烁
                if (state == TIMER_STATE_DONE) {
                    state = TIMER_STATE_SET;
                    elapsed_total_sec = 0;
                    backlight_is_on = true;
                    lcd_backlight_on();
                    show_time_from_total_sec(target_total_sec);
                }

                // PAUSED 状态下旋钮一动：回到 SET 模式，基于原目标时间调整
                if (state == TIMER_STATE_PAUSED) {
                    state = TIMER_STATE_SET;
                    elapsed_total_sec = 0;
                    backlight_is_on = true;
                    lcd_backlight_on();
                    show_time_from_total_sec(target_total_sec);
                }

                // RUNNING 状态下旋钮不改目标时间
                if (state == TIMER_STATE_RUNNING) {
                    delta = 0;
                } else {
                    // SET 状态下才调整 target_total_sec
                    uint64_t diff_ms = 0;
                    if (last_rot_us != 0) {
                        uint64_t diff = now - last_rot_us;
                        diff_ms = diff / 1000;
                    }

                    if (last_rot_us == 0) {
                        step_idx = 0;
                    } else {
                        if (dir == last_dir && diff_ms > 0 && diff_ms < FAST_MS) {
                            if (step_idx < STEP_TAB_LEN - 1) {
                                step_idx++;
                            }
                        } else if (diff_ms > SLOW_MS || dir != last_dir) {
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

                    if (state == TIMER_STATE_SET) {
                        show_time_from_total_sec(target_total_sec);
                    }

                    printf("[ENC]  before=%4d  after=%4d  step=%2d  delta=%+4d  ev=%s\n",
                           before_target,
                           target_total_sec,
                           step_seconds,
                           delta,
                           ev_name);
                }
            }
        }

        tight_loop_contents();
    }

    return 0;
}
