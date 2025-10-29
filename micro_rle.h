/*
 * micro_rle.h - 1 kB embedded logger
 * Drift-aware RLE-XOR compression for serial streams
 */

#pragma once

#include <stdint.h>

void log_init(void);
void log_byte(uint8_t b);
void log_flush(void);
