# Okto für Akai Force / MPC

Repo: mpc-vst-okto

8-Operator-FM-Synthesizer als VST2-**Instrument** für den Plugin-Host von MPC OS (Force,
MPC Live/One/X/Key). Die FM-Engine folgt [libfmsynth](https://github.com/Themaister/libfmsynth)
von Hans-Kristian Arntzen (MIT); dazu kommen Hüllkurven, die sich wie ein Step-Sequencer nutzen
lassen, im Stil von FM8. Keine Abhängigkeiten, eine Header-Datei für den Klang.

**Stand:** vollständiges Repo (Klang, Plugin-Hülle, Skin, 32 Werksklänge, Build, Workflow).
Offline getestet (Host-Test mit ASan/UBSan bestanden); der Build über GitHub Actions und der Test
auf der Force stehen noch aus.

## Was Okto kann

- **8 Operatoren, keine festen Algorithmen:** freie 8×8-Modulationsmatrix, jeder Operator kann
  jeden anderen modulieren, auch sich selbst. Jeder Operator kann Träger sein, mit eigenem Pegel
  (OUT) und eigenem Panorama.
- **Echtes FM wie libfmsynth** (Phasenintegration): Der Modulationsindex bleibt über die Tastatur
  gleich. Selbst-Feedback (Diagonale der Matrix) läuft als Phasenmodulation wie beim DX7 und
  verstimmt den Ton deshalb nicht.
- **6 Wellenformen pro Operator:** Sinus, Half, Abs, Quarter, Soft Square, Saw (3 Teiltöne),
  alle ohne Gleichanteil.
- **Ratio** 0.5 bis 32 plus FINE, oder **FIX** (feste Frequenz über OFFSET, bis ±4 kHz). OFFSET bei
  normaler Ratio = Schwebung in Hz.
- **Hüllkurven mit bis zu 8 Punkten** pro Operator, Modi:
  - `ONCE` – Rampen von Punkt zu Punkt, bleibt am letzten Punkt stehen
  - `LOOP` – Rampen, springt ab dem letzten Punkt zurück auf LOOP FROM
  - `STEP` – Stufen statt Rampen (einmal durch)
  - `STEP LOOP` – Stufen in Schleife = **Step-Sequencer pro Operator**
  
  SYNC teilt die Zeiten in Notenwerte zum Force-Tempo (1/32 bis 1/1, auch triolisch); TIME gibt
  dann die Anzahl der Notenwerte pro Schritt an (1–16). So wird z. B. der Pegel eines Modulators
  zur 1/16-Klangfarbensequenz oder ein Träger zum Trancegate.
- **Pitch-Hüllkurve** derselben Art (bipolar, Tiefe bis 48 Halbtöne), Anteil pro Operator.
- **LFO:** Sinus, Dreieck, Säge ab/auf, Rechteck, S&H; frei oder im Tempo; Delay; Key-Sync.
  Wirkt pro Operator auf Pegel und Tonhöhe, global als Vibrato (auch per Modrad).
- **Stimmen:** POLY (1–16 Stimmen), MONO, LEGATO, Glide, Unison bis 3 mit Detune und Stereo-Breite.
- **Makros:** BRIGHT (alle Modulationstiefen, 0–200 %), FEEDBACK (nur die Diagonale), ENV TIME
  und RELEASE (alle Zeiten ×0,25 … ×4).
- MIDI: sample-genaue Noten, Pitch Bend (Bereich einstellbar), Modrad (Pegel pro Operator,
  Vibrato, Brightness), Sustain-Pedal, All Notes Off.
- **32 Speicherplätze** mit LOAD/SAVE (Muster von Acid/Rattler), erscheinen auch in der
  Preset-Liste der Force; gemeinsam für alle Instanzen in `okto_presets.txt` neben dem
  Plugin-Ordner.
- Es werden nur die Operatoren gerechnet, die irgendwo am Ausgang ankommen.

## Seiten

Ein Synth mit 8 Operatoren hat zu viele Regler für eine Seite. Okto arbeitet deshalb wie FM8 mit
einer Auswahl: **EDIT** wählt OP 1–8 oder PITCH, und die Seiten OP, OP MOD, MATRIX, ENV, LEVELS
und TIMES zeigen und ändern genau diesen Operator. Intern hat jeder Wert seinen eigenen
Parameter (Presets, Projekte und Automation arbeiten damit).

| Seite | Regler 1–4 | Regler 5–8 |
| --- | --- | --- |
| MAIN | Bright, Feedback, Env Time, Release | LFO Rate, Vibrato, Glide, Volume |
| OP | **EDIT**, Wave, Ratio, Fine | Offset, Level, Out, Pan |
| OP MOD | **EDIT**, Velocity, Key Low, Key High | LFO Amp, LFO Pitch, Pitch Env, Wheel |
| MATRIX | From OP 1–4 → TARGET (= EDIT) | From OP 5–8 → TARGET |
| ENV | **EDIT**, Mode, Points, Loop From | Sync, Release, Pitch Depth, Env Time |
| LEVELS | Level 1–4 | Level 5–8 (darunter Times 1–8 zum Antippen) |
| TIMES | Time 1–4 | Time 5–8 (darunter Levels 1–8 zum Antippen) |
| LFO | Rate, Wave, Sync, Delay | Vibrato, Wheel Vibrato, Wheel Bright (Key Sync nur per Touch) |
| VOICE | Mode, Voices, Glide, Unison | Detune, Width, Transpose, Bend Range |
| PRESET | Preset (blättern), Load, Save | |

Auf OP, OP MOD und ENV liegt EDIT auf dem ersten Q-Link, auf MATRIX, LEVELS und TIMES sind alle acht
Q-Links Werte (EDIT dort per Touch oben). Bei EDIT = PITCH ist die Mitte der LEVEL-Regler 0.

**Matrix lesen:** Auf der Seite MATRIX mit EDIT = OP 1 regelt „FROM OP 2“, wie stark OP 2 den OP 1
moduliert. „FROM OP 1“ ist dann das Feedback von OP 1.

## Werksklänge (Plätze 1–32)

1 Goa Lead · 2 Alien Talk · 3 Acid FM Bass · 4 Psy Roll Bass · 5 Trancegate Pad · 6 Timbre Seq ·
7 Pulse Grid · 8 Bell Tree · 9 E-Piano · 10 Brass Stab · 11 Supersaw FM · 12 Hoover FM ·
13 Metal Zap · 14 Laser · 15 Riser · 16 FM Kick · 17 FM Hat · 18 Wobble Bass · 19 Glass Pad ·
20 Drone Cluster · 21 Drawbar Organ · 22 Marimba · 23 Clav · 24 Squelch Seq · 25 Psy Stab ·
26–32 die sieben Presets aus libfmsynth (Angry Overture, Clocky, Expressive Harp, Lead Pad,
Old-School Organ, Retro Sci-Fi, Square Emotions), umgerechnet.

Alle auf ähnliche Lautstärke abgeglichen (eine Note um −20 dBFS RMS). SAVE überschreibt einen
Platz; ein Werksklang kommt zurück, wenn man seine Zeile aus `okto_presets.txt` löscht.

## Bauen und installieren

GitHub → Actions → „VST release (draft)“ → Run workflow (Version z. B. `0.1.0`, `dry_run` an
für einen reinen Build). Der Workflow baut `okto.so` für armhf, erzeugt den Skin und lässt
`vst/test.sh` laufen. Installation wie bei Rattler und LIRA 8 (ZIP aus dem Lauf, Ordner nach
`/tmp` kopieren, `sh install.sh`). **Das Installationsskript stoppt und startet MPC neu, vorher
Projekt speichern.**

## Aufbau

```
vst/okto_core.h      Klangerzeugung (FM-Kern nach libfmsynth + Hüllkurven, LFO, Stimmen)
vst/okto_vst.cpp     Plugin-Hülle: Parameter, EDIT-Auswahl, MIDI, 32 Speicherplätze, Projekt-Chunk
vst/okto_params.h    erzeugt: Schlüssel, Startwerte, Werksklänge
vst/module.json      erzeugt: Parameterliste = VST-Index-Reihenfolge (nur anhängen!)
vst/params.lock      prüft beim Build, dass sich kein Index verschiebt
vst/layout.conf      Skin: 10 Seiten im Raster von Rattler / carp 2000
vst/vst.json         Name, UID (Okto), .so
vst/build.sh         Build (ruft die Werkzeuge von sd88me/mpc-vst-plugins)
vst/test.sh          Host-Test (ASan/UBSan) + CPU-Messung
vst/host_test.cpp    der Test selbst
tools/gen_okto.py    erzeugt module.json + okto_params.h (nach Änderungen an Parametern/Klängen)
tools/factory.py     die Werksklänge 1–25
vst/presets-libfmsynth/   die Original-Presets von libfmsynth (26–32)
```

## Hinweise

- Die Plugin-Hülle hält sich an die Host-Regeln von mpc-vst-plugins (Stand 2026-10-25): kein
  UpdateDisplay bei normalen Reglerbewegungen, höchstens eines pro 50 ms, Parameter 0 (EDIT)
  übersteht MPCs Hin-und-zurück-Setzen beim Einfügen und bei STOP.
- CPU (x86-Messung, nur relativ): typische Klänge mit 1–8 Noten 0,5–1,5 % eines Kerns; der
  Extremfall (alle 8 Operatoren, volle Matrix, 16 Stimmen) 7 %. Auf der Force ein Vielfaches
  davon – bei schweren Klängen VOICES klein halten.
- Feedback wirkt anders als in libfmsynth (dort Frequenz-Feedback, das den Ton verstimmt); die
  umgerechneten libfmsynth-Presets mit Selbst-Modulation klingen daher etwas anders als im Original.

## Lizenz

MIT (siehe `LICENSE`). FM-Kern nach libfmsynth, MIT (`LICENSE-libfmsynth.txt`).
„FM8“ ist ein Produkt von Native Instruments; keine Verbindung.
