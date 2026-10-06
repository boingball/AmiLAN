/* Bounded RFC 1950/1951 profile: one final fixed-Huffman DEFLATE block,
 * no preset dictionary, Adler-32. Standard zlib can decompress the output;
 * this decoder does not accept arbitrary zlib streams. */
#ifndef AMILAN_ZLIB_H
#define AMILAN_ZLIB_H
#include <stdint.h>
#define AMILAN_ZLIB_RAW 4096
#define AMILAN_ZLIB_PACKED 512
/* Caller-owned, non-overlapping input/output buffers. No allocation or
 * persistent dictionary. size must be 1..AMILAN_ZLIB_RAW. Encoder scratch
 * includes a 512-byte automatic hash table; integer-only, reentrant.
 * Returns encoded byte count, or 0 for invalid input / insufficient space.
 * capacity is capped at AMILAN_ZLIB_PACKED. On failure output may be touched.
 * A nonzero result need not be smaller: callers choose raw fallback. */
unsigned amilan_zlib_deflate(uint8_t *out,unsigned capacity,
                            const uint8_t *in,unsigned size);
/* size is the expected exact decoded length / output capacity; bytes must
 * be 8..AMILAN_ZLIB_PACKED. Accepts header 78 01 and a single final fixed
 * block only. Rejects malformed/trailing bytes, invalid distances and
 * size/checksum mismatch. Returns 1 on success, 0 on failure.
 * Output may be touched on failure: decode into scratch and commit only
 * after success. No transport, framing or message IDs are imposed. */
int amilan_zlib_inflate(uint8_t *out,unsigned size,
                        const uint8_t *in,unsigned bytes);
#endif
