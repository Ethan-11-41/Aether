# Milestone 9: what to test

About 25 minutes. Install **Aether-Setup-0.10.0.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). It also has
the fixes from your M8 feedback (0.9.1), so section 1 checks those first.

## 1. Your M8 feedback (0.9.1)

1. **Exclusive Mode:** Settings > Audio settings, choose **Windows Audio (Exclusive Mode)** as
   the driver, once with your Fifine microphone chosen and once with **None**.
   **You should see** either Exclusive Mode working, or an orange message saying *what* Aether
   changed (e.g. "your microphone can't be used with Exclusive Mode, so it's switched off" or
   "only works at 44.1 kHz"), rather than going straight back to Windows Audio. **Please send me
   the message's exact words again**, whatever it says.
2. **Tooltips:** hover over the Record and Loop buttons. The explanation appears **below** the
   button after a short moment. Move straight to the next button: its tooltip also waits a
   moment instead of appearing instantly.
3. **The on-screen keyboard** (Ctrl+K) in full screen: the keys keep their size, and you see the
   whole piano, centred.
4. **The font** is new (Inter). Tell me if anything looks cut off or too small anywhere.
5. **Delete project:** on the start screen, right-click a test project > **Delete project...**.
   It asks first, then the project is in the Recycle Bin (you can restore it from there).
6. **Effects on several tracks:** in the mixer, Ctrl+click two tracks' names, then **+ Effect** on
   one of them > Reverb: both get it. One Ctrl+Z removes both.

## 2. Opening the piano roll

1. Make a MIDI track (Ctrl+Shift+T). **Double-click an empty part of its lane** in bar 2. **You
   should see** a one-bar clip appear and the **piano roll** open in its own window.
2. Close it, then double-click the clip: it opens again. Right-click the clip: **Edit notes
   (piano roll)...** is there too.

## 3. Writing notes

1. **Click** on an empty place: a note appears there and you **hear it**.
2. Click and **drag to the right** before letting go: a longer note. The next click makes a note
   that long ("Length: last used").
3. **Drag a note** up/down and sideways: it moves, and you hear the new pitch. Drag its **right
   edge** to make it longer or shorter. Hold **Alt** while dragging to place it between grid lines.
4. **Right-click** a note: it's deleted.
5. **Ctrl+drag** a box around a few notes: they're selected (outlined). **Up / Down** moves them
   a semitone, **Shift+Up** an octave. **Ctrl+D** duplicates them after themselves. **Delete**
   removes them. **Ctrl+Z** undoes each step.
6. Press **Space**: the song plays, with the playhead moving in the piano roll too.
7. Turn **Sound: off** in the toolbar: adding and moving notes is silent. Clicking the keys on
   the left still plays them.

## 4. Velocity, quantize, humanize

1. In the **Velocity** lane at the bottom, drag a stem down: that note gets quieter (and its
   colour darker). Play it back to hear it.
2. Click a few notes slightly off the grid with **Alt** held, then press **Q**: they snap onto the
   grid. Try **Quantize** in the toolbar with **Strength 50%**: they move only half way.
3. Set **Snap: 1/8** and **Length: 1/8**, write eight notes in a row (one per grid step), select
   them all (Ctrl+A), then **Quantize** with **Swing 60%**: every second note moves later and it
   sounds like a shuffle. (Swing works on the snap grid's steps.)
4. Select some notes and click **Humanize** > Humanize: they move slightly, at random, and their
   velocities vary a little.

## 5. Scales

1. Choose **Key: A** and **Minor** in the toolbar. **You should see** the rows of notes outside A
   minor shaded darker, and the A rows tinted. You can still put notes anywhere.
2. Save, close, reopen the project: the key and scale are remembered.

## 6. With your MIDI keyboard (when the cable arrives)

While the piano roll is open, playing your keyboard plays the clip's instrument.

## Not there yet (so don't worry about these)

- Editing several clips at once, or a drum grid (M11, step sequencer).
- The mod wheel or other controllers in the piano roll (M14, automation).
- Snapping notes to the scale (shading only for now; tell me if you'd like snapping).

## Reporting back

How did writing notes feel? Anything you expected to work differently? Screenshots of piano
rolls you like (from other programs) are very welcome: they help with the look of the next
milestones.
