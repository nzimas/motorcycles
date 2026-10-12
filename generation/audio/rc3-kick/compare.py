"""Offline current stock kick versus representative prior low-control fixture.
Creates audio/report files only. Does not edit or reconstruct production code.
"""
from pathlib import Path
import json,ctypes,hashlib,sys
BASE=Path(__file__).resolve().parent.parent
scope={'__file__':str(BASE/'render_examples.py')}
exec((BASE/'render_examples.py').read_text().split('rows=[];baseline={}')[0],scope)
E=scope['E'];np=scope['np'];Request=scope['Request'];Sound=scope['Sound']
measure=scope['measure'];write_wav=scope['write_wav'];lib=scope['lib']
HERE=Path(__file__).resolve().parent
current_only='--current-only' in sys.argv
rows=[r for r in json.loads((HERE/'report.json').read_text())['examples'] if r['revision']=='representative-prior-low-fixture'] if current_only else []
for generator,path,name in [(1,1,'rhythmic-kit'),(0,3,'free-patch')]:
 q=Request();q.generator=generator;q.path=path;q.target=0;q.density=45;q.brutality=55;q.machine_mask=63 if generator else 1;q.seed=7
 out=(ctypes.c_uint8*1612)();assert lib.mc_generate(ctypes.byref(q),out)
 sound=Sound.from_buffer_copy(bytes(out[:10]))
 current={n:int(getattr(sound,n)) for n,_ in Sound._fields_};assert current['machine']==0
 prior=dict(current);prior.update(pitch=40,decay=18,punch=1,finetune=64)
 for revision,params,notes in [('representative-prior-low-fixture',prior,(52,60) if generator and not current_only else ()),('rc3-generated',current,(60,))]:
  for note in notes:
   engine=E.Engine(scope['stock']);engine.solo(0);engine.set(0,**params,note=note)
   x=engine.render(452,trig_at=(1,))[64:];assert not engine.unmapped
   metrics=measure(x);body=x[960:7200].astype(float)
   metrics['body20to150ms_rms_q31']=float(np.sqrt(np.mean(body*body))/2**31)
   metrics['body20to150ms_peak_q31']=float(np.max(np.abs(body))/2**31)
   filename=f'{name}-{revision}-note{note}.wav';write_wav(HERE/filename,x)
   row={'fixture':name,'revision':revision,'note':note,'params':params,'file':filename,'metrics':metrics}
   rows.append(row);print(json.dumps(row),flush=True)
report={'source':'Actual stock OS1.13 DSP with EMAC correction; prior comparison is representative low pitch40/decay18 fixture, not exact RC2 output',
 'generation_sha256':hashlib.sha256((BASE.parent/'generation.c').read_bytes()).hexdigest(),
 'seed':7,'density':45,'brutality':55,'body_window_ms':[20,150],'examples':rows,
 'instrument_access':False,'subjective_listening_performed':False,'MACRO_DSP_evaluated':False,
 'velocity_applied':'default isolated voice; sequence velocity96 bound separately tested'}
(HERE/'report.json').write_text(json.dumps(report,indent=2)+'\n')
