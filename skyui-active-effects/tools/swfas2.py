"""AS2 (AVM1) SWF analyser: ExportAssets symbol names + ActionConstantPool strings.

Skyrim's Scaleform is AS2, so SkyUI's menus use DoInitAction / DoAction tags rather
than DoABC. Each exported symbol typically gets a DoInitAction carrying its class
definition, and every string literal lives in an ActionConstantPool (0x88).

Usage:
  python swfas2.py <file.swf> [--symbols] [--strings [filter]] [--per-symbol]
"""
import struct, zlib, sys, os


def read_swf_body(path):
    d = open(path, 'rb').read()
    sig = d[:3]
    if sig == b'CWS':
        return zlib.decompress(d[8:])
    if sig == b'FWS':
        return d[8:]
    raise SystemExit("unsupported SWF sig %r" % sig)


def skip_rect(b, o):
    nbits = b[o] >> 3
    return o + ((5 + nbits * 4) + 7) // 8


def iter_tags(body):
    o = skip_rect(body, 0) + 4
    while o + 2 <= len(body):
        cl = struct.unpack('<H', body[o:o + 2])[0]; o += 2
        code = cl >> 6
        ln = cl & 0x3F
        if ln == 0x3F:
            ln = struct.unpack('<I', body[o:o + 4])[0]; o += 4
        yield code, body[o:o + ln]
        o += ln
        if code == 0:
            break


def cstr(d, o):
    e = d.index(b'\x00', o)
    return d[o:e].decode('utf-8', 'replace'), e + 1


def parse_exports(data):
    out = []
    n = struct.unpack('<H', data[0:2])[0]
    o = 2
    for _ in range(n):
        if o + 2 > len(data):
            break
        cid = struct.unpack('<H', data[o:o + 2])[0]; o += 2
        name, o = cstr(data, o)
        out.append((cid, name))
    return out


def actions_strings(data):
    """Walk AVM1 action records, pulling ActionConstantPool + push'd strings."""
    strings = []
    o = 0
    n = len(data)
    while o < n:
        code = data[o]; o += 1
        if code == 0:
            break
        if code < 0x80:
            continue
        if o + 2 > n:
            break
        ln = struct.unpack('<H', data[o:o + 2])[0]; o += 2
        body = data[o:o + ln]; o += ln
        if code == 0x88:          # ActionConstantPool
            if len(body) >= 2:
                cnt = struct.unpack('<H', body[0:2])[0]
                p = 2
                for _ in range(cnt):
                    try:
                        s, p = cstr(body, p)
                    except ValueError:
                        break
                    strings.append(s)
        elif code == 0x96:        # ActionPush
            p = 0
            while p < len(body):
                t = body[p]; p += 1
                if t == 0:        # string
                    try:
                        s, p = cstr(body, p)
                    except ValueError:
                        break
                    strings.append(s)
                elif t == 1: p += 4
                elif t in (2, 3): pass
                elif t == 4: p += 1
                elif t == 5: p += 1
                elif t == 6: p += 8
                elif t == 7: p += 4
                elif t == 8: p += 1
                elif t == 9: p += 2
                else: break
    return strings


def main():
    path = sys.argv[1]
    body = read_swf_body(path)
    exports = []
    init_by_char = {}
    all_strings = []
    for code, data in iter_tags(body):
        if code == 56:
            exports.extend(parse_exports(data))
        elif code == 59:          # DoInitAction
            cid = struct.unpack('<H', data[0:2])[0]
            s = actions_strings(data[2:])
            init_by_char[cid] = s
            all_strings.extend(s)
        elif code == 12:          # DoAction
            s = actions_strings(data)
            all_strings.extend(s)

    name_by_cid = dict(exports)
    print("%s: %d exported symbols, %d init blocks, %d strings"
          % (os.path.basename(path), len(exports), len(init_by_char), len(all_strings)))

    if "--symbols" in sys.argv:
        print("\nexported symbols:")
        for cid, nm in sorted(exports, key=lambda e: e[1]):
            print("   %-6d %s" % (cid, nm))

    if "--per-symbol" in sys.argv:
        print("\nper-symbol string counts:")
        for cid, s in sorted(init_by_char.items(), key=lambda kv: -len(kv[1])):
            print("   %-44s %4d strings" % (name_by_cid.get(cid, "char%d" % cid), len(s)))

    if "--symbol" in sys.argv:
        k = sys.argv.index("--symbol")
        want = sys.argv[k + 1].lower()
        for cid, strs in init_by_char.items():
            nm = name_by_cid.get(cid, "char%d" % cid)
            if want not in nm.lower():
                continue
            print("=== %s (char %d) : %d strings ===" % (nm, cid, len(strs)))
            seen = set()
            for x in strs:
                if x in seen or len(x) < 2:
                    continue
                seen.add(x)
                print("   %s" % x)

    if "--strings" in sys.argv:
        k = sys.argv.index("--strings")
        filt = ""
        if len(sys.argv) > k + 1 and not sys.argv[k + 1].startswith("--"):
            filt = sys.argv[k + 1].lower()
        seen = set()
        print("\nstrings%s:" % (" matching %r" % filt if filt else ""))
        for s in all_strings:
            if filt and filt not in s.lower():
                continue
            if s in seen or len(s) < 2:
                continue
            seen.add(s)
            print("   %s" % s)


main()
