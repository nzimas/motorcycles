#!/usr/bin/env python3
"""Inventory declared upstream byte conflicts against native reference hooks.
No claim of semantic compatibility or installed option identification.
"""
import pathlib,json,hashlib
ROOT=pathlib.Path(__file__).resolve().parents[1];BASE=0x40000400
manifest=json.loads((ROOT/'build/native-stock-reference/native-stock-reference.manifest.json').read_text())
spans=[(p['address'],p['address']+len(bytes.fromhex(p['new_hex'])),p['kind']) for p in manifest['patches']]
rows=[]
for path in sorted((ROOT/'reference/Modded-Cycles/tweaks/model-cycles_OS1.13').glob('*.json')):
 t=json.loads(path.read_text())
 if 'writes' not in t:continue
 hits=[]
 for w in t['writes']:
  lo=BASE+w['off'];hi=lo+len(bytes.fromhex(w['new']))
  for a,b,kind in spans:
   if lo<b and a<hi:hits.append({'address':hex(max(a,lo)),'native_kind':kind,'upstream_span':[hex(lo),hex(hi)]})
 append=t.get('append');notes=[]
 if append:notes.append('Appended payload placement and cache reservation require composition; not covered by hook-only overlap checks.')
 if t['id'].startswith(('model-tg','syntakt')):notes.append('Machine capabilities and altered runtime paths require independent binding/default/observer review.')
 rows.append({'id':t['id'],'file':str(path.relative_to(ROOT)),'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'hook_conflicts':hits,'append':append,'notes':notes,'status':'requires_composition' if hits or append else 'no_direct_declared_hook_overlap_not_compatibility_proof'})
report={'result':'INVENTORY','native_main_sha256':manifest['reference_main_sha256'],'installed_options':None,'limitations':'Pinned upstream declarations only. Does not infer device configuration, prove runtime compatibility, or validate option combinations.','tweak_count':len(rows),'direct_hook_conflict_count':sum(bool(r['hook_conflicts']) for r in rows),'rows':rows}
(ROOT/'validation/native-mod-footprints.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='rows'},indent=2))
print('Direct hook conflicts: '+', '.join(r['id'] for r in rows if r['hook_conflicts']))
