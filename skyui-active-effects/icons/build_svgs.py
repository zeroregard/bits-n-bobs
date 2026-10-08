import skia, sys, re
out = sys.argv[1]
def P(d):
    t = re.findall(r'[MLVCZ]|-?[\d.]+', d); p = skia.Path(); i = 0; x=y=0
    while i < len(t):
        c = t[i]; i += 1
        if c=='M': x,y=float(t[i]),float(t[i+1]); p.moveTo(x,y); i+=2
        elif c=='L': x,y=float(t[i]),float(t[i+1]); p.lineTo(x,y); i+=2
        elif c=='V': y=float(t[i]); p.lineTo(x,y); i+=1
        elif c=='C':
            a=[float(v) for v in t[i:i+6]]; p.cubicTo(*a); x,y=a[4],a[5]; i+=6
        elif c=='Z': p.close()
    return p
def stroke(p, w, join=skia.Paint.kRound_Join, cap=skia.Paint.kButt_Cap):
    pt = skia.Paint(); pt.setStyle(skia.Paint.kStroke_Style); pt.setStrokeWidth(w); pt.setStrokeJoin(join); pt.setStrokeCap(cap)
    r = skia.Path(); pt.getFillPath(p, r); return r
def U(*ps):
    r = ps[0]
    for p in ps[1:]: r = skia.Op(r, p, skia.PathOp.kUnion_PathOp)
    return r
def D(a,b): return skia.Op(a,b,skia.PathOp.kDifference_PathOp)
def circ(x,y,r): p=skia.Path(); p.addCircle(x,y,r); return p
def rrect(x,y,w,h,r): p=skia.Path(); p.addRRect(skia.RRect.MakeRectXY(skia.Rect.MakeXYWH(x,y,w,h),r,r)); return p
RING = 10      # shield wall thickness; outer edge stays where the original 6-wide stroke put it
GLYPH = 0.9    # inner symbol scale, around the shield's centre
SP = P("M40 2 L76 12 V46 C76 70 60 86 40 96 C20 86 4 70 4 46 V12 Z")
shield = D(U(SP, stroke(SP, 6)), D(SP, stroke(SP, 2 * (RING - 3))))
hour = U(rrect(24,22,32,5,2), rrect(24,71,32,5,2),
         stroke(P("M28 27 L52 27 L40 49 L52 71 L28 71 L40 49 Z"),4),
         P("M34 31 L46 31 L40 41 Z"), P("M40 57 L48 67 L32 67 Z"))
skull = U(circ(40,42,17), rrect(30,50,20,18,3))
holes = U(circ(33.5,42,5), circ(46.5,42,5), P("M40 49 L37 55 L43 55 Z"),
          rrect(34.5,60,2,7.5,0), rrect(39,60,2,7.5,0), rrect(43.5,60,2,7.5,0))
skull = D(skull, holes)
crown = U(stroke(P("M24 64 L22 36 L32 47 L40 30 L48 47 L58 36 L56 64 Z"),2), P("M24 64 L22 36 L32 47 L40 30 L48 47 L58 36 L56 64 Z"),
          rrect(24,66,32,6,2), circ(22,32,3.5), circ(40,25,3.5), circ(58,32,3.5))
def svgstr(p):
    f=lambda v: ('%.2f'%v).rstrip('0').rstrip('.')
    out=[]; it=skia.Path.Iter(p, False)
    while True:
        pts=[skia.Point(0,0)]*4
        verb, pts = it.next()
        if verb==skia.Path.kDone_Verb: break
        if verb==skia.Path.kMove_Verb: out.append('M%s %s'%(f(pts[0].x()),f(pts[0].y())))
        elif verb==skia.Path.kLine_Verb: out.append('L%s %s'%(f(pts[1].x()),f(pts[1].y())))
        elif verb==skia.Path.kQuad_Verb: out.append('Q%s %s %s %s'%(f(pts[1].x()),f(pts[1].y()),f(pts[2].x()),f(pts[2].y())))
        elif verb==skia.Path.kCubic_Verb: out.append('C'+' '.join('%s %s'%(f(q.x()),f(q.y())) for q in pts[1:4]))
        elif verb==skia.Path.kConic_Verb:
            q=skia.Path.ConvertConicToQuads(pts[0],pts[1],pts[2],it.conicWeight(),2)
            for k in range(1,len(q),2): out.append('Q%s %s %s %s'%(f(q[k].x()),f(q[k].y()),f(q[k+1].x()),f(q[k+1].y())))
        elif verb==skia.Path.kClose_Verb: out.append('Z')
    return ''.join(out)
for name, g in [("effects_ongoing",hour),("effects_harmful",skull),("effects_boons",crown)]:
    g = skia.Path(g); g.transform(skia.Matrix().setScale(GLYPH, GLYPH, 40, 49))
    full = skia.Op(shield, g, skia.PathOp.kUnion_PathOp)
    full = skia.Simplify(full); full.offset(0,2)
    b = full.computeTightBounds()
    open(f"{out}/{name}.svg","w").write(
      f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 102" width="80" height="102">'
      f'<path fill="#FFFFFF" fill-rule="evenodd" d="{svgstr(full)}"/></svg>\n')
    print(name, b)