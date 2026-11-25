"""Independent WMP1 metrics provider. Python 3.10+; Windows only. No credentials read."""
import ctypes as c, ctypes.wintypes as wt
import datetime as dt, json, os, pathlib, queue, shutil, subprocess, threading, time
import urllib.request, urllib.parse, concurrent.futures, sys
ROOT=pathlib.Path(os.environ["APPDATA"])/"Shell"/"plugins"
ROOT.mkdir(parents=True,exist_ok=True)
FILE=ROOT/"metrics.wmp"
CONFIG=ROOT/"metrics.json"
ACTION=ROOT/"metrics.action"
DEFAULT={"city":"Cairo","country":"Egypt","method":5,"school":0,"midnightMode":1,"calendarMethod":"UAQ","hijriAdjustment":0,"ishaDeadline":"fajr","timeFormat":"24h"}
METHODS={0:"Shia Ithna-Ashari (Qum)",1:"Karachi",2:"ISNA",3:"Muslim World League",4:"Umm al-Qura, Makkah",5:"Egyptian General Authority of Survey",7:"Tehran",8:"Gulf Region",9:"Kuwait",10:"Qatar",11:"Singapore",12:"France (UOIF)",13:"Diyanet, Turkey",14:"Russia",15:"Moonsighting Committee",16:"Dubai",17:"Malaysia (JAKIM)",18:"Tunisia",19:"Algeria",20:"Indonesia",21:"Morocco",22:"Portugal",23:"Jordan",99:"Custom angles"}
NAMES={"Fajr":"Fajr","Sunrise":"Sunrise / Shuruq","Dhuhr":"Dhuhr","Asr":"Asr","Maghrib":"Maghrib","Isha":"Isha","Midnight":"Islamic midnight","Lastthird":"Last third of the night"}
PRAYERS=["Fajr","Dhuhr","Asr","Maghrib","Isha"]
NO_WINDOW=0x08000000
def atomic(p,data):
    tmp=p.with_suffix(p.suffix+".tmp")
    tmp.write_text(data,encoding="utf-8")
    for attempt in range(8):
        try:
            os.replace(tmp,p)
            return
        except PermissionError:
            if attempt==7: raise
            time.sleep(.025*(attempt+1))
def config():
    if not CONFIG.exists(): atomic(CONFIG,json.dumps(DEFAULT,indent=2))
    return {**DEFAULT,**json.loads(CONFIG.read_text(encoding="utf-8-sig"))}
def run(args,timeout=8):
    return subprocess.check_output(args,timeout=timeout,creationflags=NO_WINDOW,stderr=subprocess.DEVNULL,text=True,encoding="utf-8",errors="replace").strip()
def api(path,params=None):
    url="https://api.aladhan.com/v1/"+path
    if params: url+="?"+urllib.parse.urlencode(params)
    with urllib.request.urlopen(url,timeout=10) as r: data=json.load(r)
    if data["code"]!=200: raise RuntimeError("Prayer API returned error")
    return data["data"]
def prayer(cfg):
    # Calendar date is determined by the configured city's timezone, not the computer's timezone.
    args={k:cfg[k] for k in ("city","country","method","school","midnightMode","calendarMethod")}
    args.update(iso8601="true",adjustment=cfg["hijriAdjustment"])
    if cfg["method"]==99:
        args["methodSettings"]=cfg.get("methodSettings","18,null,17")
    today=api("timingsByCity",args)
    date=dt.datetime.strptime(today["date"]["gregorian"]["date"],"%d-%m-%Y").date()
    def fetch(day):return api("timingsByCity/"+day.strftime("%d-%m-%Y"),args)
    def times(d): return {k:dt.datetime.fromisoformat(d["timings"][k]) for k in NAMES}
    # Resolve the date in the configured city's timezone, not the PC timezone.
    from zoneinfo import ZoneInfo
    try:
        actual=dt.datetime.now(ZoneInfo(today["meta"]["timezone"])).date()
    except (KeyError,ValueError):
        actual=dt.datetime.now(times(today)["Fajr"].tzinfo).date()
    if date!=actual:date=actual;today=fetch(date)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        y=pool.submit(fetch,date-dt.timedelta(days=1))
        t=pool.submit(fetch,date+dt.timedelta(days=1))
        yesterday,tomorrow=y.result(),t.result()
    return date,times(today),times(tomorrow),today["date"]["hijri"],times(yesterday)
def codex_exe():
    path=shutil.which("codex.exe")
    if path: return path
    root=pathlib.Path(os.environ["LOCALAPPDATA"])/"OpenAI"/"Codex"/"bin"
    candidates=list(root.glob("*/codex.exe"))
    if candidates:return str(max(candidates,key=lambda p:p.stat().st_mtime))
    raise RuntimeError("Codex CLI not found")
def codex():
    p=subprocess.Popen([codex_exe(),"app-server","--listen","stdio://"],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding="utf-8",creationflags=NO_WINDOW)
    q=queue.Queue()
    def reader():
        for line in p.stdout:
            try:q.put(json.loads(line))
            except ValueError:pass
    threading.Thread(target=reader,daemon=True).start()
    def send(obj):
        p.stdin.write(json.dumps(obj)+"\n");p.stdin.flush()
    def reply(id):
        end=time.monotonic()+12
        while time.monotonic()<end:
            r=q.get(timeout=max(.01,end-time.monotonic()))
            if r.get("id")==id:
                if "error" in r: raise RuntimeError("Codex RPC error")
                return r["result"]
        raise TimeoutError()
    try:
        send({"method":"initialize","id":1,"params":{"clientInfo":{"name":"wacos_metrics","title":"WacOS Metrics","version":"1.0.0"}}})
        reply(1)
        send({"method":"initialized"})
        send({"method":"account/rateLimits/read","id":2})
        result=reply(2)
        limits=result.get("rateLimits") or {}
        if result.get("rateLimitsByLimitId",{}).get("codex"):limits=result["rateLimitsByLimitId"]["codex"]
        return {w["windowDurationMins"]:w for w in (limits.get("primary"),limits.get("secondary")) if w and w.get("windowDurationMins") in (300,10080)}
    finally:
        p.terminate()
        try:p.wait(timeout=2)
        except subprocess.TimeoutExpired:p.kill();p.wait()
class IFROW(c.Structure):
    _fields_=[("luid",c.c_uint64),("index",c.c_uint32),("guid",c.c_ubyte*16),("alias",c.c_wchar*257),("description",c.c_wchar*257),("pal",c.c_uint32),("pa",c.c_ubyte*32),("ppa",c.c_ubyte*32)]+[(s,c.c_uint32) for s in ("mtu","type","tunnel","media","physical","access","direction")]+[("flags",c.c_ubyte)]+[(s,c.c_uint32) for s in ("oper","admin","connected")]+[("networkguid",c.c_ubyte*16),("connection",c.c_uint32)]+[(s,c.c_uint64) for s in ("txspeed","rxspeed","rx","inucast","innucast","indiscards","inerrors","inunknown","inucastbytes","inmulti","inbroadcast","tx","outucast","outnucast","outdiscards","outerrors","outucastbytes","outmulti","outbroadcast","outqueue")]
class IFTABLE(c.Structure):
    _fields_=[("count",c.c_uint32),("rows",IFROW*1)]
ip=c.WinDLL("iphlpapi")
ip.GetIfTable2.argtypes=[c.POINTER(c.c_void_p)];ip.GetIfTable2.restype=c.c_uint32
ip.FreeMibTable.argtypes=[c.c_void_p];ip.FreeMibTable.restype=None
def counters():
    ptr=c.c_void_p()
    if ip.GetIfTable2(c.byref(ptr)):raise OSError("GetIfTable2 failed")
    try:
        n=IFTABLE.from_address(ptr.value).count
        if n>4096:raise ValueError("Invalid interface count")
        rows=(IFROW*n).from_address(ptr.value+IFTABLE.rows.offset)
        # Up, physical, Ethernet/Wi-Fi: excludes VPN, loopback and virtual adapters.
        return {r.luid:(int(r.tx),int(r.rx),r.alias) for r in rows if r.flags&1 and r.oper==1 and r.type in (6,71)}
    finally:ip.FreeMibTable(ptr)
def rate(a,b,elapsed):
    if not a or not b or elapsed<=0:return None
    return tuple(sum(max(0,b[k][j]-a[k][j]) for k in b.keys()&a.keys() if b[k][j]>=a[k][j])/elapsed/1_000_000 for j in (0,1))
def temperatures():
    cpu=gpu=None
    # Privileged read-only helper writes to a protected Program Files directory.
    # No driver access from the WacOS process or its unprivileged provider.
    try:
        sensor=pathlib.Path(os.environ.get("ProgramFiles",r"C:\Program Files"))/"WacOS Metrics Sensor"/"temperatures.json"
        data=json.loads(sensor.read_text(encoding="utf-8"))
        if -2 <= time.time()-float(data["utc"]) <= 15:
            def valid(x):return float(x) if isinstance(x,(int,float)) and 0<x<125 else None
            cpu,gpu=valid(data.get("cpu")),valid(data.get("gpu"))
    except (OSError,ValueError,KeyError,TypeError):pass
    if cpu is not None and gpu is not None:return cpu,gpu
    # NVIDIA official driver CLI; optional WMI temperature provider for CPU and non-NVIDIA GPU.
    if gpu is None:
        try:
            values=run(["nvidia-smi","--query-gpu=temperature.gpu","--format=csv,noheader,nounits"]).splitlines()
            gpu=max(float(x) for x in values if x.strip().isdigit())
        except Exception:pass
    ps=r"""$ErrorActionPreference='SilentlyContinue'; foreach($ns in @('root/LibreHardwareMonitor','root/OpenHardwareMonitor')) { Get-CimInstance -Namespace $ns -ClassName Sensor | Where-Object { $_.SensorType -eq 'Temperature' } | Select-Object Name,Identifier,Value };"""
    try:
        raw=run(["powershell.exe","-NoProfile","-NonInteractive","-Command","$a=@("+ps+"); ConvertTo-Json -InputObject $a -Compress"],timeout=6)
        sensors=json.loads(raw)
        for x in sensors:
            name=str(x.get("Name","")).lower(); ident=str(x.get("Identifier","")).lower()
            val=x.get("Value")
            if not isinstance(val,(int,float)) or not 0<val<125:continue
            if cpu is None and ("cpu" in ident or "cpu" in name) and ("package" in name or "tctl" in name or "tdie" in name):cpu=val
            if gpu is None and "gpu" in ident and ("core" in name or "temperature" in name):gpu=val
    except Exception:pass
    return cpu,gpu
def clean(s):return str(s).replace("\t"," ").replace("\r"," ").replace("\n"," ")
def item(rows,id,a,b,style="label",width=30,placement="before-stats",urgent=False):rows.append("\t".join(map(clean,("item",id,a,b,style,width,placement,"urgent" if urgent else "normal"))))
def row(rows,id,a,b):rows.append("\t".join(map(clean,("row",id,a,b))))
def section(rows,id,title):rows.append("\t".join(("heading",id,title,"-")))
def separator(rows,id):rows.append("\t".join(("separator",id,"-","-")))
def action(rows,id,label,token,checked=False,group=""):rows.append("\t".join(map(clean,("action",id,label,token,int(checked),group))))
def clock(t,fmt="24h"):
    if fmt=="12h":return f"{t.hour%12 or 12}:{t.minute:02d} "+("AM" if t.hour<12 else "PM")
    return t.strftime("%H:%M")
def next_prayer_state(now,yesterday,today,tomorrow,mode):
    # Countdown always targets the next Adhan. Warning is independent of that
    # countdown: only the final 15 minutes of the active prayer window are red.
    starts=[(day[k],k) for day in (today,tomorrow) for k in PRAYERS if day[k]>now]
    if not starts:raise ValueError("No future prayer in schedule")
    start,name=min(starts)
    current,end,active,urgent=prayer_state(now,yesterday,today,tomorrow,mode)
    return name,start,urgent,current if active else None,end if active else None
def countdown(seconds):
    s=max(0,__import__("math").ceil(seconds)); return f"{s//3600:02}:{s//60%60:02}:{s%60:02}"
def islamic_midnight(sunset,next_fajr):
    # Religious night: sunset to true dawn, independent of display-only night setting.
    return sunset+(next_fajr-sunset)/2
def prayer_windows(day,next_day,mode):
    end_isha=islamic_midnight(day["Maghrib"],next_day["Fajr"]) if mode=="midnight" else next_day["Fajr"]
    return [(k,day[k],end) for k,end in (
        ("Fajr",day["Sunrise"]),("Dhuhr",day["Asr"]),("Asr",day["Maghrib"]),
        ("Maghrib",day["Isha"]),("Isha",end_isha)) if day[k]<end]
def prayer_state(now,yesterday,today,tomorrow,mode):
    windows=prayer_windows(yesterday,today,mode)+prayer_windows(today,tomorrow,mode)
    for name,start,end in reversed(windows):
        if start<=now<end:
            seconds=(end-now).total_seconds()
            return name,end,True,0<seconds<=900
    starts=[(start,name) for name,start,end in windows if start>now]
    starts.append((tomorrow["Fajr"],"Fajr"))
    start,name=min(starts)
    return name,start,False,False
def pct(w):
    return f"{round(100-max(0,min(100,w['usedPercent'])))}%" if w else "--%"
def display(cfg,net,nics,temp,pdata,limits,ages):
    rows=["WMP1"]
    up,down=net if net else (None,None)
    def mb(v):return "-- MB/s" if v is None else f"{v:.2f} MB/s"
    item(rows,"network","↑ "+mb(up),"↓ "+mb(down),"dual",78);row(rows,"network","Upload",mb(up));row(rows,"network","Download",mb(down));row(rows,"network","Interfaces",", ".join(nics) or "None")
    for id,v in zip(("cpu","gpu"),temp):
        item(rows,id,id.upper()+" TEMP","--°C" if v is None else f"{v:.0f}°C",width=43)
        row(rows,id,"Temperature","Unavailable" if v is None else f"{v:.1f} °C")
        row(rows,id,"Provider","Hardware monitor / vendor API" if v is not None else "No supported sensor")
    if pdata:
        date,ts,tom,h,yesterday=pdata
        now=dt.datetime.now(ts["Fajr"].tzinfo)
        mode=cfg.get("ishaDeadline","fajr")
        name,target,urgent,current,end=next_prayer_state(now,yesterday,ts,tom,mode)
        fmt=cfg.get("timeFormat","24h")
        def show(t):return clock(t,fmt)+(" +1d" if t.date()>date else "")
        item(rows,"prayer",name,countdown((target-now).total_seconds()),width=65,placement="after-stats",urgent=urgent)
        section(rows,"prayer",f"{h['day']} {h['month']['en']} {h['year']} AH")
        row(rows,"prayer","Next: "+name,show(target))
        if current:
            row(rows,"prayer",current+" deadline",show(end))
            if urgent:row(rows,"prayer","Reminder",current+" ends within 15 min")
        separator(rows,"prayer")
        section(rows,"prayer","TODAY'S PRAYERS")
        windows={k:(a,e) for k,a,e in prayer_windows(ts,tom,mode)}
        for k in ("Fajr","Sunrise","Dhuhr","Asr","Maghrib","Isha"):
            row(rows,"prayer",NAMES[k],show(ts[k]))
        for k,(a,e) in windows.items():rows.append("\t".join(map(clean,("row","prayer",k,show(e),0,"Final prayer deadlines"))))
        separator(rows,"prayer")
        section(rows,"prayer","NIGHT")
        for k in ("Midnight","Lastthird"):
            row(rows,"prayer",NAMES[k],show(ts[k]))
        row(rows,"prayer","Isha midnight reference",show(islamic_midnight(ts["Maghrib"],tom["Fajr"])))
        row(rows,"prayer","Tomorrow Fajr",show(tom["Fajr"]))
    else:
        item(rows,"prayer","Prayer","--:--:--",width=65,placement="after-stats");row(rows,"prayer","Schedule","Unavailable / refreshing")
    separator(rows,"prayer")
    section(rows,"prayer",cfg["city"]+", "+cfg["country"])
    action(rows,"prayer","Asr: Shafi'i / Maliki / Hanbali","school0",cfg["school"]==0,"Asr school")
    action(rows,"prayer","Asr: Hanafi","school1",cfg["school"]==1,"Asr school")
    action(rows,"prayer","Night: sunset to Fajr","night1",cfg["midnightMode"]==1,"Night calculation")
    action(rows,"prayer","Night: sunset to sunrise","night0",cfg["midnightMode"]==0,"Night calculation")
    action(rows,"prayer","True dawn (Egypt Dar al-Ifta)","ishafajr",cfg.get("ishaDeadline","fajr")=="fajr","Isha deadline")
    action(rows,"prayer","Islamic midnight (sunset to Fajr)","ishamidnight",cfg.get("ishaDeadline","fajr")=="midnight","Isha deadline")
    for label,token in (("12-hour (AM/PM)","12h"),("24-hour","24h")):
        action(rows,"prayer",label,"format"+token,cfg.get("timeFormat","24h")==token,"Time format")
    for k,name in METHODS.items():action(rows,"prayer",name,"method"+str(k),cfg["method"]==k,"Calculation authority")
    action(rows,"prayer","Open location / custom angle settings","settings")
    action(rows,"prayer","Refresh now","refresh")
    item(rows,"codex","5h "+pct(limits.get(300)),"7d "+pct(limits.get(10080)),"dual",49,"after-stats")
    for n,k in (("5-hour",300),("Weekly",10080)):
        w=limits.get(k);row(rows,"codex",n+" remaining",pct(w))
        if w and w.get("resetsAt"):row(rows,"codex",n+" reset",dt.datetime.fromtimestamp(w["resetsAt"]).strftime("%a %d %b %H:%M"))
    row(rows,"codex","Updated",ages.get("codex","Not yet"))
    action(rows,"codex","Refresh account limits","refreshcodex")
    atomic(FILE,"\n".join(rows)+"\n")
def main():
    # Cross-session exclusive local lock: no competing snapshot writers.
    import msvcrt
    lock=open(ROOT/"metrics.lock","a+b")
    if lock.seek(0,2)==0: lock.write(b"1");lock.flush()
    lock.seek(0)
    try: msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1)
    except OSError: return
    cfg=config(); prev=None; last=time.monotonic(); net=None; nics=[]
    pdata=None;limits={};temp=(None,None);ages={}
    pool=concurrent.futures.ThreadPoolExecutor(max_workers=3)
    tasks={};prayer_request=None;next_at={k:0 for k in ("prayer","codex","temp")}
    one="--once" in sys.argv
    start=time.monotonic()
    while True:
        now=time.monotonic()
        try:
            v=counters();net=rate(prev,v,now-last);prev=v;last=now;nics=[x[2] for x in v.values()]
        except Exception:net=None
        if ACTION.exists():
            try:
                cmd=ACTION.read_text(encoding="utf-8").strip();ACTION.unlink()
                if cmd=="settings":os.startfile(str(CONFIG))
                elif cmd=="refreshcodex":next_at["codex"]=0
                elif cmd=="refresh":cfg=config();next_at["prayer"]=0;pdata=None
                elif cmd.startswith("method") and cmd[6:].isdigit() and int(cmd[6:]) in METHODS:
                    cfg["method"]=int(cmd[6:]);atomic(CONFIG,json.dumps(cfg,indent=2));next_at["prayer"]=0;pdata=None
                elif cmd in ("format12h","format24h"):
                    cfg["timeFormat"]=cmd[6:];atomic(CONFIG,json.dumps(cfg,indent=2))
                elif cmd in ("ishafajr","ishamidnight"):
                    cfg["ishaDeadline"]=cmd[4:];atomic(CONFIG,json.dumps(cfg,indent=2))
                elif cmd in ("school0","school1","night0","night1"):
                    cfg["school" if cmd.startswith("school") else "midnightMode"]=int(cmd[-1]);atomic(CONFIG,json.dumps(cfg,indent=2));next_at["prayer"]=0;pdata=None
            except Exception:pass
        if pdata and dt.datetime.now(pdata[1]["Fajr"].tzinfo).date()!=pdata[0]:next_at["prayer"]=0;pdata=None
        for k,func,interval in (("prayer",lambda:prayer(dict(cfg)),3600),("codex",codex,120),("temp",temperatures,5)):
            if k in tasks and tasks[k].done():
                try:
                    res=tasks[k].result()
                    if k=="prayer":
                        if prayer_request=={k:v for k,v in cfg.items() if k not in ("ishaDeadline","timeFormat","prayerBufferMinutes","prayerBuffers")}:pdata=res
                        else:next_at[k]=0
                    elif k=="codex":limits=res;ages[k]=dt.datetime.now().strftime("%H:%M:%S")
                    else:temp=res
                except Exception as e:
                    if one:print(k,type(e).__name__,str(e)[:100])
                    if k=="codex":limits={};ages[k]="Unavailable"
                    if k=="temp":temp=(None,None)
                    next_at[k]=min(next_at[k],now+60)
                del tasks[k]
            if k not in tasks and now>=next_at[k]:
                if k=="prayer":
                    prayer_request={k:v for k,v in cfg.items() if k not in ("ishaDeadline","timeFormat","prayerBufferMinutes","prayerBuffers")}
                    tasks[k]=pool.submit(prayer,dict(cfg))
                else:tasks[k]=pool.submit(func)
                next_at[k]=now+interval
        try: display(cfg,net,nics,temp,pdata,limits,ages)
        except PermissionError: pass
        if one and now-start>=20:
            print("Network",net,nics,"Temperatures",temp,"Codex windows",sorted(limits),"Prayer schedule",bool(pdata))
            break
        time.sleep(1)
if __name__=="__main__": main()
