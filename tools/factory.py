"""Okto's factory sounds (slots 1-25) - read by tools/gen_okto.py, which adds libfmsynth's own seven
presets (slots 26-32, converted from vst/presets-libfmsynth/*.fmp).

A sound is written in musical units and converted to the plugin's knob units here:
  op(ratio, level, out, env, ...)    level = operator amplitude (1 = unity; as a modulator ~ the FM index)
  env([(level %, seconds), ...])     free times; ENV_SYNC: env([(level %, divisions)], sync="1/16")
  mod {(src, dst): amount %}        src modulates dst; (n, n) is feedback
Only what differs from the start values ends up in the preset."""

IMPORTS = ["angry_overture.fmp", "clocky.fmp", "expressive_harp.fmp", "lead_pad.fmp", "old_school_organ.fmp",
           "retro_sci_fi.fmp", "square_emotions.fmp"]


def sounds(g):
    WAVE = {w: i for i, w in enumerate(g.WAVES)}
    EMODE = {m: i for i, m in enumerate(g.EMODES)}
    ESYNC = {s: i for i, s in enumerate(g.ESYNC)}
    LSYNC = {s: i for i, s in enumerate(g.LFOSYNC)}
    LWAVE = {s: i for i, s in enumerate(g.LFOW)}

    def env(points, mode="ONCE", loop=1, sync=None, rel=0.5):
        d = {"emode": EMODE[mode], "epts": len(points) - 1, "eloop": loop - 1, "esync": ESYNC[sync or "FREE"],
             "erel": g.u_time(rel)}
        for i, (lv, t) in enumerate(points):
            d["l%d" % (i + 1)] = lv
            d["t%d" % (i + 1)] = (t - 1) * 100.0 / 15 if sync else g.u_time(t)
        return d

    def op(ratio=1.0, level=1.0, out=0, env=None, wave="SINE", fixed=None, offset=0.0, pan=0, vel=100,
           kslo=0, kshi=0, am=0, pm=0, penv=100, wheel=0):
        d = {}
        if fixed is not None:
            d["coarse"], d["fine"], d["offset"] = 0, 0, g.u_offset(fixed)
        else:
            d["coarse"], d["fine"] = g.u_coarse(ratio)
            d["offset"] = g.u_offset(offset)
        d.update({"level": g.u_level(level), "out": out, "wave": WAVE[wave], "pan": pan, "vel": vel, "kslo": kslo,
                  "kshi": kshi, "am": am, "pm": pm, "penv": penv, "wheel": wheel})
        d.update(env or {})
        return d

    S = []

    def snd(name, ops, mod=None, pe=None, pe_depth=None, **glob):
        d = {"o1_out": 0}
        for k, v in glob.items():
            if k == "lfo_sync": v = LSYNC[v]
            elif k == "lfo_wave": v = LWAVE[v]
            elif k == "vmode": v = ["POLY", "MONO", "LEGATO"].index(v)
            elif k == "unison": v = v - 1
            elif k == "transpose": v = v + 24
            elif k == "lfo_rate": v = g.u_lfo(v)
            elif k == "glide": v = 0 if v <= 0 else 100 * __import__("math").log(v / 0.002) / __import__("math").log(1000)
            elif k == "volume": v = v
            d[k] = v
        for n, o in ops.items():
            for k, v in o.items():
                d["o%d_%s" % (n, k)] = v
        for (s, t), v in (mod or {}).items():
            d["m%d_%d" % (s, t)] = v
        if pe:
            for k, v in pe.items():
                d["pe_" + k] = v
        if pe_depth is not None:
            d["pe_depth"] = pe_depth
        S.append((name, d))

    PLUCK = lambda dec, rel=0.2: env([(100, 0.001), (0, dec)], rel=rel)
    SUS = lambda a=0.003, d=0.3, s=80, rel=0.3: env([(100, a), (s, d)], rel=rel)

    # 1 - the classic psytrance lead: a feedback stack (saw-like) with a bite on the attack, legato glide
    snd("GOA LEAD", {
        1: op(1, 1, 100, SUS(0.004, 0.4, 85, 0.18)),
        2: op(1, 2.6, 0, env([(100, 0.002), (60, 0.5)], rel=0.2), vel=40),
        3: op(2, 1.6, 0, env([(100, 0.001), (15, 0.15)], rel=0.1)),
    }, mod={(2, 1): 100, (2, 2): 55, (3, 1): 60},
        vmode="LEGATO", glide=0.06, unison=2, detune=10, width=60, mw_vib=45, vibrato=4, lfo_rate=5.5, lfo_delay=35, volume=62)

    # 2 - talking lead: the modulator's level is a 1/16 step sequence, an S&H LFO shakes a formant carrier
    snd("ALIEN TALK", {
        1: op(1, 1, 100, SUS(0.003, 0.2, 90, 0.15)),
        2: op(1, 3.0, 0, env([(100, 1), (25, 1), (80, 1), (10, 1), (60, 1), (95, 1), (20, 1), (50, 1)], "STEP LOOP", sync="1/16")),
        3: op(5, 1, 45, SUS(0.003, 0.2, 90, 0.15), am=80),
        4: op(2, 1.5, 0, env([(100, 1), (40, 1), (0, 1)], "LOOP", sync="1/8")),
    }, mod={(2, 1): 100, (2, 3): 70, (4, 2): 60, (2, 2): 30},
        vmode="LEGATO", glide=0.05, lfo_wave="S&H", lfo_sync="1/16", lfo_key=1, mw_vib=30, volume=60)

    # 3 - a squelchy mono bass: the feedback modulator decays like a closing filter
    snd("ACID FM BASS", {
        1: op(1, 1, 100, env([(100, 0.001), (75, 0.3)], rel=0.08)),
        2: op(1, 2.8, 0, env([(100, 0.001), (12, 0.2)], rel=0.08), vel=70),
        3: op(0.5, 1, 55, env([(100, 0.001), (80, 0.3)], rel=0.08)),
    }, mod={(2, 1): 100, (2, 2): 70},
        vmode="MONO", glide=0.04, transpose=-12, mw_bright=60, volume=64)

    # 4 - short, round psytrance rolling bass for 1/16 notes
    snd("PSY ROLL BASS", {
        1: op(1, 1, 100, env([(100, 0.001), (55, 0.06), (0, 0.09)], rel=0.03)),
        2: op(1, 1.8, 0, env([(100, 0.0005), (0, 0.06)], rel=0.03), vel=60),
        3: op(0.5, 1, 70, env([(100, 0.001), (60, 0.07), (0, 0.08)], rel=0.03)),
    }, mod={(2, 1): 100, (2, 2): 25},
        vmode="MONO", transpose=-12, volume=70)

    # 5 - a detuned pad chopped by a 1/16 gate pattern on the carriers (trancegate inside the synth)
    gate = [(100, 1), (0, 1), (100, 1), (100, 1), (0, 1), (100, 1), (0, 1), (100, 1)]
    snd("TRANCEGATE PAD", {
        1: op(1, 1, 100, env(gate, "STEP LOOP", sync="1/16", rel=0.4), pan=-30),
        2: op(1, 1.3, 0, env([(100, 0.3), (70, 1.0)], rel=0.6)),
        3: op(1, 1, 100, env(gate, "STEP LOOP", sync="1/16", rel=0.4), offset=1.2, pan=30),
        4: op(2, 0.9, 0, env([(100, 0.5), (60, 1.5)], rel=0.6)),
    }, mod={(2, 1): 100, (4, 3): 100, (2, 2): 20},
        unison=3, detune=22, width=80, volume=52)

    # 6 - hold a key: the brightness walks through an 8-step pattern while a 5-point loop bends it
    snd("TIMBRE SEQ", {
        1: op(1, 1, 100, SUS(0.005, 0.2, 90, 0.3)),
        2: op(2, 2.5, 0, env([(100, 1), (20, 1), (70, 1), (5, 1), (90, 1), (40, 1), (60, 1), (10, 1)], "STEP LOOP", sync="1/16")),
        3: op(3, 1.5, 0, env([(0, 1), (100, 2), (30, 1), (80, 1), (0, 1)], "LOOP", sync="1/16")),
    }, mod={(2, 1): 100, (3, 2): 70, (2, 2): 25}, volume=60)

    # 7 - polyrhythm from one key: three carriers gated by patterns of 8, 6 and 5 steps
    snd("PULSE GRID", {
        1: op(1, 1, 100, env([(100, 1), (0, 1), (0, 1), (100, 1), (0, 1), (0, 1), (100, 1), (0, 1)], "STEP LOOP", sync="1/16", rel=0.1), pan=-40),
        2: op(3, 1.5, 0, env([(100, 0.001), (30, 0.1)], rel=0.1)),
        3: op(2, 1, 80, env([(0, 1), (0, 1), (100, 1), (0, 1), (100, 1), (0, 1)], "STEP LOOP", sync="1/16", rel=0.1), pan=40),
        4: op(5, 1.2, 0, env([(100, 0.001), (40, 0.2)], rel=0.1)),
        5: op(4, 1, 60, env([(100, 1), (0, 1), (0, 1), (60, 1), (0, 1)], "STEP LOOP", sync="1/16", rel=0.1)),
        6: op(1, 2.0, 0),
    }, mod={(2, 1): 100, (4, 3): 100, (6, 5): 80, (6, 6): 40}, volume=58)

    # 8 - inharmonic bells, long ring
    snd("BELL TREE", {
        1: op(1, 1, 100, env([(100, 0.002), (40, 1.2), (0, 3.5)], rel=1.5), pan=-25),
        2: op(3.5, 1.8, 0, env([(100, 0.001), (30, 1.5), (0, 2.5)], rel=1.5), vel=80),
        3: op(2, 1, 70, env([(100, 0.002), (30, 0.8), (0, 2.5)], rel=1.2), offset=0.7, pan=25),
        4: op(7, 1.2, 0, env([(100, 0.001), (0, 0.9)], rel=1)),
    }, mod={(2, 1): 100, (4, 3): 100}, volume=56)

    # 9 - DX-style tine piano
    snd("E-PIANO", {
        1: op(1, 1, 100, env([(100, 0.001), (55, 1.2), (0, 3.0)], rel=0.35)),
        2: op(1, 1.2, 0, env([(100, 0.001), (30, 1.0), (10, 2.0)], rel=0.35), vel=100),
        3: op(1, 1, 55, env([(100, 0.001), (40, 0.8), (0, 2.0)], rel=0.3), offset=0.8),
        4: op(14, 0.8, 0, env([(100, 0.0005), (0, 0.15)], rel=0.1), vel=100),
    }, mod={(2, 1): 100, (4, 3): 100}, volume=62)

    # 10 - brassy stab, slow-ish bite
    snd("BRASS STAB", {
        1: op(1, 1, 100, env([(100, 0.03), (75, 0.4)], rel=0.15)),
        2: op(1, 2.4, 0, env([(40, 0.001), (100, 0.06), (65, 0.4)], rel=0.15), vel=80),
        3: op(1, 1, 80, env([(100, 0.03), (75, 0.4)], rel=0.15), offset=1.5),
    }, mod={(2, 1): 100, (2, 3): 90, (2, 2): 35}, unison=2, detune=12, width=50, volume=56)

    # 11 - three saw-wave carriers, wide unison
    snd("SUPERSAW FM", {
        1: op(1, 1, 100, SUS(0.01, 0.3, 90, 0.4), wave="SAW"),
        2: op(1, 1, 80, SUS(0.01, 0.3, 90, 0.4), wave="SAW", offset=-1.8, pan=-60),
        3: op(1, 1, 80, SUS(0.01, 0.3, 90, 0.4), wave="SAW", offset=2.1, pan=60),
        4: op(2, 0.4, 0, env([(100, 0.002), (40, 0.4)], rel=0.4)),
    }, mod={(4, 1): 100, (4, 2): 100, (4, 3): 100}, unison=3, detune=28, width=100, volume=46)

    # 12 - hoover: pitch dive and rise, saw carrier, fat unison
    snd("HOOVER FM", {
        1: op(1, 1, 100, SUS(0.01, 0.3, 90, 0.3), wave="SAW"),
        2: op(0.5, 2.0, 0, SUS(0.01, 0.3, 80, 0.3)),
        3: op(1, 1, 70, SUS(0.01, 0.3, 90, 0.3), wave="SQUARE", offset=-2),
    }, mod={(2, 1): 100, (2, 3): 60},
        pe={"epts": 2, "l1": 0, "t1": g.u_time(0.001), "l2": -100, "t2": g.u_time(0.12), "l3": 0, "t3": g.u_time(0.3)},
        pe_depth=7, vmode="MONO", glide=0.08, unison=3, detune=40, width=90, volume=48)

    # 13 - metallic psy zap: steep pitch drop on a screaming feedback stack
    snd("METAL ZAP", {
        1: op(1, 1, 100, env([(100, 0.001), (0, 0.3)], rel=0.1)),
        2: op(3.5, 4.0, 0, env([(100, 0.001), (20, 0.25)], rel=0.1)),
    }, mod={(2, 1): 100, (2, 2): 80},
        pe={"epts": 1, "l1": 100, "t1": g.u_time(0.001), "l2": 0, "t2": g.u_time(0.12)}, pe_depth=36, volume=58)

    # 14 - laser: a long pitch sweep down
    snd("LASER", {
        1: op(1, 1, 100, env([(100, 0.001), (0, 0.8)], rel=0.1)),
        2: op(2, 2.0, 0, env([(100, 0.001), (30, 0.7)], rel=0.1)),
    }, mod={(2, 1): 100},
        pe={"epts": 1, "l1": 100, "t1": g.u_time(0.001), "l2": -100, "t2": g.u_time(0.7)}, pe_depth=24, volume=58)

    # 15 - riser: four seconds of pitch and brightness going up, shaken by S&H
    snd("RISER", {
        1: op(1, 1, 100, env([(0, 0.001), (100, 4.0)], rel=1.0), wave="SAW"),
        2: op(1, 2.5, 0, env([(0, 0.001), (100, 4.0)], rel=1.0), am=30),
    }, mod={(2, 1): 100, (2, 2): 40},
        pe={"epts": 1, "l1": -100, "t1": g.u_time(0.001), "l2": 100, "t2": g.u_time(4.0)}, pe_depth=24,
        unison=3, detune=30, width=100, lfo_wave="S&H", lfo_sync="1/16", volume=48)

    # 16 - FM kick: a sine with a fast pitch drop and a fixed click
    snd("FM KICK", {
        1: op(1, 1, 100, env([(100, 0.001), (70, 0.08), (0, 0.3)], rel=0.05)),
        2: op(fixed=3000, level=1, out=25, env=env([(100, 0.0003), (0, 0.006)], rel=0.01), penv=0),
        3: op(1, 0.5, 0, env([(100, 0.0005), (0, 0.03)], rel=0.02)),
    }, mod={(3, 1): 100},
        pe={"epts": 1, "l1": 100, "t1": g.u_time(0.001), "l2": 0, "t2": g.u_time(0.07)}, pe_depth=36,
        vmode="MONO", transpose=-24, volume=80)

    # 17 - FM hat: fixed high operators in feedback, almost noise
    snd("FM HAT", {
        1: op(fixed=3300, level=1, out=100, env=env([(100, 0.0005), (0, 0.07)], rel=0.04), penv=0),
        2: op(fixed=2770, level=6.0, out=0, env=env([(100, 0.0005), (60, 0.07)], rel=0.04), penv=0),
        3: op(fixed=3900, level=1, out=60, env=env([(100, 0.0005), (0, 0.05)], rel=0.04), penv=0),
    }, mod={(2, 1): 100, (2, 2): 100, (2, 3): 100}, volume=50)

    # 18 - wobble: the LFO opens and closes the modulator in 1/8
    snd("WOBBLE BASS", {
        1: op(1, 1, 100, SUS(0.002, 0.3, 90, 0.1)),
        2: op(1, 2.0, 0, SUS(0.002, 0.3, 90, 0.1), am=100),
        3: op(0.5, 1, 60, SUS(0.002, 0.3, 90, 0.1)),
    }, mod={(2, 1): 100, (2, 2): 45},
        vmode="MONO", glide=0.03, transpose=-12, lfo_wave="TRIANGLE", lfo_sync="1/8", volume=62)

    # 19 - glassy pad, slow
    snd("GLASS PAD", {
        1: op(1, 1, 100, env([(100, 1.2), (80, 1.5)], rel=2.0), pan=-30),
        2: op(2, 0.8, 0, env([(100, 1.0), (60, 2.0)], rel=2.0), am=20),
        3: op(4, 1, 45, env([(100, 1.5), (70, 2.0)], rel=2.0), offset=0.6, pan=30),
        4: op(7, 0.6, 0, env([(100, 2.0), (40, 2.0)], rel=2.0)),
    }, mod={(2, 1): 100, (4, 3): 100}, unison=2, detune=10, width=70, lfo_rate=0.3, volume=56)

    # 20 - beating drone: four carriers a little apart, a slow looping modulator
    snd("DRONE CLUSTER", {
        1: op(1, 1, 100, env([(100, 2.0), (90, 1.0)], rel=3.0), offset=-0.4, pan=-60),
        2: op(1.5, 1, 70, env([(100, 2.5), (90, 1.0)], rel=3.0), offset=0.3, pan=60),
        3: op(2, 1, 60, env([(100, 3.0), (90, 1.0)], rel=3.0), offset=0.9, pan=-20),
        4: op(3, 1, 40, env([(100, 3.5), (90, 1.0)], rel=3.0), offset=-0.7, pan=20),
        5: op(0.5, 1.0, 0, env([(0, 3.0), (100, 4.0), (30, 3.0)], "LOOP", loop=1, rel=3.0)),
    }, mod={(5, 1): 100, (5, 2): 80, (5, 3): 60, (5, 4): 50}, lfo_rate=0.15, volume=48)

    # 21 - eight drawbars (16', 8', 5 1/3', 4', 2 2/3', 2', 1 3/5', 1') with leslie-ish vibrato
    draw = env([(100, 0.004), (100, 0.1)], rel=0.05)
    snd("DRAWBAR ORGAN", {
        1: op(0.5, 1, 80, draw, pm=25), 2: op(1, 1, 100, draw, pm=25), 3: op(1.5, 1, 60, draw, pm=25),
        4: op(2, 1, 70, draw, pm=25), 5: op(3, 1, 45, draw, pm=25), 6: op(4, 1, 50, draw, pm=25),
        7: op(5, 1, 30, draw, pm=25), 8: op(8, 1, 35, draw, pm=25),
    }, lfo_rate=6.5, volume=44)

    # 22 - marimba
    snd("MARIMBA", {
        1: op(1, 1, 100, env([(100, 0.001), (0, 0.6)], rel=0.2)),
        2: op(4, 1.6, 0, env([(100, 0.0005), (0, 0.05)], rel=0.05), vel=100),
        3: op(10, 1, 20, env([(100, 0.0005), (0, 0.08)], rel=0.05)),
    }, mod={(2, 1): 100}, volume=66)

    # 23 - clav: a nasal half-sine modulator
    snd("CLAV", {
        1: op(1, 1, 100, env([(100, 0.001), (50, 0.3), (0, 1.0)], rel=0.08)),
        2: op(3, 2.2, 0, env([(100, 0.001), (30, 0.4)], rel=0.08), wave="HALF", vel=80),
        3: op(1, 1, 50, env([(100, 0.001), (40, 0.2), (0, 0.6)], rel=0.08), wave="ABS"),
    }, mod={(2, 1): 100, (2, 3): 60}, volume=60)

    # 24 - squelch sequence: a looping 1/32 ramp on a feedback modulator (acid wah without a filter)
    snd("SQUELCH SEQ", {
        1: op(1, 1, 100, SUS(0.002, 0.2, 90, 0.1)),
        2: op(1, 3.0, 0, env([(100, 1), (10, 1), (50, 1), (5, 1)], "LOOP", sync="1/32")),
    }, mod={(2, 1): 100, (2, 2): 60}, vmode="LEGATO", glide=0.05, transpose=-12, volume=60)

    # 25 - psy stab: root and fifth, short, wide
    snd("PSY STAB", {
        1: op(1, 1, 100, env([(100, 0.001), (40, 0.15), (0, 0.3)], rel=0.1)),
        2: op(1, 2.2, 0, env([(100, 0.001), (20, 0.15)], rel=0.1)),
        3: op(1.5, 1, 80, env([(100, 0.001), (40, 0.15), (0, 0.3)], rel=0.1)),
        4: op(1.5, 2.0, 0, env([(100, 0.001), (20, 0.15)], rel=0.1)),
    }, mod={(2, 1): 100, (4, 3): 100, (2, 2): 40, (4, 4): 40}, unison=3, detune=18, width=100, volume=50)

    return S
