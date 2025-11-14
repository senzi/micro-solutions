/**
 * @file    encoder_ec11.c
 * @brief   EC11 旋转编码器 (RP2040 精细版，带 spin lock 同步)
 */

#include "drivers/encoder_ec11.h"
#include "drivers/board.h"

#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/sync.h"
#include "hardware/gpio.h"

/* 一般 EC11 一格 4 个边沿，如果你那个实际是一格 2 步，可以改成 2 */
#define ENCODER_STEPS_PER_NOTCH  4

/* 1 kHz 采样，单位 us，负号表示从“现在”起反复触发 */
#define ENCODER_SAMPLE_PERIOD_US (-1000)

static repeating_timer_t encoder_timer;

/* 正交解码查表：prev(2bit) << 2 | curr(2bit) => -1 / 0 / +1 */
static const int8_t quad_table[16] = {
    /* prev=00 -> curr=00,01,10,11 */
     0,  +1, -1,  0,
    /* prev=01 -> curr=00,01,10,11 */
    -1,   0,  0, +1,
    /* prev=10 -> curr=00,01,10,11 */
    +1,   0,  0, -1,
    /* prev=11 -> curr=00,01,10,11 */
     0,  -1, +1,  0
};

/* 编码器内部状态（原始边沿累积 / 已确认“格数”） */
static volatile int32_t encoder_accum   = 0;   // 原始 +1/-1 累积
static volatile int32_t encoder_notches = 0;   // 确认的一格步进累计
static uint8_t          prev_ab_state   = 0;   // 上一次 A/B 状态 0..3

/* 按键内部状态：稳定去抖 + 按下事件 */
static bool     btn_last_level   = false;   // 上一次采样的“是否按下”
static bool     btn_stable_level = false;   // 去抖后的稳定状态
static uint8_t  btn_stable_count = 0;       // 连续相同采样计数
static volatile bool btn_press_event = false;

/* 用于在 timer 回调 和 Encoder_ReadData() 之间同步 */
static spin_lock_t *enc_lock = NULL;

/* 内部：单次采样步骤，由定时器回调周期调用 */
static inline void encoder_sample_step(void) {
    uint32_t flags = spin_lock_blocking(enc_lock);

    /* 读取 A/B：EC11 通常上拉，未触发为 1，触发为 0 */
    uint8_t a = gpio_get(ENCODER_EC11_PIN_A) ? 1 : 0;
    uint8_t b = gpio_get(ENCODER_EC11_PIN_B) ? 1 : 0;
    uint8_t curr_ab = (b << 1) | a;  // 低位 A，高位 B，范围 0..3

    uint8_t idx   = (prev_ab_state << 2) | curr_ab;
    int8_t  delta = quad_table[idx];

    if (delta != 0) {
        encoder_accum += delta;

        if (encoder_accum >= ENCODER_STEPS_PER_NOTCH) {
            encoder_accum   -= ENCODER_STEPS_PER_NOTCH;
            encoder_notches += 1;  // 顺时针一格
        } else if (encoder_accum <= -ENCODER_STEPS_PER_NOTCH) {
            encoder_accum   += ENCODER_STEPS_PER_NOTCH;
            encoder_notches -= 1;  // 逆时针一格
        }
    }

    prev_ab_state = curr_ab;

    /* 按键去抖：引脚上拉，按下为 0；raw_pressed = true 表示“按下” */
    bool raw_pressed = !gpio_get(ENCODER_EC11_PIN_C);

    if (raw_pressed == btn_last_level) {
        if (btn_stable_count < 5) {
            btn_stable_count++;
        } else if (btn_stable_level != raw_pressed) {
            btn_stable_level = raw_pressed;
            if (btn_stable_level) {
                /* 从未按下 -> 按下，记一次事件 */
                btn_press_event = true;
            }
        }
    } else {
        btn_stable_count = 0;
        btn_last_level   = raw_pressed;
    }

    spin_unlock(enc_lock, flags);
}

/* 定时器回调：每 1ms 调用一次 encoder_sample_step */
static bool encoder_timer_callback(repeating_timer_t *t) {
    (void)t;
    encoder_sample_step();
    return true;
}

void Encoder_Init(void) {
    /* GPIO 初始化：全部上拉输入 */
    gpio_init(ENCODER_EC11_PIN_A);
    gpio_set_dir(ENCODER_EC11_PIN_A, GPIO_IN);
    gpio_pull_up(ENCODER_EC11_PIN_A);

    gpio_init(ENCODER_EC11_PIN_B);
    gpio_set_dir(ENCODER_EC11_PIN_B, GPIO_IN);
    gpio_pull_up(ENCODER_EC11_PIN_B);

    gpio_init(ENCODER_EC11_PIN_C);
    gpio_set_dir(ENCODER_EC11_PIN_C, GPIO_IN);
    gpio_pull_up(ENCODER_EC11_PIN_C);

    /* 初始化 prev_ab_state，避免上电瞬间的假边沿 */
    uint8_t a = gpio_get(ENCODER_EC11_PIN_A) ? 1 : 0;
    uint8_t b = gpio_get(ENCODER_EC11_PIN_B) ? 1 : 0;
    prev_ab_state = (b << 1) | a;

    btn_last_level   = !gpio_get(ENCODER_EC11_PIN_C);
    btn_stable_level = btn_last_level;
    btn_stable_count = 0;

    encoder_accum   = 0;
    encoder_notches = 0;
    btn_press_event = false;

    /* 申请一个硬件自旋锁，用于 timer 与主循环之间的同步 */
    uint32_t lock_num = spin_lock_claim_unused(true);
    enc_lock = spin_lock_init(lock_num);

    /* 创建 1 kHz 定时器，后台自动跑状态机 */
    add_repeating_timer_us(ENCODER_SAMPLE_PERIOD_US,
                           encoder_timer_callback,
                           NULL,
                           &encoder_timer);
}

/**
 * 读取一次事件：
 *   - 有按键事件：返回 key
 *   - 否则有旋转步进：顺时针返回 cw，逆时针返回 ccw（每次只消费一步）
 *   - 否则返回 encoder_none
 *
 * 注意：这里用 spin lock 做原子快照，避免与 1kHz 定时器回调并发修改。
 */
uint8_t Encoder_ReadData(void) {
    uint8_t result = encoder_none;

    uint32_t flags = spin_lock_blocking(enc_lock);

    if (btn_press_event) {
        btn_press_event = false;
        result = key;
    } else if (encoder_notches > 0) {
        encoder_notches--;
        result = cw;
    } else if (encoder_notches < 0) {
        encoder_notches++;
        result = ccw;
    }

    spin_unlock(enc_lock, flags);

    return result;
}
