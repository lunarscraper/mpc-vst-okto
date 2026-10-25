/* =============================================================================
 * okto_core.h - the sound engine of Okto: an 8-operator FM synthesizer.
 *
 * The FM core follows libfmsynth by Hans-Kristian Arntzen (MIT, ../LICENSE-libfmsynth.txt):
 * 8 operators, no fixed algorithms, a free 8 x 8 modulation matrix (every operator can modulate
 * every other one and itself), every operator can be a carrier with its own pan, true frequency
 * modulation with phase integration (a modulator's output times its own frequency is added to
 * the carrier's phase increment, so the index stays the same over the keyboard), a polynomial
 * sine, envelopes and LFO updated every 32 samples with a linear ramp in between. Kept from
 * libfmsynth as they are: the level law, keyboard scaling (around 440 Hz), velocity and mod
 * wheel sensitivity, LFO depth to amplitude and frequency, the exponential release.
 *
 * New in Okto (in the manner of NI FM8):
 *   - envelopes with up to 8 breakpoints per operator, modes ONCE / LOOP / STEP / STEP LOOP,
 *     a loop start, steps in tempo divisions (SYNC) - an envelope doubles as a step sequencer
 *   - a pitch envelope of the same kind, with an amount per operator
 *   - six waveforms per operator (sine, half, abs, quarter, soft square, 3-partial saw)
 *   - an LFO with six shapes, tempo sync, delay and key sync
 *   - POLY / MONO / LEGATO, glide, unison up to 3 with detune and width
 *   - self-feedback (the matrix diagonal) as phase modulation in the manner of the DX7, so it doesn't detune
 *   - macros: BRIGHT (all modulation), FEEDBACK (the diagonal), ENV TIME, RELEASE
 *   - only the operators that reach the output are computed
 *
 * MIT license (see ../LICENSE). No dependencies; one header.
 * ========================================================================== */
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace okto {

enum { NOPS = 8, NPTS = 8, BLOCK = 32, MAXV = 16 };
enum { W_SINE, W_HALF, W_ABS, W_QUARTER, W_SQUARE, W_SAW, NWAVES };
enum { E_ONCE, E_LOOP, E_STEP, E_STEP_LOOP, NEMODES };
enum { L_SINE, L_TRI, L_SAW_DOWN, L_SAW_UP, L_SQUARE, L_SH, NLFOW };
enum { V_POLY, V_MONO, V_LEGATO };

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* sin(2 pi ph) for ph in [0, 1): odd 7th-order polynomial on [-1/4, 1/4] (libfmsynth's
 * oscillator, re-centred so phase 0 is a zero crossing going up) */
static inline float sin2pi(float ph) {
    float x = ph < 0.25f ? ph : ph < 0.75f ? 0.5f - ph : ph - 1.0f;
    const float PI2 = 6.28318530718f;
    x *= PI2;
    const float x2 = x * x;
    return x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 - x2 * (1.0f / 5040))));
}
static inline float wrap01(float p) { return p - std::floor(p); }

/* the operator waveforms; all without DC (in true FM a DC offset would detune the carrier) */
static inline float wave(int w, float ph) {
    const float s = sin2pi(ph);
    switch (w) {
    default: return s;
    case W_HALF: return (ph < 0.5f ? s : 0.0f) - 0.318309886f;
    case W_ABS: return std::fabs(s) - 0.636619772f;
    case W_QUARTER: { float h = ph < 0.5f ? ph : ph - 0.5f; return (h < 0.25f ? std::fabs(s) : 0.0f) - 0.318309886f; }
    case W_SQUARE: { float x = 4.0f * s; return 1.25f * x / (1.0f + std::fabs(x)); }   /* soft square, peak 1 */
    case W_SAW: {   /* sin t - sin 2t / 2 + sin 3t / 3, peak ~1 */
        float c = sin2pi(ph < 0.75f ? ph + 0.25f : ph - 0.75f);
        return 0.5f * (s - s * c + s - 1.3333333f * s * s * s);
    }
    }
}

/* ---- the patch: everything the plugin sets, in engine units ---------------- */
struct EnvP {
    float lvl[NPTS] = {1, 0.5f, 0.25f, 0, 0, 0, 0, 0};   /* operator: 0..1, pitch: -1..1 */
    float sec[NPTS] = {0.05f, 0.05f, 0.25f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};   /* free time per segment */
    int units[NPTS] = {1, 1, 1, 1, 1, 1, 1, 1};           /* synced: length in divisions */
    int npts = 3, mode = E_ONCE, loop = 0;
    double sync_beats = 0;                                /* 0 = free, else beats per division */
    float rel = 0.5f;                                     /* seconds to -60 dB */
};
struct Patch {
    int wave[NOPS] = {0};
    float ratio[NOPS] = {1, 1, 1, 1, 1, 1, 1, 1};   /* 0 = fixed frequency (offset only) */
    float offset[NOPS] = {0};                        /* Hz */
    float amp[NOPS] = {1, 1, 1, 1, 1, 1, 1, 1};      /* operator level (libfmsynth "Volume") */
    float out[NOPS] = {1, 0, 0, 0, 0, 0, 0, 0};      /* carrier mix */
    float pan[NOPS] = {0};                           /* -1..1 */
    float vel[NOPS] = {1, 1, 1, 1, 1, 1, 1, 1};
    float kslo[NOPS] = {0}, kshi[NOPS] = {0};        /* (f / 440 Hz) ^ factor */
    float am[NOPS] = {0};                            /* LFO -> amplitude, 0..1 */
    float pm[NOPS] = {0};                            /* LFO -> frequency, factor 1 + pm * lfo */
    float penv[NOPS] = {1, 1, 1, 1, 1, 1, 1, 1};     /* how much of the pitch envelope */
    float wheel[NOPS] = {0};                         /* mod wheel sensitivity of the level */
    EnvP env[NOPS];
    float mod[NOPS][NOPS] = {{0}};                   /* mod[src][dst] */
    EnvP pe;                                         /* pitch envelope, levels -1..1 */
    float pe_depth = 0;                              /* semitones at level 1 */
    float volume = 0.2f, bright = 1, feedback = 1, env_time = 1, rel_scale = 1;
    float lfo_hz = 2; int lfo_wave = L_SINE; double lfo_beats = 0; float lfo_delay = 0; bool lfo_key = true;
    float vibrato = 0, mw_vib = 0, mw_bright = 0;
    int vmode = V_POLY, voices = 8, unison = 1;
    float glide = 0, detune = 0, width = 0, transpose = 0, bend_range = 2;
    Patch() { pe.lvl[0] = 0; pe.npts = 1; }
};

/* ---- envelope runtime ------------------------------------------------------ */
struct Env {
    int seg = 0;
    double t = 0, dur = 1;
    float from = 0, rel_v = 0, rel_k = 1, cur = 0;
    bool released = false, done = false;

    static double seg_len(const EnvP &p, int s, float sr, double tempo, float tscale) {
        double d = p.sync_beats > 0 ? p.units[s] * p.sync_beats * 60.0 / tempo * sr : (double)p.sec[s] * tscale * sr;
        return d < 1 ? 1 : d;
    }
    void start(const EnvP &p, float sr, double tempo, float tscale, float start_from) {
        seg = 0; t = 0; released = false; done = false;
        from = start_from;
        dur = seg_len(p, 0, sr, tempo, tscale);
        cur = level(p);
    }
    float level(const EnvP &p) const {
        if (released) return rel_v;
        const int n = p.npts < 1 ? 1 : p.npts > NPTS ? NPTS : p.npts;
        if (done || seg >= n) return p.lvl[n - 1];
        if (p.mode == E_STEP || p.mode == E_STEP_LOOP) return p.lvl[seg];
        const double f = t / dur;
        return from + (p.lvl[seg] - from) * (float)(f > 1 ? 1 : f);
    }
    /* advance by n samples and return the level reached */
    float advance(const EnvP &p, int n, float sr, double tempo, float tscale) {
        if (released) { rel_v *= rel_k; return cur = rel_v; }
        if (done) return cur = level(p);
        const int np = p.npts < 1 ? 1 : p.npts > NPTS ? NPTS : p.npts;
        if (seg >= np) seg = np - 1;
        t += n;
        for (int guard = 0; t >= dur && guard < 64; guard++) {
            t -= dur;
            from = p.lvl[seg];
            seg++;
            if (seg >= np) {
                if (p.mode == E_LOOP || p.mode == E_STEP_LOOP) seg = p.loop < 0 ? 0 : p.loop >= np ? np - 1 : p.loop;
                else { seg = np - 1; done = true; t = 0; break; }
            }
            dur = seg_len(p, seg, sr, tempo, tscale);
        }
        return cur = level(p);
    }
    void release(const EnvP &p, float sr, float rscale) {
        if (released) return;
        released = true;
        rel_v = cur;
        const double r = (double)p.rel * rscale;
        rel_k = r < 1e-4 ? 0.0f : (float)std::exp(std::log(0.001) * BLOCK / (r * sr));
    }
    /* finished: nothing more will come out (released below -80 dB, or a one-shot that ended at 0) */
    bool silent(const EnvP &p) const {
        if (released) return rel_v < 1e-4f;
        return done && std::fabs(p.lvl[(p.npts < 1 ? 1 : p.npts > NPTS ? NPTS : p.npts) - 1]) < 1e-6f;
    }
};

/* ---- voice ----------------------------------------------------------------- */
struct Voice {
    bool active = false, held = false, sustained = false, released = false;
    int note = 0;
    uint32_t age = 0;
    float note_cur = 60, note_tgt = 60;   /* glide in semitones */
    float detune = 0;                      /* semitones (unison) */
    float gl = 1, gr = 1;                  /* unison pan gains */
    float velf[NOPS], ksf[NOPS];
    float phase[NOPS];
    float fy1[NOPS], fy2[NOPS];            /* the last two outputs (envelope x wave) for self-feedback */
    float e[NOPS], estep[NOPS];
    Env env[NOPS], penv;
    float lfo_ph = 0, lfo_sh = 0, lfo_age = 0;
    int count = 0;                         /* samples into the current control block */
    /* per control block, compact (index = position in the active list) */
    float inc[NOPS], K[NOPS][NOPS], amp[NOPS], pl[NOPS], pr[NOPS], fb[NOPS];
};

class Engine {
public:
    void init(float sr) {
        sr_ = sr > 1000 ? sr : 44100;
        for (auto &v : vo_) v = Voice();
        nheld_ = 0; sustain_ = false; wheel_ = 0; bend_ = 0; lfo_ph_ = 0; lfo_sh_ = 0; age_ = 0;
        last_note_ = -1;
        update_active();
    }
    void set_patch(const Patch &p) { p_ = p; update_active(); }
    const Patch &patch() const { return p_; }
    void set_tempo(double bpm) { if (bpm > 20 && bpm < 1000) tempo_ = bpm; }
    void set_wheel(float w) { wheel_ = clampf(w, 0, 1); }
    void set_bend(float b) { bend_ = clampf(b, -1, 1); }
    void set_sustain(bool on) {
        sustain_ = on;
        if (!on) for (auto &v : vo_) if (v.active && v.sustained) { v.sustained = false; release(v); }
    }
    void all_off() {   /* immediate */
        for (auto &v : vo_) v.active = false;
        nheld_ = 0;
    }
    void all_release() {
        sustain_ = false; nheld_ = 0;
        for (auto &v : vo_) if (v.active) { v.held = v.sustained = false; release(v); }
    }
    int active_voices() const { int n = 0; for (auto &v : vo_) n += v.active; return n; }
    int active_ops() const { return nact_; }

    void note_on(int n, int velocity) {
        const float vel = clampf(velocity / 127.0f, 0, 1);
        const int U = p_.unison < 1 ? 1 : p_.unison > 3 ? 3 : p_.unison;
        if (p_.vmode != V_POLY) {
            push_held(n, vel);
            bool legato = false;
            for (int u = 0; u < U; u++) if (vo_[u].active && vo_[u].held && !vo_[u].released) legato = true;
            for (int i = U; i < MAXV; i++) vo_[i].active = false;   /* mono uses voices 0..U-1 */
            const bool glide = p_.glide > 0 && (p_.vmode == V_MONO || legato) && last_note_ >= 0;
            for (int u = 0; u < U; u++) {
                Voice &v = vo_[u];
                if (legato && p_.vmode == V_LEGATO && v.active) { v.note = n; v.note_tgt = (float)n; v.held = true; continue; }
                start_voice(v, n, vel, u, U, glide);
            }
            last_note_ = n;
            return;
        }
        const int maxv = p_.voices < 1 ? 1 : p_.voices > MAXV ? MAXV : p_.voices;
        const bool glide = p_.glide > 0 && last_note_ >= 0;
        /* a key pressed again while its voices still sound: those voices restart */
        for (int u = 0; u < U; u++) {
            Voice *v = find_voice(maxv, n);
            start_voice(*v, n, vel, u, U, glide);
        }
        last_note_ = n;
    }
    void note_off(int n) {
        if (p_.vmode != V_POLY) {
            remove_held(n);
            if (nheld_ > 0) {   /* back to the key still held, no new attack */
                const int m = held_[nheld_ - 1];
                for (auto &v : vo_) if (v.active && !v.released) { v.note = m; v.note_tgt = (float)m; if (p_.glide <= 0) v.note_cur = (float)m; }
                last_note_ = m;
                return;
            }
            for (auto &v : vo_) if (v.active && v.held) { v.held = false; if (sustain_) v.sustained = true; else release(v); }
            return;
        }
        for (auto &v : vo_)
            if (v.active && v.held && v.note == n) { v.held = false; if (sustain_) v.sustained = true; else release(v); }
    }

    /* adds into L and R */
    void render(float *L, float *R, int n) {
        lfo_global(n);
        for (auto &v : vo_) {
            if (!v.active) continue;
            int pos = 0;
            while (pos < n) {
                if (v.count == 0) { tick(v); if (!v.active) break; }
                int m = BLOCK - v.count;
                if (m > n - pos) m = n - pos;
                frames(v, L + pos, R + pos, m);
                v.count += m;
                if (v.count >= BLOCK) v.count = 0;
                pos += m;
            }
        }
    }

private:
    Patch p_;
    Voice vo_[MAXV];
    float sr_ = 44100;
    double tempo_ = 120;
    float wheel_ = 0, bend_ = 0;
    bool sustain_ = false;
    int held_[32]; float held_vel_[32]; int nheld_ = 0;
    int last_note_ = -1;
    uint32_t age_ = 0, rs_ = 0x2545f491u;
    float lfo_ph_ = 0, lfo_sh_ = 0, lfo_val_ = 0;
    int act_[NOPS], nact_ = 0;    /* the operators that reach the output, in index order */
    bool carrier_[NOPS];

    float rnd() { uint32_t x = rs_; x ^= x << 13; x ^= x >> 17; x ^= x << 5; rs_ = x; return (float)(x >> 8) * (2.0f / 16777216.0f) - 1.0f; }

    void update_active() {
        bool need[NOPS];
        for (int o = 0; o < NOPS; o++) { carrier_[o] = p_.out[o] > 0 && p_.amp[o] > 0; need[o] = carrier_[o]; }
        for (bool more = true; more;) {
            more = false;
            for (int o = 0; o < NOPS; o++) {
                if (need[o] || p_.amp[o] <= 0) continue;
                for (int j = 0; j < NOPS; j++)
                    if (need[j] && j != o && p_.mod[o][j] > 0) { need[o] = more = true; break; }
            }
        }
        nact_ = 0;
        for (int o = 0; o < NOPS; o++) if (need[o]) act_[nact_++] = o;
    }

    void push_held(int n, float vel) {
        remove_held(n);
        if (nheld_ == 32) { std::memmove(held_, held_ + 1, 31 * sizeof(int)); std::memmove(held_vel_, held_vel_ + 1, 31 * sizeof(float)); nheld_ = 31; }
        held_[nheld_] = n; held_vel_[nheld_] = vel; nheld_++;
    }
    void remove_held(int n) {
        int k = 0;
        for (int i = 0; i < nheld_; i++) if (held_[i] != n) { held_[k] = held_[i]; held_vel_[k] = held_vel_[i]; k++; }
        nheld_ = k;
    }

    Voice *find_voice(int maxv, int n) {
        (void)n;
        Voice *best = nullptr;
        for (int i = 0; i < maxv; i++) if (!vo_[i].active) return &vo_[i];
        for (int i = 0; i < maxv; i++)   /* the oldest released voice, else the oldest */
            if (vo_[i].released && (!best || vo_[i].age < best->age)) best = &vo_[i];
        if (best) return best;
        for (int i = 0; i < maxv; i++) if (!best || vo_[i].age < best->age) best = &vo_[i];
        return best;
    }

    void start_voice(Voice &v, int n, float vel, int u, int U, bool glide) {
        const bool was = v.active && !v.released;
        v.note = n; v.held = true; v.sustained = false; v.released = false;
        v.age = ++age_;
        v.note_tgt = (float)n;
        if (!glide || !v.active) v.note_cur = glide && last_note_ >= 0 ? (float)last_note_ : (float)n;
        if (!glide) v.note_cur = (float)n;
        /* unison: detune and pan spread evenly, level 1/sqrt(U) */
        const float sp = U > 1 ? (2.0f * u / (U - 1) - 1.0f) : 0.0f;
        v.detune = sp * p_.detune * (U == 2 ? 0.5f : 1.0f) / 100.0f;
        const float pan = sp * p_.width, g = 1.0f / std::sqrt((float)U);
        v.gl = g * (pan > 0 ? 1 - pan : 1); v.gr = g * (pan < 0 ? 1 + pan : 1);
        const float f = 440.0f * std::exp2((n - 69) / 12.0f);
        for (int o = 0; o < NOPS; o++) {
            v.velf[o] = 1 - p_.vel[o] + p_.vel[o] * vel;
            const float fac = f > 440 ? p_.kshi[o] : p_.kslo[o];
            v.ksf[o] = clampf(std::pow(f / 440.0f, fac), 0, 16);
            const float from = was ? v.env[o].cur : 0.0f;
            v.env[o].start(p_.env[o], sr_, tempo_, p_.env_time, from);
            if (!was) { v.phase[o] = 0; v.e[o] = 0; v.fy1[o] = v.fy2[o] = 0; }
            v.estep[o] = 0;
        }
        v.penv.start(p_.pe, sr_, tempo_, p_.env_time, 0);
        if (p_.lfo_key || !was) { v.lfo_ph = 0; v.lfo_sh = rnd(); }
        v.lfo_age = 0;
        v.count = 0;
        v.active = true;
    }
    void release(Voice &v) {
        if (v.released) return;
        v.released = true;
        for (int o = 0; o < NOPS; o++) v.env[o].release(p_.env[o], sr_, p_.rel_scale);
        v.penv.release(p_.pe, sr_, p_.rel_scale);
    }

    double lfo_inc() const {
        const double hz = p_.lfo_beats > 0 ? tempo_ / 60.0 / p_.lfo_beats : p_.lfo_hz;
        return hz / sr_;
    }
    static float lfo_shape(int w, float ph, float sh) {
        switch (w) {
        default: return sin2pi(ph);
        case L_TRI: return ph < 0.25f ? 4 * ph : ph < 0.75f ? 2 - 4 * ph : 4 * ph - 4;
        case L_SAW_DOWN: return 1 - 2 * ph;
        case L_SAW_UP: return 2 * ph - 1;
        case L_SQUARE: return ph < 0.5f ? 1.0f : -1.0f;
        case L_SH: return sh;
        }
    }
    void lfo_global(int n) {   /* the free-running LFO (key sync off): one phase for all voices */
        float ph = lfo_ph_ + (float)(lfo_inc() * n);
        if (ph >= 1) { ph -= std::floor(ph); lfo_sh_ = rnd(); }
        lfo_ph_ = ph;
        lfo_val_ = lfo_shape(p_.lfo_wave, lfo_ph_, lfo_sh_);
    }

    /* the control block: LFO, envelopes, glide, then the per-sample coefficients */
    void tick(Voice &v) {
        const Patch &p = p_;
        /* LFO */
        float lfo;
        if (p.lfo_key) {
            v.lfo_ph += (float)(lfo_inc() * BLOCK);
            if (v.lfo_ph >= 1) { v.lfo_ph -= std::floor(v.lfo_ph); v.lfo_sh = rnd(); }
            lfo = lfo_shape(p.lfo_wave, v.lfo_ph, v.lfo_sh);
        } else lfo = lfo_val_;
        v.lfo_age += BLOCK / sr_;
        if (p.lfo_delay > 0 && v.lfo_age < p.lfo_delay) lfo *= v.lfo_age / p.lfo_delay;

        /* envelopes: the level at the end of this block, reached by a linear ramp */
        bool sounding = false;
        for (int o = 0; o < NOPS; o++) {
            const float tgt = v.env[o].advance(p.env[o], BLOCK, sr_, tempo_, p.env_time);
            v.estep[o] = (tgt - v.e[o]) * (1.0f / BLOCK);
            if (carrier_[o] && !(v.env[o].silent(p.env[o]) && std::fabs(v.e[o]) < 1e-4f)) sounding = true;
        }
        if (!sounding) { v.active = false; return; }
        const float pe = v.penv.advance(p.pe, BLOCK, sr_, tempo_, p.env_time) * p.pe_depth;

        /* glide */
        if (v.note_cur != v.note_tgt) {
            if (p.glide <= 0) v.note_cur = v.note_tgt;
            else {
                const float k = 1.0f - std::exp(-4.0f * BLOCK / (p.glide * sr_));   /* ~98 % there after GLIDE */
                v.note_cur += (v.note_tgt - v.note_cur) * k;
                if (std::fabs(v.note_tgt - v.note_cur) < 0.001f) v.note_cur = v.note_tgt;
            }
        }
        const float semis = v.note_cur - 69 + p.transpose + v.detune + bend_ * p.bend_range;
        const float f0 = 440.0f * std::exp2(semis / 12.0f);
        const float vib = p.vibrato + p.mw_vib * wheel_;
        const float bright = p.bright + p.mw_bright * wheel_;
        const float isr = 1.0f / sr_;

        float inc[NOPS];
        for (int a = 0; a < nact_; a++) {
            const int o = act_[a];
            const float fm = 1.0f + (p.pm[o] + vib) * lfo;
            float f;
            if (p.ratio[o] > 0) f = (f0 * p.ratio[o] * std::exp2(pe * p.penv[o] / 12.0f) + p.offset[o]) * fm;
            else f = std::fabs(p.offset[o]) * std::exp2(pe * p.penv[o] / 12.0f) * fm;
            inc[a] = clampf(f * isr, -0.49f, 0.49f);
            v.inc[a] = inc[a];
            const float w = 1.0f - p.wheel[o] + p.wheel[o] * wheel_;
            v.amp[a] = p.amp[o] * v.velf[o] * v.ksf[o] * (1.0f + p.am[o] * lfo) * w;
            /* libfmsynth's pan law: full level on both sides at the centre */
            const float g = p.out[o] * p.volume;
            v.pl[a] = g * v.gl * (p.pan[o] > 0 ? 1 - p.pan[o] : 1);
            v.pr[a] = g * v.gr * (p.pan[o] < 0 ? 1 + p.pan[o] : 1);
        }
        for (int b = 0; b < nact_; b++) {
            const int o = act_[b];
            for (int a = 0; a < nact_; a++) {
                const int j = act_[a];
                v.K[b][a] = o == j ? 0.0f : inc[b] * p.mod[o][j] * bright;
            }
            /* self-feedback as phase modulation (DX7 style: the mean of the last two outputs added to the
             * phase), so it adds harmonics without pulling the pitch the way frequency feedback does;
             * 1 = half a cycle at FEEDBACK 100 */
            v.fb[b] = 0.5f * p.mod[o][o] * bright * p.feedback;
        }
    }

    void frames(Voice &v, float *L, float *R, int n) {
        const int na = nact_;
        float ph[NOPS], e[NOPS], es[NOPS], val[NOPS], y1[NOPS], y2[NOPS];
        int wv[NOPS];
        for (int a = 0; a < na; a++) {
            const int o = act_[a];
            ph[a] = v.phase[o]; e[a] = v.e[o]; es[a] = v.estep[o]; wv[a] = p_.wave[o];
            y1[a] = v.fy1[o]; y2[a] = v.fy2[o];
        }
        for (int f = 0; f < n; f++) {
            float l = 0, r = 0;
            for (int a = 0; a < na; a++) {
                float q = ph[a];
                if (v.fb[a] != 0) q = wrap01(q + v.fb[a] * 0.5f * (y1[a] + y2[a]));
                const float w = wv[a] ? wave(wv[a], q) : sin2pi(q);
                y2[a] = y1[a]; y1[a] = e[a] * w;
                const float x = y1[a] * v.amp[a];
                val[a] = x;
                e[a] += es[a];
                l += x * v.pl[a];
                r += x * v.pr[a];
            }
            for (int a = 0; a < na; a++) {
                float s = v.inc[a];
                for (int b = 0; b < na; b++) s += val[b] * v.K[b][a];
                ph[a] = wrap01(ph[a] + s);
            }
            L[f] += l;
            R[f] += r;
        }
        for (int a = 0; a < na; a++) {
            const int o = act_[a];
            v.phase[o] = ph[a]; v.e[o] = e[a]; v.fy1[o] = y1[a]; v.fy2[o] = y2[a];
        }
        /* operators off the active list keep their envelope moving (they may come back) */
        for (int o = 0; o < NOPS; o++) {
            bool on = false;
            for (int a = 0; a < na; a++) on |= act_[a] == o;
            if (!on) v.e[o] += v.estep[o] * n;
        }
    }
};

}  // namespace okto
