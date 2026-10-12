#!/usr/bin/env python3
"""Real stock notifier dispatch, synthetic registered callbacks/parent observer.
Proves observer ABI and suppression, not full-device/audio publication."""
exec(open(__file__.replace('test_observable.py','test_stock_primitives.py')).read().split('# Actual PadEvent')[0])
MANAGER=0x90020100;CALLBACK=0x90020104;PARENT=0x400d0f6c
seen=[]
def observer_hook(uc,a,size,data):
 sp=uc.reg_read(R.UC_M68K_REG_A7)
 if a==MANAGER:
  args=struct.unpack('>4I',uc.mem_read(sp,16));assert args[3] in(2,3)
 elif a==CALLBACK:
  args=struct.unpack('>4I',uc.mem_read(sp,16));assert args[2]==DATA and args[3]==0
  seen.append(bytes(uc.mem_read(DATA,100)))
 elif a==PARENT:pass
 else:return
 ret=struct.unpack('>I',uc.mem_read(sp,4))[0];uc.reg_write(R.UC_M68K_REG_A7,sp+4);uc.reg_write(R.UC_M68K_REG_PC,ret)
u.hook_add(UC_HOOK_CODE,observer_hook)
# Real OS vtable primary sound observable, owner no parent callback, registered
# std::function invokes spy manager/observer through genuine notifier traversal.
w(OBJ,0x400fef54);w(OBJ+16,DATA,VECTOR,VECTOR+16,VECTOR+16,0)
u.mem_write(OBJ+12,b'\0\0');u.mem_write(OBJ+36,b'\0')
w(VECTOR,0,0,MANAGER,CALLBACK)
u.mem_write(DATA,b'X'*100)
call(0x400d5cc8,OBJ,0);assert seen==[b'X'*100]
call(0x400d563c,OBJ,1,1);call(0x400d5cc8,OBJ,0);assert len(seen)==1
call(0x400d563c,OBJ,0,0);call(0x400d5cc8,OBJ,0);assert len(seen)==2
print('PASS real sound notifier std::function clone/invoke/destroy dispatch and suppression restore')
print('LIMIT first traversal uses synthetic callback/parent; actual audio callback proof follows')
# Real track traversal uses same observer ABI; invoke with unchanged canonical
# pointer and test independent suppression method.
w(OBJ,0x400ff894);u.mem_write(OBJ+12,b'\0\0');u.mem_write(OBJ+36,b'\0');seen.clear()
call(0x4001908a,OBJ,0);assert seen==[b'X'*100]
call(0x400d63ce,OBJ,1,1);call(0x4001908a,OBJ,0);assert len(seen)==1
call(0x400d63ce,OBJ,0,0);call(0x4001908a,OBJ,0);assert len(seen)==2
print('PASS real track notifier observer ABI and suppression restore')
# Registration reproduced from app initialization40011186..111c6. Execute the
# actual manager4000e572 and actual audio observer4000e2ec, including cache clear
#40059614. Only heap allocation/free and unrelated parent observable are stubbed.
u.mem_map(0x80000000,0x10000)
FUNCTOR=0x90040000;CLONE=0x90041000
heapcalls=[]
def heap_hook(uc,a,size,data):
 if a not in(0x400802e0,0x400802ec):return
 sp=uc.reg_read(R.UC_M68K_REG_A7);args=struct.unpack('>2I',uc.mem_read(sp,8));heapcalls.append((a,args[1]))
 if a==0x400802e0:assert args[1]==8;uc.reg_write(R.UC_M68K_REG_D0,CLONE)
 uc.reg_write(R.UC_M68K_REG_A7,sp+4);uc.reg_write(R.UC_M68K_REG_PC,args[0])
u.hook_add(UC_HOOK_CODE,heap_hook)
w(OBJ,0x400fef54);w(OBJ+16,DATA,VECTOR,VECTOR+16,VECTOR+16,0)
u.mem_write(OBJ+12,b'\0\0');u.mem_write(OBJ+36,b'\0')
for track in range(6):
 w(VECTOR,FUNCTOR,0,0x4000e572,0x4000e2ec);w(FUNCTOR,0x90050000,track)
 for t in range(6):w(0x800015a0+(147+t)*4,0x90060000+t*100)
 heapcalls.clear();call(0x400d5cc8,OBJ,0)
 assert heapcalls==[(0x400802e0,8),(0x400802ec,CLONE)]
 for t in range(6):
  actual=struct.unpack('>I',u.mem_read(0x800015a0+(147+t)*4,4))[0]
  assert actual==(0 if t==track else 0x90060000+t*100)
print('PASS real registered sound manager/audio observer six tracks clear only targeted audio cache pointer')
print('LIMIT fullboot registration is statically traced; heap/parentstubbed, ISR rendering not executed here')
