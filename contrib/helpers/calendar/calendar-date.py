from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import datetime as dt, os, ctypes as c, ctypes.wintypes as wt, hashlib, json, sys
ROOT=Path(__file__).resolve().parent
TARGET=Path(os.environ["APPDATA"])/"Shell/icones-dock/notion calendar.png"
ORIGINAL=ROOT/"original.png"
STATE=ROOT/"state.json"
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def render(date):
    im=Image.open(ORIGINAL).convert("RGBA")
    assert im.size==(512,512)
    # Sample the original blank gradient alongside the date lettering.
    for y in range(92,411):
        l=im.getpixel((105,y));r=im.getpixel((406,y))
        for x in range(130,382):
            t=(x-105)/301
            im.putpixel((x,y),tuple(round(a+(b-a)*t) for a,b in zip(l,r)))
    draw=ImageDraw.Draw(im)
    fonts=Path(os.environ["WINDIR"])/"Fonts"
    def centered(label,font,cy,fill):
        box=draw.textbbox((0,0),label,font=font)
        draw.text((256-(box[0]+box[2])/2,cy-(box[1]+box[3])/2),label,font=font,fill=fill)
    day=ImageFont.truetype(str(fonts/"segoeuib.ttf"),80)
    num=ImageFont.truetype(str(fonts/"segoeuil.ttf"),245)
    centered(("Mon","Tue","Wed","Thu","Fri","Sat","Sun")[date.weekday()],day,146,(255,83,76,255))
    centered(str(date.day),num,299,(20,20,22,255))
    return im
def main():
    if len(sys.argv)>1 and sys.argv[1]=="--preview":
        for d in [dt.date(2026,11,25),dt.date(2026,12,1),dt.date(2026,12,31)]:
            render(d).save(ROOT/f"preview-{d}.png")
        return
    today=dt.date.today().isoformat()
    state=json.loads(STATE.read_text()) if STATE.exists() else {}
    if not TARGET.exists():return # honor removal of the override
    old=digest(TARGET)
    if old not in (digest(ORIGINAL),state.get("sha256")):
        return # don't overwrite a manually changed icon
    if state.get("date")==today and old==state.get("sha256"):return
    tmp=TARGET.with_suffix(".calendar-tmp")
    render(dt.date.today()).save(tmp,format="PNG")
    os.replace(tmp,TARGET)
    STATE.write_text(json.dumps({"date":today,"sha256":digest(TARGET)}))
    u=c.windll.user32
    u.FindWindowW.restype=wt.HWND
    u.PostMessageW.argtypes=[wt.HWND,wt.UINT,wt.WPARAM,wt.LPARAM]
    h=u.FindWindowW("ShellMensagens",None)
    if h:u.PostMessageW(h,u.RegisterWindowMessageW("ShellIconeMudou"),0,0)
if __name__=="__main__":main()
