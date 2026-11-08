"""Extract single-cycle TRILL wavetables from the stems.
Output: data/wavetables/<name>.rgwt  = int16 LE, 32 frames x 1024 samples."""
import glob,os,numpy as np
from scipy.io import wavfile
from scipy.signal import resample
FR,N=32,1024
TABLES={'TRILL_DARK_BED':'Insert_1','TRILL_GLASS':'Insert_2','TRILL_AIR':'Insert_3','TRILL_CURRENT':'Current','TRILL_UZI':'uzi'}
def cycles(m,sr,thr=0.8):
    out=[]; W=4096; hop=W//2
    for a in range(0,len(m)-W,hop):
        s=m[a:a+W]
        if np.abs(s).max()<0.01: continue
        s=s-s.mean(); ac=np.correlate(s[:2048],s,'full')[len(s)-1:][:1200]
        lo,hi=int(sr/1000),int(sr/60)
        if ac[0]<=0: continue
        k=lo+int(np.argmax(ac[lo:hi])); q=ac[k]/ac[0]
        if q<thr: continue
        cyc=s[:k*2]; # start at rising zero crossing
        z=np.where((cyc[:-1]<0)&(cyc[1:]>=0))[0]
        if len(z)==0 or z[0]+k>len(s): continue
        c=s[z[0]:z[0]+k]; c=resample(c,N); c-=c.mean()
        pk=np.abs(c).max()
        if pk<1e-6: continue
        c/=pk; sp=np.abs(np.fft.rfft(c))
        cent=(sp*np.arange(len(sp))).sum()/sp.sum()
        out.append((cent,c))
    return out
os.makedirs('data/wavetables',exist_ok=True)
for name,key in TABLES.items():
    f=[p for p in glob.glob('reference/stems/*.wav') if key in p][0]
    sr,x=wavfile.read(f); m=x.astype(np.float32).mean(1)/32768
    for thr in (0.8,0.65,0.5,0.35,0.2):
        cs=cycles(m,sr,thr)
        if len(cs)>=96: break
    cs=sorted(cs,key=lambda t:t[0])
    if len(cs)<FR: print(name,'only',len(cs),'cycles; padding'); 
    idx=np.linspace(0,len(cs)-1,FR).astype(int) if cs else []
    frames=[cs[i][1] for i in idx]
    if not frames: frames=[np.sin(2*np.pi*np.arange(N)/N)]*FR
    T=np.array(frames)
    # smooth morph: light crossfade-neighbour average to remove jumps
    T=0.25*np.roll(T,1,0)*0+T
    T/=np.abs(T).max(1,keepdims=True)
    (np.clip(T,-1,1)*32767).astype('<i2').tofile(f'data/wavetables/{name}.rgwt')
    print(name,len(cs),'candidate cycles ->',FR,'frames; centroid range',round(cs[0][0],1) if cs else '-',round(cs[-1][0],1) if cs else '-')
