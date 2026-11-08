// Offline renderer used by the preset pipeline and tests.
// usage: rg_render <wtdir> <patch.txt> <out.wav|-> <note[,note..]> <seconds> <release_s> [velocity]
// patch.txt lines:  "G <id> <value>" or "A|B <id> <value>"
#include "../Source/engine/Synth.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
using namespace rg;
static int findLayer(const std::string& id){ for(int i=0;i<NLAYER;++i) if(id==LAYER_DEFS[i].id) return i; return -1; }
static int findGlobal(const std::string& id){ for(int i=0;i<NGLOBAL;++i) if(id==GLOBAL_DEFS[i].id) return i; return -1; }
int main(int argc,char**argv){
    if(argc<7){ fprintf(stderr,"usage\n"); return 2; }
    std::string wtdir=argv[1];
    for(int t=0;t<NTABLES;++t){
        std::string f=wtdir+"/"+TABLE_FILES[t]+".rgwt"; std::ifstream in(f,std::ios::binary);
        if(!in) continue; std::vector<int16_t> d(WT_FRAMES*WT_N); in.read((char*)d.data(),d.size()*2);
        Wavetables::get().setRawInt16(t,d.data(),d.size());
    }
    Synth s; const float SR=44100.f; s.prepare(SR,512);
    { std::ifstream in(argv[2]); std::string line;
      while(std::getline(in,line)){ std::istringstream ss(line); std::string k,id; float v; if(!(ss>>k>>id>>v)) continue;
        if(k=="G"){int i=findGlobal(id); if(i>=0) s.g[i]=v;} else { int i=findLayer(id); if(i<0) continue; (k=="A"?s.A:s.B).p[i]=v; } } }
    std::vector<int> notes; { std::stringstream ss(argv[4]); std::string t; while(std::getline(ss,t,',')) notes.push_back(atoi(t.c_str())); }
    float dur=atof(argv[5]), rel=atof(argv[6]); int vel=argc>7?atoi(argv[7]):100;
    int total=(int)((dur+rel)*SR), on=(int)(dur*SR);
    std::vector<float> L(total),R(total); int pos=0; bool started=false, stopped=false;
    while(pos<total){
        if(!started){ for(int n:notes) s.noteOn(0,n,vel); started=true; }
        if(!stopped&&pos>=on){ for(int n:notes) s.noteOff(0,n); stopped=true; }
        int c=std::min(256,total-pos); if(!stopped) c=std::min(c,on-pos);
        s.process(L.data()+pos,R.data()+pos,c); pos+=c; }
    double sum=0,pk=0,tailSum=0; long nan=0; int tailN=(int)(0.25*SR);
    for(int i=0;i<total;++i){ float a=L[i],b=R[i]; if(!std::isfinite(a)||!std::isfinite(b)){++nan;continue;}
        pk=std::max(pk,(double)std::max(std::fabs(a),std::fabs(b))); if(i<on){ sum+=a*a+b*b; } if(i>=total-tailN) tailSum+=a*a+b*b; }
    double rms=std::sqrt(sum/(2.0*std::max(1,on))), tail=std::sqrt(tailSum/(2.0*tailN));
    printf("{\"peak_db\":%.2f,\"rms_db\":%.2f,\"nan\":%ld,\"tail_db\":%.2f}\n",20*std::log10(pk+1e-9),20*std::log10(rms+1e-9),nan,20*std::log10(tail+1e-9));
    if(std::string(argv[3])!="-"){ FILE*f=fopen(argv[3],"wb"); int dl=total*4, sr=44100; short ch=2,bits=16; int br=sr*4; short ba=4,fmt=1; int fs=16;
        fwrite("RIFF",1,4,f); int rs=36+dl; fwrite(&rs,4,1,f); fwrite("WAVEfmt ",1,8,f); fwrite(&fs,4,1,f); fwrite(&fmt,2,1,f); fwrite(&ch,2,1,f); fwrite(&sr,4,1,f); fwrite(&br,4,1,f); fwrite(&ba,2,1,f); fwrite(&bits,2,1,f); fwrite("data",1,4,f); fwrite(&dl,4,1,f);
        for(int i=0;i<total;++i){ short a=(short)(std::clamp(L[i],-1.f,1.f)*32767), b=(short)(std::clamp(R[i],-1.f,1.f)*32767); fwrite(&a,2,1,f); fwrite(&b,2,1,f);} fclose(f);}
    return 0; }
