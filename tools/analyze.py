import sys,glob,numpy as np
from scipy.io import wavfile
from scipy.signal import welch
NOTES=['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B']
for f in sorted(glob.glob('reference/stems/*.wav')):
    sr,x=wavfile.read(f); x=x.astype(np.float32)/32768; m=x.mean(1)
    seg=m[:sr*60]
    rms=20*np.log10(np.sqrt((seg**2).mean())+1e-9)
    N=min(1<<18,len(seg)); a=len(seg)//3 if len(seg)>N*2 else 0; X=np.abs(np.fft.rfft(seg[a:a+N]*np.hanning(N))); fr=np.fft.rfftfreq(N,1/sr)
    cent=(X*fr).sum()/X.sum()
    chroma=np.zeros(12)
    for k in range(len(fr)):
        if 40<fr[k]<2000: chroma[int(round(69+12*np.log2(fr[k]/440)))%12]+=X[k]**2
    top=np.argsort(chroma)[::-1][:4]
    print(f.split('/')[-1][:30],f"rms {rms:.1f}dB centroid {cent:.0f}Hz peak {np.abs(m).max():.2f} chroma",[NOTES[i] for i in top])
