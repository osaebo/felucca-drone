# Felucca DRONE fork

This fork of [Felucca](https://github.com/hugelton/Felucca) (GPL-3.0) for the M-VAVE FM-1 adds two things:

- **DRONE**, a new engine. It is engine 15, and it comes first in PRESETS and in the EDIT layer.
- **HOLD for drones**, which latches notes.

Everything else in Felucca is unchanged.

## The engine (`firmware/src/eng_drone.c`)

| Knob | What it does |
| --- | --- |
| MODE | The oscillator stack. Three oscillators: UNI, OCT, 5TH, 4TH, MIN, TRI (tritone), CLUS (semitone cluster), DARK (octave below and minor second above), SUS4, MAJ7, O+5 (octave and twelfth), WIDE (twelfth up and octave down). Six drones at once, each with its own pitch and drift: SOL6 (root, fifth, octave, twelfth, two octaves, octave below), MIN6, CLU6, VOI6 (tritones and minor seconds down two octaves). |
| DTN | Spread in cents: OSC 2 up, OSC 3 down. In a six-drone stack the others sit between and beyond. |
| WAVE | One knob through five oscillator types, crossfading between neighbours: SAW (0), SQR (32), RING (64: triangle × the sub's sine, giving bells and metal), FOLD (96: a wavefolder whose gain rises to 5×), VOID (127: noise tuned by each oscillator, like wind). The shape routings sweep it (ENV, LFO SHP, matrix SHP). |
| SUB | Sine one octave below |
| CUT / RES | Filter cutoff and resonance. EDIT > FILTER adds TYPE (LP / BP / HP) and GRIT. |
| DRFT | A random walk per oscillator (up to about ±25 ct) and on the cutoff, different for every voice |
| GRIT | Drive into the filter plus saturation inside its loop. Set it high with a high RES and BP for a screaming filter. |

HOME knobs: CUT RES WAVE DRFT. A part plays at most 4 DRONE voices.

**Presets (32)** set more than the eight knobs:
- the filter TYPE
- the track LFO: a slow rate or a sync of 2 or 4 bars, free-running, with depths on cutoff, WAVE or level
- matrix slot 1: S&H or SLEW onto the cutoff, pitch or pan

So each preset moves as it loads. They are ABYSS, MONOLITH, CATACOMB, TRITONE, SWARM, DIRGE, FURNACE, VOID HUM,
SOLAR WIND, RED GIANT, ORBIT, COSMODRONE, SCREAMER, DEEP FIELD, SOL SIX, IRON CHOIR, BLACK SUN, HIVE, BELL TOMB,
FOLDED, STATIC SEA, AURORA, HALO, SUS VAULT, WIDE ARC, RUST, PULSAR, ICE CAVE, GHOST, MAGMA, DESERT and NEUTRON.

## LP24 ladder and the monosynth presets

The filter TYPE (EDIT > FILTER, on ANALOG and DRONE) has a fourth value, **LP24**: a 4-pole, 24 dB transistor-ladder
low-pass.
- There is a tanh at its input. Drive (ANALOG's DRV, DRONE's GRIT) and high resonance round off into it, as they do
  on the classic 70s monosynths.
- The bass thins out as the resonance rises.
- RES 127 self-oscillates.

Fourteen monosynth presets use it:
- **DRONE**, three oscillators: 3OSC BASS, BIG MONO, SOLO LEAD, FIFTH LEAD, PEDAL BASS, SQUELCH, STAB BRASS,
  THICK SQR. They use the new MODE stacks UNI- (two in unison and one an octave down), UNI+ and 5+8.
- **ANALOG**: LADDER BASS, PROG LEAD, FLUTE 70S, DEEP TRI, SQR FUNK, FAT BRASS.

They play MONO (legato) with low-note priority. Leads have glide and a late vibrato. The DRONE ones track the keyboard
through the matrix (KEY → CUT). Felucca has one envelope for both amplitude and filter, so a pluck comes from the
decay down to the sustain level with the filter envelope's amount.

## HOLD (any engine)

Set ARP 2 > HOLD to ON while ARP MODE is OFF:
- A key latches its note, and letting go of the key does nothing.
- Pressing the same key again releases that note.
- Turning HOLD off, turning the ARP on, or a panic releases every latched note.

With the ARP on, HOLD is the arpeggiator's latch, as before.

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
- **Install:** run `pip install mido python-rtmidi` and then `python3 tools/fm1_install.py build/felucca-1.5.1-dr2.fwsc`.

Going back: the installer's "Return to official V15", or M-VAVE's M-UPGRADE. Installing firmware is at your own risk.

The CPU cost on this computer: the heaviest preset (IRON CHOIR: six drones, RING to FOLD) costs about 1.1–1.5× the
heaviest stock engine for one part. Check the FM-1's CPU readout when two parts play six-drone stacks at once.
