// RAYGUN layer: one full synth voice-bank (2 wavetable oscs + sub + noise -> SVF -> amp), mod matrix, arp, FX.
#pragma once
#include "Dsp.h"
#include "Wavetables.h"
#include <cstring>

namespace rg {

constexpr int kMaxVoices = 16;
constexpr int kChunk = 16;       // control-rate: modulation/filter updated every 16 samples
constexpr int kMaxUni = 7;

struct Voice {
    bool active = false, gate = false;
    int note = 60; float vel = 1.f; uint64_t age = 0;
    float curNote = 60.f;                      // glided pitch
    float ph1[kMaxUni] = {}, ph2[kMaxUni] = {}, subPh = 0;
    float rnd = 0; uint32_t rng = 1;
    ADSR amp, env2;
    SVF fL[2], fR[2];
    float lastCutoff = 8000;
    float gainScale = 1.f;                     // split/velocity crossfade scaling
};

class Layer {
public:
    float p[P_COUNT];            // raw parameters (set by host/preset)
    float e[P_COUNT];            // effective parameters (after macros)
    float macro[8];
    float modWheel = 0, pitchBend = 0, aftertouch = 0;
    float bpm = 120.f; int quality = 1;

    Layer() { for (int i = 0; i < P_COUNT; ++i) p[i] = LAYER_DEFS[i].def; for (int i = 0; i < 8; ++i) macro[i] = 0.5f; std::memcpy(e, p, sizeof(e)); }

    void prepare(float sampleRate) {
        sr = sampleRate; chorus.prepare(sr); delay.prepare(sr); reverb.prepare(sr);
        wt = &Wavetables::get(); wt->build();
        for (auto& v : voices) { v = Voice(); v.rng = 777u + (uint32_t)(&v - voices); }
        arpHeld.clear(); held.clear();
    }

    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active; return n; }

    // ------------------------------------------------------------------ note handling
    void noteOn(int note, float vel, float gainScale = 1.f) {
        if (p[P_ARP_ON] > 0.5f) {
            arpHeld.erase(std::remove(arpHeld.begin(), arpHeld.end(), note), arpHeld.end());
            arpHeld.push_back(note); arpVel = vel; arpGain = gainScale;
            if (arpHeld.size() == 1) { arpCounter = 1e9; arpIndex = -1; }
            return;
        }
        voiceOn(note, vel, gainScale);
    }
    void noteOff(int note) {
        if (p[P_ARP_ON] > 0.5f) {
            arpHeld.erase(std::remove(arpHeld.begin(), arpHeld.end(), note), arpHeld.end());
            if (arpHeld.empty()) { releaseNote(arpCurrent); arpCurrent = -1; }
            return;
        }
        releaseNote(note);
    }
    void allNotesOff() { arpHeld.clear(); held.clear(); for (auto& v : voices) if (v.active) { v.gate = false; v.amp.gateOff(); v.env2.gateOff(); } }
    void panic() { arpHeld.clear(); held.clear(); for (auto& v : voices) { v.active = false; v.gate = false; v.amp.kill(); v.env2.kill(); } }

    // ------------------------------------------------------------------ render (adds nothing: overwrites L/R)
    void process(float* L, float* R, int n) {
        computeEffective();
        std::memset(L, 0, sizeof(float) * n); std::memset(R, 0, sizeof(float) * n);
        int pos = 0;
        while (pos < n) {
            int c = std::min(kChunk, n - pos);
            arpTick(c);
            float l1 = lfo1.next(e[P_L1_RATE], (int) e[P_L1_SHAPE], sr / (float) kChunk);
            float l2 = lfo2.next(e[P_L2_RATE], (int) e[P_L2_SHAPE], sr / (float) kChunk);
            for (auto& v : voices) if (v.active) renderVoice(v, L + pos, R + pos, c, l1, l2);
            pos += c;
        }
        // ---- layer FX
        float ch = e[P_CHARGE];
        if (ch > 0.001f) {
            float g = 1.f + ch * 5.f, comp = 1.f / fastTanh(g * 0.7f) * 0.7f;
            for (int i = 0; i < n; ++i) { L[i] = fastTanh(L[i] * g) * comp; R[i] = fastTanh(R[i] * g) * comp; }
        }
        if (quality > 0) chorus.process(L, R, n, e[P_CHORUS]);
        delay.process(L, R, n, e[P_DELAY_MIX], e[P_DELAY_TIME], e[P_DELAY_FB]);
        reverb.process(L, R, n, quality == 0 ? e[P_VERB_MIX] * 0.7f : e[P_VERB_MIX], e[P_VERB_SIZE]);
        // ---- level + pan (equal-power)
        float g = dbToLin(e[P_VOL]); float pn = std::clamp(e[P_PAN], -1.f, 1.f);
        float gl = g * std::cos((pn * 0.5f + 0.5f) * 1.5707963f) * 1.41421356f, gr = g * std::sin((pn * 0.5f + 0.5f) * 1.5707963f) * 1.41421356f;
        for (int i = 0; i < n; ++i) { L[i] *= gl; R[i] *= gr; }
    }

private:
    float sr = 44100.f;
    Wavetables* wt = nullptr;
    Voice voices[kMaxVoices];
    std::vector<int> held, arpHeld; uint64_t ageCounter = 0;
    LFO lfo1, lfo2;
    Chorus chorus; Delay delay; Reverb reverb;
    // arp
    float arpCounter = 0, arpVel = 1, arpGain = 1; int arpIndex = -1, arpCurrent = -1, arpDir = 1; uint32_t arpRng = 99;
    bool arpNoteOpen = false; float arpGateCounter = 0;

    static float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

    void computeEffective() {
        std::memcpy(e, p, sizeof(e));
        auto m = [&](int i) { return (macro[i] - 0.5f) * 2.f; };
        auto add = [&](int id, float d) { e[id] += d; };
        auto clampP = [&](int id) { e[id] = std::clamp(e[id], LAYER_DEFS[id].mn, LAYER_DEFS[id].mx); };
        e[P_F_CUT] *= std::exp2(m(0) * 3.f);
        add(P_CHORUS, m(1) * 0.5f);
        float a = std::exp2(m(2) * 3.f); e[P_A_ATK] *= a; e[P_E_ATK] *= a;
        float r = std::exp2(m(3) * 3.f); e[P_A_REL] *= r; e[P_E_REL] *= r;
        add(P_CHARGE, m(4) * 0.6f); add(P_F_DRIVE, m(4) * 0.4f);
        add(P_O1_POS, m(5) * 0.5f); add(P_O2_POS, m(5) * 0.5f); e[P_L1_RATE] *= std::exp2(m(5) * 1.5f);
        add(P_VERB_MIX, m(6) * 0.4f); add(P_DELAY_MIX, m(6) * 0.25f);
        add(P_UNI_DET, m(7) * 0.5f); add(P_O2_FINE, m(7) * 20.f);
        for (int id : {P_F_CUT, P_CHORUS, P_A_ATK, P_E_ATK, P_A_REL, P_E_REL, P_CHARGE, P_F_DRIVE, P_O1_POS, P_O2_POS, P_L1_RATE,
                       P_VERB_MIX, P_DELAY_MIX, P_UNI_DET, P_O2_FINE}) clampP(id);
        if (quality == 0) e[P_UNI_N] = std::min(e[P_UNI_N], 3.f);
    }

    // ------------------------------------------------------------------ voice allocation
    Voice* allocVoice() {
        int maxv = std::clamp((int) p[P_MAXV], 1, kMaxVoices);
        if (p[P_VOICE] > 0.5f) maxv = 1;
        int count = 0; Voice* freeV = nullptr; Voice* oldestRel = nullptr; Voice* oldest = nullptr;
        for (int i = 0; i < kMaxVoices; ++i) {
            Voice& v = voices[i];
            if (v.active) { ++count; if (!v.gate && (!oldestRel || v.age < oldestRel->age)) oldestRel = &v; if (!oldest || v.age < oldest->age) oldest = &v; }
            else if (!freeV) freeV = &v;
        }
        if (p[P_VOICE] > 0.5f) return &voices[0];
        if (freeV && count < maxv) return freeV;
        return oldestRel ? oldestRel : oldest;   // steal: prefer releasing voices, else oldest
    }

    void startVoice(Voice& v, int note, float vel, float gs, bool retrigger) {
        bool wasActive = v.active;
        v.note = note; v.vel = vel; v.gainScale = gs; v.age = ++ageCounter; v.gate = true; v.active = true;
        float g = p[P_GLIDE];
        if (!wasActive || g <= 0.0001f) v.curNote = (float) note;
        if (retrigger || !wasActive) {
            v.rng = v.rng * 1664525u + 1013904223u; v.rnd = (v.rng >> 8) * (1.f / 8388608.f) - 1.f;
            for (int u = 0; u < kMaxUni; ++u) {
                v.rng = v.rng * 1664525u + 1013904223u; v.ph1[u] = (v.rng >> 8) * (1.f / 16777216.f);
                v.rng = v.rng * 1664525u + 1013904223u; v.ph2[u] = (v.rng >> 8) * (1.f / 16777216.f);
            }
            if (!wasActive) { v.fL[0].reset(); v.fL[1].reset(); v.fR[0].reset(); v.fR[1].reset(); v.amp.level = 0; v.env2.level = 0; }
            v.amp.gateOn(); v.env2.gateOn();
        }
    }

    void voiceOn(int note, float vel, float gs) {
        int mode = (int) p[P_VOICE];
        if (mode == 0) { startVoice(*allocVoice(), note, vel, gs, true); return; }
        held.erase(std::remove(held.begin(), held.end(), note), held.end()); held.push_back(note);
        Voice& v = voices[0];
        bool legatoSlur = (mode == 2) && v.active && v.gate;
        startVoice(v, note, vel, gs, !legatoSlur);
    }

    void releaseNote(int note) {
        if (note < 0) return;
        if (p[P_VOICE] > 0.5f) {
            held.erase(std::remove(held.begin(), held.end(), note), held.end());
            Voice& v = voices[0];
            if (!held.empty()) { startVoice(v, held.back(), v.vel, v.gainScale, p[P_VOICE] < 1.5f); }
            else if (v.active) { v.gate = false; v.amp.gateOff(); v.env2.gateOff(); }
            return;
        }
        for (auto& v : voices) if (v.active && v.gate && v.note == note) { v.gate = false; v.amp.gateOff(); v.env2.gateOff(); }
    }

    // ------------------------------------------------------------------ arp
    void arpTick(int nSamples) {
        if (p[P_ARP_ON] < 0.5f) { if (arpCurrent >= 0) { releaseNote(arpCurrent); arpCurrent = -1; } return; }
        static const float beats[6] = {1.f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f};
        float stepSamples = beats[(int) p[P_ARP_RATE]] * 60.f / std::max(20.f, bpm) * sr;
        if (arpNoteOpen) { arpGateCounter -= nSamples; if (arpGateCounter <= 0) { releaseNote(arpCurrent); arpNoteOpen = false; } }
        if (arpHeld.empty()) return;
        arpCounter += nSamples;
        if (arpCounter >= stepSamples) {
            arpCounter -= stepSamples; if (arpCounter > stepSamples) arpCounter = 0;
            if (arpNoteOpen) releaseNote(arpCurrent);
            std::vector<int> seq = arpHeld; int mode = (int) p[P_ARP_MODE];
            if (mode != 4) std::sort(seq.begin(), seq.end());
            int octs = (int) p[P_ARP_OCT]; std::vector<int> full;
            for (int o = 0; o < octs; ++o) for (int nn : seq) full.push_back(nn + 12 * o);
            int N = (int) full.size();
            switch (mode) {
            case 0: case 4: arpIndex = (arpIndex + 1) % N; break;
            case 1: arpIndex = (arpIndex <= 0 ? N : arpIndex) - 1; break;
            case 2: if (N == 1) arpIndex = 0; else { arpIndex += arpDir; if (arpIndex >= N) { arpIndex = N - 2; arpDir = -1; } else if (arpIndex < 0) { arpIndex = 1; arpDir = 1; } } break;
            default: arpRng = arpRng * 1664525u + 1013904223u; arpIndex = (int)((arpRng >> 8) % (uint32_t) N);
            }
            arpIndex = std::clamp(arpIndex, 0, N - 1);
            arpCurrent = full[arpIndex]; voiceOn(arpCurrent, arpVel, arpGain);
            arpNoteOpen = true; arpGateCounter = stepSamples * p[P_ARP_GATE];
        }
    }

    // ------------------------------------------------------------------ wavetable read
    inline float readWT(int table, int level, float pos, float ph) const {
        float fp = pos * (WT_FRAMES - 1); int f0 = (int) fp; if (f0 > WT_FRAMES - 2) f0 = WT_FRAMES - 2; float fr = fp - f0;
        const float* a = wt->frame(table, level, f0); const float* b = wt->frame(table, level, f0 + 1);
        float x = ph * WT_N; int i = (int) x; if (i > WT_N - 1) i = WT_N - 1; float t = x - i;
        float va = a[i] + (a[i + 1] - a[i]) * t, vb = b[i] + (b[i + 1] - b[i]) * t;
        return va + (vb - va) * fr;
    }
    static inline float warpPhase(float ph, int mode, float w) {
        switch (mode) {
        case 1: { float q = ph * (1.f + w * 7.f); return q - std::floor(q); }
        case 2: { float b = 0.5f + w * 0.45f; return ph < b ? ph * 0.5f / b : 0.5f + (ph - b) * 0.5f / (1.f - b); }
        case 3: { float m = ph < 0.5f ? ph * 2.f : 2.f - ph * 2.f; return ph + w * (m * 0.5f - ph) * 1.0f; }
        default: return ph;
        }
    }
    static inline float fold(float y, float w) { float x = y * (1.f + w * 5.f); float t = x * 0.25f + 0.25f; t -= std::floor(t); return std::fabs(t * 4.f - 2.f) - 1.f; }
    int mipLevel(float freq, float mult) const {
        float maxH = 0.45f * sr / std::max(20.f, freq * mult);
        int lv = 0; while ((512 >> lv) > maxH && lv < WT_LEVELS - 1) ++lv; return lv;
    }

    // ------------------------------------------------------------------ voice render
    void renderVoice(Voice& v, float* L, float* R, int n, float lfo1v, float lfo2v) {
        // ---- sources
        float sv[17];
        float env2 = v.env2.level;
        sv[0] = 0; sv[1] = lfo1v; sv[2] = lfo2v; sv[3] = env2; sv[4] = v.vel; sv[5] = modWheel; sv[6] = (v.curNote - 60.f) / 60.f;
        sv[7] = aftertouch; sv[8] = v.rnd; for (int i = 0; i < 8; ++i) sv[9 + i] = (macro[i] - 0.5f) * 2.f;
        float d[19] = {};
        for (int s = 0; s < 8; ++s) {
            int src = (int) e[P_M1_SRC + s * 3], dst = (int) e[P_M1_DST + s * 3]; if (src == 0 || dst == 0) continue;
            d[dst] += sv[src] * e[P_M1_AMT + s * 3];
        }
        // ---- glide
        float gl = e[P_GLIDE];
        if (gl > 0.0001f) { float c = 1.f - std::exp(-(float) n / (sr * gl * 0.35f)); v.curNote += ((float) v.note - v.curNote) * c; } else v.curNote = (float) v.note;
        // ---- envelopes
        v.amp.set(e[P_A_ATK], e[P_A_DEC], e[P_A_SUS], e[P_A_REL], sr / 1.f);
        v.env2.set(e[P_E_ATK], e[P_E_DEC], e[P_E_SUS], e[P_E_REL], sr);
        // ---- pitch
        float base = v.curNote + 12.f * (e[P_OCT]) + pitchBend * e[P_BEND] + d[5] * 24.f;
        auto freqOf = [&](float semis) { return 440.f * std::exp2((semis - 69.f) / 12.f); };
        float f1 = freqOf(base + 12.f * e[P_O1_OCT] + e[P_O1_SEMI] + e[P_O1_FINE] * 0.01f + d[3] * 24.f);
        float f2 = freqOf(base + 12.f * e[P_O2_OCT] + e[P_O2_SEMI] + e[P_O2_FINE] * 0.01f + d[4] * 24.f);
        float fsub = freqOf(base - 12.f);
        float lvl1 = clamp01(e[P_O1_LVL] + d[9]), lvl2 = clamp01(e[P_O2_LVL] + d[10]);
        float subL = clamp01(e[P_SUB_LVL] + d[18]), noiseL = clamp01(e[P_NOISE_LVL] + d[17]);
        float pos1 = clamp01(e[P_O1_POS] + d[1]), pos2 = clamp01(e[P_O2_POS] + d[2]);
        float w1 = clamp01(e[P_O1_WARP] + d[12]), w2 = clamp01(e[P_O2_WARP] + d[16]);
        int wm1 = (int) e[P_O1_WARPMODE], wm2 = (int) e[P_O2_WARPMODE];
        float fmAmt = clamp01(e[P_FM] + d[13]);
        int uni = std::clamp((int) e[P_UNI_N], 1, kMaxUni);
        float det = clamp01(e[P_UNI_DET] + d[11]) * 0.6f;       // semitone spread at detune=1 (0.6 st)
        float wid = e[P_UNI_WID];
        int t1 = (int) e[P_O1_TABLE], t2 = (int) e[P_O2_TABLE];
        float mulA = (wm1 == 1) ? 1.f + w1 * 7.f : 1.f, mulB = (wm2 == 1) ? 1.f + w2 * 7.f : 1.f;
        float maxDet = std::exp2(det / 12.f * 0.5f * (uni > 1 ? 1 : 0));
        int lv1 = mipLevel(f1 * maxDet, mulA * (1.f + fmAmt * 0.5f)), lv2 = mipLevel(f2 * maxDet, mulB);
        int lvS = 0; (void) lvS;
        // unison offsets
        float inc1[kMaxUni], inc2[kMaxUni], gL[kMaxUni], gR[kMaxUni];
        float norm = 1.f / std::sqrt((float) uni);
        for (int u = 0; u < uni; ++u) {
            float off = uni > 1 ? ((float) u / (uni - 1) * 2.f - 1.f) : 0.f;
            float ratio = std::exp2(off * det / 12.f);
            inc1[u] = f1 * ratio / sr; inc2[u] = f2 * ratio / sr;
            float pn = off * wid; float ang = (pn * 0.5f + 0.5f) * 1.5707963f;
            gL[u] = std::cos(ang) * 1.41421356f * norm; gR[u] = std::sin(ang) * 1.41421356f * norm;
        }
        float subInc = fsub / sr;
        int subW = (int) e[P_SUB_WAVE];
        // ---- filter coefficients
        int ftype = (int) e[P_F_TYPE];
        float oct = d[6] * 5.f + e[P_F_ENV] * env2 * 5.f + e[P_F_KEY] * (v.curNote - 60.f) / 12.f;
        float cut = std::clamp(e[P_F_CUT] * std::exp2(oct), 20.f, 20000.f);
        float res = clamp01(e[P_F_RES] + d[7]);
        float drive = clamp01(e[P_F_DRIVE] + d[8]);
        bool two = (ftype == 1) && quality > 0;   // Eco: LP24 degrades to LP12
        for (int k = 0; k < 2; ++k) v.fL[k].setup(cut, k == 0 ? res : res * 0.3f, sr);
        for (int k = 0; k < 2; ++k) { v.fR[k].k = v.fL[k].k; v.fR[k].a1 = v.fL[k].a1; v.fR[k].a2 = v.fL[k].a2; v.fR[k].a3 = v.fL[k].a3; }
        bool stereo = uni > 1;
        if (!stereo) { v.fR[0].ic1 = v.fL[0].ic1; v.fR[0].ic2 = v.fL[0].ic2; v.fR[1].ic1 = v.fL[1].ic1; v.fR[1].ic2 = v.fL[1].ic2; }
        float dg = 1.f + drive * 6.f, dcomp = 1.f / std::sqrt(dg);
        float ampMod = std::max(0.f, 1.f + d[14]);
        float velGain = (0.25f + 0.75f * v.vel) * v.gainScale * ampMod * 0.5f;
        float panMod = d[15];
        float pl = 1.f, pr = 1.f;
        if (panMod != 0.f) { float a = (std::clamp(panMod, -1.f, 1.f) * 0.5f + 0.5f) * 1.5707963f; pl = std::cos(a) * 1.41421356f; pr = std::sin(a) * 1.41421356f; }

        for (int i = 0; i < n; ++i) {
            float sumL = 0, sumR = 0;
            for (int u = 0; u < uni; ++u) {
                float o2 = 0, o1 = 0;
                // osc 2
                float ph = v.ph2[u]; v.ph2[u] += inc2[u]; if (v.ph2[u] >= 1.f) v.ph2[u] -= 1.f;
                if (lvl2 > 0.0005f || fmAmt > 0.0005f) {
                    float y = readWT(t2, lv2, pos2, std::min(0.99999f, warpPhase(ph, wm2, w2)));
                    if (wm2 == 4) y = fold(y, w2);
                    o2 = y;
                }
                // osc 1 (with FM from osc2)
                ph = v.ph1[u]; v.ph1[u] += inc1[u]; if (v.ph1[u] >= 1.f) v.ph1[u] -= 1.f;
                if (lvl1 > 0.0005f) {
                    float pph = ph; if (fmAmt > 0.0005f) { pph += fmAmt * 0.5f * o2; pph -= std::floor(pph); }
                    float y = readWT(t1, lv1, pos1, std::min(0.99999f, warpPhase(pph, wm1, w1)));
                    if (wm1 == 4) y = fold(y, w1);
                    o1 = y;
                }
                float s = o1 * lvl1 + o2 * lvl2;
                sumL += s * gL[u]; sumR += s * gR[u];
            }
            float extra = 0;
            if (subL > 0.0005f) {
                v.subPh += subInc; if (v.subPh >= 1.f) v.subPh -= 1.f;
                float sp = v.subPh;
                extra += subL * (subW == 0 ? std::sin(6.2831853f * sp) : subW == 1 ? 4.f * std::fabs(sp - 0.5f) - 1.f : (sp < 0.5f ? 0.7f : -0.7f));
            }
            if (noiseL > 0.0005f) { v.rng = v.rng * 1664525u + 1013904223u; extra += noiseL * 0.5f * ((v.rng >> 8) * (1.f / 8388608.f) - 1.f); }
            sumL += extra; sumR += extra;
            // ---- filter
            float outL = sumL, outR = sumR;
            if (ftype != 5) {
                float a = drive > 0.001f ? fastTanh(sumL * dg) * dcomp : sumL, b = stereo ? (drive > 0.001f ? fastTanh(sumR * dg) * dcomp : sumR) : a;
                float lp, bp, hp;
                v.fL[0].process(a, lp, bp, hp);
                float yL = ftype <= 1 ? lp : ftype == 2 ? hp : ftype == 3 ? bp * (0.5f + 0.5f * v.fL[0].k) : lp + hp;
                if (two) { float l2, b2, h2; v.fL[1].process(yL, l2, b2, h2); yL = l2; }
                outL = yL;
                if (stereo) {
                    v.fR[0].process(b, lp, bp, hp);
                    float yR = ftype <= 1 ? lp : ftype == 2 ? hp : ftype == 3 ? bp * (0.5f + 0.5f * v.fR[0].k) : lp + hp;
                    if (two) { float l2, b2, h2; v.fR[1].process(yR, l2, b2, h2); yR = l2; }
                    outR = yR;
                } else outR = outL;
            }
            float a = v.amp.next(); v.env2.next();
            float g = a * velGain;
            L[i] += outL * g * pl; R[i] += outR * g * pr;
        }
        if (!v.amp.active()) v.active = false;
        v.lastCutoff = cut;
    }
};

} // namespace rg
