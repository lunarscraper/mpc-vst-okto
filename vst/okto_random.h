/* =============================================================================
 * okto_random.h - Okto's preset generator (PRESET tab: STYLE, VARIATION, MODULATION, RANDOM, UNDO;
 * the idea of carp 2000's random function, made for FM).
 *
 * Every STYLE has a home sound (an archetype). VARIATION 0 gives exactly that sound; the higher it
 * goes, the further the generator moves away from it: other ratios (from harmonic to inharmonic),
 * modulation indices, envelope times, waveforms, feedback, an extra modulator, new step patterns.
 * MODULATION decides how much moves on its own: step and loop envelopes on the modulators, the LFO
 * on levels and pitch, a pitch envelope, the mod wheel, and (since 1.2.0) SHAPE: a contour per style -
 * TALK says "wow" per note or in tempo on its formant carriers, SEQ / PAD / DRONE get a trance gate
 * or a slow scan, BASS / ACID accents on BRIGHT, FX a riser or chaos on the pitch, the rest a movement of
 * BRIGHT per note. MUTATE varies the sound that is playing (VARIATION = how far), ANY picks a style at random.
 *
 * Works in the parameters' own units (module.json): ui[k] for every key k of okto_params.h. The
 * caller fills ui[] with the start values (or the current sound for MUTATE) and writes the result
 * back. The level comes out matched: in true FM a sine carrier keeps its level whatever the
 * modulation does, so the loudness follows from the carriers' OUT, LEVEL and envelopes, and VOLUME
 * is set for about -20 dBFS rms per note, like the factory sounds.
 *
 * MIT license (see ../LICENSE).
 * ========================================================================== */
#pragma once
#include <cmath>
#include <cstdint>
#include "okto_params.h"
#include "okto_shape.h"

namespace oktornd {

enum Style { S_TALK, S_LEAD, S_BASS, S_ACID, S_STAB, S_PAD, S_PLUCK, S_BELL, S_SEQ, S_FX, S_DRONE, S_PERC,
             S_ANY, S_MUTATE, NSTYLES };
static const char *const STYLE_NAME[NSTYLES] = {"TALK", "LEAD", "BASS", "ACID", "STAB", "PAD", "PLUCK", "BELL",
                                                "SEQ", "FX", "DRONE", "PERC", "ANY", "MUTATE"};

/* option indices (module.json) */
enum { ES_FREE = 0, ES_32 = 1, ES_16T = 2, ES_16 = 3, ES_8T = 4, ES_8 = 5, ES_4T = 6, ES_4 = 7, ES_2 = 8, ES_1 = 9 };
enum { EM_ONCE, EM_LOOP, EM_STEP, EM_STEP_LOOP };
enum { LW_SINE, LW_TRI, LW_SAWD, LW_SAWU, LW_SQUARE, LW_SH };
enum { LS_FREE, LS_4_1, LS_2_1, LS_1_1, LS_1_2, LS_1_4, LS_1_8, LS_1_16, LS_1_4T, LS_1_8T, LS_1_16T };
enum { WV_SINE, WV_HALF, WV_ABS, WV_QUARTER, WV_SQUARE, WV_SAW };
/* SHAPE tables and modes (okto_shape.h) */
enum { ST_DECAY, ST_SWELL, ST_LFO, ST_GATE, ST_TALK, ST_STEPS, ST_BOUNCE, ST_WOBBLE, ST_RANDOM, ST_SH, ST_PULSE, ST_SPIKE,
       ST_ACID, ST_RISE, ST_CHAOS, ST_RATCHET };
enum { SM_ENV, SM_LOOP, SM_PING, SM_FREE };

/* the knob laws, inverse (tools/gen_okto.py has the same) */
static inline float u_level(float amp) {
    if (amp <= 0) return 0;
    float v = 100.0f * std::log(amp / 0.005f) / std::log(3200.0f);
    return v < 0.5f ? 0.5f : v > 100 ? 100 : v;
}
static inline float law_level(float ui) { return ui <= 0 ? 0.0f : 0.005f * std::pow(3200.0f, ui * 0.01f); }
static inline float u_time(float s) {
    if (s <= 0.001f) return 0;
    float v = 100.0f * std::log(s / 0.001f) / std::log(20000.0f);
    return v > 100 ? 100 : v;
}
static inline float law_time(float ui) { return ui <= 0 ? 0.0f : 0.001f * std::pow(20000.0f, ui * 0.01f); }
static inline float u_units(int n) { return (n - 1) * 100.0f / 15.0f; }   /* synced step length in divisions */
static inline float u_lfo(float hz) {
    float v = 100.0f * std::log(hz / 0.05f) / std::log(1280.0f);
    return v < 0 ? 0 : v > 100 ? 100 : v;
}
static inline float u_offset(float hz) { return hz == 0 ? 0.0f : std::copysign(100.0f * std::cbrt(std::fabs(hz) / 4000.0f), hz); }
static inline float u_glide(float s) { return s <= 0 ? 0.0f : 100.0f * std::log(s / 0.002f) / std::log(1000.0f); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 0x9e3779b9u) { for (int i = 0; i < 4; i++) next(); }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float u() { return (next() >> 8) * (1.0f / 16777216.0f); }            /* 0..1 */
    float b() { return u() * 2 - 1; }                                       /* -1..1 */
    bool p(float prob) { return u() < prob; }
    int i(int n) { return n <= 1 ? 0 : (int)(next() % (uint32_t)n); }
    float range(float lo, float hi) { return lo + (hi - lo) * u(); }
};

/* ---- the builder: musical units in, knob units out --------------------------- */
struct B {
    float *ui;
    explicit B(float *u) : ui(u) {}
    static int OK(int op, int f) { return OPR0 + op * NOPALL + f; }        /* op 0..7 */
    static int EK(int op, int e) { return OK(op, NOPF + e); }
    static int MK(int s, int d) { return MAT0 + s * 8 + d; }
    float &o(int op, int f) { return ui[OK(op, f)]; }
    float &e(int op, int f) { return ui[EK(op, f)]; }
    float &m(int s, int d) { return ui[MK(s, d)]; }
    float &g(int key) { return ui[key]; }

    void ratio(int op, float r) {
        if (r <= 0) { o(op, F_COARSE) = 0; o(op, F_FINE) = 0; return; }
        if (r < 1) { o(op, F_COARSE) = 1; o(op, F_FINE) = clampf((r - 0.5f) * 100, 0, 100); return; }
        int c = (int)std::floor(r + 1e-4f);
        if (c > 32) c = 32;
        o(op, F_COARSE) = (float)(c + 1);
        o(op, F_FINE) = clampf((r - c) * 100, 0, 100);
    }
    float ratio_of(int op) {
        const int c = (int)std::lround(o(op, F_COARSE));
        if (c <= 0) return 0;
        return (c == 1 ? 0.5f : c - 1.0f) + o(op, F_FINE) * 0.01f;
    }
    void fixed(int op, float hz) { o(op, F_COARSE) = 0; o(op, F_FINE) = 0; o(op, F_OFFSET) = u_offset(hz); }
    /* an operator: ratio, level (amplitude; as a modulator ~ the FM index), out 0..100 */
    void op(int n, float r, float level, float out, int wave = WV_SINE) {
        ratio(n, r);
        o(n, F_LEVEL) = u_level(level);
        o(n, F_OUT) = out;
        o(n, F_WAVE) = (float)wave;
    }
    /* envelope with free times: up to 8 (level %, seconds) */
    void env(int n, const float (*pts)[2], int np, int mode = EM_ONCE, int loop = 1, float rel = 0.3f) {
        e(n, E_EMODE) = (float)mode;
        e(n, E_EPTS) = (float)(np - 1);
        e(n, E_ELOOP) = (float)(loop - 1);
        e(n, E_ESYNC) = ES_FREE;
        e(n, E_EREL) = u_time(rel);
        for (int i = 0; i < np && i < 8; i++) { e(n, E_L1 + i) = pts[i][0]; e(n, E_T1 + i) = u_time(pts[i][1]); }
    }
    /* attack / decay to sustain / release */
    void adsr(int n, float a, float d, float s, float r) {
        const float p[2][2] = {{100, a}, {s, d}};
        env(n, p, 2, EM_ONCE, 1, r);
    }
    /* a step pattern in tempo divisions: levels %, each one division long */
    void steps(int n, const float *lv, int np, int sync, int mode = EM_STEP_LOOP, float rel = 0.15f, int units = 1) {
        e(n, E_EMODE) = (float)mode;
        e(n, E_EPTS) = (float)(np - 1);
        e(n, E_ELOOP) = 0;
        e(n, E_ESYNC) = (float)sync;
        e(n, E_EREL) = u_time(rel);
        for (int i = 0; i < np && i < 8; i++) { e(n, E_L1 + i) = clampf(lv[i], 0, 100); e(n, E_T1 + i) = u_units(units); }
    }
    bool is_carrier(int n) { return o(n, F_OUT) > 0.5f && o(n, F_LEVEL) > 0; }
    bool used(int n) {
        if (o(n, F_LEVEL) <= 0) return false;
        if (is_carrier(n)) return true;
        for (int d = 0; d < 8; d++) if (d != n && m(n, d) > 0.5f) return true;
        return false;
    }
    int free_op() { for (int n = 0; n < 8; n++) if (!used(n)) return n; return -1; }
};

/* the rest of the operators silent and unconnected, envelopes at the start values */
static void clear_ops(B &b) {
    for (int n = 0; n < 8; n++) { b.o(n, F_OUT) = 0; b.o(n, F_LEVEL) = 0; }
    for (int s = 0; s < 8; s++) for (int d = 0; d < 8; d++) b.m(s, d) = 0;
}

/* Formant Gate (PolyForce preset 12 / Serum "Formant Gate" by ultramarinedist): the step values of
 * Serum's LFO 2, the first eight sixteenths of the bar, scaled to 0..1 */
static const float FG_STEPS[8] = {0.0f, 0.314f, 0.157f, 0.6f, 0.453f, 0.761f, 1.0f, 0.6f};

/* a step pattern: v = how far from the home pattern; keeps a groove (an accent on 1, rests) */
static void pattern(Rng &r, float v, const float *home, int n, float *out) {
    for (int i = 0; i < n; i++) {
        float x = home ? home[i] : r.u();
        if (home && r.p(v)) x = r.u();
        out[i] = x;
    }
    if (!home || r.p(v)) {   /* a generated groove: an accent on the one, one or two rests */
        out[0] = r.p(0.5f) ? 1.0f : out[0];
        const int rests = r.i(3);
        for (int k = 0; k < rests; k++) out[1 + r.i(n - 1)] = 0;
    }
}

/* ---- the home sounds ------------------------------------------------------------ */
static void home_talk(B &b, Rng &r, float v) {
    /* The FM way to the formant gate (PolyForce "Formant Growl Gate"): a growling voice - ratio 2:1 FM with
     * a feedback modulator and a saw carrier, like the Growl wavetable - whose index opens and closes with a
     * 1/16 step pattern (the filter sweep of the original), gated by the same pattern, plus three formant
     * carriers on harmonics 5, 11 and 17 (~ 330, 720, 1100 Hz at C4 with TRANSPOSE -24) whose levels follow
     * the pattern as vowels: low steps "o", middle "a", high "e"/"i". */
    float pat[8];
    pattern(r, v, FG_STEPS, 8, pat);
    const int sync = r.p(v * 0.5f) ? (r.p(0.5f) ? ES_8 : ES_16T) : ES_16;
    float gate[8], open[8], f1[8], f2[8], f3[8];
    for (int i = 0; i < 8; i++) {
        const float x = pat[i];
        gate[i] = x < 0.08f ? 8.0f : 45.0f + 55.0f * x;    /* the gate: nearly closed on the lowest step */
        open[i] = 15.0f + 85.0f * x;                       /* the index: the "filter" opens with the step */
        f1[i] = gate[i] * (0.25f + 0.75f * (1.0f - x));    /* ~330 Hz: "o", "u" */
        f2[i] = gate[i] * (0.35f + 0.65f * (1.0f - std::fabs(x - 0.5f) * 2.0f));   /* ~720 Hz: "a" */
        f3[i] = gate[i] * (0.15f + 0.85f * x);             /* ~1.1 kHz: "e", "i" */
    }
    const float r2 = r.p(v * 0.6f) ? (r.p(0.5f) ? 1.0f : 3.0f) : 2.0f;
    b.op(0, 1, 1, 100, WV_SAW);             b.steps(0, gate, 8, sync);
    b.op(1, r2, 10.5f + 2.5f * v * r.b(), 0);       b.steps(1, open, 8, sync);
    const float fr[3] = {5, 11, 17};
    const float *fl[3] = {f1, f2, f3};
    for (int k = 0; k < 3; k++) {
        float ratio = fr[k];
        if (r.p(v * 0.7f)) ratio = clampf(fr[k] + (float)(r.i(5) - 2), 2, 24);
        b.op(2 + k, ratio, 1, 82 - k * 6);
        b.steps(2 + k, fl[k], 8, sync);
    }
    b.op(5, 1, 1.1f, 0);                    b.adsr(5, 0.003f, 0.3f, 90, 0.15f);   /* the formants' bandwidth */
    b.m(1, 0) = 100; b.m(1, 1) = 95;        /* 2 -> 1, feedback on 2: the growl */
    for (int k = 2; k < 5; k++) b.m(5, k) = 65;
    b.m(1, 2) = 35;                         /* a little of the growl in the lowest formant */
    b.g(G_VMODE) = 2; b.g(G_GLIDE) = u_glide(0.04f); b.g(G_TRANSPOSE) = 24 - 24;
    b.g(G_MW_BRIGHT) = 60;
}
static void home_lead(B &b, Rng &r, float v) {
    (void)r; (void)v;
    b.op(0, 1, 1, 100);     b.adsr(0, 0.004f, 0.4f, 85, 0.18f);
    b.op(1, 1, 2.6f, 0);    { const float p[2][2] = {{100, 0.002f}, {60, 0.5f}}; b.env(1, p, 2, EM_ONCE, 1, 0.2f); } b.o(1, F_VEL) = 40;
    b.op(2, 2, 1.6f, 0);    { const float p[2][2] = {{100, 0.001f}, {15, 0.15f}}; b.env(2, p, 2, EM_ONCE, 1, 0.1f); }
    b.m(1, 0) = 100; b.m(1, 1) = 55; b.m(2, 0) = 60;
    b.g(G_VMODE) = 2; b.g(G_GLIDE) = u_glide(0.06f); b.g(G_UNISON) = 1; b.g(G_DETUNE) = 10; b.g(G_WIDTH) = 60;
    b.g(G_MW_VIB) = 45; b.g(G_VIBRATO) = 4; b.g(G_LFO_RATE) = u_lfo(5.5f); b.g(G_LFO_DELAY) = 35;
}
static void home_bass(B &b, Rng &r, float v) {
    (void)r; (void)v;
    b.op(0, 1, 1, 100);       { const float p[3][2] = {{100, 0.001f}, {55, 0.06f}, {0, 0.09f}}; b.env(0, p, 3, EM_ONCE, 1, 0.03f); }
    b.op(1, 1, 1.8f, 0);      { const float p[2][2] = {{100, 0.0005f}, {0, 0.06f}}; b.env(1, p, 2, EM_ONCE, 1, 0.03f); } b.o(1, F_VEL) = 60;
    b.op(2, 0.5f, 1, 70);     { const float p[3][2] = {{100, 0.001f}, {60, 0.07f}, {0, 0.08f}}; b.env(2, p, 3, EM_ONCE, 1, 0.03f); }
    b.m(1, 0) = 100; b.m(1, 1) = 25;
    b.g(G_VMODE) = 1; b.g(G_TRANSPOSE) = 24 - 12;
}
static void home_acid(B &b, Rng &r, float v) {
    b.op(0, 1, 1, 100);       { const float p[2][2] = {{100, 0.001f}, {75, 0.3f}}; b.env(0, p, 2, EM_ONCE, 1, 0.08f); }
    b.op(1, 1, 2.8f, 0);      { const float p[2][2] = {{100, 0.001f}, {12, 0.2f}}; b.env(1, p, 2, EM_ONCE, 1, 0.08f); } b.o(1, F_VEL) = 70;
    b.op(2, 0.5f, 1, 55);     { const float p[2][2] = {{100, 0.001f}, {80, 0.3f}}; b.env(2, p, 2, EM_ONCE, 1, 0.08f); }
    b.m(1, 0) = 100; b.m(1, 1) = 70;
    if (r.p(v * 0.5f)) {   /* the squelch as a looping ramp instead of a decay */
        const float p[4][2] = {{100, 1}, {10, 1}, {50, 1}, {5, 1}};
        b.env(1, p, 4, EM_LOOP);
        b.e(1, E_ESYNC) = ES_32;
        for (int i = 0; i < 4; i++) b.e(1, E_T1 + i) = u_units(1);
    }
    b.g(G_VMODE) = 1; b.g(G_GLIDE) = u_glide(0.04f); b.g(G_TRANSPOSE) = 24 - 12; b.g(G_MW_BRIGHT) = 60;
}
static void home_stab(B &b, Rng &r, float v) {
    (void)r; (void)v;
    const float c[3][2] = {{100, 0.001f}, {40, 0.15f}, {0, 0.3f}}, mm[2][2] = {{100, 0.001f}, {20, 0.15f}};
    b.op(0, 1, 1, 100);       b.env(0, c, 3, EM_ONCE, 1, 0.1f);
    b.op(1, 1, 2.2f, 0);      b.env(1, mm, 2, EM_ONCE, 1, 0.1f);
    b.op(2, 1.5f, 1, 80);     b.env(2, c, 3, EM_ONCE, 1, 0.1f);
    b.op(3, 1.5f, 2.0f, 0);   b.env(3, mm, 2, EM_ONCE, 1, 0.1f);
    b.m(1, 0) = 100; b.m(3, 2) = 100; b.m(1, 1) = 40; b.m(3, 3) = 40;
    b.g(G_UNISON) = 2; b.g(G_DETUNE) = 18; b.g(G_WIDTH) = 100;
}
static void home_pad(B &b, Rng &r, float v) {
    (void)r; (void)v;
    { const float p[2][2] = {{100, 1.2f}, {80, 1.5f}}; b.op(0, 1, 1, 100); b.env(0, p, 2, EM_ONCE, 1, 2.0f); } b.o(0, F_PAN) = -30;
    { const float p[2][2] = {{100, 1.0f}, {60, 2.0f}}; b.op(1, 2, 0.8f, 0); b.env(1, p, 2, EM_ONCE, 1, 2.0f); } b.o(1, F_AM) = 20;
    { const float p[2][2] = {{100, 1.5f}, {70, 2.0f}}; b.op(2, 4, 1, 45); b.env(2, p, 2, EM_ONCE, 1, 2.0f); } b.o(2, F_PAN) = 30;
    b.o(2, F_OFFSET) = u_offset(0.6f);
    { const float p[2][2] = {{100, 2.0f}, {40, 2.0f}}; b.op(3, 7, 0.6f, 0); b.env(3, p, 2, EM_ONCE, 1, 2.0f); }
    b.m(1, 0) = 100; b.m(3, 2) = 100;
    b.g(G_UNISON) = 1; b.g(G_DETUNE) = 10; b.g(G_WIDTH) = 70; b.g(G_LFO_RATE) = u_lfo(0.3f);
}
static void home_pluck(B &b, Rng &r, float v) {
    (void)r; (void)v;
    { const float p[2][2] = {{100, 0.001f}, {0, 0.5f}}; b.op(0, 1, 1, 100); b.env(0, p, 2, EM_ONCE, 1, 0.25f); }
    { const float p[2][2] = {{100, 0.0005f}, {0, 0.18f}}; b.op(1, 2, 2.0f, 0); b.env(1, p, 2, EM_ONCE, 1, 0.1f); } b.o(1, F_VEL) = 80;
    { const float p[2][2] = {{100, 0.001f}, {0, 0.35f}}; b.op(2, 3, 1, 35); b.env(2, p, 2, EM_ONCE, 1, 0.2f); }
    b.m(1, 0) = 100; b.m(1, 2) = 50; b.m(1, 1) = 30;
}
static void home_bell(B &b, Rng &r, float v) {
    (void)r; (void)v;
    { const float p[3][2] = {{100, 0.002f}, {40, 1.2f}, {0, 3.5f}}; b.op(0, 1, 1, 100); b.env(0, p, 3, EM_ONCE, 1, 1.5f); } b.o(0, F_PAN) = -25;
    { const float p[3][2] = {{100, 0.001f}, {30, 1.5f}, {0, 2.5f}}; b.op(1, 3.5f, 1.8f, 0); b.env(1, p, 3, EM_ONCE, 1, 1.5f); } b.o(1, F_VEL) = 80;
    { const float p[3][2] = {{100, 0.002f}, {30, 0.8f}, {0, 2.5f}}; b.op(2, 2, 1, 70); b.env(2, p, 3, EM_ONCE, 1, 1.2f); } b.o(2, F_PAN) = 25;
    b.o(2, F_OFFSET) = u_offset(0.7f);
    { const float p[2][2] = {{100, 0.001f}, {0, 0.9f}}; b.op(3, 7, 1.2f, 0); b.env(3, p, 2, EM_ONCE, 1, 1.0f); }
    b.m(1, 0) = 100; b.m(3, 2) = 100;
}
static void home_seq(B &b, Rng &r, float v) {
    /* hold a key: the brightness walks through an 8-step pattern (TIMBRE SEQ) */
    static const float home[8] = {1.0f, 0.2f, 0.7f, 0.05f, 0.9f, 0.4f, 0.6f, 0.1f};
    float pat[8], lv[8];
    pattern(r, v, home, 8, pat);
    for (int i = 0; i < 8; i++) lv[i] = 100 * pat[i];
    b.op(0, 1, 1, 100);       b.adsr(0, 0.005f, 0.2f, 90, 0.3f);
    b.op(1, 2, 2.5f, 0);      b.steps(1, lv, 8, ES_16);
    { const float p[5][2] = {{0, 1}, {100, 2}, {30, 1}, {80, 1}, {0, 1}};
      b.op(2, 3, 1.5f, 0); b.env(2, p, 5, EM_LOOP); b.e(2, E_ESYNC) = ES_16;
      for (int i = 0; i < 5; i++) b.e(2, E_T1 + i) = u_units((int)p[i][1]); }
    b.m(1, 0) = 100; b.m(2, 1) = 70; b.m(1, 1) = 25;
    if (r.p(0.3f + v * 0.4f)) {   /* a gate on the carrier as well (trancegate) */
        float g[8];
        for (int i = 0; i < 8; i++) g[i] = r.p(0.7f) ? 100.0f : 0.0f;
        g[0] = 100;
        b.steps(0, g, 8, ES_16, EM_STEP_LOOP, 0.2f);
    }
}
static void home_fx(B &b, Rng &r, float v) {
    (void)v;
    b.op(0, 1, 1, 100);       { const float p[2][2] = {{100, 0.001f}, {0, 0.3f + r.u() * 0.6f}}; b.env(0, p, 2, EM_ONCE, 1, 0.1f); }
    b.op(1, 3.5f, 4.0f, 0);   { const float p[2][2] = {{100, 0.001f}, {20, 0.25f}}; b.env(1, p, 2, EM_ONCE, 1, 0.1f); }
    b.m(1, 0) = 100; b.m(1, 1) = 80;
    b.ui[PE0 + E_EMODE] = EM_ONCE; b.ui[PE0 + E_EPTS] = 1;
    b.ui[PE0 + E_L1] = 100; b.ui[PE0 + E_T1] = u_time(0.001f);
    b.ui[PE0 + E_L2] = r.p(0.3f) ? -100.0f : 0.0f; b.ui[PE0 + E_T2] = u_time(0.12f);
    b.g(G_PE_DEPTH) = 36;
}
static void home_drone(B &b, Rng &r, float v) {
    (void)r; (void)v;
    const float pans[4] = {-60, 60, -20, 20}, rat[4] = {1, 1.5f, 2, 3}, outs[4] = {100, 70, 60, 40}, offs[4] = {-0.4f, 0.3f, 0.9f, -0.7f};
    for (int k = 0; k < 4; k++) {
        const float p[2][2] = {{100, 2.0f + 0.5f * k}, {90, 1.0f}};
        b.op(k, rat[k], 1, outs[k]); b.env(k, p, 2, EM_ONCE, 1, 3.0f);
        b.o(k, F_PAN) = pans[k]; b.o(k, F_OFFSET) = u_offset(offs[k]);
    }
    { const float p[3][2] = {{0, 3.0f}, {100, 4.0f}, {30, 3.0f}}; b.op(4, 0.5f, 1.0f, 0); b.env(4, p, 3, EM_LOOP, 1, 3.0f); }
    b.m(4, 0) = 100; b.m(4, 1) = 80; b.m(4, 2) = 60; b.m(4, 3) = 50;
    b.g(G_LFO_RATE) = u_lfo(0.15f);
}
static void home_perc(B &b, Rng &r, float v) {
    (void)v;
    if (r.p(0.5f)) {   /* a kick */
        b.op(0, 1, 1, 100);   { const float p[3][2] = {{100, 0.001f}, {70, 0.08f}, {0, 0.3f}}; b.env(0, p, 3, EM_ONCE, 1, 0.05f); }
        b.fixed(1, 3000); b.o(1, F_LEVEL) = u_level(1); b.o(1, F_OUT) = 25; b.o(1, F_PENV) = 0;
        { const float p[2][2] = {{100, 0.0003f}, {0, 0.006f}}; b.env(1, p, 2, EM_ONCE, 1, 0.01f); }
        b.op(2, 1, 0.5f, 0);  { const float p[2][2] = {{100, 0.0005f}, {0, 0.03f}}; b.env(2, p, 2, EM_ONCE, 1, 0.02f); }
        b.m(2, 0) = 100;
        b.ui[PE0 + E_EMODE] = EM_ONCE; b.ui[PE0 + E_EPTS] = 1;
        b.ui[PE0 + E_L1] = 100; b.ui[PE0 + E_T1] = u_time(0.001f); b.ui[PE0 + E_L2] = 0; b.ui[PE0 + E_T2] = u_time(0.07f);
        b.g(G_PE_DEPTH) = 36; b.g(G_VMODE) = 1; b.g(G_TRANSPOSE) = 0;
    } else {           /* a metallic hit: fixed operators in feedback */
        const float f1 = 2000 + r.u() * 2500, f2 = 1800 + r.u() * 2200;
        b.fixed(0, f1); b.o(0, F_LEVEL) = u_level(1); b.o(0, F_OUT) = 100; b.o(0, F_PENV) = 0;
        { const float p[2][2] = {{100, 0.0005f}, {0, 0.05f + r.u() * 0.2f}}; b.env(0, p, 2, EM_ONCE, 1, 0.04f); }
        b.fixed(1, f2); b.o(1, F_LEVEL) = u_level(3 + r.u() * 4); b.o(1, F_PENV) = 0;
        { const float p[2][2] = {{100, 0.0005f}, {60, 0.07f}}; b.env(1, p, 2, EM_ONCE, 1, 0.04f); }
        b.m(1, 0) = 100; b.m(1, 1) = 100;
    }
}

/* ---- VARIATION: away from the home sound ---------------------------------------- */
static const float MOD_RATIOS[] = {0.5f, 1, 1, 2, 2, 3, 4, 5, 7};
static const float ODD_RATIOS[] = {1.41f, 2.76f, 3.5f, 4.2f, 5.19f, 6.3f, 7.13f, 9.5f, 11.3f};

static void vary(B &b, Rng &r, float v, int style) {
    if (v <= 0) return;
    const bool tonal = style != S_PERC && style != S_FX && style != S_BELL;
    for (int n = 0; n < 8; n++) {
        if (!b.used(n)) continue;
        const bool car = b.is_carrier(n);
        const bool fixed = b.o(n, F_COARSE) < 0.5f;
        if (!car && !fixed && r.p(v * 0.55f)) {   /* a modulator's ratio: harmonic, at high VARIATION also odd ones */
            const float x = (v > 0.55f && r.p((v - 0.55f) * 1.4f)) ? ODD_RATIOS[r.i(9)] : MOD_RATIOS[r.i(9)];
            b.ratio(n, x);
        }
        if (car && !fixed && n > 0 && r.p(v * (tonal ? 0.25f : 0.5f)))   /* a second carrier: another harmonic */
            b.ratio(n, (float)(1 + r.i(tonal ? 4 : 8)) * (r.p(0.2f) ? 0.5f : 1.0f));
        if (!car) {                               /* the index */
            const float amp = law_level(b.o(n, F_LEVEL)) * std::exp(1.1f * v * r.b());
            b.o(n, F_LEVEL) = u_level(clampf(amp, 0.15f, 14.0f));
            if (r.p(v * 0.3f)) b.o(n, F_WAVE) = (float)(r.p(0.5f) ? WV_SINE : 1 + r.i(5));
        } else if (r.p(v * 0.2f)) {
            b.o(n, F_WAVE) = (float)(r.p(0.6f) ? WV_SINE : (r.p(0.5f) ? WV_SAW : WV_SQUARE));
        }
        if (!fixed && r.p(v * 0.25f)) b.o(n, F_OFFSET) = u_offset(r.b() * 2.5f * v);   /* beating */
        /* envelope times: free ones stretch or shrink, synced steps stay on the grid */
        if (b.e(n, E_ESYNC) < 0.5f) {
            const int np = (int)std::lround(b.e(n, E_EPTS)) + 1;
            const float k = std::exp(0.9f * v * r.b());
            for (int i = 0; i < np; i++) b.e(n, E_T1 + i) = u_time(clampf(law_time(b.e(n, E_T1 + i)) * k, 0.0005f, 12));
        }
        const float fb = b.m(n, n);
        if (!car && r.p(v * 0.5f)) b.m(n, n) = clampf(fb + 60 * v * r.b(), 0, 100);
    }
    if (r.p(v * 0.5f)) {                          /* one more modulator, on a carrier or on a modulator */
        const int f = b.free_op();
        int tgt[8], nt = 0;
        for (int n = 0; n < 8; n++) if (b.used(n)) tgt[nt++] = n;
        if (f >= 0 && nt > 0) {
            const int t = tgt[r.i(nt)];
            b.op(f, (v > 0.6f && r.p(0.5f)) ? ODD_RATIOS[r.i(9)] : MOD_RATIOS[r.i(9)], 0.4f + r.u() * 1.8f * v, 0);
            if (r.p(0.5f)) b.adsr(f, 0.001f + r.u() * 0.05f, 0.05f + r.u() * 0.8f, r.u() * 80, 0.2f);
            else b.adsr(f, 0.002f, 0.3f, 100, 0.3f);
            b.m(f, t) = 50 + 50 * r.u();
        }
    }
    if (style != S_PERC && r.p(v * 0.3f)) {       /* voice: unison, detune, width */
        b.g(G_UNISON) = (float)r.i(3);
        b.g(G_DETUNE) = clampf(b.g(G_DETUNE) + 20 * v * r.b(), 0, 45);
        b.g(G_WIDTH) = clampf(b.g(G_WIDTH) + 50 * v * r.b(), 0, 100);
    }
}

/* ---- SHAPE (1.2.0): a contour that fits the style ------------------------------- */
static void shape_set(B &b, int table, int mode, int sync, int units, float morph) {
    b.g(H_TABLE) = (float)table; b.g(H_MODE) = (float)mode; b.g(H_SYNC) = (float)sync;
    b.g(H_RATE) = u_units(units); b.g(H_MORPH) = clampf(morph, 0, 100);
}
static void shape(B &b, Rng &r, float m, int style, const int *cars, int nc) {
    const float prob = style == S_TALK ? 0.45f + 0.5f * m : style == S_PERC ? 0.0f : 0.15f + 0.5f * m;
    if (nc <= 0 || !r.p(prob)) return;
    const float morph = 100 * r.u();
    switch (style) {
    case S_TALK: {   /* the formant carriers (all but the first) move against each other: a vowel change */
        if (r.p(0.5f)) { shape_set(b, ST_TALK, SM_ENV, ES_FREE, 1, morph); b.g(H_RATE) = 100 * std::log((0.25f + 0.6f * r.u()) / 0.01f) / std::log(2000.0f); }   /* 0.25 .. 0.85 s */
        else { static const int U[] = {4, 8, 2, 16}; shape_set(b, ST_TALK, SM_LOOP, ES_16, U[r.i(4)], morph); }
        b.g(H_RATE) = clampf(b.g(H_RATE), 0, 100);
        for (int k = 1; k < nc; k++) b.ui[SHOP0 + cars[k] * 2] = (k % 2 ? 1.0f : -1.0f) * (40 + 50 * r.u());
        if (r.p(0.5f)) b.g(H_BRIGHT) = 20 + 40 * r.u();
        b.g(H_VEL) = r.p(0.4f) ? 30 * r.u() : 0.0f;
        break;
    }
    case S_SEQ: case S_PAD: case S_DRONE: {
        if ((style == S_PAD || style == S_DRONE) && r.p(0.5f)) {   /* a slow scan on BRIGHT */
            shape_set(b, r.p(0.5f) ? ST_RANDOM : ST_WOBBLE, SM_FREE, ES_1, 1 + r.i(4), morph * 0.4f);
            b.g(H_BRIGHT) = 30 + 50 * r.u();
            b.g(H_LFO) = r.p(0.5f) ? 30 * r.u() : 0.0f;
        } else {   /* a trance gate locked to the bar */
            static const int T[] = {ST_GATE, ST_GATE, ST_PULSE, ST_ACID, ST_RATCHET};
            shape_set(b, T[r.i(5)], SM_FREE, ES_16, 16, morph);
            for (int k = 0; k < nc; k++) b.ui[SHOP0 + cars[k] * 2] = 70 + 30 * r.u();
        }
        break;
    }
    case S_BASS: case S_ACID:   /* accents on the index */
        shape_set(b, r.p(0.6f) ? ST_ACID : ST_WOBBLE, SM_FREE, ES_16, 16, morph);
        b.g(H_BRIGHT) = 30 + 50 * r.u();
        break;
    case S_FX:   /* a riser or chaos on the pitch */
        if (r.p(0.5f)) shape_set(b, ST_RISE, SM_ENV, ES_FREE, 1, morph);
        else shape_set(b, r.p(0.5f) ? ST_CHAOS : ST_BOUNCE, SM_LOOP, ES_16, 4 + 4 * r.i(3), morph);
        b.g(H_RATE) = b.g(H_SYNC) > 0.5f ? b.g(H_RATE) : 30 + 50 * r.u();
        b.g(H_PITCH) = (r.p(0.5f) ? 1.0f : -1.0f) * (20 + 30 * r.u());
        break;
    default: {   /* LEAD, STAB, PLUCK, BELL, MUTATE: a movement of the index per note */
        static const int T[] = {ST_TALK, ST_DECAY, ST_BOUNCE, ST_SPIKE, ST_SWELL};
        shape_set(b, T[r.i(5)], SM_ENV, ES_FREE, 1, morph);
        b.g(H_RATE) = 20 + 50 * r.u();
        b.g(H_BRIGHT) = (r.p(0.75f) ? 1.0f : -1.0f) * (20 + 40 * r.u());
        b.g(H_VEL) = r.p(0.5f) ? 40 * r.u() : 0.0f;
        break;
    }
    }
    if (r.p(0.3f)) b.g(H_WARPX) = 60 * r.b();
    if (r.p(0.3f)) b.g(H_WARPY) = 60 * r.b();
}

/* ---- MODULATION: what moves by itself ---------------------------------------- */
static void modulate(B &b, Rng &r, float m, int style) {
    int mods[8], nm = 0, cars[8], nc = 0;
    for (int n = 0; n < 8; n++) if (b.used(n)) { if (b.is_carrier(n)) cars[nc++] = n; else mods[nm++] = n; }
    const bool perc = style == S_PERC;
    if (m <= 0) {
        for (int n = 0; n < 8; n++) { b.o(n, F_AM) = 0; b.o(n, F_PM) = 0; }
        b.g(G_VIBRATO) = 0;
        return;
    }
    /* the LFO: a tempo division most of the time, S&H for the talking kind */
    static const int SY[] = {LS_1_16, LS_1_8, LS_1_4, LS_1_2, LS_1_1, LS_1_8T, LS_1_16T, LS_2_1};
    if (r.p(0.75f)) b.g(G_LFO_SYNC) = (float)SY[r.i(8)];
    else { b.g(G_LFO_SYNC) = LS_FREE; b.g(G_LFO_RATE) = u_lfo(0.2f + r.u() * 7); }
    b.g(G_LFO_WAVE) = (float)((style == S_TALK || style == S_SEQ) && r.p(0.6f) ? LW_SH : r.i(6));
    b.g(G_LFO_KEY) = r.p(0.5f) ? 1.0f : 0.0f;
    for (int k = 0; k < nm; k++) {
        const int n = mods[k];
        if (r.p(m * 0.6f)) b.o(n, F_AM) = clampf(20 + 80 * m * r.u(), 0, 100);
        /* a modulator becomes a step sequence or a looping shape in tempo */
        if (!perc && b.e(n, E_ESYNC) < 0.5f && r.p(m * 0.55f)) {
            float lv[8];
            const int np = 4 + r.i(5);
            pattern(r, 1, nullptr, np, lv);
            for (int i = 0; i < np; i++) lv[i] = 10 + 90 * lv[i];
            static const int ES[] = {ES_16, ES_16, ES_8, ES_32, ES_16T, ES_8T};
            const int sync = ES[r.i(6)];
            if (r.p(0.6f)) b.steps(n, lv, np, sync);
            else { b.steps(n, lv, np, sync, EM_LOOP); }
        }
    }
    for (int k = 0; k < nc; k++)
        if (!perc && r.p(m * 0.25f)) b.o(cars[k], F_PM) = 10 + 30 * m * r.u();   /* a little drift */
    if (!perc && style != S_PAD && style != S_DRONE && r.p(m * 0.3f)) {          /* a pitch blip on the attack */
        b.ui[PE0 + E_EMODE] = EM_ONCE; b.ui[PE0 + E_EPTS] = 1;
        b.ui[PE0 + E_L1] = r.p(0.5f) ? 100.0f : -100.0f; b.ui[PE0 + E_T1] = u_time(0.001f);
        b.ui[PE0 + E_L2] = 0; b.ui[PE0 + E_T2] = u_time(0.02f + 0.15f * r.u());
        b.g(G_PE_DEPTH) = (float)(2 + r.i(12));
    }
    if (!perc) {
        if (r.p(0.4f + 0.4f * m)) b.g(G_VIBRATO) = r.p(0.5f) ? 0.0f : 5 + 15 * m * r.u();
        b.g(G_LFO_DELAY) = r.p(0.5f) ? 20 + 50 * r.u() : 0.0f;
    }
    b.g(G_MW_BRIGHT) = clampf(30 + 50 * r.u(), 0, 100);
    shape(b, r, m, style, cars, nc);
}

/* ---- the level: about -20 dBFS rms for one note, like the factory sounds ----------- */
/* The envelope played through on paper (2 s held, 120 BPM, 4 ms steps): the rms of its loudest 0.5 s. */
static float env_loudest(B &b, int n) {
    static const float SYNC_BEATS[10] = {0, 0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.5f, 2.0f / 3, 1, 2, 4};
    const int np = (int)clampf((float)std::lround(b.e(n, E_EPTS)) + 1, 1, 8);
    const int mode = (int)std::lround(b.e(n, E_EMODE));
    const int loop = (int)clampf((float)std::lround(b.e(n, E_ELOOP)), 0, (float)(np - 1));
    const int sy = (int)clampf((float)std::lround(b.e(n, E_ESYNC)), 0, 9);
    const float tscale = std::pow(4.0f, b.g(G_ENV_TIME) * 0.01f);
    float dur[8], lv[8];
    for (int i = 0; i < np; i++) {
        lv[i] = clampf(b.e(n, E_L1 + i) * 0.01f, 0, 1);
        const float t = b.e(n, E_T1 + i);
        dur[i] = sy ? (1 + (int)std::lround(t * 0.15f)) * SYNC_BEATS[sy] * 0.5f : law_time(t) * tscale;
        if (dur[i] < 0.0005f) dur[i] = 0.0005f;
    }
    const bool stepped = mode == EM_STEP || mode == EM_STEP_LOOP, loops = mode == EM_LOOP || mode == EM_STEP_LOOP;
    enum { N = 500 };                      /* 2 s in 4 ms */
    float sq[N];
    int seg = 0; float from = 0, t = 0; bool done = false;
    for (int k = 0; k < N; k++) {
        float x;
        if (done) x = lv[np - 1];
        else if (stepped) x = lv[seg];
        else x = from + (lv[seg] - from) * clampf(t / dur[seg], 0, 1);
        sq[k] = x * x;
        t += 0.004f;
        for (int guard = 0; !done && t >= dur[seg] && guard < 64; guard++) {
            t -= dur[seg]; from = lv[seg]; seg++;
            if (seg >= np) { if (loops) seg = loop; else { seg = np - 1; done = true; } }
        }
    }
    float best = 0, sum = 0;
    const int w = 125;                     /* 0.5 s */
    for (int k = 0; k < N; k++) {
        sum += sq[k];
        if (k >= w) sum -= sq[k - w];
        if (k >= w - 1 && sum > best) best = sum;
    }
    return std::sqrt(best / w);
}
/* SHAPE on a carrier's level: the rms of the gain over one pass of the contour (a sparse gate keeps little) */
static float shape_gain(B &b, int n) {
    const float a = b.ui[SHOP0 + n * 2] * 0.01f;
    if (a == 0) return 1;
    const okto::ShapeBank &sb = okto::shape_bank();
    const int t = (int)clampf((float)std::lround(b.g(H_TABLE)), 0, okto::SH_NTAB - 1);
    const float m = clampf(b.g(H_MORPH) * 0.01f, 0, 1);
    float sum = 0;
    for (int i = 0; i < 64; i++) {
        const float s = okto::shape_at(sb, t, m, i / 64.0f, true);
        const float g = a > 0 ? 1 - a * (1 - s) : 1 + a * s;
        sum += g * g;
    }
    return std::sqrt(sum / 64) + 0.02f;
}
static void level(B &b) {
    /* carriers on one pitch (same ratio, no beating offset) add up in amplitude, groups on different pitches
     * in power */
    float grp_amp[8] = {0}, grp_key[8];
    int ng = 0;
    for (int n = 0; n < 8; n++) {
        if (!b.is_carrier(n)) continue;
        const float o = b.o(n, F_OUT) * 0.01f;
        const float a = law_level(b.o(n, F_LEVEL)) * o * o * env_loudest(b, n) * shape_gain(b, n);
        const float key = std::fabs(b.o(n, F_OFFSET)) > 0.5f ? 1000.0f + n : b.ratio_of(n) + 0.001f * b.o(n, F_COARSE);
        int g = 0;
        while (g < ng && std::fabs(grp_key[g] - key) > 1e-4f) g++;
        if (g == ng) { grp_key[ng] = key; grp_amp[ng] = 0; ng++; }
        grp_amp[g] += a;
    }
    float sum = 0;
    for (int g = 0; g < ng; g++) sum += grp_amp[g] * grp_amp[g];
    const float CAL = 0.72f;               /* measured: the rms of the sounds against this estimate (host_test 9b) */
    const float est = std::sqrt(sum) * 0.70710678f * CAL;
    if (est <= 1e-6f) { b.g(G_VOLUME) = 50; return; }
    const float target = 0.1f;             /* -20 dBFS rms */
    /* volume law: 0.8 * (VOLUME / 100)^2 */
    b.g(G_VOLUME) = clampf(100.0f * std::sqrt(target / (0.8f * est)), 5, 92);
}

/* ---- the entry point ---------------------------------------------------------------- */
/* ui: the start values (the caller resets the sound first), or the current sound for MUTATE.
 * Returns the style actually made (ANY picks one). */
static int generate(float *ui, int style, float variation, float modulation, uint32_t seed) {
    Rng r(seed);
    B b(ui);
    const float v = clampf(variation, 0, 1), m = clampf(modulation, 0, 1);
    if (style == S_ANY) style = r.i(S_ANY);
    if (style == S_MUTATE) {
        vary(b, r, 0.15f + 0.85f * v, S_MUTATE);
        if (m > 0 && r.p(0.5f + 0.5f * m)) modulate(b, r, m * 0.7f, S_MUTATE);
        level(b);
        return S_MUTATE;
    }
    clear_ops(b);
    switch (style) {
    case S_TALK: home_talk(b, r, v); break;
    case S_LEAD: home_lead(b, r, v); break;
    case S_BASS: home_bass(b, r, v); break;
    case S_ACID: home_acid(b, r, v); break;
    case S_STAB: home_stab(b, r, v); break;
    case S_PAD: home_pad(b, r, v); break;
    case S_PLUCK: home_pluck(b, r, v); break;
    case S_BELL: home_bell(b, r, v); break;
    case S_SEQ: home_seq(b, r, v); break;
    case S_FX: home_fx(b, r, v); break;
    case S_DRONE: home_drone(b, r, v); break;
    default: home_perc(b, r, v); break;
    }
    vary(b, r, v, style);
    /* MODULATION 0 leaves the home sound's own movement (TALK's and SEQ's steps), above 0 it adds */
    if (m > 0) modulate(b, r, m, style);
    level(b);
    if (style == S_STAB || style == S_PAD) b.g(G_VOLUME) *= 0.84f;   /* played as chords: 3 dB of room */
    return style;
}

}  // namespace oktornd
