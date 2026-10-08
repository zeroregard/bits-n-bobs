#!/usr/bin/env python3
"""Append AEC icons to a copy of a SkyUI icon SWF (category tabs or item rows).

  patch_icons.py frames <in.swf> <out.swf> <ref_label> <label:chid> ...   add labelled frames with placeholder shapes
  patch_icons.py place  <in.swf> <out.swf> <ref_label> <label:chid> ...   position the (SVG-replaced) shapes to match ref_label

Both SkyUI icon SWFs share one layout, so the same tool serves
icons_category_psychosteve.swf (ref mag_activeeffects) and
icons_item_psychosteve.swf (ref default_effect).

Frames mirror SkyUI's layout: RemoveObject2(depth 4), FrameLabel, DefineShape,
DefineSprite(wrapping the shape), PlaceObject2(sprite, depth 4), ShowFrame.
"""
import sys, zlib, struct

NEW = []   # (label, shape chid); the wrapping sprite gets chid + 1
REF = None

class BR:
    def __init__(s, d, p): s.d = d; s.p = p * 8
    def u(s, n):
        v = 0
        for _ in range(n): v = (v << 1) | ((s.d[s.p >> 3] >> (7 - (s.p & 7))) & 1); s.p += 1
        return v
    def s(s, n):
        v = s.u(n); return v - (1 << n) if n and v >> (n - 1) else v

class BW:
    def __init__(s): s.bits = []
    def u(s, v, n): s.bits += [(v >> (n - 1 - i)) & 1 for i in range(n)]
    def s(s, v, n): s.u(v & ((1 << n) - 1), n)
    def bytes(s):
        b = s.bits + [0] * (-len(s.bits) % 8)
        return bytes(int(''.join(map(str, b[i:i + 8])), 2) for i in range(0, len(b), 8))

def sbits(*vals): return max(max((abs(v).bit_length() + 1 for v in vals), default=1), 1)

def matrix(tx, ty, sx=None, sy=None):
    w = BW()
    if sx is None: w.u(0, 1)
    else:
        a, b = round(sx * 65536), round(sy * 65536); n = sbits(a, b); w.u(1, 1); w.u(n, 5); w.s(a, n); w.s(b, n)
    w.u(0, 1)
    n = sbits(tx, ty); w.u(n, 5); w.s(tx, n); w.s(ty, n)
    return w.bytes()

def read_matrix(d, p):
    r = BR(d, p); sx = sy = None
    if r.u(1): n = r.u(5); sx, sy = r.s(n) / 65536, r.s(n) / 65536
    if r.u(1): n = r.u(5); r.s(n); r.s(n)
    n = r.u(5); return sx, sy, r.s(n), r.s(n)

def rect_at(d, p):
    r = BR(d, p); n = r.u(5); return [r.s(n) for _ in range(4)]

def tag(t, body):
    if len(body) < 63 and t not in (2, 22, 32, 39): return struct.pack('<H', (t << 6) | len(body)) + body
    return struct.pack('<HI', (t << 6) | 63, len(body)) + body

def load(path):
    b = open(path, 'rb').read()
    d = b[:8] + (zlib.decompress(b[8:]) if b[:1] == b'C' else b[8:])
    nb = d[8] >> 3; hdr_end = 8 + ((5 + 4 * nb + 7) // 8) + 4
    tags, p = [], hdr_end
    while p < len(d):
        h = struct.unpack('<H', d[p:p + 2])[0]; t, l, hl = h >> 6, h & 63, 2
        if l == 63: l = struct.unpack('<I', d[p + 2:p + 6])[0]; hl = 6
        tags.append([t, d[p + hl:p + hl + l]]); p += hl + l
        if t == 0: break
    return b[:3], d, hdr_end, tags

def save(path, sig, d, hdr_end, tags):
    frames = sum(1 for t, _ in tags if t == 1)
    head = bytearray(d[8:hdr_end]); head[-2:] = struct.pack('<H', frames)
    body = bytes(head) + b''.join(tag(t, bd) for t, bd in tags)
    out = b'CWS' + d[3:4] + struct.pack('<I', 8 + len(body)) + zlib.compress(body, 9)
    open(path, 'wb').write(out)

def frames_by_label(tags):
    lab, res = None, {}
    for i, (t, bd) in enumerate(tags):
        if t == 43: lab = bd[:-1].decode()
        res.setdefault(lab, []).append(i)
    return res

def cmd_frames(src, dst):
    sig, d, he, tags = load(src)
    fl = frames_by_label(tags)
    assert not any(l in fl for l, _ in NEW), "already patched"
    ref = [tags[i] for i in fl[REF]]
    shape = next(bd for t, bd in ref if t in (2, 22, 32))
    end = tags.pop(); assert end[0] == 0
    for label, chid in NEW:
        sp = chid + 1
        sprite = struct.pack('<HH', sp, 1) + tag(26, bytes([0x06, 1, 0]) + struct.pack('<H', chid) + b'\0') + tag(1, b'') + tag(0, b'')
        tags += [[28, struct.pack('<H', 4)], [43, label.encode() + b'\0'],
                 [2 if True else 0, struct.pack('<H', chid) + shape[2:]],
                 [39, sprite],
                 [26, bytes([0x16]) + struct.pack('<HH', 4, sp) + matrix(0, 0) + struct.pack('<H', 0)],
                 [1, b'']]
    # the reference shape may be DefineShape2/3; keep its tag type
    st = next(t for t, bd in ref if t in (2, 22, 32))
    for t in tags:
        if t[0] == 2 and struct.unpack('<H', t[1][:2])[0] in [c for _, c in NEW]: t[0] = st
    tags.append(end); save(dst, sig, d, he, tags)

def stage_bounds(tags, idxs):
    """bounds of a frame's icon in stage twips: shape bounds through the PlaceObject2 matrix"""
    t2 = [tags[i] for i in idxs]
    shape = next(bd for t, bd in t2 if t in (2, 22, 32))
    x0, x1, y0, y1 = rect_at(shape, 2)
    po = next(bd for t, bd in t2 if t == 26)
    sx, sy, tx, ty = read_matrix(po, 5)
    sx = sx or 1; sy = sy or 1
    return [tx + x0 * sx, tx + x1 * sx, ty + y0 * sy, ty + y1 * sy], (x0, x1, y0, y1)

def cmd_place(src, dst):
    sig, d, he, tags = load(src)
    fl = frames_by_label(tags)
    (rx0, rx1, ry0, ry1), _ = stage_bounds(tags, fl[REF])
    print(f"{REF}: stage twips x {rx0:.0f}..{rx1:.0f} y {ry0:.0f}..{ry1:.0f}")
    for label, chid in NEW:
        _, (x0, x1, y0, y1) = stage_bounds(tags, fl[label])
        s = (ry1 - ry0) / (y1 - y0)                     # match height
        tx = round((rx0 + rx1) / 2 - (x0 + x1) / 2 * s)  # centre horizontally
        ty = round(ry0 - y0 * s)
        i = next(i for i in fl[label] if tags[i][0] == 26)
        tags[i][1] = bytes([0x16]) + struct.pack('<HH', 4, chid + 1) + matrix(tx, ty, s, s) + struct.pack('<H', 0)
        print(f"{label}: shape {x0}..{x1} x {y0}..{y1}, scale {s:.4f}, translate {tx},{ty} ->",
              [round(v) for v in stage_bounds(tags, fl[label])[0]])
    save(dst, sig, d, he, tags)

if __name__ == '__main__':
    REF = sys.argv[4]
    NEW = [(a.split(':')[0], int(a.split(':')[1])) for a in sys.argv[5:]]
    {'frames': cmd_frames, 'place': cmd_place}[sys.argv[1]](sys.argv[2], sys.argv[3])
