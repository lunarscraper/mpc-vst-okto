/* =============================================================================
 * okto_shape.h - SHAPE: Okto's wavetable modulator (since 1.2.0).
 *
 * The idea comes from the wavetable envelopes / LFOs of the Polyend Tracker Community Edition
 * (morphable contours with a warp in X and Y, own contours loaded as WAV files); this is an own
 * implementation, no code of the Tracker firmware is used.
 *
 * A SHAPE table holds up to 8 contours (frames) of 256 points (+ the end point), all 0..1. MORPH moves through the
 * frames (linear crossfade between neighbours), the phase runs through a contour as an envelope
 * (ENV, once, holds the last point), a loop (LOOP), back and forth (PING) or free for all voices
 * (FREE; locked to the song position when synced and the transport runs).
 *
 *   16 factory tables x 8 frames = 128 contours, computed at load time (no data in the binary)
 *   16 user tables from WAV files in the folder okto_shapes (sorted by name): a Serum-style
 *      wavetable (2048-sample frames, or the size its "clm " chunk names) gives up to 8 of its
 *      frames, evenly spread; a short file (up to 4096 samples) is one contour. 8/16/24/32-bit
 *      integer or 32-bit float, the first channel. Each table is scaled to 0..1 as a whole.
 *
 * Read-only after shapes_init(); the audio thread only reads.
 * MIT license (see ../LICENSE).
 * ========================================================================== */
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>

namespace okto {

enum { SH_NPT = 256, SH_NFR = 8, SH_NFACT = 16, SH_NUSER = 16, SH_NTAB = SH_NFACT + SH_NUSER };
enum { SH_ENV, SH_LOOP, SH_PING, SH_FREE, SH_NMODES };

struct ShapeBank {
    float d[SH_NTAB][SH_NFR][SH_NPT + 1];   /* point SH_NPT: the end (x = 1) for ENV / PING; a loop wraps to point 0 */
    int nfr[SH_NTAB];
    char name[SH_NTAB][20];   /* user tables: the file name without .wav, "" = empty */
};
static inline ShapeBank &shape_bank() { static ShapeBank b; return b; }

static const char *const SHAPE_FACTORY_NAME[SH_NFACT] = {"DECAY", "SWELL", "LFO", "GATE", "TALK", "STEPS", "BOUNCE", "WOBBLE",
                                                         "RANDOM", "S&H", "PULSE", "SPIKE", "ACID", "RISE", "CHAOS", "RATCHET"};

namespace shp {
static inline float hump(float x) { return 0.5f - 0.5f * std::cos(6.28318530718f * x); }   /* 0 -> 1 -> 0 per unit */
static inline float fr(float x) { return x - std::floor(x); }
static inline float cl(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
struct R { uint32_t s; float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.0f / 16777216.0f); } };
/* a 16-step pattern ('x' = on, 'A' = accent, '.' = off): on for the first 85 % of a step (single hits, also back to back) */
static float gate16(const char *p, float x) {
    const float st = x * 16; const int i = (int)st % 16;
    return p[i] != '.' && st - std::floor(st) < 0.85f ? 1.0f : 0.0f;
}
static float acid16(const char *p, float x) {
    const float st = x * 16; const int i = (int)st % 16; const float t = st - std::floor(st);
    if (p[i] == '.') return 0;
    return (p[i] == 'A' ? 1.0f : 0.55f) * std::exp(-4.5f * t);
}
static float steps8(const int *lv, float x) { return lv[(int)(x * 8) % 8] / 7.0f; }

static float factory(int tab, int k, float x) {
    switch (tab) {
    case 0: { static const float P[8] = {1, 1.5f, 2.2f, 3, 4.5f, 6.5f, 10, 16}; return std::pow(1 - x, P[k]); }   /* DECAY */
    case 1: {   /* SWELL: slow to fast rises */
        static const float P[8] = {4, 2.5f, 1.6f, 1, 1.6f, 2.5f, 4, 8};
        return k < 4 ? std::pow(x, P[k]) : 1 - std::pow(1 - x, P[k]);
    }
    case 2:   /* LFO: sine, triangle, saw up, saw down, square, pulse 25 %, pulse 10 %, stepped sine */
        switch (k) {
        case 0: return hump(x);
        case 1: return x < 0.5f ? 2 * x : 2 - 2 * x;
        case 2: return x;
        case 3: return 1 - x;
        case 4: return x < 0.5f ? 1.0f : 0.0f;
        case 5: return x < 0.25f ? 1.0f : 0.0f;
        case 6: return x < 0.1f ? 1.0f : 0.0f;
        default: return hump(std::floor(x * 8) / 8 + 1.0f / 16);
        }
    case 3: {   /* GATE: trance gates, 16 steps */
        static const char *const G[8] = {"x.x.x.x.x.x.x.x.", "xx.xxx.xxx.xxx.x", "x.xxx.xx.xxx.x.x", "xxx.xx.xxx.xx.x.",
                                         "x..xx..xx..xx..x", "x.x..x.x..x.x..x", "xxxxxxxx.x.x.x.x", "x...x...x...x..."};
        return gate16(G[k], x);
    }
    case 4:   /* TALK: vowel movements - "wow", "ya-ya", "yoy", flutter */
        switch (k) {
        case 0: return hump(x);
        case 1: return x < 0.15f ? x / 0.15f : std::pow(1 - (x - 0.15f) / 0.85f, 1.5f);
        case 2: return hump(2 * x);
        case 3: return hump(2 * x) * (x < 0.5f ? 1.0f : 0.55f);
        case 4: return hump(3 * x) * (1 - 0.5f * x);
        case 5: return cl(0.25f + 0.5f * x + 0.25f * std::sin(6.28318530718f * 4 * x));
        case 6: return hump(8 * x) * std::pow(1 - x, 0.7f);
        default: return hump(4 * x) * (((int)(4 * x)) % 2 ? 0.5f : 1.0f);
        }
    case 5: {   /* STEPS: 8-step staircases and figures */
        static const int S[8][8] = {{0, 1, 2, 3, 4, 5, 6, 7}, {7, 6, 5, 4, 3, 2, 1, 0}, {0, 2, 4, 6, 7, 5, 3, 1}, {0, 5, 2, 7, 1, 6, 3, 4},
                                    {7, 0, 5, 0, 3, 0, 1, 0}, {0, 0, 4, 4, 7, 7, 4, 4}, {7, 6, 7, 4, 7, 2, 7, 0}, {3, 5, 3, 7, 3, 5, 3, 0}};
        return steps8(S[k], x);
    }
    case 6: {   /* BOUNCE: a ball, 2..9 bounces */
        const int nb = 2 + k;
        float h[12], d[12], sum = 0;
        for (int j = 0; j < nb; j++) { h[j] = std::pow(0.6f, (float)j); d[j] = std::sqrt(h[j]); sum += d[j]; }
        float t = x * sum;
        for (int j = 0; j < nb; j++) { if (t < d[j]) { const float u = t / d[j]; return h[j] * 4 * u * (1 - u); } t -= d[j]; }
        return 0;
    }
    case 7: { static const float C[8] = {1, 2, 3, 4, 6, 8, 12, 16}; return hump(C[k] * x); }   /* WOBBLE */
    case 8: case 9: case 14: return 0;   /* RANDOM, S&H, CHAOS: point lists, see build_factory */
    case 10: { static const float D[8] = {0.05f, 0.1f, 0.2f, 0.3f, 0.45f, 0.6f, 0.75f, 0.9f}; return fr(4 * x) < D[k] ? 1.0f : 0.0f; }   /* PULSE */
    case 11: { static const float C[8] = {1, 2, 3, 4, 6, 8, 12, 16}; return std::exp(-8 * fr(C[k] * x)); }   /* SPIKE */
    case 12: {   /* ACID: accented 16ths that decay */
        static const char *const A[8] = {"x.Ax.xA.x.xAx.Ax", "AxxAxxAxAxxAxxAx", "x.x.A.x.x.x.A.xA", "AxAxAxAxAxAxAxAx",
                                         "xA.xA.xAx.Ax.xAx", "A..xA..xA..xA.xx", "xxxAxxxAxxAxxxxA", "A.xxA.x.A.xxAxAx"};
        return acid16(A[k], x);
    }
    case 13:   /* RISE: risers, the last four with a tremolo that speeds up */
        if (k < 4) { static const float P[4] = {4, 2, 1, 0.5f}; return std::pow(x, P[k]); }
        else { const float m = (float)(4 << (k - 4)); return std::pow(x, 1.5f) * (0.35f + 0.65f * hump(m * x * x)); }
    default: {   /* RATCHET: hits that get denser towards the end */
        const float n = 2.0f + 2.0f * k, ph = n * std::pow(x, 1.0f + 0.2f * k);
        return std::exp(-6 * fr(ph));
    }
    }
}
}  // namespace shp

static void shapes_build_factory(ShapeBank &b) {
    using namespace shp;
    for (int t = 0; t < SH_NFACT; t++) {
        b.nfr[t] = SH_NFR;
        for (int k = 0; k < SH_NFR; k++) {
            float *y = b.d[t][k];
            if (t == 8 || t == 9 || t == 14) {   /* point lists */
                static const int NR[8] = {4, 4, 6, 6, 8, 8, 12, 16}, NS[8] = {4, 6, 8, 8, 12, 16, 24, 32}, NC[8] = {8, 12, 16, 16, 24, 32, 32, 32};
                const int n = t == 8 ? NR[k] : t == 9 ? NS[k] : NC[k];
                float pts[32];
                R r{0x9e3779b9u ^ (uint32_t)(t * 131 + k * 7919 + 1)};
                if (t == 14) {   /* CHAOS: the logistic map, r from 3.6 to 4.0 */
                    float xx = 0.31f + 0.05f * k; const float rr = 3.6f + 0.4f * k / 7.0f;
                    for (int j = 0; j < 20; j++) xx = rr * xx * (1 - xx);
                    for (int j = 0; j < n; j++) { xx = rr * xx * (1 - xx); pts[j] = xx; }
                } else for (int j = 0; j < n; j++) pts[j] = r.u();
                for (int i = 0; i <= SH_NPT; i++) {
                    const float p = (float)(i % SH_NPT) / SH_NPT * n;
                    const int j = (int)p % n;
                    const float u = p - std::floor(p);
                    if (t == 9) y[i] = pts[j];                                                          /* S&H: steps */
                    else if (t == 8) y[i] = pts[j] + (pts[(j + 1) % n] - pts[j]) * (0.5f - 0.5f * std::cos(3.14159265f * u));   /* smooth */
                    else y[i] = pts[j] + (pts[(j + 1) % n] - pts[j]) * u;                              /* CHAOS: lines */
                }
            } else
                for (int i = 0; i <= SH_NPT; i++) y[i] = cl(factory(t, k, i < SH_NPT ? (float)i / SH_NPT : 0.99999f));
        }
        std::snprintf(b.name[t], sizeof b.name[t], "%s", SHAPE_FACTORY_NAME[t]);
    }
}

/* ---- WAV import ------------------------------------------------------------------------------ */
static uint32_t shp_le(const uint8_t *p, int n) { uint32_t v = 0; for (int i = n - 1; i >= 0; i--) v = v << 8 | p[i]; return v; }
/* the first channel of a WAV as floats; frame = the frame size its "clm " chunk names (Serum), else 0 */
static bool shp_read_wav(const char *path, std::vector<float> &out, int &frame) {
    out.clear(); frame = 0;
    FILE *f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 44 || size > 16 * 1024 * 1024) { std::fclose(f); return false; }
    std::vector<uint8_t> b((size_t)size);
    const bool ok = std::fread(b.data(), 1, b.size(), f) == b.size();
    std::fclose(f);
    if (!ok || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) return false;
    int fmt = 0, ch = 0, bits = 0;
    const uint8_t *data = nullptr; size_t dlen = 0;
    for (size_t pos = 12; pos + 8 <= b.size();) {
        const uint8_t *c = b.data() + pos;
        const size_t len = shp_le(c + 4, 4);
        const size_t body = pos + 8, avail = b.size() - body, l = len < avail ? len : avail;
        if (!std::memcmp(c, "fmt ", 4) && l >= 16) {
            fmt = (int)shp_le(c + 8, 2); ch = (int)shp_le(c + 10, 2); bits = (int)shp_le(c + 22, 2);
            if (fmt == 0xFFFE && l >= 26) fmt = (int)shp_le(c + 32, 2);   /* extensible: the sub-format */
        } else if (!std::memcmp(c, "data", 4)) { data = c + 8; dlen = l; }
        else if (!std::memcmp(c, "clm ", 4) && l > 3) {   /* Serum: "<!>2048 ..." */
            char t[32] = {0}; std::memcpy(t, c + 8, l < 31 ? l : 31);
            if (!std::strncmp(t, "<!>", 3)) frame = std::atoi(t + 3);
        }
        pos = body + len + (len & 1);
    }
    if (!data || ch < 1 || (!(fmt == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) && !(fmt == 3 && bits == 32))) return false;
    const int bps = bits / 8, step = bps * ch;
    const size_t n = dlen / (size_t)step;
    out.resize(n);
    for (size_t i = 0; i < n; i++) {
        const uint8_t *p = data + i * step;
        float v;
        if (fmt == 3) { uint32_t u = shp_le(p, 4); std::memcpy(&v, &u, 4); }
        else if (bits == 8) v = (p[0] - 128) / 128.0f;
        else { int32_t s = (int32_t)(shp_le(p, bps) << (32 - bits)); v = (float)s / 2147483648.0f; }
        out[i] = std::isfinite(v) ? v : 0.0f;
    }
    return n > 0;
}
/* one user table from a WAV file: true if it holds anything */
static bool shapes_load_wav(ShapeBank &b, int t, const char *path) {
    std::vector<float> s; int frame;
    if (!shp_read_wav(path, s, frame)) return false;
    const int n = (int)s.size();
    if (frame <= 0 || frame > n) {
        if (n <= 4096) frame = n;
        else if (n % 2048 == 0) frame = 2048;
        else if (n % 256 == 0) frame = 256;
        else frame = n;
    }
    const int nf = n / frame;
    const int use = nf < SH_NFR ? nf : SH_NFR;
    if (use < 1) return false;
    for (int k = 0; k < use; k++) {
        const int src = use == 1 ? 0 : (int)std::lround((double)k * (nf - 1) / (use - 1));
        const float *fsrc = s.data() + (size_t)src * frame;
        for (int i = 0; i < SH_NPT; i++) {   /* box average when shrinking, linear when stretching */
            const double a = (double)i * frame / SH_NPT, z = (double)(i + 1) * frame / SH_NPT;
            float v;
            if (frame >= SH_NPT) {
                int i0 = (int)a, i1 = (int)z; if (i1 <= i0) i1 = i0 + 1; if (i1 > frame) i1 = frame;
                double sum = 0; for (int j = i0; j < i1; j++) sum += fsrc[j];
                v = (float)(sum / (i1 - i0));
            } else {
                const int j = (int)a; const float u = (float)(a - j);
                v = fsrc[j] + (fsrc[j + 1 < frame ? j + 1 : j] - fsrc[j]) * u;
            }
            b.d[t][k][i] = v;
        }
        b.d[t][k][SH_NPT] = fsrc[frame - 1];   /* the end: the frame's last sample */
    }
    float lo = 1e9f, hi = -1e9f;
    for (int k = 0; k < use; k++) for (int i = 0; i <= SH_NPT; i++) { lo = std::min(lo, b.d[t][k][i]); hi = std::max(hi, b.d[t][k][i]); }
    for (int k = 0; k < use; k++) for (int i = 0; i <= SH_NPT; i++) b.d[t][k][i] = hi - lo > 1e-6f ? (b.d[t][k][i] - lo) / (hi - lo) : 1.0f;
    b.nfr[t] = use;
    return true;
}
/* dir: the folder with the user WAVs ("" = none). Empty user tables are a flat 1 (no movement). */
static void shapes_init(const std::string &dir) {
    ShapeBank &b = shape_bank();
    shapes_build_factory(b);
    for (int t = SH_NFACT; t < SH_NTAB; t++) {
        b.nfr[t] = 1; b.name[t][0] = 0;
        for (int k = 0; k < SH_NFR; k++) for (int i = 0; i <= SH_NPT; i++) b.d[t][k][i] = 1.0f;
    }
    if (dir.empty()) return;
    std::vector<std::string> files;
    if (DIR *d = opendir(dir.c_str())) {
        while (dirent *e = readdir(d)) {
            std::string nm = e->d_name;
            if (nm.size() < 5 || nm[0] == '.') continue;
            std::string ext = nm.substr(nm.size() - 4);
            for (char &c : ext) c = (char)std::tolower((unsigned char)c);
            if (ext == ".wav") files.push_back(nm);
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end());
    int t = SH_NFACT;
    for (const std::string &nm : files) {
        if (t >= SH_NTAB) break;
        if (!shapes_load_wav(b, t, (dir + "/" + nm).c_str())) continue;
        std::string base = nm.substr(0, nm.size() - 4);
        for (char &c : base) c = (char)std::toupper((unsigned char)c);
        std::snprintf(b.name[t], sizeof b.name[t], "%s", base.c_str());
        t++;
    }
}

/* the contour value: table t, morph 0..1 over its frames, phase 0..1 (loop: periodic, else 1 = the last point) */
static inline float shape_at(const ShapeBank &b, int t, float morph, float ph, bool loop) {
    const int nf = b.nfr[t];
    const float mp = morph * (nf - 1);
    int f0 = (int)mp; if (f0 > nf - 1) f0 = nf - 1; if (f0 < 0) f0 = 0;
    const int f1 = f0 + 1 < nf ? f0 + 1 : f0;
    const float mf = mp - f0;
    const float x = ph * SH_NPT;
    int i0 = (int)x;
    float u = x - i0;
    int i1;
    if (loop) { i0 &= SH_NPT - 1; i1 = (i0 + 1) & (SH_NPT - 1); }
    else { if (i0 >= SH_NPT) { i0 = SH_NPT - 1; u = 1; } if (i0 < 0) { i0 = 0; u = 0; } i1 = i0 + 1; }
    const float *a = b.d[t][f0], *c = b.d[t][f1];
    const float va = a[i0] + (a[i1] - a[i0]) * u, vc = c[i0] + (c[i1] - c[i0]) * u;
    return va + (vc - va) * mf;
}

}  // namespace okto
