# tools/make_doc_media.py - renders the docs/ animations straight from pet_frames.h (same bits the device draws)
import re, sys
from PIL import Image, ImageDraw, ImageFont
SRC = 'firmware/pomodoro_rabbit_v1/pet_frames.h'
OUT = 'docs/media/'
txt = open(SRC).read()
arrs = {m.group(1): bytes(int(x, 0) for x in re.findall(r'0x[0-9A-Fa-f]+', m.group(2)))
        for m in re.finditer(r'const uint8_t (\w+)\[\] PROGMEM = \{(.*?)\};', txt, re.S)}
sets = {}
for m in re.finditer(r'const Sprite (\w+)\[\] = \{(.*?)\};', txt, re.S):
    sets[m.group(1)] = [(int(w), int(h), ink, None if mask == 'nullptr' else mask)
                        for w, h, ink, mask in re.findall(r'\{(\d+), (\d+), (\w+), (\w+)\}', m.group(2))]
NAVY, WHITE, BLACK = (16, 28, 64), (255, 255, 255), (0, 0, 0)
S = 2
try: F = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 18)
except Exception: F = ImageFont.load_default()

def sprite(sp, bg=NAVY):
    w, h, ink, mask = sp; rb = (w + 7) // 8; I = arrs[ink]; M = arrs[mask] if mask else None
    im = Image.new('RGB', (w, h), bg); px = im.load()
    for r in range(h):
        for c in range(w):
            b = 0x80 >> (c & 7); i = I[r * rb + c // 8] & b; mk = (M[r * rb + c // 8] & b) if M else 1
            px[c, r] = BLACK if i else (WHITE if mk else bg)
    return im

def canvas(sp, dy=0, label=None, extra=None):
    W, H = 128, 128
    im = Image.new('RGB', (W, H), NAVY)
    s = sprite(sp); im.paste(s, ((W - s.width) // 2, 16 + (96 - s.height) // 2 + dy))
    im = im.resize((W * S, H * S), Image.NEAREST)
    d = ImageDraw.Draw(im)
    if label: d.text((W * S // 2, 14), label, fill=(232, 131, 58), font=F, anchor='mm')
    if extra: extra(d)
    return im

def save(name, frames, durs):
    frames[0].save(OUT + name, save_all=True, append_images=frames[1:], duration=durs, loop=0, optimize=True)
    print(name, len(frames))

E, B, K, T, A = sets['EGG'], sets['BABY'], sets['KID'], sets['TEEN'], sets['ADULT']
# 1. hatch
fr = [canvas(E[i], label='Hatching!') for i in (0, 1, 2, 1, 2, 0, 3, 4, 5)] + [canvas(B[0], label='Hello!')]
save('hatch.gif', fr, [400, 160, 160, 160, 160, 400, 500, 500, 700, 1500])
# 2. stages
st = [(E[0], 'Egg  (0)'), (B[0], 'Baby  (1+)'), (K[0], 'Kid  (10+)'), (T[0], 'Teen  (40+)'), (A[0], 'Adult  (100+)')]
save('stages.gif', [canvas(s, label=l) for s, l in st], [1300] * 5)
# 3. play with ball (same hop pattern as the firmware)
H = [0, -6, 0, -8, 0, -6, 0, -4, 0]
fr = [canvas(K[7], dy, 'Play!') for dy in H] + [canvas(K[0], 0, 'Wheee!')]
fr += [canvas(A[4], dy, 'Play!') for dy in H] + [canvas(A[0], 0, 'Wheee!')]
save('play.gif', fr, ([180] * 8 + [600, 1000]) * 2)
# 4. feed + pet
def heart(d): 
    d.ellipse((110, 56, 124, 70), fill=(220, 40, 40)); d.ellipse((122, 56, 136, 70), fill=(220, 40, 40)); d.polygon([(108, 64), (138, 64), (123, 84)], fill=(220, 40, 40))
fr = [canvas(K[0], label='Feed'), canvas(K[5], label='Yum!'), canvas(K[0], label='Pet'), canvas(K[1], label='Hehe!', extra=heart), canvas(K[0], label='Pet'), canvas(K[1], label='Hehe!', extra=heart)]
save('care.gif', fr, [700, 1300, 600, 300, 300, 900])
# 5. idle blink / ear twitch
fr = [canvas(B[0]), canvas(B[1]), canvas(B[0]), canvas(B[2]), canvas(B[0])]
save('idle.gif', fr, [1500, 150, 1200, 300, 800])
# 6. hungry / tired / sick looks
def tint(im):
    g = im.convert('L').convert('RGB'); return Image.blend(im, g, 0.6)
fr = [canvas(K[3], label='Hungry'), canvas(K[4], label='Tired'), tint(canvas(K[0], label='Sick'))]
save('moods.gif', fr, [1400, 1400, 1400])
# 7. sleep (idle sleep after 1 h)
def zz(step):
    def f(d):
        for k in range(step + 1): d.text((180 + k * 18, 40 - k * 14), 'z' if k < 2 else 'Z', fill=WHITE, font=F)
    return f
fr = []
for k in range(4):
    sp = sets['SLEEP'][k % 2]; im = Image.new('RGB', (128, 128), (40, 40, 40)); im.paste(sprite(sp, (40, 40, 40)), (0, 20))
    im = im.resize((256, 256), Image.NEAREST); zz(k % 3)(ImageDraw.Draw(im)); fr.append(im)
save('sleep.gif', fr, [700] * 4)
