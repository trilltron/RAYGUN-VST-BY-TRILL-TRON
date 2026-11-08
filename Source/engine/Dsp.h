// RAYGUN DSP primitives. No JUCE dependency so the engine is unit-testable on any platform.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace rg {

inline float flushDenorm(float x) { return (std::fabs(x) < 1e-20f) ? 0.f : x; }

// Rational tanh approximation (cheap, smooth, bounded to +-1)
inline float fastTanh(float x) {
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

inline float dbToLin(float db) { return std::pow(10.f, db * 0.05f); }

struct Smoother {
    float v = 0, target = 0, c = 0.01f;
    void setTime(float sec, float sr) { c = 1.f - std::exp(-1.f / std::max(1.f, sec * sr)); }
    void reset(float x) { v = target = x; }
    float next() { v += (target - v) * c; return v; }
};

// ---------------- Cytomic-style zero-delay-feedback SVF ----------------
struct SVF {
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void setup(float cutoff, float res, float sr) {
        cutoff = std::min(cutoff, sr * 0.45f);
        float g = std::tan(3.14159265f * cutoff / sr);
        k = 2.f - 1.95f * res;                 // res 0..1 -> k 2..0.05 (stable)
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
        g_ = g;
    }
    // returns low,band,high via out params
    inline void process(float v0, float& lp, float& bp, float& hp) {
        float v3 = v0 - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenorm(2.f * v1 - ic1); ic2 = flushDenorm(2.f * v2 - ic2);
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
    float g_ = 0;
};

// ---------------- ADSR (linear attack, exponential decay/release) ----------------
struct ADSR {
    enum Stage { Idle, Atk, Dec, Sus, Rel } stage = Idle;
    float level = 0, aInc = 0, dC = 0, rC = 0, sus = 1, floorV = 1e-4f;
    void set(float a, float d, float s, float r, float sr) {
        aInc = 1.f / std::max(1.f, a * sr);
        dC = 1.f - std::exp(-4.6f / std::max(1.f, d * sr));   // ~ -40 dB over d
        rC = 1.f - std::exp(-4.6f / std::max(1.f, r * sr));
        sus = s;
    }
    void gateOn() { stage = Atk; }
    void gateOff() { if (stage != Idle) stage = Rel; }
    void kill() { stage = Idle; level = 0; }
    bool active() const { return stage != Idle; }
    inline float next() {
        switch (stage) {
        case Idle: return 0.f;
        case Atk: level += aInc; if (level >= 1.f) { level = 1.f; stage = Dec; } break;
        case Dec: level += (sus - level) * dC; if (std::fabs(level - sus) < 1e-4f) { level = sus; stage = Sus; } break;
        case Sus: level = sus; break;
        case Rel: level -= level * rC; if (level < floorV) { level = 0; stage = Idle; } break;
        }
        return level;
    }
};

// ---------------- LFO ----------------
struct LFO {
    float phase = 0, sh = 0; uint32_t rng = 12345;
    float next(float rate, int shape, float sr) {
        phase += rate / sr; if (phase >= 1.f) { phase -= 1.f; rng = rng * 1664525u + 1013904223u; sh = (rng >> 8) * (1.f / 8388608.f) - 1.f; }
        switch (shape) {
        case 0: return std::sin(6.2831853f * phase);
        case 1: return 4.f * std::fabs(phase - 0.5f) - 1.f;
        case 2: return 2.f * phase - 1.f;
        case 3: return phase < 0.5f ? 1.f : -1.f;
        default: return sh;
        }
    }
};

// ---------------- Stereo effects ----------------
struct Chorus {
    std::vector<float> bufL, bufR; int w = 0, size = 0; float ph = 0; float sr = 44100;
    void prepare(float s) { sr = s; size = (int)(s * 0.05f) + 4; bufL.assign(size, 0); bufR.assign(size, 0); w = 0; }
    void process(float* L, float* R, int n, float mix) {
        if (mix <= 0.001f) return;
        for (int i = 0; i < n; ++i) {
            bufL[w] = L[i]; bufR[w] = R[i];
            ph += 0.35f / sr; if (ph >= 1) ph -= 1;
            float m1 = 0.5f + 0.5f * std::sin(6.2831853f * ph), m2 = 0.5f + 0.5f * std::sin(6.2831853f * (ph + 0.5f));
            float dL = (0.012f + 0.006f * m1) * sr, dR = (0.012f + 0.006f * m2) * sr;
            L[i] += mix * 0.7f * read(bufL, dL); R[i] += mix * 0.7f * read(bufR, dR);
            if (++w >= size) w = 0;
        }
    }
    float read(const std::vector<float>& b, float d) const {
        float rp = (float) w - d; while (rp < 0) rp += size; while (rp >= (float) size) rp -= size;
        int i0 = std::min((int) rp, size - 1), i1 = (i0 + 1) % size; float f = rp - i0; return b[i0] + (b[i1] - b[i0]) * f;
    }
};

struct Delay {
    std::vector<float> bufL, bufR; int w = 0, size = 0; float sr = 44100, dl = 0; float lpL = 0, lpR = 0;
    void prepare(float s) { sr = s; size = (int)(s * 1.05f) + 4; bufL.assign(size, 0); bufR.assign(size, 0); w = 0; dl = 0.3f * s; }
    void process(float* L, float* R, int n, float mix, float time, float fb) {
        if (mix <= 0.001f) return;
        float tgt = time * sr;
        for (int i = 0; i < n; ++i) {
            dl += (tgt - dl) * 0.0005f;
            float rp = (float) w - dl; while (rp < 0) rp += size; while (rp >= (float) size) rp -= size;
            int i0 = std::min((int) rp, size - 1), i1 = (i0 + 1) % size; float f = rp - i0;
            float oL = bufL[i0] + (bufL[i1] - bufL[i0]) * f, oR = bufR[i0] + (bufR[i1] - bufR[i0]) * f;
            lpL += 0.45f * (oL - lpL); lpR += 0.45f * (oR - lpR);   // dark repeats
            bufL[w] = flushDenorm(L[i] + lpR * fb); bufR[w] = flushDenorm(R[i] + lpL * fb);  // ping-pong
            L[i] += mix * oL; R[i] += mix * oR;
            if (++w >= size) w = 0;
        }
    }
};

struct Reverb {   // compact 4-comb + 2-allpass per channel
    struct Comb { std::vector<float> b; int i = 0; float lp = 0; };
    struct AP { std::vector<float> b; int i = 0; };
    Comb cL[4], cR[4]; AP aL[2], aR[2]; float sr = 44100;
    void prepare(float s) {
        sr = s;
        const int ct[4] = {1116, 1277, 1422, 1557}, at[2] = {556, 441};
        for (int k = 0; k < 4; ++k) {
            cL[k].b.assign((int)(ct[k] * s / 44100.f), 0); cR[k].b.assign((int)((ct[k] + 23) * s / 44100.f), 0);
        }
        for (int k = 0; k < 2; ++k) { aL[k].b.assign((int)(at[k] * s / 44100.f), 0); aR[k].b.assign((int)((at[k] + 11) * s / 44100.f), 0); }
    }
    static float comb(Comb& c, float x, float fb, float damp) {
        float y = c.b[c.i]; c.lp = flushDenorm(y + (c.lp - y) * damp);
        c.b[c.i] = flushDenorm(x + c.lp * fb); if (++c.i >= (int) c.b.size()) c.i = 0; return y;
    }
    static float ap(AP& a, float x) {
        float y = a.b[a.i]; a.b[a.i] = flushDenorm(x + y * 0.5f); if (++a.i >= (int) a.b.size()) a.i = 0; return y - x;
    }
    void process(float* L, float* R, int n, float mix, float size) {
        if (mix <= 0.001f) return;
        float fb = 0.70f + 0.27f * size, damp = 0.35f;
        for (int i = 0; i < n; ++i) {
            float in = (L[i] + R[i]) * 0.5f * 0.15f, oL = 0, oR = 0;
            for (int k = 0; k < 4; ++k) { oL += comb(cL[k], in, fb, damp); oR += comb(cR[k], in, fb, damp); }
            for (int k = 0; k < 2; ++k) { oL = ap(aL[k], oL); oR = ap(aR[k], oR); }
            L[i] += mix * oL * 1.2f; R[i] += mix * oR * 1.2f;
        }
    }
};

// DC blocker
struct DCBlock { float x1 = 0, y1 = 0; inline float p(float x) { float y = x - x1 + 0.995f * y1; x1 = x; y1 = flushDenorm(y); return y; } };

} // namespace rg
