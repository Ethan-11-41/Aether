# Milestone 0: what to test

About 10 minutes. Tick each item, and note anything that doesn't match "what you should see".

## 1. Install

1. Download **Aether-Setup-0.1.0** from the newest green run on the
   [Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**), unzip it,
   and run `Aether-Setup-0.1.0.exe`.
2. If Windows says "Windows protected your PC": click **More info → Run anyway**.
3. Finish the installer and tick "Launch Aether".

**You should see:** the Aether window with three panels: *Audio device*, *Check your sound* and
*Latency (delay)*. Aether appears in the Start menu, with the wavy icon.

## 2. Sound comes out

1. Plug in wired headphones or use your speakers, at a low volume.
2. Click **Play test tone**.

**You should see:** the button turns teal and says *Stop test tone*; the two *Output* meters show
green bars at about **-18 dB**.
**You should hear:** a steady, quiet beep with no click at the start. Click again: it fades out
without a click.

## 3. Sound goes in (microphone)

1. Under *Audio device* on the left, set **Input** to your microphone (if it isn't already).
2. Speak, or clap near the microphone.

**You should see:** the *Input (microphone)* meters move. If they stay empty, check
**Windows Settings → Privacy & security → Microphone** and make sure "Let desktop apps access
your microphone" is on.

## 4. Device settings and latency

1. Note the *Latency* numbers. With *Windows Audio* they are typically 10–40 ms round trip.
2. Change the device type (top drop-down) to **Windows Audio (Exclusive Mode)**, then try
   **Windows Audio (Low Latency Mode)**.
3. Change **Audio buffer size** to a smaller and a larger value.

**You should see:** the latency numbers update immediately; smaller buffers give smaller numbers.
The test tone keeps playing (in exclusive mode other programs go silent: that's normal).
If a very small buffer makes the tone crackle, that's your computer's limit: pick the smallest
size that sounds clean.

## 5. Settings are remembered

1. Pick a device and buffer size, close Aether, and start it again.

**You should see:** the same device and buffer size as before.

## 6. Odd situations (shouldn't crash)

1. Unplug your headphones while the tone plays (if they're USB or the PC has a headphone jack).
2. Start Aether a second time while it's already open.

**You should see:** no crash. Starting it a second time brings the existing window to the front
instead of opening another copy.

## Reporting back

Tell me which steps worked and which didn't, plus your round-trip latency numbers for
*Windows Audio* and *Exclusive Mode*. A screenshot helps if something looks wrong.
