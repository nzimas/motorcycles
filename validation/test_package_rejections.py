#!/usr/bin/env python3
"""Independent validator negative cases; generated packages remain temporary/local."""
import argparse,pathlib,subprocess,sys,tempfile
from validate_package import read,UP
sys.path.insert(0,str(UP/'tools'))
from mtlib import aplib,container
from mtlib.syx import unwrap,wrap,BYTES_PER_MSG

def main():
 p=argparse.ArgumentParser();p.add_argument('stock');a=p.parse_args();stock=pathlib.Path(a.stock);raw=stock.read_bytes();stream,transport=unwrap(raw);c=container.parse(stream)
 checks=[]
 with tempfile.TemporaryDirectory(prefix='mc-validation-') as temp:
  t=pathlib.Path(temp)/'negative.syx'
  for name,mut in [('packet corruption',lambda b:b.__setitem__(50,b[50]^1)),('start marker corruption',lambda b:b.__setitem__(7,3)),('truncated transport',lambda b:b.__delitem__(slice(-128,None)))]:
   bad=bytearray(raw);mut(bad);t.write_bytes(bad)
   try:read(t)
   except (AssertionError,ValueError,IndexError):checks.append(name)
   else:raise AssertionError(name+' accepted')
  bad=bytearray(stream);bad[4]^=1;t.write_bytes(wrap(bytes(bad),transport['product'],transport['start_seq']))
  try:read(t)
  except AssertionError:checks.append('container checksum corruption with valid packets')
  else:raise AssertionError('container checksum accepted')
  meta,stored,plain=read(stock);key=container.find_key(list(plain.values()),c['blob'][:-32],c['blob'][-32:]);replacement=bytearray(stored[5]);replacement[0]^=1
  blob=container.rebuild(c,{5:bytes(replacement)},key);t.write_bytes(wrap(container.build_stream(blob,BYTES_PER_MSG),transport['product'],transport['start_seq']))
  read(t) # Integrity is valid; protected-region comparison must independently reject it.
  r=subprocess.run([sys.executable,str(pathlib.Path(__file__).with_name('validate_package.py')),str(stock),'--candidate',str(t)],capture_output=True,text=True)
  assert r.returncode!=0 and 'protected section changed' in r.stderr,r.stderr
  checks.append('correctly signed protected-section modification')
 print('PASS '+str(len(checks))+' package rejection cases: '+', '.join(checks))
if __name__=='__main__':main()
