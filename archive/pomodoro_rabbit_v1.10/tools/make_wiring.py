# tools/make_wiring.py - draws docs/media/wiring.png (connection map for docs/setup.md)
from PIL import Image, ImageDraw, ImageFont
B='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'; R='/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
f=ImageFont.truetype(B,22); fs=ImageFont.truetype(R,17); ft=ImageFont.truetype(B,30)
rows=[('TFT screen','VCC','3V3','#e53935'),('TFT screen','GND','GND','#222222'),('TFT screen','CS','D8','#8e24aa'),
('TFT screen','RESET','RST','#6d4c41'),('TFT screen','A0 / DC','D3','#1e88e5'),('TFT screen','SDA','D7','#00897b'),
('TFT screen','SCK','D5','#f9a825'),('TFT screen','LED','D4','#fb8c00'),
('MODE button','leg 1 / leg 2','D1 / GND','#43a047'),('OK button','leg 1 / leg 2','D2 / GND','#43a047'),
('BACK button','leg 1 / leg 2','D0 / GND','#43a047'),('BACK 10k resistor','one end / other end','D0 / 3V3','#c62828'),
('1k resistor','one end / other end','D6 / BC547 B','#5e35b1'),('BC547','E (emitter)','GND','#222222'),
('Buzzer (+)','+','3V3','#e53935'),('Buzzer (-)','-','BC547 C','#3949ab'),('1N4148 diode','stripe / plain','3V3 / BC547 C','#757575')]
W=1100; RH=46; top=110; H=top+RH*len(rows)+70
im=Image.new('RGB',(W,H),'white'); d=ImageDraw.Draw(im)
d.text((W//2,40),'Pomodoro Rabbit - wiring map',fill='#e8833a',font=ft,anchor='mm')
d.text((W//2,78),'Part pin  ───  NodeMCU pin      (same colour = same wire)',fill='#555',font=fs,anchor='mm')
for i,(part,pin,mcu,col) in enumerate(rows):
    y=top+i*RH+RH//2
    if i%2==0: d.rectangle((20,y-RH//2,W-20,y+RH//2),fill='#f6f6f6')
    d.text((40,y),part,fill='#222',font=f,anchor='lm'); d.text((330,y),pin,fill='#555',font=fs,anchor='lm')
    d.line((560,y,800,y),fill=col,width=7); d.ellipse((552,y-8,568,y+8),fill=col); d.ellipse((792,y-8,808,y+8),fill=col)
    d.rounded_rectangle((830,y-17,W-40,y+17),radius=8,fill='#1c2a4a'); d.text(((830+W-40)//2,y),mcu,fill='white',font=f,anchor='mm')
d.text((W//2,H-30),'Power: USB cable into the NodeMCU. Check every wire twice before plugging in.',fill='#c62828',font=fs,anchor='mm')
im.save('docs/media/wiring.png'); print(im.size)
