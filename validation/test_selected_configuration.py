#!/usr/bin/env python3
"""Independent selected-mod baseline and exact-composition preservation proof.
A candidate must supply a manifest enumerating every deliberate baseline overwrite.
This proves byte preservation, not installed revision identity or hardware timing.
"""
import argparse,json,pathlib,struct
from validate_package import read,sha
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser();p.add_argument('--candidate');p.add_argument('--manifest');p.add_argument('--out',default=str(ROOT/'validation/selected-configuration-report.json'));a=p.parse_args()
 config=json.loads((ROOT/'firmware/selected-configuration.json').read_text());base=pathlib.Path(config['baseline_package_path']);bm,bs,bp=read(base)
 assert bm['syx_sha256']==config['baseline_package_sha256'];assert sha(bp[3])==config['baseline_main_sha256']
 official=next((ROOT/'validation').glob('baseline.json'));om=json.loads(official.read_text())['stock']
 stock_path=pathlib.Path(__import__('os').environ.get('MC_STOCK_SYX','/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx'))
 _,os,op=read(stock_path)
 assert all(bs[k]==os[k] for k in os if k!=3)
 checks=['selected configuration baseline exact hash','selected MAIN exact hash','protected stored sections match official','transport/content/HMAC integrity']
 report={'result':'PASS','baseline':bm,'configuration':config,'checks':checks,'hardware_writes':False}
 if a.candidate:
  assert a.manifest,'explicit overwrite manifest required';cm,cs,cp=read(a.candidate);man=json.loads(pathlib.Path(a.manifest).read_text());image=bytearray(bp[3]);addr=0x40000400
  assert cm['vendor_version']==bm['vendor_version'];assert all(cs[k]==bs[k] for k in bs if k!=3)
  occupied=set()
  for q in man['patches']:
   off=q['address']-addr;old=bytes.fromhex(q['old_hex']);new=bytes.fromhex(q['new_hex']);assert off>=0 and len(old)==len(new) and off+len(old)<=len(image)
   assert image[off:off+len(old)]==old,'baseline mismatch at '+hex(q['address'])
   assert not any(i in occupied for i in range(off,off+len(new))),'overlapping declared overwrites'
   occupied.update(range(off,off+len(new)));image[off:off+len(new)]=new
  assert bytes(image)==cp[3][:len(image)],'undeclared selected-mod baseline alteration'
  assert len(cp[3])>=len(image),'candidate truncated selected baseline'
  report.update(candidate=cm,append_bytes=len(cp[3])-len(image),preserved_baseline_bytes=len(image)-len(occupied),declared_overwrite_bytes=len(occupied))
  checks.extend(['only enumerated baseline overwrites','all other selected-mod bytes identical','protected sections identical','vendor version preserved'])
 report['limitations']='Selected upstream configuration identity is explicit; installed binary revision is unverified. Byte preservation cannot prove ISR deadlines or peripheral boot behavior.'
 pathlib.Path(a.out).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'result':'PASS','checks':checks},indent=2))
if __name__=='__main__':main()
