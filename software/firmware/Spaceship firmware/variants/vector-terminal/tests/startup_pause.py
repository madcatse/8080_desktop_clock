"""Responding BME on the published hardware ROM during a cooperative LCD clear.
Digital bus byte fixtures, not an electrical or physical board measurement.
"""
from machine import Machine, OUT, compile_bridge
import hashlib,json
FREQ=3555556
def rx(data):return [14,14,14]+[14|((v>>i)&1) for v in data for i in range(7,-1,-1)]
# This test is runnable on its own, including a clean source-kit checkout.
compile_bridge()
m=Machine(target='hardware');m.lcd_init();m.set('rtc_need_start',0,1);m.set('app_running',1,1);m.call('setH',[255])
base=bytes.fromhex('02 34 12 13 02 10 26')
cal=[27504,26435,-1000,36477,-10685,3024,2855,140,-7,15500,-14600,6000]
c=b''.join((v&65535).to_bytes(2,'little') for v in cal)+b'\0\x4b';h=b'\x6a\x01\0\x14\x2e\x03\x1e'
raw=(415148<<4).to_bytes(3,'big')+(519888<<4).to_bytes(3,'big')+(30000).to_bytes(2,'big')
begin=rx([0x60])+[14]*3+rx([0])+rx(c)+rx(h)+[14]*9
steady=[14]*7+rx(base)+[14]
m.inputs(2,steady+begin+steady+rx(raw)+steady*14)
m.put(0xfdfc,b'\0\xff\0\0');m.d.prepare(m.addr('LCD_Clear'),0xfdfc,0);m.d.stats_reset();start=m.d.stat(0)
rows=[]
for name in ['BME init','BME measurement','DS start','forecast']:
 assert m.d.run(m.addr('app_service'),20_000_000)
 t=m.d.stat(0);pixels=m.d.stat(1);ret=int.from_bytes(m.get(m.d.stat(6),2),'little')
 assert m.d.run(ret,20_000_000)
 rows.append({'stage':name,'cycles':m.d.stat(0)-t,'ms':(m.d.stat(0)-t)*1000/FREQ,'pixels_before':pixels})
 assert m.d.stat(1)==pixels
assert m.val('bme_ok',1)==m.val('weather_ok',1)==1 and m.val('temperature',4,signed=True)==2508
assert m.d.run(0xff00,20_000_000)
first=m.d.stat(0)-start;depth=0xfe00-m.d.stat(5);assert m.d.stat(1)==153600 and m.d.stat(3)==m.d.stat(4)==0
m.inputs(2,steady*12);repeat=m.call('LCD_Clear',[0])
r={'sha256':hashlib.sha256((m.folder/'rom.bin').read_bytes()).hexdigest(),'method':'Published hardware ROM; responding RTC/BME byte fixtures, DS absent. LCD writes resume between four service stages.','stages':rows,'maximum_service_ms':max(x['ms'] for x in rows),'first_clear_cycles':first,'repeat_clear_cycles':repeat['cycles'],'stack_bytes':depth,'result':'PASS'}
(OUT/'startup-pause.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
