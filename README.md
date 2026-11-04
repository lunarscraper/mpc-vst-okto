# Okto für Akai Force / MPC

Repo: mpc-vst-okto

8-Operator-FM-Synthesizer als VST2-**Instrument** für den Plugin-Host von MPC OS (Force,
MPC Live/One/X/Key). Die FM-Engine folgt [libfmsynth](https://github.com/Themaister/libfmsynth)
von Hans-Kristian Arntzen (MIT); dazu kommen Hüllkurven, die sich wie ein Step-Sequencer nutzen
lassen, im Stil von FM8. Keine Abhängigkeiten, eine Header-Datei für den Klang.

**Stand:** vollständiges Repo (Klang, Plugin-Hülle, Skin, 32 Werksklänge, Preset-Generator, SHAPE, Build, Workflow).
Version 1.2.0: **SHAPE**, ein Wavetable-Modulator mit 128 morphbaren Konturen und eigenen WAV-Dateien
(Seiten SHAPE und SHP MOD). Version 1.1.0: Preset-Generator auf der Seite PRESET. Neue Parameter sind nur
angehängt – alte Projekte und Speicherplätze laden unverändert und klingen gleich.
1.0 läuft auf der Force. 1.1.0 und 1.2.0 sind offline getestet (Host-Test mit ASan/UBSan bestanden, auch
alle SHAPE-Prüfungen); Build über GitHub Actions und Test auf der Force stehen noch aus.

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
- **SHAPE** (seit 1.2.0): ein Wavetable-Modulator – 16 Tabellen mit je 8 Konturen (128 insgesamt), MORPH
  blendet durch die Konturen einer Tabelle, als Hüllkurve, Schleife, Ping-Pong oder freier LFO (im Tempo auf
  die Songposition eingerastet), WARP X/Y, dazu 16 eigene Tabellen aus WAV-Dateien. Wirkt pro Operator auf
  Pegel und Tonhöhe, global auf BRIGHT und die Tonhöhe – siehe unten.
- **Makros:** BRIGHT (alle Modulationstiefen, 0–200 %), FEEDBACK (nur die Diagonale), ENV TIME
  und RELEASE (alle Zeiten ×0,25 … ×4).
- MIDI: sample-genaue Noten, Pitch Bend (Bereich einstellbar), Modrad (Pegel pro Operator,
  Vibrato, Brightness), Sustain-Pedal, All Notes Off.
- **32 Speicherplätze** mit LOAD/SAVE (Muster von Acid/Rattler), erscheinen auch in der
  Preset-Liste der Force; gemeinsam für alle Instanzen in `okto_presets.txt` neben dem
  Plugin-Ordner.
- **Preset-Generator** (seit 1.1.0, nach dem Vorbild des carp 2000): STYLE, VARIATION, MODULATION,
  RANDOM und UNDO auf der Seite PRESET – siehe unten.
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
| SHAPE | Table, Morph, Mode, Sync | Rate, Warp X, Warp Y, LFO > Morph |
| SHP MOD | **EDIT**, Op Level, Op Pitch, Bright | Pitch, Vel > Morph, Wheel > Morph, Morph |
| VOICE | Mode, Voices, Glide, Unison | Detune, Width, Transpose, Bend Range |
| PRESET | Style, Variation, Modulation, **RANDOM** | Preset (blättern), **LOAD**, **SAVE**, **UNDO** |

Auf OP, OP MOD und ENV liegt EDIT auf dem ersten Q-Link, auf MATRIX, LEVELS und TIMES sind alle acht
Q-Links Werte (EDIT dort per Touch oben). Bei EDIT = PITCH ist die Mitte der LEVEL-Regler 0.

**Matrix lesen:** Auf der Seite MATRIX mit EDIT = OP 1 regelt „FROM OP 2“, wie stark OP 2 den OP 1
moduliert. „FROM OP 1“ ist dann das Feedback von OP 1.

## Preset-Generator

Oben auf der Seite PRESET: **STYLE** wählen, **VARIATION** und **MODULATION** einstellen, **RANDOM**
tippen. Der neue Klang spielt sofort; der PRESET-Regler zeigt „NEW TALK > 05“ (Stil, Zielplatz).
Gefällt er, PRESET auf einen Platz drehen und **SAVE** – der Platz heißt dann nach dem Stil
(„TALK 05“). **UNDO** geht durch die letzten 16 Klänge zurück, die RANDOM ersetzt hat.

| Stil | Grundklang (VARIATION 0) |
| --- | --- |
| TALK | Formant-Gate / Alien-Sprech: knurrende 2:1-FM mit Feedback, Index und Gate laufen in einem 1/16-Step-Muster (das Muster aus „Formant Gate“ von ultramarinedist), drei Formant-Träger (Harmonische 5, 11, 17) folgen als Vokale. TRANSPOSE −24, um C3–C5 spielen |
| LEAD | Goa-Lead (Feedback-Stapel, Legato-Glide, Unison) |
| BASS | rollender Psy-Bass für 1/16-Noten |
| ACID | quietschender Mono-Bass, bei VARIATION auch als 1/32-Schleife |
| STAB | Psy-Stab mit Quinte, breit |
| PAD | gläserne Fläche, langsam |
| PLUCK | kurzer FM-Pluck |
| BELL | unharmonische Glocken |
| SEQ | Klangfarben-Sequenz beim Halten einer Taste, ab und zu mit Trancegate |
| FX | Zap / Laser mit Pitch-Sturz |
| DRONE | schwebende Träger, langsame Schleife |
| PERC | FM-Kick oder metallischer Schlag |
| ANY | ein Stil per Zufall |
| MUTATE | verändert den Klang, der gerade spielt (VARIATION = wie weit) |

- **VARIATION** (0–100): wie weit sich der Klang vom Grundklang entfernt – andere Ratios (ab etwa
  60 auch unharmonische), Modulationsindex, Hüllkurvenzeiten, Wellenformen, Feedback, ein
  zusätzlicher Modulator, neue Step-Muster, Unison. Bei 0 kommt immer derselbe Grundklang.
- **MODULATION** (0–100): wie viel sich von selbst bewegt – Modulatoren als Step- oder
  Loop-Hüllkurven im Tempo, LFO auf Pegel und Tonhöhe (bei TALK/SEQ oft S&H), ein kurzer
  Pitch-Blip, Vibrato; das Modrad regelt immer die Helligkeit (WHEEL BRIGHT). Seit 1.2.0 auch
  **SHAPE** passend zum Stil: TALK sagt „wow“ (pro Note oder im Tempo, Formant-Träger gegeneinander),
  SEQ/PAD/DRONE bekommen ein Trancegate auf dem Takt oder einen langsamen Scan, BASS/ACID Akzente auf
  BRIGHT, FX einen Riser oder Chaos auf der Tonhöhe, der Rest eine BRIGHT-Bewegung pro Note. Bei 0 bleibt nur die
  Bewegung, die zum Stil gehört (die Steps von TALK und SEQ).
- **Lautstärke:** Bei echter FM behält ein Sinus-Träger seinen Pegel, egal wie stark er moduliert
  wird. Der Generator rechnet daher die Lautstärke aus OUT, LEVEL und den Hüllkurven der Träger
  aus und stellt VOLUME auf etwa −20 dBFS pro Note wie bei den Werksklängen (Test über alle Stile:
  rund −27 bis −15 dBFS). STAB und PAD bekommen 3 dB Reserve für Akkorde.
- STYLE, VARIATION und MODULATION gehören zum Projekt, nicht zum Klang: Sie landen nie in einem
  Speicherplatz, und LOAD ändert sie nicht.
- Q-Links auf PRESET: Style, Variation, Modulation, Preset. RANDOM, UNDO, LOAD und SAVE sind
  Tasten zum Antippen (große Beschriftung).

## SHAPE – der Wavetable-Modulator

Die Idee stammt von den Wavetable-Hüllkurven und -LFOs der Polyend Tracker Community Edition (morphbare
Konturen, Warp in X und Y, eigene Konturen als WAV). Okto setzt das selbst um, es steckt kein Code der
Tracker-Firmware darin.

**Seite SHAPE – was sich bewegt:**

- **TABLE:** eine von 16 Werkstabellen oder USER 1–16 (eigene WAVs, siehe unten). Jede Werkstabelle hat
  8 Konturen; `docs/shape_tables.png` zeigt alle 128 auf einen Blick.

  | Tabelle | Konturen (MORPH 0 → 100) |
  | --- | --- |
  | DECAY | linear bis sehr steil fallend |
  | SWELL | langsam bis schnell steigend |
  | LFO | Sinus, Dreieck, Säge auf/ab, Rechteck, Puls 25 %/10 %, gestufter Sinus |
  | GATE | 8 Trancegate-Muster, 16 Steps |
  | TALK | Vokalbewegungen: „wow“, „ya-ya“, „yoy“, Flattern |
  | STEPS | 8-Step-Treppen und Figuren |
  | BOUNCE | springender Ball, 2–9 Sprünge |
  | WOBBLE | 1–16 Wellen pro Durchlauf |
  | RANDOM | weiche Zufallskurven (fest, also wiederholbar) |
  | S&H | gestufter Zufall, 4–32 Stufen |
  | PULSE | 4 Pulse pro Durchlauf, Breite 5–90 % |
  | SPIKE | 1–16 kurze Spitzen |
  | ACID | 16tel mit Akzent, die abklingen |
  | RISE | Riser, die letzten vier mit schneller werdendem Tremolo |
  | CHAOS | logistische Abbildung, von geordnet bis chaotisch |
  | RATCHET | Schläge, die zum Ende dichter werden |

- **MORPH:** blendet weich durch die Konturen der Tabelle. Dazu kommen LFO > MORPH (Seite SHAPE),
  VEL > MORPH und WHEEL > MORPH (Seite SHP MOD).
- **MODE:** `ENV` läuft pro Note einmal durch und bleibt am Ende stehen. `LOOP` wiederholt sich und startet mit
  jeder Note neu. `PING` läuft vor und zurück. `FREE` läuft frei für alle Stimmen. Mit SYNC rastet FREE bei
  laufendem Transport auf die Songposition ein, so sitzt ein Gate immer auf dem Raster, egal wann die Note
  kommt.
- **SYNC / RATE:** Bei SYNC FREE ist RATE eine Zeit von 10 ms bis 20 s (ENV TIME wirkt mit). Mit SYNC ist
  RATE die Anzahl der Notenwerte, 1–16. Die Voreinstellung 16 × 1/16 ist ein Takt, also 1/16 pro Gate-Step.
- **WARP X:** verschiebt die Kontur zeitlich (+ später, − früher). **WARP Y:** macht sie voller (+) oder
  dünner (−).

**Seite SHP MOD – wohin es geht:**

- **OP LEVEL** (pro Operator, folgt EDIT): +100 % heißt, die Kontur *ist* der Pegel. −100 % heißt, die
  Kontur steht auf dem Kopf (laut, wo sie 0 ist). Dazwischen anteilig. Auf einem Träger ergibt das ein Gate
  oder Tremolo, auf einem Modulator eine Klangfarbe.
- **OP PITCH** (pro Operator): bis ±12 Halbtöne bei Kontur 1.
- **BRIGHT:** alle Modulationstiefen ×4 (+100 %) bzw. ×0,25 (−100 %) bei Kontur 1.
- **PITCH:** alle Operatoren, bis ±24 Halbtöne.
- Ohne einen Betrag bewegt SHAPE nichts – alte Klänge bleiben unverändert.

**Rezepte:**

- *Trancegate:* TABLE GATE, MODE FREE, SYNC 1/16, RATE 16; EDIT auf den Träger, OP LEVEL +100 %. MORPH wählt
  das Muster.
- *Alien-Sprech:* TABLE TALK, MODE ENV, RATE ca. 400 ms; bei mehreren Formant-Trägern OP LEVEL abwechselnd
  + und − (zum Beispiel OP 3 +70 %, OP 4 −60 %, OP 5 +50 %). Dann sagt jede Note „wow“. Mit VEL > MORPH
  klingen harte Anschläge anders als weiche.
- *Acid-Akzente:* TABLE ACID, MODE FREE, SYNC 1/16, RATE 16, BRIGHT +60 %.
- *Wavetable-Scan:* RANDOM oder WOBBLE, LFO > MORPH 50 %, BRIGHT +40 %.

**Eigene Tabellen (USER 1–16):** WAV-Dateien in einen Ordner `okto_shapes` legen, der neben
`okto_presets.txt` liegt (also neben dem Plugin-Ordner; alternativ im Plugin-Ordner selbst). Okto liest
beim Start bis zu 16 Dateien, alphabetisch sortiert. Die Seite SHAPE zeigt sie mit Namen („U1 GROWL“).

- Eine Wavetable im Serum-Format (Frames zu 2048 Samples, oder so viele, wie ihr „clm “-Chunk angibt) liefert
  bis zu 8 ihrer Frames, gleichmäßig verteilt; MORPH blendet dazwischen. So werden auch die Serum- und
  Virus-Wavetables der Force zu Modulationskurven.
- Eine kurze Datei (bis 4096 Samples) ist eine einzelne Kontur.
- 8/16/24/32 Bit Integer oder 32 Bit Float, nur der erste Kanal. Jede Tabelle wird als Ganzes auf 0..1
  skaliert. Kaputte Dateien werden übersprungen, leere Plätze zeigen „USER n --“ und bewegen nichts.
- Neue Dateien zählen erst, nachdem MPC neu gestartet wurde.

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
vst/okto_random.h    Preset-Generator (Stile, VARIATION, MODULATION, Lautstärkeabgleich)
vst/okto_shape.h     SHAPE: die 128 Werkskonturen (beim Laden berechnet) und der WAV-Import
docs/shape_tables.png  alle Werkskonturen als Bild
vst/okto_vst.cpp     Plugin-Hülle: Parameter, EDIT-Auswahl, MIDI, 32 Speicherplätze, Projekt-Chunk
vst/okto_params.h    erzeugt: Schlüssel, Startwerte, Werksklänge
vst/module.json      erzeugt: Parameterliste = VST-Index-Reihenfolge (nur anhängen!)
vst/params.lock      prüft beim Build, dass sich kein Index verschiebt
vst/layout.conf      Skin: 12 Seiten im Raster von Rattler / carp 2000
vst/vst.json         Name, UID (Okto), .so
vst/build.sh         Build (ruft die Werkzeuge von open-mpc/mpc-vst-plugins)
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
- SHAPE kostet kaum etwas: Der Wert wird alle 32 Samples einmal pro Stimme gerechnet, der Pegel läuft
  dazwischen als Rampe (keine Klicks auch bei harten Gates). x86-Messung: TALK mit SHAPE auf allen
  Operatoren 0,27 % statt 0,26 %.
- CPU (x86-Messung, nur relativ): typische Klänge mit 1–8 Noten 0,5–1,5 % eines Kerns; der
  Extremfall (alle 8 Operatoren, volle Matrix, 16 Stimmen) 7 %. Auf der Force ein Vielfaches
  davon – bei schweren Klängen VOICES klein halten.
- Feedback wirkt anders als in libfmsynth (dort Frequenz-Feedback, das den Ton verstimmt); die
  umgerechneten libfmsynth-Presets mit Selbst-Modulation klingen daher etwas anders als im Original.

## Lizenz

MIT (siehe `LICENSE`). FM-Kern nach libfmsynth, MIT (`LICENSE-libfmsynth.txt`).
„FM8“ ist ein Produkt von Native Instruments, „Polyend Tracker“ eines von Polyend, „Serum“ eines von Xfer
Records; keine Verbindung.
