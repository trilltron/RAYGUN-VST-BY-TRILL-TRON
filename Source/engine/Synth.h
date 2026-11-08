// RAYGUN top-level synth: two layers (A/B), Layer / Split / Velocity routing, blend, master.
#pragma once
#include "Layer.h"

namespace rg {

class Synth {
public:
    float g[G_COUNT];
    Layer A, B;

    Synth() { for (int i = 0; i < G_COUNT; ++i) g[i] = GLOBAL_DEFS[i].def; }

    void prepare(float sampleRate, int maxBlock) {
        sr = sampleRate; A.prepare(sr); B.prepare(sr);
        tA.assign(maxBlock + kChunk, 0.f); tB.assign(maxBlock + kChunk, 0.f); tA2.assign(maxBlock + kChunk, 0.f); tB2.assign(maxBlock + kChunk, 0.f);
        maxN = maxBlock;
    }

    void noteOn(int ch, int note, int vel127) {
        float v = vel127 / 127.f; float gA, gB; route(note, vel127, gA, gB);
        if (gA > 0.0005f && accepts(A, ch, note)) A.noteOn(note, v, gA);
        if (gB > 0.0005f && accepts(B, ch, note)) B.noteOn(note, v, gB);
    }
    void noteOff(int ch, int note) { (void) ch; A.noteOff(note); B.noteOff(note); }
    void allNotesOff() { A.allNotesOff(); B.allNotesOff(); }
    void panic() { A.panic(); B.panic(); }
    void setPitchBend(float v) { A.pitchBend = B.pitchBend = v; }
    void setModWheel(float v) { A.modWheel = B.modWheel = v; }
    void setAftertouch(float v) { A.aftertouch = B.aftertouch = v; }
    void setBpm(float b) { A.bpm = B.bpm = b; }

    void process(float* L, float* R, int n) {
        int q = (int) g[G_QUALITY]; A.quality = B.quality = q;
        for (int i = 0; i < 8; ++i) { A.macro[i] = B.macro[i] = g[G_MACRO1 + i]; }
        int pos = 0;
        while (pos < n) {
            int c = std::min(n - pos, maxN); renderBlock(L + pos, R + pos, c); pos += c;
        }
    }

private:
    float sr = 44100; int maxN = 512; std::vector<float> tA, tB, tA2, tB2; DCBlock dcL, dcR;

    bool accepts(const Layer& l, int ch, int note) const {
        int mc = (int) l.p[P_MIDICH]; if (mc != 0 && ch + 1 != mc) return false;
        return note >= (int) l.p[P_KEYLO] && note <= (int) l.p[P_KEYHI];
    }
    void route(int note, int vel, float& gA, float& gB) const {
        int mode = (int) g[G_MODE]; gA = gB = 0;
        const float hp = 1.5707963f;
        switch (mode) {
        case 0: gA = 1; break;
        case 1: gB = 1; break;
        case 2: { float b = g[G_BLEND]; gA = std::min(1.f, 2.f * (1.f - b)); gB = std::min(1.f, 2.f * b); break; }
        case 3: { int sp = (int) g[G_SPLIT], xf = (int) g[G_XFADE];
            if (note >= sp) gB = 1; else if (xf <= 0 || note < sp - xf) gA = 1;
            else { float t = (note - (sp - xf) + 0.5f) / (float) xf; gA = std::cos(t * hp); gB = std::sin(t * hp); }
            break; }
        default: { int vs = (int) g[G_VSPLIT], vf = (int) g[G_VFADE];
            if (vel >= vs) gB = 1; else if (vf <= 0 || vel < vs - vf) gA = 1;
            else { float t = (vel - (vs - vf) + 0.5f) / (float) vf; gA = std::cos(t * hp); gB = std::sin(t * hp); }
        }}
    }
    float tailA = 0, tailB = 0;
    bool layerNeeded(const Layer& l, float& tail, int n) const {
        if (l.activeVoices() > 0 || l.p[P_ARP_ON] > 0.5f) {
            bool fx = l.p[P_VERB_MIX] > 0.001f || l.p[P_DELAY_MIX] > 0.001f || l.macro[6] > 0.55f;
            tail = sr * (fx ? 8.f : 0.2f); return true; }
        tail -= n; return tail > 0; }

    void renderBlock(float* L, float* R, int n) {
        std::memset(L, 0, sizeof(float) * n); std::memset(R, 0, sizeof(float) * n);
        int mode = (int) g[G_MODE];
        bool runA = true, runB = true; (void) mode;
        if (runA && layerNeeded(A, tailA, n)) { A.process(tA.data(), tA2.data(), n); for (int i = 0; i < n; ++i) { L[i] += tA[i]; R[i] += tA2[i]; } }
        if (runB && layerNeeded(B, tailB, n)) { B.process(tB.data(), tB2.data(), n); for (int i = 0; i < n; ++i) { L[i] += tB[i]; R[i] += tB2[i]; } }
        float mg = dbToLin(g[G_MASTER]); int q = (int) g[G_QUALITY]; float pre = (q == 2) ? 1.6f : 1.f;
        for (int i = 0; i < n; ++i) {
            float l = dcL.p(L[i]) * mg * pre, r = dcR.p(R[i]) * mg * pre;
            L[i] = fastTanh(l); R[i] = fastTanh(r);
        }
    }
};

} // namespace rg
