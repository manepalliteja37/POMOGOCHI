from PIL import Image, ImageDraw
import numpy as np, sys
T=int(sys.argv[1]) if len(sys.argv)>1 else 190
GROUPS=[
 ('egg',48,['egg_intact','egg_wobble_left','egg_wobble_right','egg_crack_one','egg_crack_two','egg_open']),
 ('baby',56,['idle','idle_blink','idle_ear_twitch','idle_hungry','idle_tired','feed','pet','play']),
 ('kid',68,['child_idle','child_idle_blink','child_idle_ear_twitch','child_idle_hungry','child_idle_tired','child_feed','child_pet','child_play']),
 ('teen',80,['youth_active_idle','youth_active_signature','youth_active_feed','youth_active_pet','youth_active_play']),
 ('adult',96,['adult_active_idle','adult_active_signature','adult_active_feed','adult_active_pet','adult_active_play']),
]
def outside(im):
    a=im.copy(); w,h=a.size
    for s in [(0,0),(w-1,0),(0,h-1),(w-1,h-1)]:
        if a.getpixel(s)==255: ImageDraw.floodfill(a,s,128)
    return np.array(a)==128
def pack(b):
    h,w=b.shape; rb=(w+7)//8; out=[]
    for y in range(h):
        for bx in range(rb):
            v=0
            for i in range(8):
                x=bx*8+i
                if x<w and b[y,x]: v|=0x80>>i
            out.append(v)
    return out
def arr(name,data):
    s=f'const uint8_t {name}[] PROGMEM = {{\n'
    for i in range(0,len(data),16): s+='  '+','.join(f'0x{v:02X}' for v in data[i:i+16])+',\n'
    return s+'};\n'
H=['// Auto-generated from limengdu/reTerminal_Sticky_Bunny assets/desktop_pet/firmware (MIT License, (c) 2026 limengdu)',
   '// ink = black line pixels, mask = rabbit body (opaque). 1 bit per pixel, MSB first, rows padded to bytes.',
   '#pragma once','#include <Arduino.h>',
   'struct Sprite { uint8_t w, h; const uint8_t* ink; const uint8_t* mask; };','']
total=0; groups=[]; previews=[]
for g,target,names in GROUPS:
    ims=[Image.open(n+'.png').convert('L') for n in names]
    W0,H0=ims[0].size; assert all(i.size==(W0,H0) for i in ims)
    ink=np.zeros((H0,W0),bool)
    for i in ims: ink|=np.array(i)<128
    ys,xs=np.where(ink); p=2
    x0,x1,y0,y1=max(xs.min()-p,0),min(xs.max()+p+1,W0),max(ys.min()-p,0),min(ys.max()+p+1,H0)
    s=target/(y1-y0); nw,nh=round((x1-x0)*s),target
    entries=[]; row=[]
    for n,im in zip(names,ims):
        g_=np.array(im.crop((x0,y0,x1,y1)).resize((nw,nh),Image.BOX))
        out=outside(im)[y0:y1,x0:x1]
        mk=np.array(Image.fromarray(((~out)*255).astype('uint8')).resize((nw,nh),Image.BOX))
        L=g_<T; M=(mk>=128)|L
        cname=f'{g}_{n}'.replace('youth_active_','').replace('adult_active_','').replace('child_','').replace('idle_','')
        cname=cname.replace(f'{g}_{g}_',f'{g}_')
        a1,a2=pack(L),pack(M); total+=len(a1)+len(a2)
        H.append(arr(cname+'_ink',a1)); H.append(arr(cname+'_mask',a2))
        entries.append(f'  {{{nw}, {nh}, {cname}_ink, {cname}_mask}}, // {n}')
        rgb=np.zeros((nh,nw,3),'uint8'); rgb[:]=(16,24,64); rgb[M]=255; rgb[L]=0
        row.append(Image.fromarray(rgb).resize((nw*3,nh*3),Image.NEAREST))
    H.append(f'const Sprite {g.upper()}[] = {{\n'+'\n'.join(entries)+'\n};')
    H.append(f'const uint8_t {g.upper()}_N = {len(names)};\n')
    groups.append((g,len(names),nw,nh)); previews.append(row)
# sleep scenes: full width 128, ink only
for n in ['sleep_scene_a','sleep_scene_b']:
    im=Image.open(n+'.png').convert('L'); nw=128; nh=round(im.height*128/im.width)
    g_=np.array(im.resize((nw,nh),Image.BOX)); L=g_<T; M=np.ones_like(L)
    a1=pack(L); total+=len(a1)
    H.append(arr(n+'_ink',a1))
    rgb=np.zeros((nh,nw,3),'uint8'); rgb[:]=255; rgb[L]=0
    previews.append([Image.fromarray(rgb).resize((nw*3,nh*3),Image.NEAREST)])
H.append(f'const Sprite SLEEP[] = {{\n  {{128, {nh}, sleep_scene_a_ink, nullptr}},\n  {{128, {nh}, sleep_scene_b_ink, nullptr}},\n}};')
H.append(f'#define PET_FRAMES_BYTES {total}')
open('pet_frames.h','w').write('\n'.join(H)+'\n')
print('total bytes',total,groups,'sleep h',nh)
# preview sheet
rows=[]
for r in previews:
    W=sum(i.width for i in r)+8*len(r); Hh=max(i.height for i in r)
    im=Image.new('RGB',(W,Hh+8),(60,60,60)); x=0
    for i in r: im.paste(i,(x,0)); x+=i.width+8
    rows.append(im)
W=max(r.width for r in rows); Hh=sum(r.height for r in rows)
sheet=Image.new('RGB',(W,Hh),(60,60,60)); y=0
for r in rows: sheet.paste(r,(0,y)); y+=r.height
sheet.save('frames_preview.png'); print(sheet.size)
