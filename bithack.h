#ifdef BITHACK_H
#error "double-included bithack.h"
#endif

static inline int
ctz16(uint16_t x)
{
    /* http://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightBinSearch */
    int tz = 1;
    if ((x & 0xff) == 0) {
        x >>= 8;
        tz += 8;
    }
    if ((x & 0xf) == 0) {
        x >>= 4;
        tz += 4;
    }
    if ((x & 0x3) == 0) {
        x >>= 2;
        tz += 2;
    }
    tz -= x & 0x1;
    return tz;
}

static inline int
ctz32(uint32_t x)
{
    /* http://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightBinSearch */
    int tz = 0;
    if ((x & 0xffff) == 0) {
        x >>= 16;
        tz += 16;
    }
    tz += ctz16(x & 0xffff);
    return tz;
}

static inline uint8_t
bitrev8(uint8_t n)
{
    /* http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64Bits */
    return ((n * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32;
}

