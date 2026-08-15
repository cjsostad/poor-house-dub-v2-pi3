# README-PI3

This document is the operating manual for the Poor House Dub v2 build running
on this Raspberry Pi 3 (64-bit Raspberry Pi OS, Debian Trixie). It supersedes
`README.md`, `HARDWARE.md`, and `QUICKSTART.md` wherever they disagree with the
source, and it flags each of those disagreements explicitly. The rule
throughout: the C++ source in `cpp/` is authoritative. If prose contradicts
the code, the code wins.

A separate note before anything else. This fork was cloned into
`~/poor-house-dub-v2` and re-hosted on the author's account with fresh history.
The GPIO 2 to GPIO 5 hardware fix described in section 3 is **still pending in
this checkout**. The source constant, the `ALL_PINS` array in the C++ file, and
the encoder list in `gpio_cleanup.sh` all still say `2`. Read section 3 before
you build. Nothing on GPIO 5 will work until the three edits described there
are made.

---

## 1. Running it

From a fresh boot, log in and run:

```
cd ~/poor-house-dub-v2/cpp
./build/dubsiren
```

That is the normal case with no arguments. The program will initialise the
audio engine at 48 kHz / 256 samples / stereo, open the default ALSA device
(see section 2), initialise all encoders, buttons, the pitch envelope switch
and the optional LED, and print a control-surface banner. It will then sit
in its main loop until you send it `Ctrl+C` or press the shutdown button.

To rebuild after code changes:

```
cd ~/poor-house-dub-v2/cpp
./build.sh
```

The binary lands at `cpp/build/dubsiren`.

### Command-line flags

Parsed in `cpp/src/main.cpp`. All are optional.

| Flag              | Argument   | Default   | Effect                                                 |
| ----------------- | ---------- | --------- | ------------------------------------------------------ |
| `--sample-rate`   | integer Hz | `48000`   | Passed to `AudioEngine` and `AudioOutput`.             |
| `--buffer-size`   | samples    | `256`     | ALSA period size. See section 12 for tuning.           |
| `--device`        | ALSA name  | `default` | e.g. `hw:1,0` to bypass `/etc/asound.conf` (section 2). |
| `--simulate`      | none       | off       | Uses `SimulatedAudioOutput` and `SimulatedController`. No GPIO or ALSA. |
| `--interactive`   | none       | off       | Uses the real `AudioOutput` but the `SimulatedController` for keyboard input. |
| `--help`, `-h`    | none       | —         | Print usage and exit.                                  |

Two important consequences follow from how `main.cpp` wires these up:

- `--interactive` and `--simulate` both instantiate `SimulatedController`
  (see the `if (simulate || interactive)` branch in `main`). Neither of them
  reads GPIO. **You cannot use either flag to test the physical encoders,
  switches, or trigger button.** For hardware testing, run with no flags.
- The only difference between `--simulate` and `--interactive` is the audio
  path. `--simulate` uses `SimulatedAudioOutput` (no ALSA); `--interactive`
  uses the real ALSA output. Neither one talks to the control surface
  hardware.

---

## 2. Audio configuration — this Pi uses card 1

This is the most consequential Pi 3 detail on this box. It is not the same as
the project's Pi 4 and it will bite anyone who assumes it is.

On this Pi 3, `aplay -l` shows exactly two cards:

```
card 0: vc4hdmi
card 1: sndrpihifiberry [snd_rpi_hifiberry_dac]
```

The PCM5102A DAC is **card 1**. On the Pi 4 build the same DAC is card 2,
because the Pi 4 exposes three audio devices instead of two. **The card
number is a property of the machine, not the DAC.** If you move an SD card
between the two Pis, run `aplay -l` on first boot and adjust
`/etc/asound.conf` before you launch the siren.

### The failure mode

When `/etc/asound.conf` does not exist and no `~/.asoundrc` overrides it,
ALSA defaults to card 0. On this board that is HDMI. The dub siren opens the
"default" device successfully, ALSA reports no error, the trigger button
works, debug logs scroll past — and there is simply no sound at the PCM5102A.
This looks exactly like a hardware fault (bad solder joint, dead DAC, broken
jack) and it is not. Check `/etc/asound.conf` before you unsolder anything.

### The working config

`/etc/asound.conf` is authoritative and applies to every user on the system.
`~/.asoundrc` is a per-user override; do not use it here. Contents:

```
pcm.!default {
    type hw
    card 1
    device 0
}

ctl.!default {
    type hw
    card 1
}
```

Write it with `sudo`, then verify from the dubsiren user account with
`aplay -D default /usr/share/sounds/alsa/Front_Center.wav`.

### Bypassing the config for testing

Before committing to `/etc/asound.conf`, you can prove which card is the DAC:

```
./build/dubsiren --device hw:1,0
```

If sound comes out of the DAC with `hw:1,0` and not with `default`, the
config file is the problem.

---

## 3. The GPIO 2 to GPIO 5 modification

This is the single most important local difference between this fork and
upstream. It is currently **not applied to the source in this checkout** —
the fork's own physical wiring uses GPIO 5, but the C++ constant, the pin
array, and the cleanup script still all say `2`. All three must be changed
together or the build will silently misbehave in one of two ways: either the
program will read pin 2 (which is not wired anywhere) and Encoder 1 will look
dead, or `gpio_cleanup.sh` will fail to release pin 5 after a crash and
subsequent runs will fail to acquire it.

### What needs to change

**1) `cpp/include/Hardware/GPIOController.h`, line 25.** Current source:

```cpp
constexpr int ENCODER_1_CLK = 17;
constexpr int ENCODER_1_DT = 2;
```

Change `2` to `5`.

**2) `cpp/src/Hardware/GPIOController.cpp`, in the `ALL_PINS` array around
line 38.** Current source:

```cpp
const unsigned int ALL_PINS[] = {
    2, 3, 4, 9, 10, 13, 14, 15, 17, 20, 22, 23, 24, 26, 27
};
```

Replace the leading `2` with `5`. Keep the array sorted or the lookup table
`gpioToLineIndex[28]` still works — the index is by GPIO number, not by
position — but sorted is easier to read.

**3) `gpio_cleanup.sh`, line 12.** Current source:

```
ENCODER_PINS=(17 2 27 22 23 24 20 26 14 13)
```

Change the `2` to `5`.

All three edits must land in the same commit. Any merge or rebase from
upstream `parkredding/poor-house-dub-v2` will revert them silently; add them
back before rebuilding.

### Why the change is necessary

GPIO 2 and GPIO 3 are the Pi's I2C bus (SDA and SCL). Every 40-pin Raspberry
Pi ties both pins to 3.3 V through fixed **1.8 kΩ hardware pull-up resistors
on the board**. These resistors are not part of the SoC — they are on the Pi
PCB — and they cannot be disabled in software. `raspi-config`, device-tree
overlays, and libgpiod all leave them in place. This is by design: the Pi is
supposed to be able to act as an I2C master without external pull-ups.

The KY-040-style rotary encoder breakout modules used in this build add their
own 10 kΩ pull-up to VCC on the CLK and DT lines, plus a small RC debounce
network in series with the switch. When such a module is wired to GPIO 2 or
3, the Pi's 1.8 kΩ pull-up sits in parallel with the module's 10 kΩ pull-up
on one side, and the encoder's series resistance (~10 kΩ from the RC network)
sits between the pin and ground on the other. The result is a voltage
divider that clamps the pin's low state at roughly:

```
V_low ≈ 3.3 V × 10 kΩ / (10 kΩ + ~1.5 kΩ effective) ≈ 2.8 V
```

The Pi's input threshold is around 0.8 V. 2.8 V is nowhere near it. The pin
therefore never registers a logical LOW, the CLK edge in `RotaryEncoder::update()`
never fires, and the encoder appears completely dead — no debug lines at all
from `[ENC 2/17]`. Upstream's default only works with a bare EC11 encoder that
shorts CLK and DT directly to ground, because a hard short wins against the
divider. With a breakout module in the path, GPIO 2 is unusable.

**General rule for this hardware family: GPIO 2 and GPIO 3 are never safe for
encoder breakout modules.** GPIO 3 remains fine for the shutdown button
because a bare tactile switch shorts hard to ground and beats the divider
comfortably.

GPIO 5 (physical pin 29) has no fixed pull-up and no alternate-function
conflict on Pi 3 in the default configuration, which is why it is the chosen
replacement.

---

## 4. Complete GPIO map as built

BCM numbering with the physical 40-pin header positions. Every value below
matches the constants in `cpp/include/Hardware/GPIOController.h` (except
`ENCODER_1_DT`, which awaits the section 3 patch).

### Encoders

Each encoder controls one Bank A parameter (encoders default to Bank A when
Shift is not held) and one Bank B parameter (with Shift held). This table
lists the physical wiring and the Bank A assignment; Bank B is in section 8.

| Encoder | Bank A parameter  | CLK              | DT                              |
| ------- | ----------------- | ---------------- | ------------------------------- |
| 1       | LFO Depth         | GPIO 17 (pin 11) | **GPIO 5 (pin 29)** *— after section 3 patch; source currently says GPIO 2 (pin 3)* |
| 2       | Base Frequency    | GPIO 27 (pin 13) | GPIO 22 (pin 15)                |
| 3       | Filter Frequency  | GPIO 23 (pin 16) | GPIO 24 (pin 18)                |
| 4       | Delay Feedback    | GPIO 20 (pin 38) | GPIO 26 (pin 37)                |
| 5       | Reverb Mix        | GPIO 14 (pin 8)  | GPIO 13 (pin 33)                |

### Buttons and switch

| Control          | GPIO        | Physical pin |
| ---------------- | ----------- | ------------ |
| Trigger          | GPIO 4      | pin 7        |
| Shift            | GPIO 15     | pin 10       |
| Shutdown         | GPIO 3      | pin 5        |
| Pitch env — up   | GPIO 10 (MOSI) | pin 19    |
| Pitch env — down | GPIO 9 (MISO)  | pin 21    |

The pitch envelope switch is a SPDT ON/OFF/ON with the centre lug wired to
ground. Only one of GPIO 9 and GPIO 10 is grounded at a time; in the middle
position both float and libgpiod's pull-up returns them both to logic HIGH,
which `ThreePositionSwitch::readPosition()` reads as `Off`.

### Optional LED

| Signal   | GPIO         | Physical pin |
| -------- | ------------ | ------------ |
| WS2812   | GPIO 12 (PWM0) | pin 32    |

The LED is optional — `LEDController::init()` failing is not fatal, the code
just resets the unique_ptr and continues.

### Power to the encoder modules

Every KY-040-style encoder module has three power pads: `GND`, `+`, and
sometimes `SW` (for the pushbutton, unused here). **The `+` pad must go to
3.3 V, not 5 V.** Physical pins 1 or 17 are the two 3.3 V rails on the header;
either is fine. Without VCC on `+`, the module's onboard pull-up network is
unpowered, both CLK and DT float, no edges fire, and the encoder appears
broken. Never wire the `+` pad to 5 V — the Pi's GPIO inputs are not 5 V
tolerant and you will damage the SoC.

### DAC (PCM5102A) wiring

Only one important note here, because it contradicts `HARDWARE.md` and cost
real debugging time. **The `XSMT` pin on the PCM5102A must be tied to 3.3 V,
not ground.** `XSMT` is active-low soft-mute: pull it low and the DAC mutes
its output. `HARDWARE.md` documents grounding it, which produces the same
"program runs, no sound" symptom as the ALSA card mistake in section 2 and
looks identical to it from the outside. The working build in this fork ties
`XSMT` to 3.3 V. It stays there.

Beyond that, the DAC's I²S wiring is standard: BCK from GPIO 18 (pin 12),
LRCK from GPIO 19 (pin 35), DIN from GPIO 21 (pin 40), GND to GND, VIN to
5 V. These are set by the device-tree overlay, not by dubsiren, and are not
in the C++ pin table.

---

## 5. Pin conflicts with other peripherals

Two of the encoder pins share silicon with other Pi peripherals that must be
disabled or they will hold the pin and prevent libgpiod from claiming it.

### UART on GPIO 14

**Encoder 5 CLK is GPIO 14, which is UART TX.** If the serial console is
enabled, the kernel's UART driver owns the pin and libgpiod's
`gpiod_chip_request_lines()` call will still succeed — but the pin's state
will be driven by the UART, not by the encoder, and rotating the encoder will
produce no `[ENC 14/13]` debug lines at all. The failure is silent.

Disable the serial console with:

```
sudo raspi-config
```

then Interface Options → Serial Port → No (login shell) → No (hardware
enabled). Reboot. Confirm with `dmesg | grep tty` — you should not see
`ttyAMA0` bound to `/dev/serial0`.

### SPI on GPIO 9 and 10

**The pitch envelope switch uses GPIO 9 (SPI MISO) and GPIO 10 (SPI MOSI).**
If SPI is enabled, the SPI kernel driver holds both pins and the switch
appears dead — no `[PITCH SW]` debug lines when you flip it. On this
particular Pi 3's SD card SPI was already disabled and the switch worked on
first test. If you re-image the card or move it to another Pi, disable SPI
with `raspi-config` → Interface Options → SPI → No, then reboot.

The same silent-failure pattern applies to both: no error message, no
exception, no failed init line in the dubsiren banner. The only symptom is
that the debug logs from section 11 stop appearing for the affected pin.

---

## 6. Encoders have no software debounce

`RotaryEncoder::update()` in `cpp/src/Hardware/GPIOController.cpp` (starting
around line 215) is a bare edge detector. On every 1 ms poll it reads CLK and
DT via libgpiod, compares CLK to its previous value, and if it changed, fires
the callback once with a direction derived from whether DT agrees with CLK.
There is no time filter, no minimum interval between edges, and no quadrature
state table. Any glitch on CLK is a callback.

By contrast, `MomentarySwitch` (line ~250) uses `DEBOUNCE_MS = 10` and
enforces `MIN_PRESS_MS = 30`; `ThreePositionSwitch` (line ~355) uses
`DEBOUNCE_MS = 20`. Buttons and the pitch switch are debounced. Encoders are
not.

The practical consequence: **clean encoder behaviour depends entirely on the
RC filter on the breakout module.** The KY-040 modules used here have a
small cap-and-resistor low-pass on each of CLK and DT that smooths contact
bounce below the 1 ms poll interval. A bare EC11 encoder wired straight to a
GPIO pin has no such filter, and produces spurious direction reversals and
skipped detents on every rotation. If you plan to substitute a different
encoder, verify that either the module has an RC filter or the encoder
itself has clean transitions under a scope.

### Bourns PEC12R was evaluated and rejected

Bourns PEC12R-4022F-S0024 (24 pulse, no detent, PC pin) was tried during the
build and rejected. It has no detents (so parameter values scroll continuously
under any hand tremor), no threaded bushing (so it cannot be panel-mounted
without a machined bracket), and no onboard filtering (so it inherits the
debounce problem above). The KY-040 breakout with a mechanical detent EC11 is
the right choice for this application.

---

## 7. Appliance mode is broken — both scripts

Both `enable_appliance_mode.sh` and `disable_appliance_mode.sh` in the repo
root have distinct bugs. Neither should be trusted. Read this section before
running either.

### `enable_appliance_mode.sh` half-succeeds and then aborts

The script runs under `set -e`. Toward the bottom it does:

```
raspi-config nonint do_overlayfs 0     # succeeds — overlayfs enabled
raspi-config nonint do_boot_ro 0       # ← function does not exist
```

`do_boot_ro` is not a function in the version of `raspi-config` shipped on
Raspberry Pi OS Trixie (and has not been for some time). `raspi-config nonint`
returns non-zero on an unknown function name, `set -e` aborts the script, and
the "Appliance Mode Enabled!" banner, the reboot prompt, and the final status
messages never print. What actually happened: overlayfs **is** enabled, the
boot partition is **not** write-protected (which is fine, since that step
never really worked), and the user is left staring at what looks like an
error message — believing the whole thing failed and did nothing.

It half-succeeded. Reboot and overlayfs will be live.

### `disable_appliance_mode.sh` returns a false negative

Early in the script:

```
if ! mount | grep -q "overlay on / "; then
    echo "ℹ️  Appliance mode is not currently enabled."
    exit 0
fi
```

The mount source for the overlay on this system is `overlayroot`, not
`overlay`. `mount` prints:

```
overlayroot on / type overlay (rw,...)
```

The grep pattern `overlay on / ` never matches. The script cheerfully
announces "Appliance mode is not currently enabled" and exits 0 without
touching anything — even though the filesystem is very much read-only and any
changes you were about to make will be discarded on reboot. This is worse
than a crash: it makes the real state invisible.

### Reliable check

Do not use either script's built-in detection. Use:

```
findmnt -n -o SOURCE,FSTYPE /
```

- `overlayroot overlay` → appliance mode is active, writes are going to RAM.
- `/dev/mmcblk0p2 ext4` (or similar) → the root filesystem is writable.

### Reliable disable

Skip `disable_appliance_mode.sh`. Do it by hand:

```
sudo raspi-config nonint do_overlayfs 1
sudo reboot
```

After reboot, run `findmnt -n -o SOURCE,FSTYPE /` again and confirm you see
the real block device, not `overlayroot`.

### The symptom this caused

The confusion this created is worth documenting. With overlayfs active:

```
cd ~/poor-house-dub-v2/cpp
./build.sh          # succeeds
./build/dubsiren    # runs, plays audio, everything looks fine
```

Both commands succeed. The compiler writes object files, `dubsiren` runs from
the freshly-linked binary, ALSA output works. Every one of those writes goes
to a tmpfs overlay in RAM. On the next reboot:

```
find ~ -name dubsiren
# (nothing)
```

The binary is gone. The source tree survives only because it was cloned before
overlayfs was enabled — that clone was written to the underlying ext4. Any
subsequent `git pull`, edit, or rebuild dies at reboot.

### Correct ordering

**Appliance mode is the last step in setup, not the first.** The correct
sequence is:

1. Assemble hardware, wire the DAC and controls.
2. Boot with a writable filesystem, install packages, configure ALSA, build
   `dubsiren`, disable UART and SPI, wire and test every encoder and switch.
3. Run the siren for a rehearsal. Confirm audio, confirm every encoder,
   confirm the shutdown button.
4. **Only then**, enable appliance mode — protecting the SD card against
   the eventual power yank at load-out.

Any `raspi-config` change made while overlayfs is active — including SPI or
serial toggles — is discarded on reboot. If you find you need to change
something, disable appliance mode first.

---

## 8. Control reference

The following tables reflect what `handleEncoder()` in
`cpp/src/Hardware/GPIOController.cpp` (around line 590) actually does. The
class comment in `GPIOController.h` (around line 172) and the current
`README.md` both say Encoder 1 in Bank A is Volume. **They are wrong. The code
says LFO Depth.** The comment survives from an older revision and has not been
updated.

Shift is a **momentary hold**. Pressing and holding Shift activates Bank B;
releasing returns to Bank A. There is no latch. In NJD and UFO secret modes
(section 9), Shift behaves differently — it cycles presets instead of
switching banks.

### Bank A

| Encoder | Parameter        | Range              | Response      | Notes |
| ------- | ---------------- | ------------------ | ------------- | ----- |
| 1       | LFO Depth        | 0.0 – 1.0          | Linear (0.042/step) | Filter modulation depth, not amplitude. |
| 2       | Base Frequency   | 50 – 2000 Hz       | Logarithmic (×1.165/step) | Full range in ~24 detents. |
| 3       | Filter Frequency | 20 – 20000 Hz      | Logarithmic (×1.32/step) | Full range in ~24 detents. |
| 4       | Delay Feedback   | 0.0 – 0.95         | Linear (0.04/step)  | Capped below 1.0 to prevent runaway. |
| 5       | Reverb Mix       | 0.0 – 1.0          | Linear (0.042/step) | Wet/dry. |

### Bank B (Shift held)

| Encoder | Parameter        | Range           | Response      | Notes |
| ------- | ---------------- | --------------- | ------------- | ----- |
| 1       | LFO Rate         | 0.1 – 20 Hz     | Logarithmic (×1.15/step) | |
| 2       | Delay Time       | 0.001 – 2.0 s   | Linear (0.083/step) | |
| 3       | Filter Resonance | 0.0 – 0.95      | Linear (0.04/step)  | |
| 4       | Osc Waveform     | Sine / Square / Saw / Triangle | Discrete cycle | Wraps `(x + dir + 4) % 4`. See `Waveform` in `Common.h`. |
| 5       | Reverb Size      | 0.0 – 1.0       | Linear (0.042/step) | |

### Controls not reachable from any encoder

`Parameters.volume = 0.6f` and `Parameters.release = 0.5f` are set at
startup and never touched by encoder input. The docstring in the header
mentioning "Release Time" on Bank B encoder 1 is stale — that slot is now LFO
Rate. **Master volume and release time can only be changed by editing the
source and rebuilding.**

Similarly, `engine.setLfoWaveform(Waveform::Triangle)` is called during
`start()` and never called again. The LFO waveform is effectively hardcoded
to Triangle, even though `AudioEngine::setLfoWaveform()` exists and works —
there is simply no encoder or command bound to it.

### Pitch envelope switch (SPDT ON/OFF/ON)

| Position | `SwitchPosition` | Effect on engine                        |
| -------- | ---------------- | --------------------------------------- |
| Up       | `Up`             | `PitchEnvelopeMode::Up` — pitch rises on trigger. |
| Middle   | `Off`            | `PitchEnvelopeMode::None` — no pitch env. |
| Down     | `Down`           | `PitchEnvelopeMode::Down` — pitch falls on trigger. |

The pitch envelope is applied on every trigger and works in all secret modes.

### Shutdown button

Pressing GPIO 3 fires `onShutdownPress()`, which prints a banner, invokes the
shutdown callback (which sets the main-loop exit flag), and then executes
`sudo shutdown -h now`. The dubsiren binary must be launched with sudo
credentials cached, or `sudo shutdown` must be configured passwordless in
`/etc/sudoers.d/`, or the shutdown will fail silently.

---

## 9. Secret modes

Rapidly tap the Shift button. The number of taps within a rolling **2-second
window** activates a mode. Repeating the same tap count while already in that
mode exits it and restores the Auto Wail defaults (for NJD and UFO — Pitch-
Delay is not a preset change and restores nothing on exit).

| Tap count | Mode             | Effect |
| --------- | ---------------- | ------ |
| 3         | Pitch-Delay Link | Base-frequency encoder also scales delay time inversely (higher pitch → shorter delay). No preset change. Shift still switches banks A/B normally. |
| 5         | NJD Siren        | Classic dub siren presets. Shift **cycles presets** instead of switching banks. |
| 10        | UFO              | Sci-fi presets. Shift **cycles presets** instead of switching banks. |

### NJD presets (5 total, indexed 0–4)

| # | Name       | Character                                                   |
| - | ---------- | ----------------------------------------------------------- |
| 1 | Auto Wail  | 440 Hz A4, square, LFO at 2 Hz with pitch depth 0.5 — the wee-woo. |
| 2 | Classic    | 587 Hz D5, square, dotted-eighth delay, deep dub reverb.    |
| 3 | Alert      | 440 Hz square, short 0.3 s release for rapid re-triggering. |
| 4 | Bright     | 880 Hz A5, 6 kHz filter — cuts through a busy mix.          |
| 5 | Wobble     | 392 Hz G4, sawtooth, 0.75 resonance — heavy formant motion. |

### UFO presets (4 total, indexed 0–3)

| # | Name          | Character                                                  |
| - | ------------- | ---------------------------------------------------------- |
| 1 | Laser Blast   | 1.6 kHz, 6 kHz filter, 30 ms delay, dry — Star Wars pew.   |
| 2 | Flying Saucer | 1.2 kHz sine, 2 s release, 0.7 feedback, huge reverb.      |
| 3 | Alien Signal  | 1.8 kHz square, 8 kHz filter, heavy feedback — beeps.      |
| 4 | Warp Drive    | 80 Hz sawtooth, 0.85 resonance, 3 s release — sub rumble.  |

### Exit behaviour

- **NJD and UFO exit**: restores every Auto Wail parameter — base freq 440,
  filter 3 kHz, resonance 0.5, release 0.5 s, square, delay time 0.375 s,
  feedback 0.55, reverb size 0.7, mix 0.4, LFO 2 Hz Triangle with pitch
  depth 0.5. Whatever you had set before entering the mode is gone.
- **Pitch-Delay exit**: does not touch parameters. Only the inverse
  base-frequency-to-delay-time link is removed. Any encoder tweaks you made
  while in the mode are preserved.

---

## 10. Default patch (startup values)

Every value in the `Parameters` struct at
`cpp/include/Hardware/GPIOController.h` around line 219, applied to the
engine in `GPIOController::start()`. This is the Auto Wail patch — pressing
trigger immediately without touching anything produces a wee-woo siren.

| Parameter        | Value    | Encoder access           |
| ---------------- | -------- | ------------------------ |
| Master Volume    | 0.6      | none (hardcoded)         |
| LFO Depth        | 0.5      | Bank A, encoder 1        |
| LFO Rate         | 2.0 Hz   | Bank B, encoder 1        |
| LFO Pitch Depth  | 0.5      | none (hardcoded)         |
| LFO Waveform     | Triangle | none (hardcoded)         |
| Base Frequency   | 440 Hz   | Bank A, encoder 2        |
| Filter Frequency | 3000 Hz  | Bank A, encoder 3        |
| Filter Resonance | 0.5      | Bank B, encoder 3        |
| Delay Feedback   | 0.55     | Bank A, encoder 4        |
| Delay Time       | 0.375 s  | Bank B, encoder 2        |
| Reverb Mix       | 0.4      | Bank A, encoder 5        |
| Reverb Size      | 0.7      | Bank B, encoder 5        |
| Release Time     | 0.5 s    | none (hardcoded)         |
| Osc Waveform     | Square   | Bank B, encoder 4        |
| Pitch Env Mode   | (from switch position at startup) | Pitch env switch |

---

## 11. Troubleshooting

Debug logging is enabled by `#define DEBUG_INPUTS 1` near the top of
`cpp/src/Hardware/GPIOController.cpp`. Every encoder edge prints:

```
[ENC 17/5] CLK=1 DT=0
```

and every button state change prints:

```
[BTN 4] state=0 (PRESSED)
```

and the pitch switch prints:

```
[PITCH SW] UP_pin(10)=0 DOWN_pin(9)=1 -> UP
```

**The absence of these lines when you wiggle the physical control is
diagnostic.** No line means no electrical edge reached the pin. That
separates wiring problems (nothing to log) from software problems (log is
present but action is wrong).

| Symptom | Likely cause | Check |
| ------- | ------------ | ----- |
| No sound but the trigger button prints its log line and the banner says "running" | ALSA is routing to the wrong card | `aplay -l` — DAC should be card 1. `cat /etc/asound.conf`. See section 2. |
| `dubsiren` binary missing after reboot; source tree still present | Overlayfs is active; build wrote to RAM | `findmnt -n -o SOURCE,FSTYPE /` — expect `overlayroot overlay`. See section 7. |
| One encoder shows no `[ENC …]` lines at all when rotated | `+` pad not powered, or the GPIO 2/3 pull-up conflict from section 3 | Confirm 3.3 V on the module's `+` pad. If it's Encoder 1, apply the section 3 patch. |
| One encoder logs edges but rotates the parameter the wrong way | CLK and DT swapped | Swap the two wires at the header. |
| Encoder 5 dead with no error message | UART TX on GPIO 14 holding the pin | `raspi-config` → Serial Port → No login shell, No hardware. Reboot. |
| Pitch envelope switch dead with no error message | SPI holding GPIO 9/10 | `raspi-config` → SPI → No. Reboot. |
| `dubsiren` fails to start with "Failed to request GPIO lines" | Previous run crashed and left pins exported | `./gpio_cleanup.sh` in the repo root, then retry. |
| Trigger button prints `[BTN 4]` but no audio | Not `XSMT` on the DAC grounded (should be 3.3 V, section 4) — but check ALSA card first. | `speaker-test -D hw:1,0 -c 2` will play white noise through the DAC directly. |

---

## 12. Performance on Pi 3

Measured on this build with default settings (48 kHz, 256-sample buffer):

- Average CPU: ~20–25%
- Peak CPU: ~30% during heavy reverb / feedback
- Buffer underruns across a ~24,000-buffer test run (roughly 2 minutes at
  full activity): **zero**

The Pi 3 handles the current DSP graph comfortably. No buffer-size increase
is needed for normal operation.

If audible dropouts do appear — most likely under heavy system load from
something else on the SD card — the buffer size can be raised without
recompiling:

```
./build/dubsiren --buffer-size 512
```

This doubles the audio latency (roughly 5 ms → 11 ms round-trip at 48 kHz)
in exchange for headroom against scheduling jitter. Values above 1024 are
not recommended; the trigger response becomes noticeably sluggish.
