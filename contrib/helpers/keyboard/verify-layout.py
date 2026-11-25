import ctypes as c,ctypes.wintypes as t,json,pathlib
u=c.windll.user32
u.LoadKeyboardLayoutW.argtypes=[t.LPCWSTR,t.UINT];u.LoadKeyboardLayoutW.restype=t.HKL if hasattr(t,'HKL') else c.c_void_p
u.ToUnicodeEx.argtypes=[t.UINT,t.UINT,c.POINTER(c.c_ubyte),t.LPWSTR,c.c_int,t.UINT,c.c_void_p]
u.GetKeyboardLayout.restype=c.c_void_p
orig=u.GetKeyboardLayout(0)
h=u.LoadKeyboardLayoutW("A0000C01",0)
r={"loaded":hex(h or 0),"original":hex(orig or 0),"keys":{}}
if h:
 for key,sc,expected in [('C',0x2e,0x630),('V',0x2f,0x62f),('Z',0x2c,0x638),('X',0x2d,0x637),('Q',0x10,0x636)]:
  buf=c.create_unicode_buffer(8);state=(c.c_ubyte*256)()
  n=u.ToUnicodeEx(ord(key),sc,state,buf,8,0,h)
  r["keys"][key]={"value":hex(ord(buf[0])) if n>0 else None,"pass":n>0 and ord(buf[0])==expected}
 r["active_unchanged"]=u.GetKeyboardLayout(0)==orig
(pathlib.Path(__file__).parent/"keyboard-proof.json").write_text(json.dumps(r,indent=2))
