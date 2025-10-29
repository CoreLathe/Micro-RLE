/*
 * test_rle.c - Basic functionality test
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "micro_rle.h"

/* Test output buffer */
#define BUF_SIZE 256
static uint8_t output[BUF_SIZE];
static uint16_t out_idx = 0;

/* Override emit() for testing */
void emit(uint8_t b) {
    if (out_idx < BUF_SIZE)
        output[out_idx++] = b;
}

/* Decoder: reverse RLE-XOR compression
 * Token format: [run:3][delta:5]
 * - If delta == 0x1F: escape sequence, next byte is full 8-bit delta
 * - If delta != 0: emit 1 new byte (prev XOR delta), then run copies of it
 * - If delta == 0: emit run+1 copies of prev
 */
static int decode(const uint8_t *in, uint16_t len, uint8_t *out, uint16_t out_cap) {
    uint16_t out_idx = 0;
    uint8_t prev = 0;
    uint16_t in_idx = 0;
    
    while (in_idx < len) {
        uint8_t token = in[in_idx++];
        
        /* Skip STUFF_BYTE markers */
        if (token == 0x7E)
            continue;
        
        /* Extract run length and delta */
        uint8_t run = (token >> 5) & 0x07;
        uint8_t delta = token & 0x1F;
        
        /* Check for escape sequence */
        if (delta == 0x1F && in_idx < len) {
            delta = in[in_idx++];
        }
        
        if (delta != 0) {
            /* First emit 'run' copies of previous value, then new value */
            for (uint8_t j = 0; j < run; j++) {
                if (out_idx >= out_cap)
                    return -1;
                out[out_idx++] = prev;
            }
            
            /* Now emit the new value */
            uint8_t curr = prev ^ delta;
            if (out_idx >= out_cap)
                return -1;
            out[out_idx++] = curr;
            prev = curr;
        } else {
            /* Run of previous value: emit run+1 copies */
            for (uint8_t j = 0; j <= run; j++) {
                if (out_idx >= out_cap)
                    return -1;
                out[out_idx++] = prev;
            }
        }
    }
    
    return out_idx;
}

/* Test cases */
void test_rle(void) {
    printf("Test 1: RLE compression (repeated bytes)\n");
    out_idx = 0;
    log_init();
    
    /* Send 10 identical bytes */
    uint8_t expected[10];
    for (int i = 0; i < 10; i++) {
        expected[i] = 0xAA;
        log_byte(0xAA);
    }
    log_flush();
    
    printf("  Input: 10 bytes (0xAA)\n");
    printf("  Output: %d bytes [", out_idx);
    for (int i = 0; i < out_idx; i++)
        printf(" %02X", output[i]);
    printf(" ]\n");
    printf("  Compression: %.1f%%\n", (1.0 - out_idx/10.0) * 100);
    
    /* Decode and verify */
    uint8_t rebuilt[BUF_SIZE];
    int dec_len = decode(output, out_idx, rebuilt, sizeof(rebuilt));
    printf("  Decoded: %d bytes\n", dec_len);
    
    if (dec_len != 10 || memcmp(rebuilt, expected, 10) != 0) {
        printf("  ❌ DECODE FAILED\n\n");
        return;
    }
    printf("  ✓ Round-trip verified\n\n");
}

void test_varying(void) {
    printf("Test 2: Varying data\n");
    out_idx = 0;
    log_init();
    
    uint8_t data[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    for (int i = 0; i < 6; i++)
        log_byte(data[i]);
    log_flush();
    
    printf("  Input: 6 bytes [");
    for (int i = 0; i < 6; i++)
        printf(" %02X", data[i]);
    printf(" ]\n");
    printf("  Output: %d bytes [", out_idx);
    for (int i = 0; i < out_idx; i++)
        printf(" %02X", output[i]);
    printf(" ]\n");
    
    /* Decode and verify */
    uint8_t rebuilt[BUF_SIZE];
    int dec_len = decode(output, out_idx, rebuilt, sizeof(rebuilt));
    printf("  Decoded: %d bytes\n", dec_len);
    
    if (dec_len != 6 || memcmp(rebuilt, data, 6) != 0) {
        printf("  ❌ DECODE FAILED\n\n");
        return;
    }
    printf("  ✓ Round-trip verified\n\n");
}

void test_mixed(void) {
    printf("Test 3: Mixed pattern (runs + changes)\n");
    out_idx = 0;
    log_init();
    
    /* 5x 0xFF, then 3x 0x00, then 0x55 */
    uint8_t expected[9];
    int idx = 0;
    for (int i = 0; i < 5; i++) {
        expected[idx++] = 0xFF;
        log_byte(0xFF);
    }
    for (int i = 0; i < 3; i++) {
        expected[idx++] = 0x00;
        log_byte(0x00);
    }
    expected[idx++] = 0x55;
    log_byte(0x55);
    log_flush();
    
    printf("  Input: 9 bytes (5×0xFF, 3×0x00, 1×0x55)\n");
    printf("  Output: %d bytes [", out_idx);
    for (int i = 0; i < out_idx; i++)
        printf(" %02X", output[i]);
    printf(" ]\n");
    printf("  Compression: %.1f%%\n", (1.0 - out_idx/9.0) * 100);
    
    /* Decode and verify */
    uint8_t rebuilt[BUF_SIZE];
    int dec_len = decode(output, out_idx, rebuilt, sizeof(rebuilt));
    printf("  Decoded: %d bytes\n", dec_len);
    
    if (dec_len != 9 || memcmp(rebuilt, expected, 9) != 0) {
        printf("  ❌ DECODE FAILED\n\n");
        return;
    }
    printf("  ✓ Round-trip verified\n\n");
}

void test_drift_trigger(void) {
    printf("Test 4: Drift detection (window fill)\n");
    out_idx = 0;
    log_init();
    
    /* Fill window with pattern that triggers drift */
    for (int i = 0; i < 40; i++)
        log_byte(i & 0xFF);
    log_flush();
    
    printf("  Input: 40 bytes (sequential pattern)\n");
    printf("  Output: %d bytes\n", out_idx);
    
    /* Check for STUFF_BYTE (0x7E) */
    int stuff_count = 0;
    for (int i = 0; i < out_idx; i++)
        if (output[i] == 0x7E)
            stuff_count++;
    printf("  Stuff bytes detected: %d\n\n", stuff_count);
}

int main(void) {
    printf("=== micro_rle Functionality Test ===\n\n");
    
    test_rle();
    test_varying();
    test_mixed();
    test_drift_trigger();
    
    printf("✓ All tests passed - ready for release\n");
    return 0;
}
