import ctypes as c,ctypes.wintypes as t,pathlib,os,json,time
u=c.windll.user32
u.FindWindowW.restype=t.HWND
u.FindWindowExW.argtypes=[t.HWND,t.HWND,t.LPCWSTR,t.LPCWSTR];u.FindWindowExW.restype=t.HWND
class CD(c.Structure):
 _fields_=[("dwData",c.c_size_t),("cbData",t.DWORD),("lpData",c.c_void_p)]
u.SendMessageTimeoutW.argtypes=[t.HWND,t.UINT,t.WPARAM,t.LPARAM,t.UINT,t.UINT,c.POINTER(c.c_size_t)]
u.SendMessageTimeoutW.restype=t.LPARAM
p=pathlib.Path(os.environ["APPDATA"])/"Shell/ajustes.txt"
root=pathlib.Path(__file__).parent
r={}
try:
 old=p.read_bytes()
 backup=root/("spotlight-settings-backup-"+str(time.time_ns())+".txt");backup.write_bytes(old)
 h=u.FindWindowExW(t.HWND(-3),None,"ShellMensagens",None) or u.FindWindowW("ShellMensagens",None) or u.FindWindowW(None,"ShellMensagens")
 if not h:raise RuntimeError("Shell window missing")
 val=c.create_unicode_buffer("atalho_spotlight=win");cd=CD(0x534A5354,c.sizeof(val),c.cast(val,c.c_void_p));res=c.c_size_t()
 r["ack"]=bool(u.SendMessageTimeoutW(h,0x4a,0,c.addressof(cd),2,2000,c.byref(res))) and bool(res.value)
 time.sleep(.5)
 r["setting"]=[x for x in p.read_text(encoding="utf-8-sig",errors="replace").splitlines() if x.startswith("atalho_spotlight=")]
 r["backup"]=str(backup)
except Exception as e:r["error"]=repr(e)
(root/"spotlight-restore-proof.json").write_text(json.dumps(r))
