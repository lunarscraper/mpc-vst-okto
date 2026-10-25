#!/usr/bin/env python3
"""Generates Okto's parameter list and factory sounds - the single source for:
  vst/module.json     the VST parameters (key, name, range/options) for mpc-vst-plugins' gen_vst.py
  vst/okto_params.h   the key table and index constants for okto_vst.cpp, the start values, the factory sounds

Run after changing a parameter or a factory sound:  python3 tools/gen_okto.py
New parameters are appended at the END of their family (params.lock in vst/ keeps the skin bound by index:
an existing key must never move). The factory sounds live in tools/factory.py; libfmsynth's own presets
(vst/presets-libfmsynth/*.fmp) are converted here."""
import json, math, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
VST = os.path.join(HERE, "..", "vst")
sys.path.insert(0, HERE)

WAVES = ["SINE", "HALF", "ABS", "QUARTER", "SQUARE", "SAW"]
COARSE = ["FIX", "0.5"] + [str(i) for i in range(1, 33)]
EMODES = ["ONCE", "LOOP", "STEP", "STEP LOOP"]
POINTS = [str(i) for i in range(1, 9)]
ESYNC = ["FREE", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1/1"]
LFOW = ["SINE", "TRIANGLE", "SAW DOWN", "SAW UP", "SQUARE", "S&H"]
LFOSYNC = ["FREE", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T", "1/16T"]
SEL = ["OP 1", "OP 2", "OP 3", "OP 4", "OP 5", "OP 6", "OP 7", "OP 8", "PITCH"]

def P(key, name, lo=None, hi=None, options=None, access=None):
    d = {"key": key, "name": name}
    if options is not None: d["options"] = options
    elif lo is not None: d["min"], d["max"] = lo, hi
    if access: d["access"] = access
    return d

# operator fields (proxy order = real order)
OPF = [("wave", "Wave", None, None, WAVES), ("coarse", "Ratio", None, None, COARSE), ("fine", "Fine", 0, 100, None),
       ("offset", "Offset", -100, 100, None), ("level", "Level", 0, 100, None), ("out", "Out", 0, 100, None),
       ("pan", "Pan", -100, 100, None), ("vel", "Velocity", 0, 100, None), ("kslo", "Key Lo", -100, 100, None),
       ("kshi", "Key Hi", -100, 100, None), ("am", "LFO Amp", 0, 100, None), ("pm", "LFO Pitch", 0, 100, None),
       ("penv", "Pitch Env", 0, 100, None), ("wheel", "Wheel", 0, 100, None)]
def envf(bipolar):
    lo = -100 if bipolar else 0
    return ([("emode", "Env Mode", None, None, EMODES), ("epts", "Env Points", None, None, POINTS),
             ("eloop", "Env Loop", None, None, POINTS), ("esync", "Env Sync", None, None, ESYNC),
             ("erel", "Env Release", 0, 100, None)]
            + [("l%d" % i, "Env L%d" % i, lo, 100, None) for i in range(1, 9)]
            + [("t%d" % i, "Env T%d" % i, 0, 100, None) for i in range(1, 9)])
ENVF = envf(False)
GLOBAL = [P("volume", "Volume", 0, 100), P("bright", "Bright", 0, 200), P("feedback", "Feedback", 0, 200),
          P("env_time", "Env Time", -100, 100), P("rel_scale", "Release", -100, 100), P("lfo_rate", "LFO Rate", 0, 100),
          P("lfo_wave", "LFO Wave", options=LFOW), P("lfo_sync", "LFO Sync", options=LFOSYNC), P("lfo_delay", "LFO Delay", 0, 100),
          P("lfo_key", "LFO Key Sync", options=["FREE", "KEY"]), P("vibrato", "Vibrato", 0, 100), P("mw_vib", "Wheel Vibrato", 0, 100),
          P("mw_bright", "Wheel Bright", 0, 100), P("vmode", "Voice Mode", options=["POLY", "MONO", "LEGATO"]),
          P("voices", "Voices", options=[str(i) for i in range(1, 17)]), P("glide", "Glide", 0, 100),
          P("unison", "Unison", options=["1", "2", "3"]), P("detune", "Detune", 0, 100), P("width", "Width", 0, 100),
          P("transpose", "Transpose", options=["%+d" % i if i else "0" for i in range(-24, 25)]),
          P("bend", "Bend Range", options=[str(i) for i in range(0, 13)]), P("pe_depth", "Pitch Env Depth", 0, 48)]

def params():
    ps = [P("sel", "Edit", options=SEL)]
    for k, n, lo, hi, o in OPF: ps.append(P("x_" + k, "Edit " + n, lo, hi, o))
    for s in range(1, 9): ps.append(P("x_from%d" % s, "Edit From %d" % s, 0, 100))
    for k, n, lo, hi, o in ENVF: ps.append(P("x_" + k, "Edit " + n, lo, hi, o))
    ps += GLOBAL
    ps += [P("slot", "Preset", 1, 32), P("load", "Load", access="write"), P("save", "Save", access="write")]
    for op in range(1, 9):
        for k, n, lo, hi, o in OPF + ENVF: ps.append(P("o%d_%s" % (op, k), "Op%d %s" % (op, n), lo, hi, o))
    for s in range(1, 9):
        for d in range(1, 9): ps.append(P("m%d_%d" % (s, d), "Op%d > Op%d" % (s, d), 0, 100))
    for k, n, lo, hi, o in envf(True): ps.append(P("pe_" + k, "PEnv " + n[4:], lo, hi, o))
    return ps

# ---- unit laws (must match okto_vst.cpp) ----------------------------------------------------------
def u_level(amp): return 0.0 if amp <= 0 else max(0.5, min(100.0, 100 * math.log(amp / 0.005) / math.log(3200)))
def u_time(s): return 0.0 if s <= 0.001 else min(100.0, 100 * math.log(s / 0.001) / math.log(20000))
def u_pm(v): return min(100.0, 100 * math.sqrt(max(v, 0) / 0.06))
def u_lfo(hz): return max(0.0, min(100.0, 100 * math.log(hz / 0.05) / math.log(1280)))
def u_offset(hz): return math.copysign(100 * (abs(hz) / 4000) ** (1 / 3), hz) if hz else 0.0
def u_coarse(fm):
    """ratio -> (option index, fine 0..100)"""
    if fm <= 0: return 0, 0.0
    if fm < 1: return 1, (fm - 0.5) * 100
    c = min(32, int(math.floor(fm + 1e-6)))
    return c + 1, (fm - c) * 100

# ---- start values (a plain sine on OP 1, libfmsynth's default envelope everywhere) --------------
def defaults():
    d = {"sel": 0, "volume": 50, "bright": 100, "feedback": 100, "env_time": 0, "rel_scale": 0, "lfo_rate": 60,
         "lfo_wave": 0, "lfo_sync": 0, "lfo_delay": 0, "lfo_key": 1, "vibrato": 0, "mw_vib": 30, "mw_bright": 0,
         "vmode": 0, "voices": 7, "glide": 0, "unison": 0, "detune": 15, "width": 50, "transpose": 24, "bend": 2, "pe_depth": 12}
    for op in range(1, 9):
        o = "o%d_" % op
        d.update({o + "wave": 0, o + "coarse": 2, o + "fine": 0, o + "offset": 0, o + "level": round(u_level(1.0), 3),
                  o + "out": 100 if op == 1 else 0, o + "pan": 0, o + "vel": 100, o + "kslo": 0, o + "kshi": 0, o + "am": 0,
                  o + "pm": 0, o + "penv": 100, o + "wheel": 0, o + "emode": 0, o + "epts": 2, o + "eloop": 0, o + "esync": 0,
                  o + "erel": round(u_time(0.5), 3)})
        for i, (l, t) in enumerate(zip([100, 50, 25, 0, 0, 0, 0, 0], [0.05, 0.05, 0.25, 0.1, 0.1, 0.1, 0.1, 0.1])):
            d[o + "l%d" % (i + 1)] = l
            d[o + "t%d" % (i + 1)] = round(u_time(t), 3)
    for s in range(1, 9):
        for t in range(1, 9): d["m%d_%d" % (s, t)] = 0
    d.update({"pe_emode": 0, "pe_epts": 0, "pe_eloop": 0, "pe_esync": 0, "pe_erel": round(u_time(0.5), 3)})
    for i in range(1, 9):
        d["pe_l%d" % i] = 0
        d["pe_t%d" % i] = round(u_time(0.1), 3)
    return d

# ---- libfmsynth presets ---------------------------------------------------------------------------
FMP_PARAMS = ["amp", "pan", "freq_mod", "freq_offset", "t0", "t1", "t2", "d0", "d1", "d2", "rel", "ks_mid", "ks_lo", "ks_hi",
              "vel", "wheel", "lfo_amp", "lfo_freq", "enable", "carrier"] + ["mod%d" % i for i in range(8)]
def unpack(v):
    if v == 0: return 0.0
    e = struct.unpack(">h", struct.pack(">H", v >> 16))[0]
    f = struct.unpack(">h", struct.pack(">H", v & 0xFFFF))[0] / 0x8000
    return math.ldexp(f, e)
def read_fmp(path):
    b = open(path, "rb").read()
    assert b[:8] == b"FMSYNTH1", path
    name = b[8:72].split(b"\0")[0].decode()
    vals = [unpack(struct.unpack(">I", b[136 + 4 * i:140 + 4 * i])[0]) for i in range((len(b) - 136) // 4)]
    g, v = vals[:2], vals[2:]
    p = {n: v[i * 8:(i + 1) * 8] for i, n in enumerate(FMP_PARAMS)}
    return name, {"volume": g[0], "lfo_freq": g[1]}, p
def convert_fmp(path):
    name, g, p = read_fmp(path)
    s = {"volume": 100 * math.sqrt(g["volume"] / 0.8), "lfo_rate": u_lfo(g["lfo_freq"]), "lfo_key": 1, "mw_vib": 0}
    for o in range(8):
        k = "o%d_" % (o + 1)
        ci, fine = u_coarse(p["freq_mod"][o])
        s[k + "coarse"], s[k + "fine"] = ci, fine
        s[k + "offset"] = u_offset(p["freq_offset"][o])
        s[k + "level"] = u_level(p["amp"][o]) if p["enable"][o] > 0.5 else 0
        s[k + "out"] = 100 * math.sqrt(max(0, p["carrier"][o]))
        s[k + "pan"] = 100 * p["pan"][o]
        s[k + "vel"] = 100 * p["vel"][o]
        s[k + "kslo"], s[k + "kshi"] = 50 * p["ks_lo"][o], 50 * p["ks_hi"][o]
        s[k + "am"] = 100 * p["lfo_amp"][o]
        s[k + "pm"] = u_pm(p["lfo_freq"][o])
        s[k + "wheel"] = 100 * p["wheel"][o]
        s[k + "epts"] = 2
        for i, (t, d) in enumerate([("t0", "d0"), ("t1", "d1"), ("t2", "d2")]):
            s[k + "l%d" % (i + 1)] = 100 * p[t][o]
            s[k + "t%d" % (i + 1)] = u_time(p[d][o])
        s[k + "erel"] = u_time(p["rel"][o])
        for src in range(8):   # mod<src>[dst]: src modulates dst
            s["m%d_%d" % (src + 1, o + 1)] = 100 * math.sqrt(max(0, p["mod%d" % src][o]))
    return name.upper(), s

# ---- level trims --------------------------------------------------------------------------------
# dB per factory sound so that one note peaks near -20 dBFS rms (drums: 0.5 peak), measured with the plugin
# (one note, the loudest 0.5 s). VOLUME is squared, so a trim of t dB scales it by 10^(t/40).
TRIM_DB = {
    "GOA LEAD": -2.2,
    "ALIEN TALK": -3.6,
    "ACID FM BASS": -3.7,
    "PSY ROLL BASS": 2.0,
    "TRANCEGATE PAD": -3.0,
    "TIMBRE SEQ": -3.8,
    "PULSE GRID": -1.5,
    "BELL TREE": -2.2,
    "E-PIANO": -5.2,
    "BRASS STAB": -1.7,
    "SUPERSAW FM": 5.6,
    "HOOVER FM": 3.0,
    "METAL ZAP": 3.5,
    "LASER": 0.4,
    "RISER": 11.1,
    "FM KICK": 1.5,
    "FM HAT": 8.0,
    "WOBBLE BASS": -4.6,
    "GLASS PAD": -0.8,
    "DRONE CLUSTER": 0.6,
    "DRAWBAR ORGAN": -1.5,
    "MARIMBA": -1.7,
    "CLAV": -0.8,
    "SQUELCH SEQ": -3.6,
    "PSY STAB": 4.6,
    "ANGRY OVERTURE": 4.6,
    "CLOCKY": 0.0,
    "EXPRESSIVE HARP": -2.7,
    "LEAD PAD": 6.3,
    "OLD-SCHOOL ORGAN": -3.0,
    "RETRO SCI-FI": -1.9,
    "SQUARE EMOTIONS": -1.8
}

def trimmed(name, d, base):
    t = TRIM_DB.get(name, 0.0)
    if t:
        v = min(100.0, float(d.get("volume", base["volume"])) * 10 ** (t / 40))
        d["volume"] = round(v, 1)
    return name, d

# ---- output ---------------------------------------------------------------------------------------
def c_str(s): return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'
def fmt(v): return ("%.3f" % v).rstrip("0").rstrip(".") if isinstance(v, float) else str(v)

def state(d, base):
    """only what differs from the start values; values in module.json units"""
    out = []
    for k, v in d.items():
        if k not in base: raise SystemExit("unknown key in a factory sound: " + k)
        v = round(float(v), 3)
        if abs(v - round(float(base[k]), 3)) > 1e-6: out.append("%s=%s;" % (k, fmt(v)))
    return "".join(out)

def main():
    ps = params()
    keys = [p["key"] for p in ps]
    assert len(keys) == len(set(keys))
    mod = {"_comment": "Generated by tools/gen_okto.py - do not edit. Not a real Schwung module: chain_params lists the VST "
                       "parameters of okto_vst.cpp for mpc-vst-plugins' tools/gen_vst.py (params.h + skin). New parameters are "
                       "appended; projects and presets restore by key.",
           "chain_params": ps}
    with open(os.path.join(VST, "module.json"), "w") as f:
        json.dump(mod, f, indent=1)
        f.write("\n")

    base = defaults()
    for k in base: assert k in keys, k
    import factory
    sounds = factory.sounds(sys.modules[__name__])
    fdir = os.path.join(VST, "presets-libfmsynth")
    for fn in factory.IMPORTS:
        sounds.append(convert_fmp(os.path.join(fdir, fn)))
    assert len(sounds) <= 32, len(sounds)
    sounds = [trimmed(n, d, base) for n, d in sounds]

    nopf, nenv = len(OPF), len(ENVF)
    L = ["/* Generated by tools/gen_okto.py - do not edit. */", "#pragma once", ""]
    first = lambda k: keys.index(k)
    L += ["enum {",
          "    K_SEL = %d, X_OP0 = %d, X_FROM0 = %d, X_ENV0 = %d," % (first("sel"), first("x_wave"), first("x_from1"), first("x_emode")),
          "    G0 = %d, K_SLOT = %d, K_LOAD = %d, K_SAVE = %d," % (first("volume"), first("slot"), first("load"), first("save")),
          "    OPR0 = %d, MAT0 = %d, PE0 = %d, NKEYS = %d," % (first("o1_wave"), first("m1_1"), first("pe_emode"), len(keys)),
          "    NOPF = %d, NENVF = %d, NOPALL = %d," % (nopf, nenv, nopf + nenv),
          "};"]
    L += ["enum { " + ", ".join("G_%s = %d" % (p["key"].upper(), keys.index(p["key"])) for p in GLOBAL) + " };"]
    L += ["enum { " + ", ".join("F_%s = %d" % (k.upper(), i) for i, (k, *_r) in enumerate(OPF)) + " };"]
    L += ["enum { " + ", ".join("E_%s = %d" % (k.upper(), i) for i, (k, *_r) in enumerate(ENVF)) + " };"]
    L += ["static const char *const KEYS[NKEYS] = {"] + ["    %s," % c_str(k) for k in keys] + ["};"]
    L += ["struct KV { const char *key; float v; };", "static const KV START[] = {"]
    L += ["    {%s, %sf}," % (c_str(k), fmt(float(v)) if "." in fmt(float(v)) else fmt(float(v)) + ".0") for k, v in base.items()]
    L += ["};", "struct Factory { const char *name, *state; };", "static const Factory FACTORY[] = {"]
    for name, d in sounds:
        L.append("    {%s, %s}," % (c_str(name[:23]), c_str(state(d, base))))
    L += ["};", ""]
    with open(os.path.join(VST, "okto_params.h"), "w") as f:
        f.write("\n".join(L))
    print("%d parameters, %d factory sounds" % (len(keys), len(sounds)))

if __name__ == "__main__":
    main()
