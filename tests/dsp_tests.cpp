#include "../Source/engine/Synth.h"
#include <cstdio>
#include <random>
using namespace rg;
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){ printf("FAIL: %s\n",msg); ++fails;} else printf("ok   : %s\n",msg);}while(0)
static std::vector<float> render(Synth& s,int n){ std::vector<float> L(n),R(n); int p=0; while(p<n){int c=std::min(256,n-p); s.process(L.data()+p,R.data()+p,c); p+=c;} return L; }
static Synth make(){ Synth s; s.prepare(44100,512); s.g[G_MODE]=0; s.g[G_MASTER]=0; s.A.p[P_F_TYPE]=5; s.A.p[P_O1_TABLE]=0; s.A.p[P_O1_POS]=1; s.A.p[P_A_ATK]=0.001f; return s; }
static std::vector<double> mag(const std::vector<float>& x,int off,int N){ std::vector<std::complex<double>> a(N); for(int i=0;i<N;++i) {double w=2*M_PI*i/N; a[i]=x[off+i]*(0.35875-0.48829*std::cos(w)+0.14128*std::cos(2*w)-0.01168*std::cos(3*w));} fftInPlace(a,false); std::vector<double> m(N/2); for(int i=0;i<N/2;++i) m[i]=std::abs(a[i]); return m; }
int main(){
    { Synth s=make(); s.noteOn(0,69,100); auto x=render(s,44100); auto m=mag(x,20000,16384);
      int pk=std::max_element(m.begin(),m.end())-m.begin(); double f=pk*44100.0/16384; CHECK(std::fabs(f-440)<4,"pitch A4 = 440 Hz"); }
    for(int note: {84,96,100}) { Synth s=make(); s.noteOn(0,note,100); auto x=render(s,44100); const int N=16384; auto m=mag(x,20000,N);
      double f0=440*std::pow(2.0,(note-69)/12.0), bin=f0*N/44100.0, tot=0, alias=0;
      for(int i=2;i<N/2;++i){ tot+=m[i]*m[i]; double h=i/bin, d=std::fabs(h-std::round(h)); if(d*bin>6) alias+=m[i]*m[i]; }
      double db=10*std::log10(alias/tot+1e-20); char b[96]; snprintf(b,96,"aliasing note %d: %.1f dB (< -50)",note,db); CHECK(db<-50,b); }
    { std::mt19937 rng(5); std::uniform_real_distribution<float> U(0,1); long bad=0; float worst=0;
      for(int it=0;it<120;++it){ Synth s; s.prepare(44100,512); s.g[G_MODE]=2;
        for(Layer* l:{&s.A,&s.B}) for(int i=0;i<NLAYER;++i){ const PDef&d=LAYER_DEFS[i]; float v=d.mn+U(rng)*(d.mx-d.mn); if(d.kind==KI||d.kind==KC||d.kind==KB) v=std::round(v); l->p[i]=v; }
        for(int i=0;i<8;++i) s.g[G_MACRO1+i]=U(rng);
        for(int k=0;k<6;++k) s.noteOn(0,24+(int)(U(rng)*80),20+(int)(U(rng)*107));
        auto x=render(s,22050); for(float v:x){ if(!std::isfinite(v)) ++bad; else worst=std::max(worst,std::fabs(v)); } }
      char b[96]; snprintf(b,96,"fuzz 120 random patches: no NaN/inf (bad=%ld, peak=%.2f<=1)",bad,worst); CHECK(bad==0&&worst<=1.0001f,b); }
    { Synth s=make(); s.A.p[P_MAXV]=4; for(int n=40;n<50;++n) s.noteOn(0,n,100); render(s,2048); CHECK(s.A.activeVoices()<=4,"voice stealing respects max voices"); }
    { Synth s=make(); s.A.p[P_VOICE]=2; s.noteOn(0,40,100); s.noteOn(0,43,100); s.noteOn(0,47,100); render(s,1024); CHECK(s.A.activeVoices()==1,"legato mode uses one voice"); }
    { Synth s=make(); s.g[G_MODE]=3; s.g[G_SPLIT]=60; s.g[G_XFADE]=0; s.noteOn(0,40,100); CHECK(s.A.activeVoices()==1&&s.B.activeVoices()==0,"split: low note -> A only"); s.noteOn(0,72,100); CHECK(s.B.activeVoices()==1,"split: high note -> B"); }
    { Synth s=make(); s.g[G_MODE]=3; s.g[G_SPLIT]=60; s.g[G_XFADE]=4; s.noteOn(0,58,100); CHECK(s.A.activeVoices()==1&&s.B.activeVoices()==1,"split crossfade zone triggers both"); }
    { Synth s=make(); s.g[G_MODE]=4; s.g[G_VSPLIT]=96; s.g[G_VFADE]=0; s.noteOn(0,60,50); s.noteOn(0,62,120); CHECK(s.A.activeVoices()==1&&s.B.activeVoices()==1,"velocity split routes by velocity"); }
    { Synth s=make(); s.A.p[P_A_REL]=0.05f; s.noteOn(0,60,100); render(s,4410); s.noteOff(0,60); render(s,44100); CHECK(s.A.activeVoices()==0,"voices free after release"); }
    { Synth s=make(); s.A.p[P_F_TYPE]=1; s.A.p[P_F_CUT]=200; s.A.p[P_F_RES]=0.6f; s.A.p[P_O1_POS]=1; s.noteOn(0,48,100); render(s,8192); s.A.p[P_F_CUT]=12000; auto x=render(s,1024);
      float mx=0; for(size_t i=1;i<x.size();++i) mx=std::max(mx,std::fabs(x[i]-x[i-1])); char b[96]; snprintf(b,96,"cutoff jump has no click (max step %.3f < 0.5)",mx); CHECK(mx<0.5f,b); }
    { Synth s=make(); s.A.p[P_F_TYPE]=1; s.A.p[P_F_RES]=0.9f; s.noteOn(0,60,100); render(s,22050); s.noteOff(0,60); s.A.p[P_A_REL]=0.01f; auto x=render(s,88200);
      float tail=0; for(size_t i=x.size()-4410;i<x.size();++i) tail=std::max(tail,std::fabs(x[i])); CHECK(tail<1e-4f,"decays to digital silence (no denormal tail)"); }
    { Synth s=make(); s.g[G_MODE]=0; s.A.p[P_ARP_ON]=1; s.A.p[P_ARP_RATE]=3; s.noteOn(0,60,100); s.noteOn(0,64,100); auto x=render(s,44100); float pk=0; for(float v:x) pk=std::max(pk,std::fabs(v)); CHECK(pk>0.01f,"arp produces sound"); }
    printf(fails?"\n%d FAILED\n":"\nALL PASSED\n",fails); return fails?1:0; }
