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

RC=skia.Paint.kRound_Cap
def Q(d):
    t=re.findall(r'[MLQCZ]|-?[\d.]+',d); p=skia.Path(); i=0
    while i<len(t):
        c=t[i]; i+=1
        if c=='M': p.moveTo(float(t[i]),float(t[i+1])); i+=2
        elif c=='L': p.lineTo(float(t[i]),float(t[i+1])); i+=2
        elif c=='Q': a=[float(v) for v in t[i:i+4]]; p.quadTo(*a); i+=4
        elif c=='C': a=[float(v) for v in t[i:i+6]]; p.cubicTo(*a); i+=6
        elif c=='Z': p.close()
    return p
def xf(p, tx, ty, deg):
    m=skia.Matrix(); m.setTranslate(tx,ty); m.preRotate(deg); r=skia.Path(p); r.transform(m); return r
# draining
shield = stroke(P("M40 4 L76 14 V48 C76 72 60 88 40 98 C20 88 4 72 4 48 V14 Z"), 6)
fill = Q("M14 50 Q27 44 40 50 Q53 56 66 50 C66 66 54 78 40 86 C26 78 14 66 14 50 Z")
drain = skia.Simplify(U(shield, fill))
# twin masks
face = Q("M0 0 C0 -6 40 -6 40 0 C40 26 32 42 20 46 C8 42 0 26 0 0 Z")
sad_feat = U(stroke(Q("M7 13 L15 10"),3.5,cap=RC), stroke(Q("M25 10 L33 13"),3.5,cap=RC), Q("M10 37 Q20 23 30 37 Q20 32 10 37 Z"))
happy_feat = U(stroke(Q("M7 13 Q11 7 15 13"),3.5,cap=RC), stroke(Q("M25 13 Q29 7 33 13"),3.5,cap=RC), Q("M8 22 Q20 25 32 22 Q29 37 20 37 Q11 37 8 22 Z"))
back = D(face, sad_feat); front = D(face, happy_feat)
back = xf(back,36,30,14); frontT = xf(front,4,22,-14); faceT = xf(face,4,22,-14)
gap = U(faceT, stroke(faceT,3))
masks = skia.Simplify(U(D(back,gap), frontT))
sm=skia.Matrix(); sm.setScale(1.1,1.1); masks.transform(sm); b=masks.computeTightBounds()
masks.offset(40-(b.left()+b.right())/2, 51-(b.top()+b.bottom())/2)
import sys; out=sys.argv[1]
for name,g in [("effects_ongoing",drain),("effects_lasting",masks)]:
    if name=="effects_ongoing": g.offset(0,0)
    open(f"{out}/{name}.svg","w").write('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 102" width="80" height="102"><path fill="#FFFFFF" fill-rule="evenodd" d="%s"/></svg>\n'%svgstr(g))
    print(name,g.computeTightBounds())
