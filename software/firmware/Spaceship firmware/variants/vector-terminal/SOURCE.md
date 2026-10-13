# Vector-terminal v2: source guide

This is the main version accepted on 13 October 2026. It targets the physical
Intel 8080 / KR580VM80 clock, not a desktop simulation. New code comments use
English. Inherited driver notices and their original encodings are preserved.

## Build and memory map

Run from the project root on Windows with Python 3.9+ and z88dk 2.4:

```bat
setup-env.bat
build.bat vector-terminal all
verify.bat vector-terminal
```

The setup script installs the pinned toolchain inside `.tools`, verifies its
download SHA256 and installs the local `clock80` target. An already installed
toolchain works offline. Firmware execution does not need a network, SD card,
sound or emulator services. Verification additionally needs Clang and a Windows
SDK. Full-project geometry/A-B tests also need Pillow
(`python -m pip install Pillow`); the standalone BME-stage test does not.

| Region | Address / size | Purpose |
| --- | --- | --- |
| ROM | 0000h..7FFFh / 32 KB | Code, constants, compressed initialized data |
| RAM | 8000h..FFFFh / 32 KB | BSS, initialized data, downward-growing stack |
| Initial SP | FE00h | Configured by the variant-local CRT |
| LCD | 480 x 320 RGB565 | Commands port 10h, data port 11h |
| Animation | X=218..461, Y=165..275 | Exclusive 244 x 111 pixel window |

`build.json` selects the compiled modules. The CRT initializes BSS and
decompresses DATA using the 8080 ZX7 decoder. Use
`build/vector-terminal/hardware/rom.bin` for the physical 27C256, loaded linearly
from address 0000h. The separate `emulator/emulator.bin` uses virtual RTC ports
30h..36h and belongs only in the project emulator. Both images are 32768 bytes,
with an FF-filled unused tail. Internal segment files are not complete ROMs.

## Where to start reading

| File | Responsibility |
| --- | --- |
| `src/main.c` | Cooperative main loop, button debounce/queue, RTC and staged sensors |
| `src/terminal.c` | English clock/telemetry UI, segment and character differences |
| `src/vector.c`, `src/vector.h` | Model coordinates/edges, Q6 rotation, projection, clipping, frame phases |
| `src/vector_delta.c`, `src/vector_delta.h` | Palette masks, ASM raster, changed pixels, commit ordering |
| `src/vector_draw.c` | Direct LCD raster retained for reference tests; unused by v2 animation |
| `src/gui.c`, `src/lcd.c` | LCD primitives, font rendering, bounded fill yields |
| `src/logic.c` | Calendar validation/editing, elapsed-time observation, formatting |
| `src/i2c.c`, `src/onewire.c`, `src/bme280.c` | Inherited physical RTC/sensor drivers |
| `src/forecast.c` | Integer pressure/temperature trend and forecast |
| `src/clock_fixed_crt0.asm` | Reset, RAM initialization, DATA decompression, initial stack |

Unused legacy fonts, graphics and diagnostic source files remain in `src` for
provenance. They are not automatically compiled: follow the `sources` array in
`build.json` and the include graph. Current small-font data is in `small_font.h`.
`BASE.json` records the completed `clock-en` snapshot from which this variant
was created. Firmware builds never include a live neighboring variant.

## Geometry and arithmetic

The cube has 8 vertices and 12 edges; the ship has 11 vertices and 20 edges.
Coordinates are signed integers, with a supported range of -48..48 per model
axis. The ship is a nose, two hull sections and two swept wings; these are real
coordinates connected by indexed edges.

The 17-entry quarter-sine table stores Q6 values (-64..64 after mirroring).
One revolution is 64 angle steps. Cosine is sine at an offset of 16. Rotation
applies yaw about Y, then pitch about X, dividing products by 64. Signed C
division truncates toward zero. Replacing it with signed shifts changes
negative-coordinate rounding and the reference pixels.

Camera translation defaults to Z=144, with an accepted camera range 0..256.
Projection is `x=122+X*128/Z`, `y=55-Y*128/Z`. Before division, edges are clipped
against the near plane Z=16. A crossing edge necessarily has a nonzero Z
difference; an edge wholly behind the plane is rejected. Projection inputs
are bounded to X/Y=-96..96 and Z=-96..352. The rotation/projection products
fit signed 16-bit; intersection products explicitly promote to signed 32-bit
**before** multiplication.

Projected edges then use Cohen-Sutherland screen clipping. Its input domain
is -1100..1100; intersection division has explicit zero checks. Eight maximum
iterations bound pathological integer rounding. Accepted output endpoints are
inside the viewport and fit the four-byte `VLine` structure.

Adding a model requires vertex/edge counts in `vector_step`, at most 16 vertices
and 24 edges, valid indices, and the coordinate bounds above. Test every angle
and near-plane crossing before using a larger model.

## Why the whole object no longer blinks

The LCD has no atomic framebuffer swap in this firmware. Version 2 builds the
next scene completely in RAM while the old scene remains visible. It encodes
five palette colors in three one-bit planes per scene:

| Index | RGB565 | Meaning |
| --- | --- | --- |
| 0 | 0000 | Background |
| 1 | 4AD1 | Dim star |
| 2 | B7FF | Bright star |
| 3 | 07FF | Edge |
| 4 | 87FF | Bright edge |

A row uses 31 bytes, one plane 3441 bytes, one scene 10323 bytes. Two scenes
use 20646 bytes of BSS, plus 444 bytes of inclusive row byte spans. This is
not a full-screen RGB565 buffer. Pixel X uses bit `X & 7`, least bit first;
bits 4..7 of the final byte are padding and never reach the LCD.

The frame state machine is cooperative:

| Phase | Operation per call | LCD traffic |
| --- | --- | --- |
| 0 | Latch requested model and start frame | None |
| 1 | Rotate/project one vertex | None |
| 2 | Prepare one clipped edge | None |
| 3 | Clear up to 16 working-mask rows | None |
| 4 | Move and stamp one 2x2 star | None |
| 5 | Stamp one edge over the stars | None |
| 6 | Add/recolor changed nonblack pixels | Bounded delta chunk |
| 7 | Remove obsolete pixels, then commit | Bounded delta chunk |

The changed-bit mask is the OR of XORs between the three old/new planes.
Pass 0 intersects it with nonblack new coverage. Pass 1 intersects it with
black new coverage. Therefore no obsolete edge is erased over a new star or
edge. Matching pixels are never sent; each changed final pixel is written once.
Stars are stamped before edges, resolving their intersections in the mask.

Each emission step inspects at most 16 rows and four nonempty row byte spans
(at most 976 LCD pixel writes). The assembly visits only set candidate bits.
After **both complete passes**, scene/span pointers swap without a bulk copy.
Brief overlap of old/new positions is possible during a transition; an entire
object erase is avoided.

ASM immediate operands mirror the viewport: stride 31, plane offset 3441,
right bound 462 (exclusive). If the viewport changes, update these operands,
the mask constants and tests together. Kernels are non-reentrant, preserve DE
for the sccz80 ABI, and use only Intel 8080 instructions.

## Application scheduling and controls

CPU INT is grounded and RTC INT is not connected. There is no interrupt-based
tick; HLT would stop normal operation. Each main iteration delays briefly,
debounces buttons, services the application, renders one UI region and calls
`vector_step` twice. Clock service occurs every 16 iterations and additionally
at bounded LCD-fill yields. UI fills yield every four rows. Avoid recursive
renderer entry from an application service callback.

| Button | Normal screen | Time setting |
| --- | --- | --- |
| SW1 / emulator 1 | Ship / cube | Increment field |
| SW2 / emulator 2 | Run / pause | Decrement field |
| SW3 / emulator 3 | Enter setting | Hour, minute, weekday, date, month, year, exit |

Buttons are active low on PC1..PC3. Eight stable polls accept one falling-edge
event, with no hold repeat. Poll cadence varies with work, so this is not an
eight-millisecond debounce. A seven-event ring preserves accepted actions.
Pause/model requests finish any active frame transition; geometry latches the
next model at a frame boundary. Time setting expires after six observed
seconds, with a 3000-loop fallback when RTC is unreliable.

The two-wire DS18B20 uses PB0 conversion power. Reprogramming the 8255 would
drop that power; RTC/I2C transactions and queued time edits wait for release.
LCD and visual controls continue. Eighty measured delays guarantee at least
800 ms of power; actual loaded conversion time can be longer. RTC catches up
at the next valid poll. BME initialization, measurement, DS start and forecast
are separate service stages rather than one long burst.

Temperature priority is valid DS, then valid BME. Missing data shows `--` /
`WX NO DATA`; invalid RTC shows dashes. Time is 24-hour, date DD.MM.YYYY,
temperature Celsius, humidity percent, pressure mmHg, trend Pa. Forecast
altitude is configured to 255 m. No synthetic sensor source is enabled.

## Measured accepted version

| Target | ROM used | Static RAM | Gap to initial SP |
| --- | ---: | ---: | ---: |
| Hardware | 27831 | 24358 | 7898 |
| Emulator | 27738 | 24357 | 7899 |

Observed full-main stack depth was 189 bytes; isolated calls reached 191 bytes.
The gap to SP is not a stack-depth measurement. Full-main hardware instruction
paths at 3555556 Hz averaged 2836786 cycles/ship frame (1.253 FPS) and 3015321
cycles/cube frame (1.179 FPS), using a digital RTC fixture with sensors absent.
These are instruction-core timings, not physical display refresh measurements.
The user accepted the visual improvement and selected v2 as the main version.

Expected ROM SHA256, unchanged by the explanatory source comments:

```text
hardware  f253f3194292b9d76030fa70b23b0557b0fd9c4bbe4e186e9801b234e3af46cd
emulator  fa81d69c63da051c132b60bade69fce2248875aace47aa72429b751cad451729
```

In the full project checkout the extended checks are:

```bat
python variants\vector-terminal\tests\run.py
python variants\vector-terminal\tests\startup_pause.py
python variants\vector-terminal\tests\anti_flicker.py
```

The main suite additionally checks the unchanged `clock-en` provenance snapshot;
the A/B test requires the immutable `releases/working-v1` archive. The clean
source kit includes standard `verify` and the standalone BME-stage test; it
omits these full-project fixture dependencies. It includes the captured A/B
evidence and example screens. Source-kit build checks compare both generated
ROM hashes to the accepted version, then run CRT/LCD verification.

## Source provenance

The completed clock-en drivers and CRT are copied locally. New vector terminal
modules sit alongside them; no change of license is assigned to inherited
code. Original notices identify QDtech/LCDWIKI LCD/GUI material, Zilogic Systems
I2C, the simple-circuit BME driver, and the forecast's inherited attribution.
Legacy notices have not been removed or recoded. The bundled unchanged emulator
core retains its own MIT license in `emulator/LICENSE`.
