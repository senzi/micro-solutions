#pragma once
#include <stdint.h>

/**
 * 初始化 EC11：配置 GPIO、启动 1kHz 定时器、清零内部状态。
 */
void Encoder_Init(void);

/**
 * 读取一次事件：
 *   返回 board.h 里的 cw / ccw / key / encoder_none
 *   每次调用只返回一个事件，不会吞并。
 */
uint8_t Encoder_ReadData(void);
