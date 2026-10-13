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

## The drone field (the screen)

On HOME, and on DRONE's EDIT and FILTER pages, the panel shows the drone as a 3D stage instead of the scope.
- **Stage:** a night stage with a floor grid running to a glowing horizon, and stars drifting and twinkling above it.
- **Lanes:** each oscillator of the stack is a lane on the floor, sorted by pitch. The lowest is nearest, the
  highest farthest, and SUB is the front lane. Lanes shrink, rise and fade into the violet haze as they recede.
- **Lane colours:** a spectrum running from coral at the front through amber, mint and sky to violet at the back.
  SUB is magenta.
- **Ribbons:** each lane is a ribbon of the oscillator's own wave at the current WAVE, through GRIT's drive. It
  flows at its pitch's rate, slips against the others by DTN and wobbles with DRFT's walks.
- **Depth:** each ribbon has four slices, now and 60, 120 and 180 ms ago, each one a little farther back. Every
  slice is a shaded opaque sheet, so the beating shows as a moving surface.
- **Filter:** a gate across the stage at the depth of the cutoff among the lanes: two glowing posts at the stage
  edges and a running dashed beam (doubled for LP24), with the posts wider as RES rises. It is drawn over everything
  and glides rather than jumps, so it never disappears. LP and LP24 fade the lanes behind it, HP fades those in front
  of it, and BP fades those outside its band.
- **Palettes:** the field is a colour picture on every palette, GREY included. MONO keeps its black and white.

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

## Lofi drum kits (DRUM)

There are four kits after DRUM KIT: LOFI DEEP, SUB THUMP, DUST BOWL and TAPE TOMS.
- **Sound:** each is tuned down, darker and longer, with the round kick and some drive.
- **No hats:** the hi-hat and bell lanes are set to 0 (their LANES levels).
- **Lofi texture:** each sets the track's INSERT, a CRUSH (8 to 11 bits at 9 to 15 kHz under a low-pass) or, on
  SUB THUMP, a dark soft drive.
- **Pattern:** they suggest the new pattern DEEP (SAVE > PHRASES 14): kicks, the low snare and toms, no hats.

Re-hits (every DRUM kit): striking a drum again while it still rings restarts it at once, as loud as the first hit.
The hit before no longer stops with a jump: where it stopped fades out over about 3 ms under the new hit, so a long
deep kick re-hit quickly has no click (`tests/rehit_test.c`).

## HOLD (any engine)

Set ARP 2 > HOLD to ON while ARP MODE is OFF:
- A key latches its note, and letting go of the key does nothing.
- Pressing the same key again releases that note. If that note has already ended on its own (a sample that played
  out, a SUS 0 envelope that decayed to silence), the press plays it again instead, so a press always sounds.
- Hits are never latched: on DRUM, and on SLICE in MODE ONE, every press is a new hit, with HOLD on or off.
- Turning HOLD off, turning the ARP on, or a panic releases every latched note.
- HOLD is a track setting and stays on when you load another sound or engine on that track.

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
- **Install:** run `pip install mido python-rtmidi` and then `python3 tools/fm1_install.py build/felucca-1.5.1-dr4.fwsc`.

Going back: the installer's "Return to official V15", or M-VAVE's M-UPGRADE. Installing firmware is at your own risk.

The CPU cost on this computer: the heaviest preset (IRON CHOIR: six drones, RING to FOLD) costs about 1.1–1.5× the
heaviest stock engine for one part. Check the FM-1's CPU readout when two parts play six-drone stacks at once.
