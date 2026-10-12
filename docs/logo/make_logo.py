#!/usr/bin/env python3
"""Generates the Motor:Cycles logo set (SVG, no fonts: every letter is drawn).

Mark: a twelve-tooth cog (the motor, rotation, cycles) holding a hard-clipped
kick waveform on a signal-red disc (distortion, power). Wordmark: a stencil
pixel face on the Model:Cycles screen grid, slanted forward, cut by two
machined slats. Palette: carbon black, bone, one signal red.
"""
import math,pathlib
BLACK,BONE,RED='#0b0b0c','#ece8e1','#e5281f'
OUT=pathlib.Path(__file__).resolve().parent
G={ # 5x7 glyphs, rows top to bottom
'M':['10001','11011','10101','10101','10001','10001','10001'],
'O':['11111','10001','10001','10001','10001','10001','11111'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'C':['11111','10000','10000','10000','10000','10000','11111'],
'Y':['10001','10001','01010','00100','00100','00100','00100'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'S':['11111','10000','10000','11111','00001','00001','11111'],
'H':['10001','10001','10001','11111','10001','10001','10001'],
'I':['11111','00100','00100','00100','00100','00100','11111'],
'N':['10001','11001','10101','10101','10011','10001','10001'],
'P':['11110','10001','10001','11110','10000','10000','10000'],
'W':['10001','10001','10001','10101','10101','11011','10001'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'A':['01110','10001','10001','11111','10001','10001','10001'],
'/':['00001','00010','00010','00100','01000','01000','10000'],
'.':['0','0','0','0','0','0','1'],
':':['0','1','1','0','1','1','0'],
' ':['000'],
}
def text_rects(s,x,y,c,gap=1,colors=None):
    """Rects for string s with cell size c, top-left (x,y). Returns (svg, width)."""
    out=[];cx=x
    for ch in s:
        g=G[ch];w=len(g[0])
        col=(colors or {}).get(ch,BONE)
        for r,row in enumerate(g if len(g)==7 else ['0'*w]*7):
            k=0
            while k<w:
                if row[k]=='1':
                    e=k
                    while e<w and row[e]=='1':e+=1
                    out.append(f'<rect x="{cx+k*c-0.3:.2f}" y="{y+r*c-0.3:.2f}" width="{(e-k)*c+0.6:.2f}" height="{c+0.6:.2f}" fill="{col}"/>')
                    k=e
                else:k+=1
        cx+=(w+gap)*c
    return '\n'.join(out),cx-gap*c-x
def cog(cx,cy,r_out,r_in,teeth=12):
    pts=[]
    for i in range(teeth):
        a0=2*math.pi*i/teeth
        for da,r in ((-0.30,r_in),(-0.17,r_out),(0.17,r_out),(0.30,r_in)):
            a=a0+da*2*math.pi/teeth
            pts.append(f'{cx+r*math.sin(a):.2f},{cy-r*math.cos(a):.2f}')
        a=a0+0.5*2*math.pi/teeth
        pts.append(f'{cx+r_in*math.sin(a):.2f},{cy-r_in*math.cos(a):.2f}')
    return ' '.join(pts)
def mark(cx,cy,s,uid):
    """Cog of outer radius s with the clipped kick inside."""
    ro,ri,ring,disc=s,s*0.80,s*0.62,s*0.53
    w=[]
    for i in range(241):
        t=i/240;x=cx-disc*0.86+2*disc*0.86*t
        ph=2*math.pi*(0.9*t+2.6*t*t)          # pitch rising across the disc: the kick's cycles tighten
        v=math.sin(ph)*3.0*math.exp(-2.6*t)     # driven far past full scale, decaying
        v=max(-1,min(1,v))                      # then hard-clipped: flat-topped first cycles
        w.append(f'{x:.2f},{cy-v*disc*0.50:.2f}')
    return f'''<g>
<polygon points="{cog(cx,cy,ro,ri)}" fill="{BONE}"/>
<circle cx="{cx}" cy="{cy}" r="{ring:.2f}" fill="{BLACK}"/>
<circle cx="{cx}" cy="{cy}" r="{disc:.2f}" fill="{RED}"/>
<clipPath id="disc{uid}"><circle cx="{cx}" cy="{cy}" r="{disc:.2f}"/></clipPath>
<polyline points="{' '.join(w)}" fill="none" stroke="{BLACK}" stroke-width="{s*0.068:.2f}" stroke-linejoin="miter" stroke-linecap="square" clip-path="url(#disc{uid})"/>
</g>'''
def slats(x,y,w,c,color=BLACK):
    """Two machined cuts through the wordmark."""
    h=c*0.22
    return ''.join(f'<rect x="{x-c:.2f}" y="{y+r*c-h/2:.2f}" width="{w+2*c:.2f}" height="{h:.2f}" fill="{color}"/>' for r in (2,5))
def svg(w,h,body,title,bg=True):
    plate=f'<rect width="{w}" height="{h}" fill="{BLACK}"/>' if bg else ''
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" role="img"><title>{title}</title>{plate}{body}</svg>\n'

# Horizontal lockup.
c=14;W,H=1280,300
word,ww=text_rects('MOTOR:CYCLES',0,0,c,colors={':':RED})
tag,tw=text_rects('RHYTHMIC NOISE / POWER NOISE FIRMWARE',0,0,4.2,gap=1)
x0,y0=300,82
lock=(mark(150,150,118,'a')
 +f'<g transform="translate({x0},{y0}) skewX(-10)">{word}{slats(0,0,ww,c)}</g>'
 +f'<rect x="{x0}" y="{y0+7*c+26}" width="{ww-10}" height="4" fill="{RED}"/>'
 +f'<g transform="translate({x0},{y0+7*c+46})">{tag}</g>')
(OUT/'motorcycles-logo.svg').write_text(svg(W,H,lock,'Motor:Cycles'))
# Square mark (app icon, avatar).
(OUT/'motorcycles-mark.svg').write_text(svg(512,512,mark(256,256,210,'b'),'Motor:Cycles mark'))
# Stacked: mark over wordmark.
c2=12;word2,ww2=text_rects('MOTOR:CYCLES',0,0,c2,colors={':':RED})
tag2,tw2=text_rects('RHYTHMIC NOISE / POWER NOISE FIRMWARE',0,0,3.4,gap=1)
SW,SH=1000,960
stack=(mark(SW/2,320,240,'c')
 +f'<g transform="translate({(SW-ww2)/2+10},{640}) skewX(-10)">{word2}{slats(0,0,ww2,c2)}</g>'
 +f'<rect x="{(SW-ww2)/2}" y="{640+7*c2+26}" width="{ww2}" height="4" fill="{RED}"/>'
 +f'<g transform="translate({(SW-tw2)/2},{640+7*c2+48})">{tag2}</g>')
(OUT/'motorcycles-logo-stacked.svg').write_text(svg(SW,SH,stack,'Motor:Cycles'))
print('word',ww,'tag',tw)
# macOS app icon: the mark on a rounded plate inside Apple's icon grid.
icon=(f'<rect x="100" y="100" width="824" height="824" rx="185" fill="{BLACK}"/>'+mark(512,512,330,'d'))
(OUT/'motorcycles-appicon.svg').write_text(svg(1024,1024,icon,'Motor:Cycles',bg=False))
