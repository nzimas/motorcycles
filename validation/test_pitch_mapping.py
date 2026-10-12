#!/usr/bin/env python3
"""Actual stock Tone DSP pitch/note/fine relative-frequency verification."""
import pathlib,sys,os,json,hashlib
import numpy as np
R=pathlib.Path(__file__).resolve().parents[1];T=R/'reference/Modded-Cycles/tools';sys.path[:0]=[str(T),str(T/'emu')];os.environ['PATH']=str(R/'generation/audio/bin')+os.pathsep+os.environ['PATH']
import mcengine as E
from test_sdvintage import main_os_from_syx
fw=pathlib.Path('/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx');image=main_os_from_syx(fw)
rows=[]
for pitch,note,fine in [(64,60,64),(64,52,64),(76,60,64),(64,60,0),(64,60,127)]:
 e=E.Engine(image);e.solo(0);e.set(0,machine=4,pitch=pitch,note=note,finetune=fine,color=0,shape=0,sweep=0,contour=0,decay=100,punch=0,gate=0)
 x=e.render(452,trig_at=(1,))[96:].astype(float);assert not e.unmapped
 # Estimate fundamental from autocorrelation around expected C4 range.
 crossings=np.where((x[:-1]<=0)&(x[1:]>0))[0];pos=crossings-x[crossings]/(x[crossings+1]-x[crossings]);freq=48000/np.median(np.diff(pos))
 rows.append({'pitch':pitch,'note':note,'fine':fine,'frequency_hz':float(freq)})
base=rows[0]['frequency_hz']
for row in rows:
 expected=2**((row['pitch']-64+row['note']-60+(row['fine']-64)/32)/12);ratio=row['frequency_hz']/base;row.update(ratio=ratio,expected_ratio=expected);assert abs(ratio/expected-1)<0.015,row
report={'result':'PASS','firmware_sha256':hashlib.sha256(fw.read_bytes()).hexdigest(),'rows':rows,'stock_pad_note_evidence':'40061426 initializes track+712=60; normalpad4001d1fc→40015a34 getter40015a58 reads +712, then4001d380 sendsnote to4001d05e','fine_formula':'(control−64)/32 semitones:0→−2,64→0,127→+1.96875','macro_source_evidence':'macro.cc121 exact Q16formula:(pitch8.8<<8)+((fine8.8−0x4000)<<3)−(64<<16)+pmod','limitations':'Actual stock DSP loop with upstream EMAC correction and synthetic note-modulation input; zero-crossing relative-frequency tolerance1.5%. Not final hardware mix, modified effects or live pad playback.'}
(R/'validation/pitch-mapping-report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
