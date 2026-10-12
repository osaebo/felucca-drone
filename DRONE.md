# Felucca DRONE fork

This fork of [Felucca](https://github.com/hugelton/Felucca) (GPL-3.0) for the M-VAVE FM-1 adds a **DRONE** engine.
It is engine 15, and it comes first in PRESETS and in the EDIT layer. Everything else in Felucca is unchanged.

## The engine (`firmware/src/eng_drone.c`)

Three band-limited oscillators per voice are stacked on an interval set and slowly beat against each other. A
12 dB state-variable filter follows; it growls when driven. Each voice wanders a little, with a different drift per
voice. A part plays at most 4 DRONE voices.

| Knob | What it does |
| --- | --- |
| MODE | Interval stack: UNI, OCT, 5TH, 4TH, MIN (minor triad), TRI (tritone), CLUS (semitone cluster), DARK (octave below and minor second above) |
| DTN | Spread in cents: OSC 2 up, OSC 3 down. The shape routings (ENV/LFO/matrix SHP) widen it. |
| WAVE | Saw (0) to square (127) |
| SUB | Sine one octave below |
| CUT / RES | Filter cutoff and resonance. EDIT > FILTER also has TYPE (LP / BP / HP) and GRIT. |
| DRFT | A random walk per oscillator (up to about ±25 ct) and on the cutoff: the breathing of an old drone box |
| GRIT | Drive into the filter, plus saturation inside its loop. Set it high with a high RES and BP for a screaming filter. |

HOME knobs: CUT RES DTN DRFT.

Presets: ABYSS, MONOLITH, CATACOMB, TRITONE, SWARM, DIRGE, FURNACE, VOID HUM, plus a set in the spirit of the big
drone machines: SOLAR WIND (BP), RED GIANT, ORBIT (BP), COSMODRONE, SCREAMER (BP) and DEEP FIELD.
For slow cyclic sweeps, use the track LFO (SYNC to 4 bars) on CUT. The chorus, delay and HALL reverb sends finish
the sound.

## Engine numbers

Engine 14 is left as a hole, and DRONE is 15. Because 14 is never an engine number, the MIDI LEARN map
(`favorites.factory[14]`) and the user-preset number of the favourites keep working. On the editor protocol, the
user presets are `NENGINES` (the engine count INFO reports), as before.

Projects and user presets that use DRONE do not open on upstream Felucca. Everything upstream Felucca saved still
opens here.

## Build and install

- **GitHub:** push this repo (branch `drone`) to GitHub, or run "build DRONE firmware" from the Actions tab. The
  package is the run's artifact.
- **Linux or WSL:** run `tools/build_drone.sh`. It fetches the JieLi toolchain and the AC79 SDK the first time.
- **Install:** run `pip install mido python-rtmidi` and then `python3 tools/fm1_install.py build/felucca-1.5.1-dr.fwsc`.
  You can also serve a local copy of the web installer (see BUILDING.md).

Going back: the installer's "Return to official V15", or M-VAVE's M-UPGRADE. Installing firmware is at your own risk.
