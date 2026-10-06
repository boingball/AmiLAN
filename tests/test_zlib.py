"""Check the bounded wire codec against independent Python zlib, not itself."""
import ctypes
import random
import zlib
from pathlib import Path

codec = ctypes.CDLL(str(Path('build/zlib.so').resolve()))
ptr = ctypes.POINTER(ctypes.c_ubyte)
codec.amilan_zlib_deflate.argtypes = [ptr, ctypes.c_uint, ptr, ctypes.c_uint]
codec.amilan_zlib_deflate.restype = ctypes.c_uint
codec.amilan_zlib_inflate.argtypes = [ptr, ctypes.c_uint, ptr, ctypes.c_uint]
codec.amilan_zlib_inflate.restype = ctypes.c_int

def decode(data, size):
    src = (ctypes.c_ubyte * len(data)).from_buffer_copy(data)
    guarded = (ctypes.c_ubyte * (size + 2))()
    guarded[0] = guarded[size + 1] = 0xa5
    dst = ctypes.cast(ctypes.byref(guarded, 1), ptr)
    ok = codec.amilan_zlib_inflate(dst, size, src, len(data))
    assert guarded[0] == guarded[size + 1] == 0xa5
    return ok, bytes(guarded)[1:-1]

rng = random.Random(932)
cases = 0
for n in (1, 2, 3, 4, 10, 143, 258, 508, 1024, 4096):
    fixtures = [bytes(n), b'\xff' * n,
                (bytes(range(256)) * 16)[:n],
                (b'ABCDEFGH' * 512)[:n], rng.randbytes(n)]
    for raw in fixtures:
        src = (ctypes.c_ubyte * n).from_buffer_copy(raw)
        out = (ctypes.c_ubyte * 514)()
        out[0] = out[513] = 0xa5
        dst = ctypes.cast(ctypes.byref(out, 1), ptr)
        k = codec.amilan_zlib_deflate(dst, 512, src, n)
        assert out[0] == out[513] == 0xa5
        if k:
            packed = bytes(out)[1:k + 1]
            assert zlib.decompress(packed) == raw
            assert decode(packed, n) == (1, raw)
            for cut in range(len(packed)):
                assert not decode(packed[:cut], n)[0]
            damaged = bytearray(packed); damaged[-1] ^= 1
            assert not decode(damaged, n)[0]
            assert not decode(packed + b'\0', n)[0]
            assert not decode(packed, n + 1)[0]
        c = zlib.compressobj(1, zlib.DEFLATED, 15, 8, zlib.Z_FIXED)
        packed = c.compress(raw) + c.flush()
        if len(packed) <= 512 and packed[2] & 7 == 3:
            assert decode(packed, n) == (1, raw)
        cases += 1

for _ in range(300):
    pattern = rng.randbytes(rng.randint(1, 128))
    raw = (pattern * 4096)[:rng.randint(1, 4096)]
    src = (ctypes.c_ubyte * len(raw)).from_buffer_copy(raw)
    out = (ctypes.c_ubyte * 512)()
    k = codec.amilan_zlib_deflate(out, 512, src, len(raw))
    if k:
        packed = bytes(out)[:k]
        assert zlib.decompress(packed) == raw
        assert decode(packed, len(raw)) == (1, raw)
    cases += 1

for _ in range(4000):
    n = rng.randint(0, 512)
    raw = rng.randbytes(n)
    if n >= 3:
        raw = b'\x78\x01\x03' + raw[3:]
    decode(raw, rng.randint(1, 4096))
assert not decode(b'\x78\x01\x03\x00\x00\x00\x00\x01', 4097)[0]
print(f'zlib interoperability: {cases} fixtures, truncation/checksum/bounds checks and 4000 malformed streams passed')
