"""SWF -> tag list -> DoABC -> ActionScript3 constant pool.

Enough of the format to map a SWF's structure without a decompiler:
  - handles CWS (zlib) / FWS / ZWS headers
  - walks the tag stream
  - parses the ABC constant pool (strings, namespaces, multinames)
  - lists classes and their traits (method/slot names)

Usage: python swfabc.py <file.swf> [--tags] [--strings [filter]] [--classes]
"""
import struct, zlib, sys, os

TAGS = {0: "End", 1: "ShowFrame", 9: "SetBackgroundColor", 24: "Protect",
        34: "DefineButton2", 39: "DefineSprite", 43: "FrameLabel",
        56: "ExportAssets", 59: "DoInitAction", 65: "ScriptLimits",
        69: "FileAttributes", 76: "SymbolClass", 77: "Metadata",
        82: "DoABC", 72: "DoABCDefine", 86: "DefineSceneAndFrameLabelData"}


def read_swf_body(path):
    d = open(path, 'rb').read()
    sig = d[:3]
    ver = d[3]
    length = struct.unpack('<I', d[4:8])[0]
    if sig == b'CWS':
        body = zlib.decompress(d[8:])
    elif sig == b'FWS':
        body = d[8:]
    elif sig == b'ZWS':
        raise SystemExit("LZMA SWF not supported here")
    else:
        raise SystemExit("not a SWF: %r" % sig)
    return sig.decode(), ver, length, body


def skip_rect(b, o):
    nbits = b[o] >> 3
    total = 5 + nbits * 4
    return o + (total + 7) // 8


def iter_tags(body):
    o = skip_rect(body, 0)
    o += 4  # frame rate (8.8) + frame count
    while o + 2 <= len(body):
        code_len = struct.unpack('<H', body[o:o + 2])[0]; o += 2
        code = code_len >> 6
        ln = code_len & 0x3F
        if ln == 0x3F:
            ln = struct.unpack('<I', body[o:o + 4])[0]; o += 4
        yield code, body[o:o + ln]
        o += ln
        if code == 0:
            break


class ABCReader:
    def __init__(self, data):
        self.d = data
        self.o = 0

    def u8(self):
        v = self.d[self.o]; self.o += 1; return v

    def u16(self):
        v = struct.unpack('<H', self.d[self.o:self.o + 2])[0]; self.o += 2; return v

    def u30(self):
        r = 0; shift = 0
        for _ in range(5):
            b = self.d[self.o]; self.o += 1
            r |= (b & 0x7F) << shift
            if not (b & 0x80):
                break
            shift += 7
        return r

    def s24(self):
        v = self.d[self.o] | (self.d[self.o+1] << 8) | (self.d[self.o+2] << 16)
        self.o += 3
        return v

    def string(self):
        n = self.u30()
        s = self.d[self.o:self.o + n]; self.o += n
        return s.decode('utf-8', 'replace')


def parse_abc(data):
    r = ABCReader(data)
    minor = r.u16(); major = r.u16()
    ints = [0]
    for _ in range(max(r.u30() - 1, 0)): r.u30()
    for _ in range(max(r.u30() - 1, 0)): r.u30()
    n = max(r.u30() - 1, 0)
    r.o += 8 * n
    strings = [""]
    n = r.u30()
    for _ in range(max(n - 1, 0)):
        strings.append(r.string())
    namespaces = [None]
    n = r.u30()
    for _ in range(max(n - 1, 0)):
        kind = r.u8(); name = r.u30()
        namespaces.append((kind, name))
    nssets = [None]
    n = r.u30()
    for _ in range(max(n - 1, 0)):
        c = r.u30()
        nssets.append([r.u30() for _ in range(c)])
    multinames = [None]
    n = r.u30()
    for _ in range(max(n - 1, 0)):
        kind = r.u8()
        if kind in (0x07, 0x0D):      # QName
            ns = r.u30(); nm = r.u30(); multinames.append(("QName", ns, nm))
        elif kind in (0x0F, 0x10):    # RTQName
            nm = r.u30(); multinames.append(("RTQName", 0, nm))
        elif kind in (0x11, 0x12):    # RTQNameL
            multinames.append(("RTQNameL", 0, 0))
        elif kind in (0x09, 0x0E):    # Multiname
            nm = r.u30(); ns = r.u30(); multinames.append(("Multiname", ns, nm))
        elif kind in (0x1B, 0x1C):    # MultinameL
            ns = r.u30(); multinames.append(("MultinameL", ns, 0))
        elif kind == 0x1D:            # TypeName
            r.u30(); c = r.u30()
            for _ in range(c): r.u30()
            multinames.append(("TypeName", 0, 0))
        else:
            multinames.append(("?k%d" % kind, 0, 0))
    return dict(minor=minor, major=major, strings=strings,
                namespaces=namespaces, multinames=multinames, reader=r)


def mn_name(abc, idx):
    try:
        mn = abc["multinames"][idx]
        if mn and len(mn) >= 3:
            return abc["strings"][mn[2]]
    except Exception:
        pass
    return "?"


def main():
    path = sys.argv[1]
    sig, ver, length, body = read_swf_body(path)
    print("%s  sig=%s version=%d declaredLen=%d bodyLen=%d" % (os.path.basename(path), sig, ver, length, len(body)))
    abcs = []
    counts = {}
    for code, data in iter_tags(body):
        counts[code] = counts.get(code, 0) + 1
        if code in (72, 82):
            abcs.append(data)
    if "--tags" in sys.argv:
        print("tags:")
        for c in sorted(counts, key=lambda k: -counts[k]):
            print("   %-28s x%d" % (TAGS.get(c, "tag%d" % c), counts[c]))
    print("DoABC blocks: %d" % len(abcs))
    for i, data in enumerate(abcs):
        d = data
        o = 0
        if len(abcs) and d[:4] == b'\x00\x00\x00\x00' or True:
            # DoABC (82) has flags u32 + name string before the abc
            try:
                flags = struct.unpack('<I', d[0:4])[0]
                j = 4
                end = d.index(b'\x00', j)
                name = d[j:end].decode('utf-8', 'replace')
                cand = d[end + 1:]
                abc = parse_abc(cand)
                d = cand
                print("  abc[%d] name=%r flags=%d strings=%d multinames=%d"
                      % (i, name, flags, len(abc["strings"]), len(abc["multinames"])))
            except Exception as e:
                abc = parse_abc(d)
                print("  abc[%d] (raw) strings=%d" % (i, len(abc["strings"])))
        if "--strings" in sys.argv:
            filt = ""
            k = sys.argv.index("--strings")
            if len(sys.argv) > k + 1 and not sys.argv[k + 1].startswith("--"):
                filt = sys.argv[k + 1].lower()
            for s in abc["strings"]:
                if filt and filt not in s.lower():
                    continue
                if len(s) > 1:
                    print("     %s" % s)


main()
