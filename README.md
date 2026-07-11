# k4 Wobble 🌀

A free **tempo-synced wobble filter** for FL Studio (and any VST3 music software) on **Windows** and **Mac**. Drop it on a bass, hit play, and it wobs.

- **The classic dubstep wobble** — an LFO synced to your project tempo sweeps a resonant filter
- **A rate pattern sequencer** — each beat can pick its own LFO speed, so you can program the
  "wob… wob… wobwobwob" basslines that normally need hand-drawn automation lanes
- **Five filter flavours** — Low-pass, Band-pass, High-pass, Notch, and **Talk** (vowel formants for talking bass)
- **Drive** for growl, **Width** for stereo swirl, and a **live mesh display** so you can *see* the
  filter chewing on your sound (violet = closed, cyan = open; the thin line is the LFO itself)
- **Presets** to start from, and hover **tips** on every control (turn them off with the Tips box once you know your way around)

---

## 🎛️ Controls

| Control | What it does |
|---|---|
| **Rate** | Wobble speed as a note length (1/4 = one wob per beat). Triplets and dotted included. |
| **Shape** | The motion: Sine (smooth), Triangle, Saw Down (snap open, sweep shut), Saw Up, Square (hard chop), Random (locked to the song — repeats identically every loop). |
| **Filter** | Low-pass is *the* wobble. Band-pass is hollow, High-pass thins, Notch is phasery, Talk morphs "oo"→"ah" vowels. |
| **Cutoff** | Top of the sweep — the filter opens up to here. In Talk mode it shifts the voice character. |
| **Res** | Resonance. High = the aggressive squelchy zone. |
| **Depth** | How far down the sweep goes (up to 5 octaves). In Talk mode, how far the vowels morph. |
| **Drive** | Saturation after the filter. Growl knob. |
| **Split** | **Sub Guard** — everything below this frequency stays clean and dry, so the wobble growls *on top of* a solid sub. 0 = off. Try 90–120 Hz. |
| **Width** | Offsets the right channel's LFO — the wobble swirls between speakers. 0° is mono-safe. |
| **Mix** | Dry/wet blend. |

### Advanced

The **Advanced** button reveals the deeper controls — the simple view stays exactly as it was:

| Control | What it does |
|---|---|
| **Swing** | Shuffles every second pattern step for a head-nod groove. |
| **Push** | Shifts the LFO phase so the wob peaks ahead of / behind the beat. |
| **Lazy** | Glides the filter between LFO values instead of snapping — rubbery, smeared wobbles. |
| **Trim** | Output level, after everything else. |
| **Slope** | 12 or 24 dB filter steepness. |
| **Drive mode** | Soft (tube-ish), Hard (clip), Fold (metallic wavefold). |
| **Step Len** | How long each pattern step lasts: half a beat, one beat, or two. |
| **Auto-Gain** | Drive changes the tone, not the loudness — honest A/B comparisons. |

### The Pattern

Turn **Pattern** on and the Rate box hands control to the step boxes (up to **16 steps**). Each
box is one step and sets the LFO rate — click to cycle, right-click to cycle back, drag up/down
or scroll to dial one in. "—" is a rest (the filter stays open). **Steps** sets the pattern
length. The pattern is locked to your song position, so it always lands the same way, every
loop, every render.

With **Advanced** on, lane tabs appear above the strip: besides **Rate**, each step can also set
its own **Depth** (accents), **Cutoff** (per-step filter shifts) and **Shape** (override the LFO
shape for that step). Right-click resets a step on the continuous lanes.

### Presets & the Dice

30+ factory presets organized by category, and **Save** writes your own presets to
`Documents/k4 Audio/k4 Wobble Presets` — they show up in the same menu under *User Presets*.

The **Dice** button rolls a random patch that is always usable: every value comes from a curated
musical range, and it never touches **Mix, Split, Trim or Auto-Gain** — your levels and routing
stay exactly as you set them.

Everything syncs to the host tempo; when the transport is stopped, the wobble free-runs at the
last known tempo (140 BPM before the first play) so you can still hear what you're dialling in.

---

## ⬇️ Download

Get the latest version from the **[Releases page](https://github.com/kr4wford/k4Wobble/releases/latest)** and download the file for your computer:

| Your computer | Download this |
|---|---|
| **Windows** | `k4Wobble-Windows.zip` |
| **Mac (Apple Silicon — M1/M2/M3/M4)** | `k4Wobble-macOS-AppleSilicon.zip` |

> Not sure which Mac you have? Click the  Apple menu → **About This Mac**. If it says **Apple M1/M2/M3/M4**, you're on Apple Silicon.

## 🪟 Install on Windows

1. **Unzip** the downloaded file. You'll get a folder named **`k4 Wobble.vst3`**.
2. **Copy that folder** into:
   ```
   C:\Program Files\Common Files\VST3
   ```
3. Open **FL Studio** → **Options → Manage plugins → Find more plugins**.
4. "k4 Wobble" now appears in your plugin list. 🎉

## 🍎 Install on Mac (Apple Silicon)

1. **Unzip** the downloaded file. You'll get **`k4 Wobble.vst3`**.
2. In Finder, press **Go → Go to Folder…**, paste this and hit Enter, then drop the file in:
   ```
   ~/Library/Audio/Plug-Ins/VST3
   ```
3. **One-time unblock step.** Because this is a free plugin not signed by Apple, macOS blocks it
   at first. Open the **Terminal** app, paste the line below, and press Enter:
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"k4 Wobble.vst3"
   ```

---

## 🔧 Build from source

Needs CMake 3.22+ and a C++17 compiler. JUCE is found next to the project (`../JUCE`) or fetched
automatically.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

The plugin lands in `build/k4Wobble_artefacts/Release/VST3/`. Engine smoke tests:

```sh
cmake --build build --target k4WobbleTests && ./build/k4WobbleTests_artefacts/Release/k4WobbleTests
```
