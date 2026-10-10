"""Row icons for active effects (frames added to icons_item_psychosteve.swf).
Fill-only single paths, same 80x102 box as the tab icons. Needs skia-python.
  python build_item_svgs.py <outdir>
"""
import skia, sys, math, re
out = sys.argv[1]

def P(d):
    t = re.findall(r'[MLCZ]|-?[\d.]+', d); p = skia.Path(); i = 0
    while i < len(t):
        c = t[i]; i += 1
        if c == 'M': p.moveTo(float(t[i]), float(t[i+1])); i += 2
        elif c == 'L': p.lineTo(float(t[i]), float(t[i+1])); i += 2
        elif c == 'C': p.cubicTo(*[float(v) for v in t[i:i+6]]); i += 6
        elif c == 'Z': p.close()
    return p
def stroke(p, w, cap=skia.Paint.kRound_Cap):
    pt = skia.Paint(); pt.setStyle(skia.Paint.kStroke_Style); pt.setStrokeWidth(w)
    pt.setStrokeJoin(skia.Paint.kRound_Join); pt.setStrokeCap(cap)
    r = skia.Path(); pt.getFillPath(p, r); return r
def U(*ps):
    r = ps[0]
    for p in ps[1:]: r = skia.Op(r, p, skia.PathOp.kUnion_PathOp)
    return r
def D(a, b): return skia.Op(a, b, skia.PathOp.kDifference_PathOp)
def circ(x, y, r): p = skia.Path(); p.addCircle(x, y, r); return p
def rrect(x, y, w, h, r): p = skia.Path(); p.addRRect(skia.RRect.MakeRectXY(skia.Rect.MakeXYWH(x, y, w, h), r, r)); return p

# affliction: virus body with knobbed spikes and three spots (curses, diseases)
cx, cy = 40, 51
parts = [circ(cx, cy, 19)]
for k in range(8):
    a = k * math.pi / 4 + math.pi / 8
    parts.append(stroke(P("M%f %f L%f %f" % (cx + 17*math.cos(a), cy + 17*math.sin(a), cx + 28*math.cos(a), cy + 28*math.sin(a))), 4.5, skia.Paint.kButt_Cap))
    parts.append(circ(cx + 30*math.cos(a), cy + 30*math.sin(a), 5))
affliction = D(U(*parts), U(circ(cx - 7, cy - 5, 4.5), circ(cx + 7, cy - 2, 3.5), circ(cx - 1, cy + 8, 3.8)))

# perk: four-point star with two sparkles (perk tree constellation)
def star(x, y, R, r):
    pts = []
    for k in range(8):
        a = -math.pi / 2 + k * math.pi / 4; rr = R if k % 2 == 0 else r
        pts.append((x + rr*math.cos(a), y + rr*math.sin(a)))
    return P("M%f %f " % pts[0] + " ".join("L%f %f" % q for q in pts[1:]) + " Z")
perk = U(star(36, 56, 34, 9), star(64, 22, 11, 3.2), star(66, 82, 7, 2.2))

# race: head and shoulders bust
head = circ(40, 32, 15)
shoulders = P("M12 92 C12 66 26 54 40 54 C54 54 68 66 68 92 Z")
race = U(head, D(shoulders, circ(40, 32, 19.5)))

# standing stone: menhir with a carved rune, on the ground
stone = U(P("M26 88 L21 32 C20 18 30 10 41 10 C53 10 61 18 59 32 L55 88 Z"), rrect(12, 86, 56, 7, 3))
stone = D(stone, stroke(P("M40 32 L40 70 M40 45 L31 36 M40 45 L49 36 M40 58 L33 65"), 4))

# black book: closed tome with Hermaeus Mora's eye on the cover
book = U(rrect(18, 12, 46, 80, 5), rrect(14, 16, 8, 72, 3))
eye = P("M24 50 C32 36 50 36 58 50 C50 64 32 64 24 50 Z")
eye_ring = D(eye, P("M28 50 C34 40 48 40 54 50 C48 60 34 60 28 50 Z"))
blackbook = D(book, U(eye_ring, stroke(P("M22 16 L22 88"), 2.5), D(circ(41, 50, 7), circ(41, 50, 3.5))))

def svgstr(p):
    f = lambda v: ('%.2f' % v).rstrip('0').rstrip('.')
    o = []; it = skia.Path.Iter(p, False)
    while True:
        verb, pts = it.next()
        if verb == skia.Path.kDone_Verb: break
        if verb == skia.Path.kMove_Verb: o.append('M%s %s' % (f(pts[0].x()), f(pts[0].y())))
        elif verb == skia.Path.kLine_Verb: o.append('L%s %s' % (f(pts[1].x()), f(pts[1].y())))
        elif verb == skia.Path.kQuad_Verb: o.append('Q%s %s %s %s' % (f(pts[1].x()), f(pts[1].y()), f(pts[2].x()), f(pts[2].y())))
        elif verb == skia.Path.kCubic_Verb: o.append('C' + ' '.join('%s %s' % (f(q.x()), f(q.y())) for q in pts[1:4]))
        elif verb == skia.Path.kConic_Verb:
            q = skia.Path.ConvertConicToQuads(pts[0], pts[1], pts[2], it.conicWeight(), 2)
            for k in range(1, len(q), 2): o.append('Q%s %s %s %s' % (f(q[k].x()), f(q[k].y()), f(q[k+1].x()), f(q[k+1].y())))
        elif verb == skia.Path.kClose_Verb: o.append('Z')
    return ''.join(o)

for name, g in [("aec_affliction", affliction), ("aec_perk", perk), ("aec_race", race), ("aec_stone", stone), ("aec_blackbook", blackbook)]:
    g = skia.Simplify(g)
    open(f"{out}/{name}.svg", "w").write(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 102" width="80" height="102">'
        f'<path fill="#FFFFFF" fill-rule="evenodd" d="{svgstr(g)}"/></svg>\n')
    print(name, g.computeTightBounds())
