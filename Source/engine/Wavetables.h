// RAYGUN wavetable bank: 1024-sample frames, 32 frames/table, band-limited mip levels.
#pragma once
#include "../ParamTable.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <vector>
#include <complex>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>

namespace rg {

constexpr int WT_N = 1024;       // samples per cycle
constexpr int WT_FRAMES = 32;    // frames per table
constexpr int WT_LEVELS = 9;     // mip levels: max harmonics 512 >> level

inline void fftInPlace(std::vector<std::complex<double>>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double ang = 2 * M_PI / (double) len * (inverse ? 1 : -1);
        std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= (double) n;
}

struct Table {
    // mip[level] holds WT_FRAMES * (WT_N + 1) floats (guard sample for interpolation)
    std::vector<float> mip[WT_LEVELS];
    bool ready = false;
};

class Wavetables {
public:
    static Wavetables& get() { static Wavetables w; return w; }

    const float* frame(int table, int level, int fr) const {
        return tables[table].mip[level].data() + (size_t) fr * (WT_N + 1);
    }

    // Provide raw int16 stem data (32 frames x 1024) for a TRILL table. Call before build().
    void setRawInt16(int table, const int16_t* data, size_t numSamples) {
        raw[table].assign(data, data + numSamples);
    }

    // Build every table (procedural, or from raw data). Idempotent.
    void build() {
        if (built) return;
        for (int t = 0; t < NTABLES; ++t) buildOne(t);
        built = true;
    }

private:
    Table tables[NTABLES];
    std::vector<int16_t> raw[NTABLES];
    bool built = false;

    using Spec = std::vector<double>; // harmonic amplitudes, index = harmonic number (1..511), phase handled by gen

    // Frame generators return harmonic amplitude + phase lists for frame position p in 0..1
    static void gen(int t, double p, std::vector<double>& amp, std::vector<double>& ph) {
        const int H = 511;
        amp.assign(H + 1, 0.0); ph.assign(H + 1, 0.0);
        auto set = [&](int h, double a, double f = 0) { if (h >= 1 && h <= H) { amp[h] = a; ph[h] = f; } };
        std::mt19937 rng(1234u + (unsigned) t * 77u);
        std::uniform_real_distribution<double> U(0, 2 * M_PI);
        switch (t) {
        case 0: // Sine -> Saw
            for (int h = 1; h <= 128; ++h) set(h, (h == 1 ? 1.0 : p * 1.0 / h) , 0);
            amp[1] = 1.0; break;
        case 1: { // Pulse width morph (sweeping duty 50%..5%)
            double d = 0.5 - 0.45 * p;
            for (int h = 1; h <= 200; ++h) set(h, std::fabs(std::sin(M_PI * h * d)) / h, M_PI * h * d);
            break; }
        case 2: { // Warm organ drawbars sweeping
            const int dr[9] = {1, 2, 3, 4, 6, 8, 10, 12, 16};
            for (int i = 0; i < 9; ++i) {
                double w = std::pow(std::max(0.0, 1.0 - std::fabs(p * 8 - i) / 3.5), 1.2) + (i < 3 ? 0.5 : 0);
                set(dr[i], w / (1 + 0.1 * i));
            }
            break; }
        case 3: { // Glass bell: inharmonic-ish partial clusters
            const double r[8] = {1, 2, 3, 4.2 / 1, 5.4 / 1, 6.8 / 1, 8.5, 11};
            for (int i = 0; i < 8; ++i) {
                int h = (int) std::lround(r[i]);
                set(h, (1.0 / (1 + i * 0.8)) * (0.4 + 0.6 * std::fabs(std::sin(M_PI * (p * (1 + i) + i * 0.3)))), U(rng));
            }
            break; }
        case 4: { // Chrome FM: operator ratio sweep via Bessel-ish spectrum
            double idx = 0.3 + p * 6.0; int ratio = 1 + (int)(p * 3);
            for (int k = 1; k <= 24; ++k) {
                double a = std::pow(idx / (idx + 1.0), k) / std::sqrt((double) k);
                set(k * ratio, a, (k & 1) ? 0 : M_PI);
                if (ratio > 1 && k == 1) set(1, 0.7);
            }
            break; }
        case 5: { // Vox formant: vowel morph a-e-i-o-u
            const double F[5][3] = {{730, 1090, 2440}, {530, 1840, 2480}, {270, 2290, 3010}, {570, 840, 2410}, {300, 870, 2240}};
            double pos = p * 4; int i0 = (int) pos; if (i0 > 3) i0 = 3; double fr = pos - i0;
            double f0 = 140.0;
            for (int h = 1; h <= 100; ++h) {
                double f = h * f0, a = 0;
                for (int k = 0; k < 3; ++k) {
                    double fc = F[i0][k] * (1 - fr) + F[i0 + 1][k] * fr;
                    a += std::exp(-0.5 * std::pow((f - fc) / (90 + 40 * k), 2)) / (1 + k);
                }
                set(h, a / std::sqrt((double) h) * 3.0, 0);
            }
            break; }
        case 6: { // Brass stack: odd-rich with rising brightness
            double bright = 0.25 + 0.75 * p;
            for (int h = 1; h <= 60; ++h) set(h, std::pow(bright, h * 0.12) / std::pow((double) h, 0.9), 0.3 * h * p);
            break; }
        case 7: { // String ensemble: detuned saw clusters -> smear by phase
            for (int h = 1; h <= 90; ++h) set(h, (1.0 / h) * (0.6 + 0.4 * std::sin(h * 0.5 + p * 6)), U(rng) * (0.2 + 0.8 * p));
            break; }
        case 8: { // Digital grit: decimated saw / odd-even mix
            for (int h = 1; h <= 120; ++h) { double a = 1.0 / h; if (h % 2 == 0) a *= (1 - p); set(h, a * (1 + 2 * p * (h % 7 == 0)), U(rng) * p); }
            break; }
        case 9: { // Hollow reed: clarinet-ish odd harmonics
            for (int h = 1; h <= 80; h += 2) set(h, std::pow(1.0 / h, 0.6 + 0.8 * (1 - p)), 0);
            for (int h = 2; h <= 40; h += 2) set(h, 0.1 * p / h, 0);
            break; }
        case 10: { // Metal pluck: comb-ish spectrum
            int c = 2 + (int)(p * 6);
            for (int h = 1; h <= 100; ++h) set(h, (1.0 / h) * (0.35 + 0.65 * (h % c == 0)), 0);
            break; }
        case 11: { // Sub heavy: fundamental + controlled 2nd/3rd
            set(1, 1.0); set(2, 0.5 * p); set(3, 0.35 * p * p); set(4, 0.2 * p * p);
            break; }
        default:
            set(1, 1.0);
        }
    }

    void finishLevels(Table& T, const std::vector<std::vector<std::complex<double>>>& spectra) {
        for (int L = 0; L < WT_LEVELS; ++L) {
            int maxH = 512 >> L; if (maxH < 2) maxH = 2;
            T.mip[L].assign((size_t) WT_FRAMES * (WT_N + 1), 0.f);
            for (int f = 0; f < WT_FRAMES; ++f) {
                std::vector<std::complex<double>> s(WT_N);
                for (int h = 1; h <= maxH && h < WT_N / 2; ++h) {
                    // gentle band-edge taper to reduce Gibbs ringing
                    double taper = (h > maxH * 0.8) ? 0.5 * (1 + std::cos(M_PI * (h - maxH * 0.8) / (maxH * 0.2 + 1e-9))) : 1.0;
                    s[h] = spectra[f][h] * taper;
                    s[WT_N - h] = std::conj(s[h]);
                }
                fftInPlace(s, true);
                float* dst = T.mip[L].data() + (size_t) f * (WT_N + 1);
                double pk = 0;
                for (int i = 0; i < WT_N; ++i) pk = std::max(pk, std::fabs(s[i].real()));
                (void) pk;
                for (int i = 0; i < WT_N; ++i) dst[i] = (float) s[i].real();
                dst[WT_N] = dst[0];
            }
        }
        // normalise each frame against its level-0 peak so all levels share one gain
        for (int f = 0; f < WT_FRAMES; ++f) {
            const float* d0 = T.mip[0].data() + (size_t) f * (WT_N + 1);
            float pk = 1e-9f; for (int i = 0; i < WT_N; ++i) pk = std::max(pk, std::fabs(d0[i]));
            float g = 0.95f / pk;
            for (int L = 0; L < WT_LEVELS; ++L) {
                float* d = T.mip[L].data() + (size_t) f * (WT_N + 1);
                for (int i = 0; i <= WT_N; ++i) d[i] *= g;
            }
        }
        T.ready = true;
    }

    void buildOne(int t) {
        std::vector<std::vector<std::complex<double>>> spectra(WT_FRAMES, std::vector<std::complex<double>>(WT_N / 2 + 1));
        if (!raw[t].empty() && raw[t].size() >= (size_t) WT_FRAMES * WT_N) {
            for (int f = 0; f < WT_FRAMES; ++f) {
                std::vector<std::complex<double>> a(WT_N);
                for (int i = 0; i < WT_N; ++i) a[i] = raw[t][(size_t) f * WT_N + i] / 32768.0;
                fftInPlace(a, false);
                for (int h = 0; h <= WT_N / 2; ++h) spectra[f][h] = a[h];
                spectra[f][0] = 0; // kill DC
            }
            // convert forward-FFT bins into the inverse-FFT convention used above (same convention: X[h]/N scaling)
            for (auto& fr : spectra) for (auto& c : fr) c /= 1.0;
            // inverse() divides by N already, forward doesn't scale; net result reproduces the original at full band.
        } else {
            std::vector<double> amp, ph;
            for (int f = 0; f < WT_FRAMES; ++f) {
                gen(t, f / (double) (WT_FRAMES - 1), amp, ph);
                for (int h = 1; h <= WT_N / 2 - 1; ++h) {
                    // sine-phase convention, X[h] = -i*A*N/2 * e^{i ph}
                    double A = (h < (int) amp.size()) ? amp[h] : 0.0, P = (h < (int) ph.size()) ? ph[h] : 0.0;
                    spectra[f][h] = std::polar(A * WT_N / 2.0, P - M_PI / 2);
                }
            }
        }
        finishLevels(tables[t], spectra);
    }
};

} // namespace rg
