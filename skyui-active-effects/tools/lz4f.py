"""Minimal LZ4 frame + block decoder (pure Python).

Skyrim SE BSA v105 stores compressed files as LZ4 *frame* (magic 04 22 4D 18),
not raw LZ4 block. Frame = header, then a sequence of blocks, then a 0 endmark.
"""
import struct

MAGIC = 0x184D2204


def lz4_block_decompress(src, hint=0):
    dst = bytearray()
    i = 0
    n = len(src)
    while i < n:
        token = src[i]; i += 1
        lit = token >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1
                lit += b
                if b != 255:
                    break
        dst += src[i:i + lit]
        i += lit
        if i >= n:
            break
        if i + 2 > n:
            break
        offset = src[i] | (src[i + 1] << 8)
        i += 2
        if offset == 0:
            raise ValueError("lz4: zero offset")
        ml = token & 0x0F
        if ml == 15:
            while True:
                b = src[i]; i += 1
                ml += b
                if b != 255:
                    break
        ml += 4
        start = len(dst) - offset
        if start < 0:
            raise ValueError("lz4: offset before start")
        for k in range(ml):
            dst.append(dst[start + k])
    return bytes(dst)


def lz4_frame_decompress(data):
    if len(data) < 7:
        raise ValueError("too short for lz4 frame")
    magic = struct.unpack('<I', data[:4])[0]
    if magic != MAGIC:
        raise ValueError("not an lz4 frame (magic %08X)" % magic)
    o = 4
    flg = data[o]; bd = data[o + 1]; o += 2
    version = (flg >> 6) & 0x3
    if version != 1:
        raise ValueError("lz4 frame version %d" % version)
    block_checksum = bool(flg & 0x10)
    content_size = bool(flg & 0x08)
    content_checksum = bool(flg & 0x04)
    dict_id = bool(flg & 0x01)
    if content_size:
        o += 8
    if dict_id:
        o += 4
    o += 1  # header checksum
    out = bytearray()
    while True:
        if o + 4 > len(data):
            break
        bsize = struct.unpack('<I', data[o:o + 4])[0]; o += 4
        if bsize == 0:
            break
        uncompressed = bool(bsize & 0x80000000)
        bsize &= 0x7FFFFFFF
        chunk = data[o:o + bsize]; o += bsize
        if block_checksum:
            o += 4
        out += chunk if uncompressed else lz4_block_decompress(chunk)
    return bytes(out)


def decompress(data, expected=0):
    """zlib, else lz4 frame, else raw lz4 block."""
    import zlib
    try:
        return zlib.decompress(data)
    except Exception:
        pass
    if len(data) >= 4 and struct.unpack('<I', data[:4])[0] == MAGIC:
        return lz4_frame_decompress(data)
    return lz4_block_decompress(data, expected)
