/* Offline x86 test of okto_vst.cpp (test.sh builds it with ASan/UBSan): silence without a key,
 * tuning (ratio, fine, fixed frequency, transpose, pitch bend), FM sidebands through the matrix,
 * feedback, BRIGHT and FEEDBACK macros, the six waveforms (sound, no DC), the envelopes (one-shot
 * decay ends the voice, release, STEP LOOP in tempo divisions as a gate, LOOP), the pitch envelope
 * and its amount per operator, mono / legato with glide, unison width, sustain pedal, the EDIT
 * selector (edit knobs read and write the selected operator, PITCH, Q-Link steps), the 32 preset
 * slots (program list, every factory sound plays, SAVE/LOAD, shared bank file), chunk restore,
 * output ceiling, a random parameter/MIDI stress run, NaN/denormal-free output.
 * With "bench" as the second argument it only measures CPU load. Prints PASSED/FAILED. */
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dlfcn.h>

#include "params.h"   /* build/params.h: the same table the plugin is built with (ranges, options) */

struct AEffect;
typedef intptr_t (*audioMasterCallback)(AEffect *, int32_t, int32_t, intptr_t, void *, float);
struct AEffect {
    int32_t magic;
    intptr_t (*dispatcher)(AEffect *, int32_t, int32_t, intptr_t, void *, float);
    void (*process)(AEffect *, float **, float **, int32_t);
    void (*setParameter)(AEffect *, int32_t, float);
    float (*getParameter)(AEffect *, int32_t);
    int32_t numPrograms, numParams, numInputs, numOutputs, flags;
    intptr_t resvd1, resvd2;
    int32_t initialDelay, realQualities, offQualities;
    float ioRatio;
    void *object, *user;
    int32_t uniqueID, version;
    void (*processReplacing)(AEffect *, float **, float **, int32_t);
    void (*processDoubleReplacing)(AEffect *, double **, double **, int32_t);
    char future[56];
};
typedef struct { int32_t type, byteSize, deltaFrames, flags, noteLength, noteOffset; unsigned char midiData[4]; char detune, noteOffVelocity, reserved1, reserved2; } VstMidiEvent;
typedef struct { int32_t numEvents; intptr_t reserved; VstMidiEvent *events[2]; } VstEvents;
typedef struct {
    double samplePos, sampleRate, nanoSeconds, ppqPos, tempo, barStartPos, cycleStartPos, cycleEndPos;
    int32_t timeSigNumerator, timeSigDenominator, smpteOffset, smpteFrameRate, samplesToNextClock, flags;
} VstTimeInfo;
static VstTimeInfo g_ti;
static bool g_time = true;
static double g_tempo = 120;
static int g_updates = 0;
static intptr_t master(AEffect *, int32_t op, int32_t, intptr_t, void *, float) {
    if (op == 42) g_updates++;
    if (op != 7 || !g_time) return 0;
    std::memset(&g_ti, 0, sizeof g_ti);
    g_ti.tempo = g_tempo;
    g_ti.flags = 1 << 10;
    return (intptr_t)&g_ti;
}
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; std::printf("FAIL: " __VA_ARGS__); std::printf("\n"); } } while (0)
static const float SR = 44100;
static const int BS = 256;
static bool bad = false;
typedef std::vector<float> Buf;

static int pidx(const char *key) {
    for (int i = 0; i < NPARAMS; i++) if (!std::strcmp(PARAMS[i].key, key)) return i;
    std::printf("FAIL: no parameter %s\n", key); fails++; return 0;
}
static std::string display(AEffect *e, const char *key) { char buf[64] = {0}; e->dispatcher(e, 7, pidx(key), 0, buf, 0); return buf; }
static std::string pname(AEffect *e, int i) { char buf[64] = {0}; e->dispatcher(e, 8, i, 0, buf, 0); return buf; }
/* set by key, in the units of module.json (an option by its index) */
static void set(AEffect *e, const char *key, double v) {
    const param_t *p = &PARAMS[pidx(key)];
    float n = p->nopts ? (p->nopts > 1 ? (float)(v / (p->nopts - 1)) : 0.f) : (float)((v - p->min) / (p->max - p->min));
    e->setParameter(e, pidx(key), n);
}
static void setk(AEffect *e, const std::string &k, double v) { set(e, k.c_str(), v); }
static float getn(AEffect *e, const char *key) { return e->getParameter(e, pidx(key)); }
static void press(AEffect *e, const char *key) { set(e, key, 1); set(e, key, 0); }
static void send(AEffect *e, int a, int b, int c, int frame = 0) {
    VstMidiEvent m; std::memset(&m, 0, sizeof m);
    m.type = 1; m.byteSize = sizeof m; m.deltaFrames = frame;
    m.midiData[0] = (unsigned char)a; m.midiData[1] = (unsigned char)b; m.midiData[2] = (unsigned char)c;
    VstEvents ev; ev.numEvents = 1; ev.reserved = 0; ev.events[0] = &m; ev.events[1] = nullptr;
    e->dispatcher(e, 25, 0, 0, &ev, 0);
}
static void on(AEffect *e, int n, int v = 100) { send(e, 0x90, n, v); }
static void off(AEffect *e, int n) { send(e, 0x80, n, 0); }
static Buf run(AEffect *e, double sec, Buf *right = nullptr) {
    Buf outL, ol(BS), orr(BS);
    float *out[2] = {ol.data(), orr.data()};
    for (int b = 0; b < (int)(sec * SR / BS); b++) {
        e->processReplacing(e, nullptr, out, BS);
        for (int i = 0; i < BS; i++) {
            for (float s : {ol[i], orr[i]}) if (!std::isfinite(s) || std::fpclassify(s) == FP_SUBNORMAL) bad = true;
            outL.push_back(ol[i]);
            if (right) right->push_back(orr[i]);
        }
    }
    return outL;
}
static double goertzel(const Buf &a, double f, double t0 = 0.1, double t1 = -1) {
    size_t i0 = (size_t)(t0 * SR), i1 = t1 < 0 ? a.size() : std::min(a.size(), (size_t)(t1 * SR));
    double w = 2 * M_PI * f / SR, c = std::cos(w), s1 = 0, s2 = 0;
    for (size_t i = i0; i < i1; i++) { double s0 = a[i] + 2 * c * s1 - s2; s2 = s1; s1 = s0; }
    return 2 * std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - 2 * c * s1 * s2)) / (i1 - i0);
}
static double rms(const Buf &a, double t0 = 0.1, double t1 = -1) {
    double s = 0; size_t i0 = (size_t)(t0 * SR), i1 = t1 < 0 ? a.size() : std::min(a.size(), (size_t)(t1 * SR));
    if (i1 <= i0) return 0;
    for (size_t i = i0; i < i1; i++) s += (double)a[i] * a[i];
    return std::sqrt(s / (i1 - i0));
}
static double mean(const Buf &a, double t0 = 0.1) { double s = 0; size_t i0 = (size_t)(t0 * SR); for (size_t i = i0; i < a.size(); i++) s += a[i]; return s / (a.size() - i0); }
static double peak(const Buf &a, double t0 = 0) { double p = 0; for (size_t i = (size_t)(t0 * SR); i < a.size(); i++) p = std::max(p, (double)std::fabs(a[i])); return p; }
static double freq(const Buf &a, double f0, double t0 = 0.2, double t1 = -1) {
    double best = 0, bf = f0;
    for (double f = f0 * 0.94; f <= f0 * 1.06; f *= 1.0005) { double g = goertzel(a, f, t0, t1); if (g > best) { best = g; bf = f; } }
    return bf;
}
static double cents(double f, double ref) { return 1200 * std::log2(f / ref); }
static double db(double x) { return 20 * std::log10(x + 1e-12); }
static std::string progname(AEffect *e, int i) { char b[64] = {0}; e->dispatcher(e, 29, i, 0, b, 0); return b; }
static Buf note(AEffect *e, int n, double sec, Buf *right = nullptr) { on(e, n); Buf a = run(e, sec, right); off(e, n); run(e, 1.5); return a; }
static std::string op(int o, const char *f) { return "o" + std::to_string(o) + "_" + f; }

/* the start sound: a sine on OP 1; plus a held organ envelope (100 % right away, held) */
static void plain(AEffect *e) {
    e->dispatcher(e, 2, 0, 31, nullptr, 0);   /* slot 32 starts empty: LOAD does nothing ... */
    std::string chunk = "OKTO1;";            /* ... so reset through a chunk: start values only */
    e->dispatcher(e, 24, 0, (intptr_t)chunk.size(), (void *)chunk.data(), 0);
    for (int o = 1; o <= 8; o++) { setk(e, op(o, "epts"), 0); setk(e, op(o, "l1"), 100); setk(e, op(o, "t1"), 0); setk(e, op(o, "erel"), 20); }
    send(e, 0xe0, 0, 64); send(e, 0xb0, 1, 0); send(e, 0xb0, 123, 0);
    run(e, 0.3);
}

int main(int argc, char **argv) {
    if (argc < 2) { std::printf("usage: host_test plugin.so [bench]\n"); return 2; }
    const char *bank = "/tmp/okto_test_presets.txt";
    std::remove(bank);
    setenv("OKTO_PRESETS", bank, 1);
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { std::printf("FAIL: %s\n", dlerror()); return 1; }
    typedef AEffect *(*MainF)(audioMasterCallback);
    MainF mainf = (MainF)dlsym(h, "VSTPluginMain");
    if (!mainf) { std::printf("FAIL: no VSTPluginMain\n"); return 1; }
    AEffect *e = mainf(master);
    e->dispatcher(e, 10, 0, 0, nullptr, SR);
    e->dispatcher(e, 12, 0, 1, nullptr, 0);
    CHECK(e->numParams == NPARAMS && e->numPrograms == 32, "params %d programs %d", e->numParams, e->numPrograms);

    if (argc > 2 && !std::strcmp(argv[2], "bench")) {
        struct { const char *what; int slot, notes; } B[] = {
            {"GOA LEAD (3 op, unison 2), 1 note", 0, 1}, {"TRANCEGATE PAD (unison 3), 2 notes", 4, 2},
            {"DRAWBAR ORGAN (8 carriers), 4 notes", 20, 4}, {"SUPERSAW FM (unison 3), 2 notes", 10, 2},
            {"E-PIANO, 8 notes", 8, 8},
        };
        for (auto &b : B) {
            e->dispatcher(e, 2, 0, b.slot, nullptr, 0);
            for (int i = 0; i < b.notes; i++) on(e, 48 + 4 * i);
            run(e, 0.05);
            auto t0 = std::chrono::steady_clock::now();
            run(e, 10.0);
            double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            for (int i = 0; i < b.notes; i++) off(e, 48 + 4 * i);
            run(e, 3);
            std::printf("bench (this CPU, one core): %-40s %.2f %%\n", b.what, 100 * s / 10.0);
        }
        /* worst case: all eight operators sounding, the full 8 x 8 matrix, 16 voices */
        plain(e);
        set(e, "voices", 15);
        for (int o = 1; o <= 8; o++) { setk(e, op(o, "out"), 30); for (int d = 1; d <= 8; d++) setk(e, "m" + std::to_string(o) + "_" + std::to_string(d), 20); }
        for (int i = 0; i < 16; i++) on(e, 40 + 2 * i);
        run(e, 0.05);
        auto t0 = std::chrono::steady_clock::now();
        run(e, 10.0);
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("bench (this CPU, one core): %-40s %.2f %%\n", "8 op, full matrix, 16 voices", 100 * s / 10.0);
        return 0;
    }

    /* 1. silence without a key; names */
    CHECK(peak(run(e, 0.5)) == 0, "sound without a key");
    CHECK(pname(e, pidx("x_coarse")) == "Edit Ratio" && pname(e, pidx("m2_1")) == "Op2 > Op1", "names: %s / %s", pname(e, pidx("x_coarse")).c_str(), pname(e, pidx("m2_1")).c_str());

    /* 2. tuning */
    plain(e);
    {
        Buf a = note(e, 69, 1.0);
        double c = cents(freq(a, 440), 440);
        std::printf("  A4 %+.2f ct, %.1f dBFS\n", c, db(rms(a)));
        CHECK(std::fabs(c) < 3, "A4 off by %.1f cents", c);
        CHECK(rms(a) > 0.05 && rms(a) < 0.5, "start sound level %.3f", rms(a));
        set(e, op(1, "coarse").c_str(), 3);                       /* ratio 2 */
        c = cents(freq(note(e, 69, 1.0), 880), 880);
        CHECK(std::fabs(c) < 3 && display(e, op(1, "coarse").c_str()) == "2", "ratio 2: %.1f ct, %s", c, display(e, op(1, "coarse").c_str()).c_str());
        set(e, op(1, "coarse").c_str(), 2); set(e, op(1, "fine").c_str(), 50);   /* 1.5 */
        c = cents(freq(note(e, 69, 1.0), 660), 660);
        CHECK(std::fabs(c) < 3, "ratio 1.5: %.1f ct (%s)", c, display(e, op(1, "fine").c_str()).c_str());
        set(e, op(1, "fine").c_str(), 0);
        set(e, op(1, "coarse").c_str(), 1);                       /* 0.5 */
        c = cents(freq(note(e, 69, 1.0), 220), 220);
        CHECK(std::fabs(c) < 3, "ratio 0.5: %.1f ct", c);
        set(e, op(1, "coarse").c_str(), 0);                       /* FIX: 1000 Hz on any key */
        set(e, op(1, "offset").c_str(), 100 * std::cbrt(1000.0 / 4000));
        double f1 = freq(note(e, 60, 1.0), 1000), f2 = freq(note(e, 84, 1.0), 1000);
        CHECK(std::fabs(cents(f1, 1000)) < 3 && std::fabs(cents(f2, 1000)) < 3, "fixed 1 kHz: %.1f / %.1f Hz (%s)", f1, f2, display(e, op(1, "offset").c_str()).c_str());
        set(e, op(1, "coarse").c_str(), 2);                       /* ratio 1 + 3 Hz offset beats against a plain carrier */
        set(e, op(1, "offset").c_str(), 100 * std::cbrt(3.0 / 4000));
        c = cents(freq(note(e, 69, 2.0), 443, 0.2), 443);
        CHECK(std::fabs(c) < 3, "offset +3 Hz: %.1f ct from 443", c);
        set(e, op(1, "offset").c_str(), 0);
        set(e, "transpose", 24 + 12);
        c = cents(freq(note(e, 69, 1.0), 880), 880);
        CHECK(std::fabs(c) < 3 && display(e, "transpose") == "+12", "transpose +12: %.1f ct, %s", c, display(e, "transpose").c_str());
        set(e, "transpose", 24);
        send(e, 0xe0, 127, 127);
        c = cents(freq(note(e, 69, 1.0), 493.88), 493.88);
        CHECK(std::fabs(c) < 5, "bend +2: %.1f cents", c);
        send(e, 0xe0, 0, 64);
    }

    /* 3. FM: OP 2 into OP 1 makes sidebands; feedback; BRIGHT and FEEDBACK */
    plain(e);
    {
        Buf a0 = note(e, 57, 1.0);
        set(e, "m2_1", 100);
        Buf a1 = note(e, 57, 1.0);
        std::printf("  FM 1:1, index 1: 2nd harmonic %.1f -> %.1f dB, 3rd %.1f dB\n", db(goertzel(a0, 440)), db(goertzel(a1, 440)), db(goertzel(a1, 660)));
        CHECK(goertzel(a1, 440) > 30 * goertzel(a0, 440) && goertzel(a1, 660) > 30 * goertzel(a0, 660), "no FM sidebands");
        set(e, "bright", 0);
        Buf a2 = note(e, 57, 1.0);
        CHECK(goertzel(a2, 440) < 0.05 * goertzel(a1, 440), "BRIGHT 0 leaves the modulation on");
        set(e, "bright", 200);
        Buf a3 = note(e, 57, 1.0);
        CHECK(goertzel(a3, 880) > 3 * goertzel(a1, 880), "BRIGHT 200 is not brighter");
        set(e, "bright", 100); set(e, "m2_1", 0);
        set(e, "m1_1", 80);                                       /* self-feedback on the carrier */
        Buf f1 = note(e, 57, 1.0);
        CHECK(goertzel(f1, 440) > 30 * goertzel(a0, 440), "feedback adds nothing");
        set(e, "feedback", 0);
        Buf f0 = note(e, 57, 1.0);
        CHECK(goertzel(f0, 440) < 0.05 * goertzel(f1, 440), "FEEDBACK 0 leaves the feedback on");
        set(e, "feedback", 100); set(e, "m1_1", 0);
        /* a modulator that reaches no carrier costs nothing and changes nothing */
        set(e, "m3_4", 100);
        Buf n0 = note(e, 57, 1.0);
        CHECK(std::fabs(rms(n0) - rms(a0)) < 1e-3, "an unconnected operator changes the sound");
        set(e, "m3_4", 0);
        /* the modulation index does not depend on the key (true FM after libfmsynth): same sideband ratio an octave up */
        set(e, "m2_1", 100);
        Buf lo = note(e, 57, 1.0), hi = note(e, 69, 1.0);
        double rl = goertzel(lo, 440) / goertzel(lo, 220), rh = goertzel(hi, 880) / goertzel(hi, 440);
        CHECK(std::fabs(db(rl) - db(rh)) < 1.5, "index follows the key: %.1f vs %.1f dB", db(rl), db(rh));
        set(e, "m2_1", 0);
    }

    /* 4. waveforms: each sounds, none has DC, and each is a different spectrum */
    {
        const char *names[] = {"SINE", "HALF", "ABS", "QUARTER", "SQUARE", "SAW"};
        double prev = -1;
        for (int w = 0; w < 6; w++) {
            plain(e); set(e, op(1, "wave").c_str(), w);
            on(e, 57); Buf a = run(e, 1.0); off(e, 57); run(e, 1);
            double h2 = goertzel(a, 440), h3 = goertzel(a, 660), dc = mean(a);
            std::printf("  %-8s %6.1f dBFS, 2nd %6.1f dB, 3rd %6.1f dB, DC %.4f\n", names[w], db(rms(a)), db(h2), db(h3), dc);
            CHECK(display(e, op(1, "wave").c_str()) == names[w], "wave name %s", display(e, op(1, "wave").c_str()).c_str());
            CHECK(rms(a) > 0.03, "%s silent", names[w]);
            CHECK(std::fabs(dc) < 0.01, "%s has DC %.4f", names[w], dc);
            const double sig = h2 + 2 * h3;
            if (w > 0) CHECK(sig > 0.003 && std::fabs(sig - prev) > 1e-4, "%s: no harmonics of its own", names[w]);
            prev = sig;
        }
    }

    /* 5. envelopes */
    plain(e);
    {
        /* one-shot decay to 0: the voice ends by itself while the key is still held */
        set(e, op(1, "epts").c_str(), 1); set(e, op(1, "l2").c_str(), 0); set(e, op(1, "t2").c_str(), 100 * std::log(0.2 / 0.001) / std::log(20000.0));
        on(e, 69);
        Buf a = run(e, 1.0);
        CHECK(rms(a, 0.0, 0.05) > 0.05 && peak(a, 0.4) == 0, "one-shot: start %.3f, after decay %.5f", rms(a, 0, 0.05), peak(a, 0.4));
        CHECK(display(e, op(1, "t2").c_str()) == "200 ms", "time display %s", display(e, op(1, "t2").c_str()).c_str());
        off(e, 69); run(e, 0.5);
        /* release: held level, then the release time to -60 dB */
        plain(e);
        set(e, op(1, "erel").c_str(), 100 * std::log(0.5 / 0.001) / std::log(20000.0));
        on(e, 69); run(e, 0.3); off(e, 69);
        Buf r = run(e, 1.0);
        std::printf("  release 0.5 s: %.1f dB after 0.25 s, %.1f dB after 0.5 s\n", db(rms(r, 0.24, 0.26) / rms(r, 0.0, 0.01)), db(rms(r, 0.49, 0.51) / rms(r, 0.0, 0.01)));
        CHECK(std::fabs(db(rms(r, 0.24, 0.26) / rms(r, 0.0, 0.01)) + 30) < 4 && peak(r, 0.9) < 1e-3, "release curve");
        /* STEP LOOP, 2 points of 1/16 at 120 BPM: a gate of 1/8 period (4 Hz on/off) */
        plain(e);
        g_tempo = 120;
        set(e, op(1, "emode").c_str(), 3); set(e, op(1, "epts").c_str(), 1); set(e, op(1, "esync").c_str(), 3);
        set(e, op(1, "l1").c_str(), 100); set(e, op(1, "l2").c_str(), 0); set(e, op(1, "t1").c_str(), 0); set(e, op(1, "t2").c_str(), 0);
        on(e, 69);
        Buf g = run(e, 2.0);
        off(e, 69); run(e, 1);
        int edges = 0; bool was = true; double first_off = -1;
        for (double t = 0.0; t + 0.01 < 2.0; t += 0.005) {
            bool now = rms(g, t, t + 0.005) > 0.02;
            if (now != was) { edges++; if (!now && first_off < 0) first_off = t; }
            was = now;
        }
        std::printf("  STEP LOOP 1/16 @120: %d edges in 2 s, first off at %.3f s (%s)\n", edges, first_off, display(e, op(1, "t1").c_str()).c_str());
        CHECK(edges >= 14 && edges <= 17 && std::fabs(first_off - 0.125) < 0.012, "gate pattern: %d edges, first off %.3f", edges, first_off);
        CHECK(display(e, op(1, "t1").c_str()) == "1 x 1/16", "synced time display %s", display(e, op(1, "t1").c_str()).c_str());
        /* the same at 150 BPM: steps of 0.1 s */
        g_tempo = 150;
        on(e, 69); Buf g2 = run(e, 1.0); off(e, 69); run(e, 1);
        double off2 = -1;
        for (double t = 0.0; t < 0.5 && off2 < 0; t += 0.002) if (rms(g2, t, t + 0.004) < 0.02) off2 = t;
        CHECK(std::fabs(off2 - 0.1) < 0.01, "gate follows the tempo: first off %.3f s at 150 BPM", off2);
        g_tempo = 120;
        /* T sets the number of divisions: 3 x 1/16 on, 1 x 1/16 off */
        set(e, op(1, "t1").c_str(), 100.0 * 2 / 15);
        on(e, 69); Buf g3 = run(e, 1.0); off(e, 69); run(e, 1);
        double off3 = -1;
        for (double t = 0.0; t < 0.8 && off3 < 0; t += 0.002) if (rms(g3, t, t + 0.004) < 0.02) off3 = t;
        CHECK(std::fabs(off3 - 0.375) < 0.012 && display(e, op(1, "t1").c_str()) == "3 x 1/16", "3 divisions: off at %.3f (%s)", off3, display(e, op(1, "t1").c_str()).c_str());
        /* LOOP (ramps) keeps moving while held */
        set(e, op(1, "emode").c_str(), 1); set(e, op(1, "esync").c_str(), 0);
        set(e, op(1, "t1").c_str(), 100 * std::log(0.1 / 0.001) / std::log(20000.0)); set(e, op(1, "t2").c_str(), 100 * std::log(0.1 / 0.001) / std::log(20000.0));
        on(e, 69); Buf lp = run(e, 2.0); off(e, 69); run(e, 1);
        double mn = 1, mx = 0;
        for (double t = 1.0; t < 1.9; t += 0.01) { double v = rms(lp, t, t + 0.01); mn = std::min(mn, v); mx = std::max(mx, v); }
        CHECK(mx > 0.1 && mn < 0.3 * mx, "LOOP stopped: %.3f .. %.3f", mn, mx);
    }

    /* 6. pitch envelope and its amount per operator */
    plain(e);
    {
        set(e, "pe_depth", 12); set(e, "pe_epts", 0); set(e, "pe_l1", 100); set(e, "pe_t1", 0);
        double c = cents(freq(note(e, 69, 1.0), 880), 880);
        CHECK(std::fabs(c) < 3, "pitch env +12: %.1f ct", c);
        set(e, op(1, "penv").c_str(), 0);
        c = cents(freq(note(e, 69, 1.0), 440), 440);
        CHECK(std::fabs(c) < 3, "pitch env amount 0: %.1f ct", c);
        set(e, op(1, "penv").c_str(), 100);
        /* a drop: +12 to 0 over 0.1 s */
        set(e, "pe_epts", 1); set(e, "pe_l2", 0); set(e, "pe_t2", 100 * std::log(0.1 / 0.001) / std::log(20000.0));
        on(e, 69); Buf d = run(e, 1.0); off(e, 69); run(e, 1);
        double early = freq(d, 880, 0.0, 0.02), late = freq(d, 440, 0.4, 0.9);
        CHECK(early > 700 && std::fabs(cents(late, 440)) < 3, "pitch drop: %.0f Hz -> %.1f Hz", early, late);
        CHECK(display(e, "pe_l1") == "+100", "pitch level display %s", display(e, "pe_l1").c_str());
    }

    /* 7. voice modes */
    plain(e);
    {
        /* legato with glide: the second key glides, the first stays held */
        set(e, "vmode", 2); set(e, "glide", 100 * std::log(0.2 / 0.002) / std::log(1000.0));
        on(e, 57); run(e, 0.3); on(e, 69);
        Buf gl = run(e, 1.0);
        off(e, 69); off(e, 57); run(e, 1.5);
        double f_early = freq(gl, 300, 0.0, 0.05) , f_late = freq(gl, 440, 0.7, 1.0);
        std::printf("  legato glide 0.2 s: %.0f Hz after 25 ms, %.1f Hz after 0.8 s (%s)\n", f_early, f_late, display(e, "glide").c_str());
        CHECK(f_early > 240 && f_early < 400 && std::fabs(cents(f_late, 440)) < 4, "glide: %.0f / %.1f Hz", f_early, f_late);
        /* mono: back to the key still held */
        set(e, "glide", 0);
        on(e, 57); run(e, 0.2); on(e, 69); run(e, 0.2); off(e, 69);
        Buf back = run(e, 0.6);
        off(e, 57); run(e, 1.5);
        CHECK(std::fabs(cents(freq(back, 220, 0.1), 220)) < 4, "mono: not back at the held key");
        set(e, "vmode", 0);
        /* poly: two keys, two pitches */
        on(e, 57); on(e, 64); Buf pl = run(e, 1.0); off(e, 57); off(e, 64); run(e, 1.5);
        CHECK(goertzel(pl, 220) > 0.02 && goertzel(pl, 329.63) > 0.02, "poly chord");
        /* unison 3 with width: left and right differ, level stays bounded */
        set(e, "unison", 2); set(e, "detune", 30); set(e, "width", 100);
        Buf R; Buf Lu = note(e, 57, 1.0, &R);
        double diff = 0; for (size_t i = 0; i < Lu.size(); i++) diff += std::fabs(Lu[i] - R[i]);
        CHECK(diff / Lu.size() > 0.01 && peak(Lu) < 0.98, "unison: L/R diff %.4f, peak %.2f", diff / Lu.size(), peak(Lu));
        set(e, "unison", 0);
        /* sustain pedal holds after the key is up, releases with the pedal */
        send(e, 0xb0, 64, 127); on(e, 69); run(e, 0.2); off(e, 69);
        Buf s1 = run(e, 0.5);
        send(e, 0xb0, 64, 0);
        Buf s2 = run(e, 1.0);
        CHECK(rms(s1, 0.3, 0.5) > 0.05 && peak(s2, 0.8) < 1e-3, "sustain pedal: %.3f held, %.5f after", rms(s1, 0.3, 0.5), peak(s2, 0.8));
        /* voice limit: 20 keys on 4 voices never exceed four voices' level */
        set(e, "voices", 3);
        for (int i = 0; i < 20; i++) on(e, 50 + i);
        Buf many = run(e, 0.5);
        for (int i = 0; i < 20; i++) off(e, 50 + i);
        run(e, 1.5);
        CHECK(peak(many) < 0.98 && rms(many) > 0.05, "voice limit");
        set(e, "voices", 7);
    }

    /* 8. the EDIT selector */
    plain(e);
    {
        set(e, "sel", 1);                                         /* OP 2 */
        CHECK(display(e, "sel") == "OP 2", "sel display %s", display(e, "sel").c_str());
        set(e, "x_coarse", 5);
        CHECK(display(e, "o2_coarse") == "4" && display(e, "x_coarse") == "4" && display(e, "o1_coarse") == "1", "edit ratio: o2 %s, x %s, o1 %s",
              display(e, "o2_coarse").c_str(), display(e, "x_coarse").c_str(), display(e, "o1_coarse").c_str());
        set(e, "x_from1", 100);                                   /* OP 1 -> OP 2 */
        CHECK(getn(e, "m1_2") > 0.99 && getn(e, "m2_1") < 0.01, "edit matrix wrote m1_2=%.2f m2_1=%.2f", getn(e, "m1_2"), getn(e, "m2_1"));
        set(e, "x_l2", 30);
        CHECK(std::fabs(getn(e, "o2_l2") - 0.3f) < 0.01f && std::fabs(getn(e, "x_l2") - 0.3f) < 0.01f, "edit level 2");
        int before = g_updates;
        set(e, "sel", 0); run(e, 0.01);
        CHECK(display(e, "x_coarse") == "1" && getn(e, "x_from1") < 0.01 && g_updates > before, "edit knobs did not follow EDIT (x_coarse %s)", display(e, "x_coarse").c_str());
        /* a Q-Link event (1/128 of the range) on an edit option steps one option */
        set(e, "sel", 2);
        const int i = pidx("x_coarse");
        float n0 = e->getParameter(e, i);
        e->setParameter(e, i, n0 + 1.0f / 128);
        CHECK(display(e, "o3_coarse") == "2", "Q-Link step on edit ratio: %s", display(e, "o3_coarse").c_str());
        e->setParameter(e, pidx("sel"), getn(e, "sel") + 1.0f / 128);   /* EDIT steps too */
        CHECK(display(e, "sel") == "OP 4", "Q-Link step on EDIT: %s", display(e, "sel").c_str());
        /* PITCH: the envelope knobs edit the pitch envelope, operator knobs show -- */
        set(e, "sel", 8);
        set(e, "x_l1", 75);                                      /* the knob's centre is 0 for the pitch envelope */
        CHECK(display(e, "x_l1") == "+50" && display(e, "pe_l1") == "+50" && display(e, "x_coarse") == "--", "PITCH edit: %s %s %s",
              display(e, "x_l1").c_str(), display(e, "pe_l1").c_str(), display(e, "x_coarse").c_str());
        set(e, "x_coarse", 9);   /* ignored */
        bool same = true; for (int o = 1; o <= 8; o++) same &= display(e, op(o, "coarse").c_str()) == (o == 2 ? "4" : o == 3 ? "2" : "1");
        CHECK(same, "an operator knob wrote something with EDIT = PITCH");
        set(e, "sel", 0);
    }

    /* 8b. host rules of mpc-vst-plugins (NOTES 2026-10-21): no UpdateDisplay for plain writes, at most one per
     * 50 ms; parameter 0 (EDIT) set to the far end and straight back (MPC on insert and STOP) changes nothing */
    {
        run(e, 0.2);
        int before = g_updates;
        for (int k = 0; k < 32; k++) { e->setParameter(e, pidx("bright"), (k % 8) / 8.0f); e->setParameter(e, pidx("x_level"), (k % 5) / 5.0f); run(e, 0.006); }
        CHECK(g_updates == before, "%d UpdateDisplay for 32 plain writes", g_updates - before);
        before = g_updates;
        for (int k = 0; k < 64; k++) { set(e, "sel", k % 9); run(e, 0.006); }   /* 0.38 s of EDIT changes */
        CHECK(g_updates - before >= 2 && g_updates - before <= 9, "UpdateDisplay cap: %d in 0.38 s", g_updates - before);
        set(e, "sel", 3); run(e, 0.1);
        before = g_updates;
        e->setParameter(e, 0, 1.0f); e->setParameter(e, 0, 3.0f / 8);
        run(e, 0.1);
        CHECK(display(e, "sel") == "OP 4" && g_updates == before, "parameter 0 toggle: %s, %d updates", display(e, "sel").c_str(), g_updates - before);
        set(e, "sel", 0); set(e, "bright", 100);
    }

    /* 9. presets */
    {
        CHECK(progname(e, 0) == "GOA LEAD" && progname(e, 25) == "ANGRY OVERTURE" && progname(e, 31) == "SQUARE EMOTIONS", "program names: %s / %s / %s",
              progname(e, 0).c_str(), progname(e, 25).c_str(), progname(e, 31).c_str());
        for (int s = 0; s < 32; s++) {
            e->dispatcher(e, 2, 0, s, nullptr, 0);
            const bool drum = progname(e, s) == "FM KICK" || progname(e, s) == "FM HAT";
            Buf R;
            on(e, drum ? 48 : 57, 110); if (!drum) { on(e, 64, 100); on(e, 69, 90); }
            Buf a = run(e, 2.0, &R);
            off(e, drum ? 48 : 57); off(e, 64); off(e, 69);
            Buf t = run(e, 4.0);
            double lr = std::max(rms(a, 0.0), rms(R, 0.0));
            std::printf("  %2d %-18s %6.1f dBFS rms, peak %.2f, tail %.4f\n", s + 1, progname(e, s).c_str(), db(lr), std::max(peak(a), peak(R)), peak(t, 3.5));
            CHECK(drum ? std::max(peak(a), peak(R)) > 0.05 : lr > 0.01, "%s silent", progname(e, s).c_str());
            CHECK(peak(t, 3.5) < 1e-3, "%s does not end", progname(e, s).c_str());
        }
        /* SAVE / LOAD: slot 32 */
        plain(e);
        set(e, "m2_1", 77); set(e, op(2, "coarse").c_str(), 7); set(e, "sel", 1);
        e->dispatcher(e, 2, 0, 25, nullptr, 0);   /* select slot 26 (loads it) ... */
        plain(e);
        set(e, "m2_1", 77); set(e, op(2, "coarse").c_str(), 7);
        set(e, "slot", 32); press(e, "save");
        CHECK(progname(e, 31) == "Okto 32", "saved slot name %s", progname(e, 31).c_str());
        set(e, "m2_1", 0); set(e, op(2, "coarse").c_str(), 2);
        press(e, "load");
        CHECK(std::fabs(getn(e, "m2_1") - 0.77f) < 0.01f && display(e, "o2_coarse") == "6", "LOAD restored m2_1 %.2f, ratio %s", getn(e, "m2_1"), display(e, "o2_coarse").c_str());
        FILE *f = std::fopen(bank, "r");
        CHECK(f != nullptr, "bank file not written");
        if (f) std::fclose(f);
        AEffect *e2 = mainf(master);
        e2->dispatcher(e2, 10, 0, 0, nullptr, SR);
        CHECK(progname(e2, 31) == "Okto 32", "second instance does not see the saved slot");
        e2->dispatcher(e2, 2, 0, 31, nullptr, 0);
        CHECK(std::fabs(e2->getParameter(e2, pidx("m2_1")) - 0.77f) < 0.01f, "program change on another instance");
        /* chunk round trip, EDIT included */
        e->dispatcher(e, 2, 0, 0, nullptr, 0);
        set(e, "sel", 4); set(e, "bright", 150);
        void *ptr = nullptr;
        intptr_t len = e->dispatcher(e, 23, 0, 0, &ptr, 0);
        std::string ch((const char *)ptr, (size_t)len);
        std::printf("  chunk of GOA LEAD: %d bytes\n", (int)len);
        e2->dispatcher(e2, 24, 0, len, (void *)ch.data(), 0);
        bool eq = true;
        for (int i = 0; i < NPARAMS; i++) {
            const std::string k = PARAMS[i].key;
            if (k == "slot" || k == "load" || k == "save" || k.find("__open") != std::string::npos) continue;
            if (std::fabs(e->getParameter(e, i) - e2->getParameter(e2, i)) > 1e-3f) { eq = false; std::printf("  chunk differs at %s\n", k.c_str()); break; }
        }
        CHECK(eq, "chunk round trip");
        on(e, 60); on(e2, 60);
        Buf c1 = run(e, 0.5), c2 = run(e2, 0.5);
        off(e, 60); off(e2, 60); run(e, 2); run(e2, 2);
        CHECK(std::fabs(rms(c1) - rms(c2)) < 0.02 * rms(c1) + 1e-4, "chunk restored a different sound");
        e2->dispatcher(e2, 1, 0, 0, nullptr, 0);
    }

    /* 10. stress: random values on every parameter, notes, CCs */
    {
        uint32_t rs = 12345;
        auto rnd = [&]() { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (rs >> 8) / 16777216.0f; };
        double pk = 0;
        for (int round = 0; round < 40; round++) {
            for (int j = 0; j < 30; j++) {
                const int i = (int)(rnd() * NPARAMS);
                const std::string k = PARAMS[i].key;
                if (k == "save" || k == "load") continue;
                e->setParameter(e, i, rnd());
            }
            for (int j = 0; j < 4; j++) { on(e, 30 + (int)(rnd() * 70), 1 + (int)(rnd() * 126)); }
            send(e, 0xb0, 1, (int)(rnd() * 127)); send(e, 0xe0, 0, (int)(rnd() * 127));
            if (rnd() < 0.3f) send(e, 0xb0, 64, rnd() < 0.5f ? 127 : 0);
            Buf a = run(e, 0.15);
            pk = std::max(pk, peak(a));
            for (int n = 30; n < 100; n++) off(e, n);
        }
        send(e, 0xb0, 123, 0); send(e, 0xb0, 64, 0);
        run(e, 0.5);
        CHECK(pk < 0.99, "output ceiling: peak %.3f", pk);
    }
    CHECK(!bad, "NaN, Inf or denormal in the output");

    e->dispatcher(e, 1, 0, 0, nullptr, 0);
    std::printf(fails ? "FAILED (%d)\n" : "PASSED\n", fails);
    return fails ? 1 : 0;
}
