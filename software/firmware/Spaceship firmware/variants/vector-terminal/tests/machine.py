from pathlib import Path
import ctypes as C, re, json, subprocess, os, shutil
CLANG=os.environ.get('CLANG') or shutil.which('clang') or r'C:\Program Files\LLVM\bin\clang.exe'
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'variants/vector-terminal/tests/out'
OUT.mkdir(exist_ok=True)
def compile_bridge():
 subprocess.run([CLANG,'-O2','-shared','-I'+str(ROOT/'emulator'),str(Path(__file__).with_name('cpu_bridge.c')),str(ROOT/'emulator/i8080.c'),'-o',str(OUT/'cpu.dll')],check=True)
class Machine:
 observed_stack={}
 def __init__(self,variant='vector-terminal',target='emulator',folder=None):
  self.d=C.CDLL(str(OUT/'cpu.dll'))
  self.d.stat.restype=C.c_uint
  self.d.memory.restype=C.POINTER(C.c_ubyte)
  self.d.screen.restype=C.POINTER(C.c_uint16)
  self.d.reset();self.m=self.d.memory()
  self.folder=Path(folder) if folder is not None else ROOT/'build'/variant/target
  self.report=json.loads((self.folder/'build.json').read_text())
  self.sym={n:int(v,16) for n,v in re.findall(r'^(\S+)\s*=\s*\$([0-9A-Fa-f]+)',(self.folder/'clock80.map').read_text(),re.M)}
  self.put(0,(self.folder/('emulator.bin' if target=='emulator' else 'rom.bin')).read_bytes())
  self.d.prepare(0,0,0)
  assert self.d.run(self.sym['_main'],20_000_000),'CRT'
  data_path=self.folder/'clock80_DATA.bin'
  if not data_path.exists():data_path=ROOT/self.report['work_directory']/'clock80_DATA.bin'
  data=data_path.read_bytes()
  assert self.get(self.sym['__DATA_head'],len(data))==data
 def put(self,a,b):
  for i,v in enumerate(b):self.m[a+i]=v
 def get(self,a,n):return bytes(self.m[a:a+n])
 def addr(self,n):return self.sym['_'+n]
 def set(self,n,v,size=2):self.put(self.addr(n),(v%(1<<(8*size))).to_bytes(size,'little'))
 def val(self,n,size=2,signed=False):return int.from_bytes(self.get(self.addr(n),size),'little',signed=signed)
 def call(self,n,args=(),fastcall=False,limit=50_000_000):
  sp=0xfe00
  stack=list(args)
  hl=stack.pop() if fastcall and stack else 0
  for v in stack:
   sp-=2;self.put(sp,(v&65535).to_bytes(2,'little'))
  sp-=2;self.put(sp,(0xff00).to_bytes(2,'little'))
  self.d.prepare(self.addr(n),sp,hl);self.d.stats_reset();begin=self.d.stat(0)
  assert self.d.run(0xff00,limit),f'{n} failed PC={self.d.stat(7):04x}'
  assert self.d.stat(6)==sp+2,f'{n} SP mismatch'
  assert not self.d.stat(4),f'{n} ROM write'
  depth=0xfe00-self.d.stat(5)
  Machine.observed_stack[n]=max(depth,Machine.observed_stack.get(n,0))
  return {'cycles':self.d.stat(0)-begin,'pixels':self.d.stat(1),'lcd_writes':self.d.stat(2),'bad_pixels':self.d.stat(3),'stack_bytes':depth,'hl':self.d.stat(9),'dehl':self.d.stat(8)}
 def lcd_init(self):
  self.set('lcddev',480);self.put(self.addr('lcddev')+2,(320).to_bytes(2,'little'))
  for off,v in [(7,0x2c),(11,0x2a),(13,0x2b)]:self.put(self.addr('lcddev')+off,v.to_bytes(2,'little'))
 def pixels(self):return bytes(C.string_at(self.d.screen(),480*320*2))
 def inputs(self,p,b):self.d.script(p,(C.c_ubyte*len(b))(*b),len(b))
 def rtc(self,seconds,date=(0x12,1,0x10,0x26)):
  for i,v in enumerate([seconds%60,seconds//60%60,seconds//3600%24]):self.d.port(0x30+i,v//10*16+v%10)
  for i,v in enumerate(date):self.d.port(0x33+i,v)
 def save_screen(self,path):
  b=self.pixels();rgb=bytearray()
  for i in range(0,len(b),2):
   v=int.from_bytes(b[i:i+2],'little');rgb.extend(((v>>11)*255//31,((v>>5)&63)*255//63,(v&31)*255//31))
  Path(path).write_bytes(b'P6\n480 320\n255\n'+rgb)
