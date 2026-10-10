"""List / extract a Skyrim SE BSA (v105). Usage:
   python bsax.py <bsa> [--list] [--extract <outdir> [substring]]
"""
import struct, zlib, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lz4f import decompress as _decomp

SEP = chr(92)


def lz4_block(src, outsize):
    dst = bytearray(); i = 0; n = len(src)
    while i < n and len(dst) < outsize:
        tok = src[i]; i += 1
        ll = tok >> 4
        if ll == 15:
            while True:
                b = src[i]; i += 1; ll += b
                if b != 255: break
        dst += src[i:i+ll]; i += ll
        if i + 1 >= n: break
        off = src[i] | (src[i+1] << 8); i += 2
        ml = tok & 0xF
        if ml == 15:
            while True:
                b = src[i]; i += 1; ml += b
                if b != 255: break
        ml += 4
        st = len(dst) - off
        for k in range(ml):
            dst.append(dst[st + k])
    return bytes(dst)


def read_bsa(path):
    f = open(path, 'rb')
    magic, ver, off, aflags, fcount, filecount, tfnl, tfilenl, ffl, pad = \
        struct.unpack('<4sIIIIIIIHH', f.read(36))
    defcomp = bool(aflags & 0x4); embed = bool(aflags & 0x100)
    folders = [struct.unpack('<QIIQ', f.read(24)) for _ in range(fcount)]
    entries = []
    for h, cnt, p, foff in folders:
        f.seek(foff - tfilenl)
        fname = ""
        if aflags & 0x1:
            ln = struct.unpack('<B', f.read(1))[0]
            fname = f.read(ln).rstrip(b'\x00').decode('cp1252', 'replace')
        recs = [struct.unpack('<QII', f.read(16)) for _ in range(cnt)]
        for r in recs:
            entries.append([fname, r[1], r[2]])
    names = f.read(tfilenl).split(b'\x00')
    for i, e in enumerate(entries):
        e.append(names[i].decode('cp1252', 'replace') if i < len(names) else '')
    return f, ver, defcomp, embed, entries


def extract(f, defcomp, embed, size, offset):
    comp = defcomp ^ bool(size & 0x40000000)
    real = size & 0x3FFFFFFF
    f.seek(offset)
    if embed:
        ln = struct.unpack('<B', f.read(1))[0]
        f.read(ln)
        real -= (ln + 1)
    data = f.read(real)
    if comp:
        orig = struct.unpack('<I', data[:4])[0]
        return _decomp(data[4:], orig)
    return data


def main():
    bsa = sys.argv[1]
    f, ver, defcomp, embed, entries = read_bsa(bsa)
    print("version %d  files %d  compressedByDefault=%s embeddedNames=%s"
          % (ver, len(entries), defcomp, embed))
    if "--extract" in sys.argv:
        out = sys.argv[sys.argv.index("--extract") + 1]
        filt = sys.argv[sys.argv.index("--extract") + 2] if len(sys.argv) > sys.argv.index("--extract") + 2 else ""
        n = 0
        for folder, size, offset, name in entries:
            full = (folder + SEP + name) if folder else name
            if filt and filt.lower() not in full.lower():
                continue
            data = extract(f, defcomp, embed, size, offset)
            dest = os.path.join(out, full.replace(SEP, os.sep))
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            open(dest, 'wb').write(data)
            print("  %8d  %s" % (len(data), full))
            n += 1
        print("extracted %d" % n)
    else:
        for folder, size, offset, name in sorted(entries, key=lambda e: (e[0], e[3])):
            full = (folder + SEP + name) if folder else name
            print("  %9d  %s" % (size & 0x3FFFFFFF, full))


main()
