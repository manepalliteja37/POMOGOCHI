# shop_art_src.py - the shop pixel art (hats) and scene drawings, used by gen_shop_art.py
# hats: '.' clear, '#' ink, 'o' colour A, 'x' colour B
HATS = {
 'bow': (0xF8B2, 0xF8B2, [   # pink
  ".###.....###.",
  "#ooo#...#ooo#",
  "#oooo###oooo#",
  "#ooo#ooo#ooo#",
  "#oooo###oooo#",
  "#ooo#...#ooo#",
  ".###.....###.",
 ]),
 'flower': (0xFBD5, 0xFFE0, [   # pink petals, yellow middle
  "....###....",
  "...#ooo#...",
  ".###ooo###.",
  "#ooo#x#ooo#",
  "#oo#xxx#oo#",
  "#ooo#x#ooo#",
  ".###ooo###.",
  "...#ooo#...",
  "....###....",
 ]),
 'party': (0x3B7F, 0xFFE0, [   # blue cone, yellow stripes + pompom
  ".....###.....",
  "....#xxx#....",
  "....#xxx#....",
  ".....###.....",
  ".....#o#.....",
  "....#ooo#....",
  "....#xxx#....",
  "...#ooooo#...",
  "...#ooooo#...",
  "..#xxxxxxx#..",
  "..#ooooooo#..",
  ".#ooooooooo#.",
  ".#xxxxxxxxx#.",
  "#############",
 ]),
 'cap': (0xE8E4, 0xFFFF, [   # red cap, white button
  "......###......",
  ".....#xxx#.....",
  "...##ooooo##...",
  "..#ooooooooo#..",
  ".#ooooooooooo#.",
  ".#ooooooooooo#.",
  "#ooooooooooooo#",
  "###############",
  "........#ooooo#",
  ".........######",
 ]),
 'tophat': (0x4208, 0xE8E4, [   # dark grey, red band
  "...#########...",
  "...#ooooooo#...",
  "...#ooooooo#...",
  "...#ooooooo#...",
  "...#ooooooo#...",
  "...#xxxxxxx#...",
  "...#xxxxxxx#...",
  "###############",
  "#ooooooooooooo#",
  "###############",
 ]),
 'crown': (0xFEA0, 0xE8E4, [   # gold, red gems
  "#.....#.....#",
  "##...###...##",
  "#o#.#ooo#.#o#",
  "#oo#ooxoo#oo#",
  "#ooooxxxoooo#",
  "#oooooxooooo#",
  "#xooooooooox#",
  "#############",
  "#ooooooooooo#",
  "#############",
 ]),
}
HAT_ORDER = ['bow','flower','party','cap','tophat','crown']
HAT_COST  = [5, 8, 12, 15, 25, 40]
HAT_NEED_STREAK = [0,0,0,0,0,30]
SCENE_ORDER = ['grass','rain','stars','lamps']
SCENE_COST = [10, 15, 25, 40]
SCENE_NEED_STREAK = [0,0,0,100]
SCENE_COL = {'grass':0x3E66,'rain':0x8E7F,'stars':0xFEA0,'lamps':0xFC60}
SCENE_TOP = {'grass':False,'rain':True,'stars':True,'lamps':True}
HAT_LABEL = ['Bow','Flower','Party hat','Cap','Top hat','Crown']
SCENE_LABEL = ['Grass','Rain','Stars','Festival lamps']

from PIL import Image, ImageDraw
import math, random
W,H=128,24
def grass():
    im=Image.new('1',(W,H),0); d=ImageDraw.Draw(im); random.seed(3)
    d.rectangle([0,H-3,W-1,H-1],fill=1)
    x=0
    while x<W:
        h=random.randint(5,11); lean=random.choice([-2,-1,0,1,2])
        d.line([x,H-3,x+lean,H-3-h],fill=1); d.line([x+1,H-3,x+lean+1,H-3-h+2],fill=1)
        x+=random.randint(3,5)
    for fx in (14,60,104):    # little flowers
        d.line([fx,H-3,fx,H-14],fill=1); d.ellipse([fx-3,H-19,fx+3,H-13],outline=1); d.point((fx,H-16),fill=1)
    return im
def rain():
    im=Image.new('1',(W,H),0); d=ImageDraw.Draw(im)
    for cx in (22,98):        # two clouds
        for ox,oy,r in ((-9,7,5),(0,5,7),(9,7,5)): d.ellipse([cx+ox-r,oy-r,cx+ox+r,oy+r],fill=1)
        d.rectangle([cx-14,8,cx+14,12],fill=1)
    random.seed(5)
    for i in range(26):
        x=random.randint(2,W-3); y=random.randint(15,H-4); d.line([x,y,x-1,y+3],fill=1)
    return im
def star(d,cx,cy,r):
    pts=[]
    for i in range(10):
        a=-math.pi/2+i*math.pi/5; rr=r if i%2==0 else r*0.45
        pts.append((cx+rr*math.cos(a),cy+rr*math.sin(a)))
    d.polygon(pts,fill=1)
def stars():
    im=Image.new('1',(W,H),0); d=ImageDraw.Draw(im)
    d.ellipse([104,2,118,16],fill=1); d.ellipse([108,0,121,13],fill=0)   # moon
    for cx,cy,r in ((10,6,4),(32,15,3),(52,5,5),(74,13,3),(90,4,3),(20,20,2),(64,21,2),(122,20,2)): star(d,cx,cy,r)
    return im
def lamps():
    im=Image.new('1',(W,H),0); d=ImageDraw.Draw(im)
    pts=[(x, 2+int(5*math.sin(math.pi*((x%43)/43.0)))) for x in range(W)]
    for x,y in pts: d.point((x,y),fill=1)
    for lx in (10,32,53,75,96,118):
        y=2+int(5*math.sin(math.pi*((lx%43)/43.0)))
        d.line([lx,y,lx,y+6],fill=1)
        d.polygon([(lx-5,y+12),(lx+5,y+12),(lx+3,y+16),(lx-3,y+16)],fill=1)   # diya bowl
        d.polygon([(lx,y+6),(lx+2,y+10),(lx,y+11),(lx-2,y+10)],fill=1)        # flame
    return im
SCENES={'grass':grass,'rain':rain,'stars':stars,'lamps':lamps}
