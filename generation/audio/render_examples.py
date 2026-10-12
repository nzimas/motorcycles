"""Local stock DSP renders of generated semantic sounds; no device connection.
Uses upstream mcengine + its documented EMAC correction, synthetic peripherals.
"""
from pathlib import Path
import ctypes,hashlib,json,os,sys,time,wave
import numpy as np
ROOT=Path(__file__).resolve().parent
PROJECT=ROOT.parent.parent
TOOLS=PROJECT/'reference/Modded-Cycles/tools'
sys.path[:0]=[str(TOOLS),str(TOOLS/'emu')]
os.environ['PATH']=str(ROOT/'bin')+os.pathsep+os.environ['PATH']
import mcengine as E
from test_sdvintage import main_os_from_syx
FIRMWARE=Path('/Users/nzimas/Downloads/model-cycles_OS1.13/model-cycles_OS1.13.syx')
assert hashlib.sha256(FIRMWARE.read_bytes()).hexdigest()=='44fe586269631a0ca7da25a3383fc6733c314809505fc3cc52f1e0ed9800640c'
stock=main_os_from_syx(FIRMWARE)
class Sound(ctypes.Structure):
 _fields_=[(n,ctypes.c_uint8) for n in ('machine','pitch','color','shape','sweep','contour','punch','gate','finetune','decay')]
class Request(ctypes.Structure):
 _fields_=[(n,ctypes.c_uint8) for n in ('generator','path','target','density','brutality','machine_mask')]+[('source_sound_valid_mask',ctypes.c_uint8),('music',ctypes.c_uint8),('seed',ctypes.c_uint32),('machine_valid_mask',ctypes.c_uint8),('current_machine',ctypes.c_uint8*6),('phrase_length',ctypes.c_uint8),('current_sound',Sound*6),('default_note',ctypes.c_uint8*6),('note_valid_mask',ctypes.c_uint8),('pad',ctypes.c_uint8)]
lib=ctypes.CDLL(str(ROOT.parent/'generation-host.dylib'))
lib.mc_generate.argtypes=[ctypes.POINTER(Request),ctypes.c_void_p]
def render(params):
 e=E.Engine(stock);e.solo(0);e.set(0,**params,note=60)
 x=e.render(452,trig_at=(1,))[64:]
 assert not e.unmapped
 return x

def measure(x):
 y=x.astype(float);peak=float(np.max(np.abs(y)));rms=float(np.sqrt(np.mean(y*y)))
 seg=y[:9600]-np.mean(y[:9600]);spec=np.abs(np.fft.rfft(seg*np.hanning(len(seg))))
 freqs=np.fft.rfftfreq(len(seg),1/48000)
 power=spec*spec+1e-24
 centroid=float(np.sum(freqs*spec)/(np.sum(spec)+1e-12));flatness=float(np.exp(np.mean(np.log(power[1:])))/np.mean(power[1:]))
 dominant=float(freqs[1+np.argmax(spec[1:])]);dc=float(np.mean(y))
 return {'peak_raw':peak,'rms_raw':rms,'peak_q31':peak/2**31,'rms_q31':rms/2**31,'dc_q31':dc/2**31,'centroid_hz':round(centroid,2),'spectral_flatness':round(flatness,6),'dominant_bin_hz':round(dominant,2),'raw_i32_saturation_samples':int(np.count_nonzero(np.abs(y)>=2**31-1)),'silent':not bool(peak),'raw_sha256':hashlib.sha256(x.astype('>i4').tobytes()).hexdigest()}

def write_wav(path,x):
 # All files share one Q31→PCM conversion; no individual peak normalization.
 y=np.clip(x.astype(float)/2**31*32767,-32768,32767).astype('<i2')
 with wave.open(str(path),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(48000);w.writeframes(y.tobytes())
rows=[];baseline={};start=time.monotonic()
for path,name in enumerate(('abstract','rhythmic','melodic','free')):
 for seed in (7,19,41):
  q=Request();q.generator=0;q.path=path;q.target=0;q.density=30;q.brutality=35;q.machine_mask=63;q.seed=seed
  buf=(ctypes.c_uint8*1612)();assert lib.mc_generate(ctypes.byref(q),buf)==1
  sound=Sound.from_buffer_copy(bytes(buf[:10]));params={n:int(getattr(sound,n)) for n,_ in Sound._fields_}
  x=render(params);metrics=measure(x)
  if sound.machine not in baseline:
   defaults=dict(zip(('color','shape','sweep','contour','decay'),E.DEFAULTS[sound.machine]));defaults.update(machine=sound.machine,pitch=64,punch=0,gate=0,finetune=64)
   baseline[sound.machine]=measure(render(defaults))
  metrics['rms_vs_machine_default']=metrics['rms_raw']/max(1,baseline[sound.machine]['rms_raw'])
  filename=f'{name}-seed{seed}.wav';write_wav(ROOT/filename,x)
  rows.append({'path':name,'seed':seed,'params':params,'file':filename,'wav_sha256':hashlib.sha256((ROOT/filename).read_bytes()).hexdigest(),'metrics':metrics})
  print(json.dumps({'file':filename,'machine':params['machine'],'peak_q31':metrics['peak_q31'],'rms_vs_default':metrics['rms_vs_machine_default']}),flush=True)
report={'generation_source_sha256':hashlib.sha256((ROOT.parent/'generation.c').read_bytes()).hexdigest(),'test_controls':{'density':30,'brutality':35,'machine_mask':63,'meaning':'fixed fixture, not interface defaults'},'firmware_sha256':hashlib.sha256(FIRMWARE.read_bytes()).hexdigest(),'source':'actual stock OS1.13 voice loop via upstream mcengine with EMAC correction','sample_rate':48000,'frames_per_example':len(x),'note':60,'velocity':'voice loop default; no final mix/FX stage','wav_level_policy':'fixed Q31→16bit, no per-file normalization','examples':rows,'machine_default_baselines':baseline,'elapsed_seconds':round(time.monotonic()-start,2),'instrument_access':False,'installed_configuration_validated':False,'hardware_timing_validated':False,'subjective_listening_performed':False}
(ROOT/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print('DONE',flush=True)
