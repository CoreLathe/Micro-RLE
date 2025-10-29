# Micro-RLE
[https://www.CoreLathe.com](https://www.corelathe.com)

**≤264-byte stream compressor with drift-aware RLE-XOR—halves MCU UART bandwidth.**

## Specs
- **Flash**: 252 B (M4) / 264 B (M0+) @ `-Os -mthumb -nostdlib`
- **RAM**: 36 B state (no heap, no initialized data)
- **Compression**: 33-70% on typical data (RLE + XOR delta with escape)
- **Timing**: 6-14 cycles/byte deterministic
- **Boot**: < 600 µs @ 24 MHz
- **Lossless**: Full 8-bit delta support via escape sequence

### Measured Footprint (Cortex-M4)
```
Function      Size    Notes
log_byte      196 B   core compression + escape handling
log_init       30 B   state initialization
log_flush      24 B   pending run flush
emit (weak)     2 B   output hook
Total         252 B flash, 36 B RAM
```

### Encoding Format
- **Token**: `[run:3][delta:5]`
- **Escape**: If delta > 0x1F, emit `[run:3][0x1F]` followed by full 8-bit delta
- **Cost**: +1 byte per large delta (rare for sensor streams)
- **Benefit**: Lossless for all input patterns

## API
```c
void log_init(void);   // Initialize state
void log_byte(uint8_t b);  // Encode single byte
void log_flush(void);  // Flush pending run
```

## Integration
Replace `emit()` in `micro_rle.c` with your output (UART/DMA/ringbuf).

## Build
```bash
arm-none-eabi-gcc -Os -mthumb -mcpu=cortex-m0plus -nostdlib \
  -c micro_rle.c -o micro_rle.o
```

## Parameters
- `WIN=32`: Drift observation window (must be power-of-2)
- `WIN_MSK=31`: Bitmask for modulo (WIN - 1)
- `STUFF_TH=2`: Hamming threshold for frame sync
- `RLE_MAX=7`: Max run length (3 bits)
- `STUFF_BYTE=0x7E`: Frame marker (consecutive occurrences suppressed)

All constants tuned for 115200 bps serial @ 24 MHz.

## Decoder Notes
- Receiver should ignore consecutive `STUFF_BYTE` markers
- Drift detection uses 1-cycle AND mask instead of 2-cycle MUL on M0+

## License

MIT License - See LICENSE file

**No warranty. Test thoroughly before production use.**

---

## Contributing

Issues and PRs welcome for:
- Bug fixes
- Documentation improvements
- Additional test cases
- Platform-specific examples

---

## Support

- GitHub Issues: Bug reports and questions
- Email: sales@corelathe.com
- Website: http://www.corelathe.com
