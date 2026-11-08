#include "../Source/engine/Synth.h"
#include <chrono>
#include <cstdio>
using namespace rg;
static double run(const char* name, int voices,int uni,int quality,bool both,bool fx){
    Synth s; s.prepare(44100,512); s.g[G_MODE]=both?2:0; s.g[G_QUALITY]=quality;
    for(Layer* l:{&s.A,&s.B}){ l->p[P_UNI_N]=uni; l->p[P_O2_LVL]=0.7f; l->p[P_F_TYPE]=1; l->p[P_F_DRIVE]=0.3f; l->p[P_MAXV]=16; l->p[P_A_SUS]=1; l->p[P_O1_WARP]=0.3f; l->p[P_O1_WARPMODE]=2;
        if(fx){ l->p[P_CHORUS]=0.5f; l->p[P_DELAY_MIX]=0.3f; l->p[P_VERB_MIX]=0.3f; l->p[P_CHARGE]=0.3f; } }
    for(int i=0;i<voices;++i) s.noteOn(0,36+i*3,100);
    std::vector<float> L(512),R(512); for(int i=0;i<20;++i) s.process(L.data(),R.data(),512);
    auto t0=std::chrono::steady_clock::now(); const int blocks=800; for(int i=0;i<blocks;++i) s.process(L.data(),R.data(),512);
    double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count(), rt=blocks*512/44100.0;
    printf("%-46s %5.2f%% of one core\n",name,100*sec/rt); return 100*sec/rt; }
int main(){
    run("typical: 6 voices, 1 layer, uni 1, Clean",6,1,1,false,false);
    run("pad: 8 voices, 1 layer, uni 3, FX, Clean",8,3,1,false,true);
    run("both layers, 8 voices each, uni 3, FX",8,3,1,true,true);
    run("both layers, 8 voices each, uni 3, FX, ECO",8,3,0,true,true);
    run("WORST: both layers, 16 voices, uni 7, FX, HOT",16,7,2,true,true);
    run("WORST ECO",16,7,0,true,true); }
