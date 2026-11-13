# Changelog

All notable changes to Aether are listed here, newest first.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and version numbers
follow [Semantic Versioning](https://semver.org/) (0.x while Aether is in early development).

## [Unreleased]

## [0.17.0] - 2026-11-13 - Milestone 16: sound effects and cinematic sounds

### Added
- **Sound effects for your arrangements**: right-click an empty part of a track's lane (on the
  timeline) and choose **Add a sound effect here**:
  - **Build-ups**, made to the song's tempo and lasting 1, 2, 4 or 8 bars, which **end on the bar
    line** nearest where you clicked, so they rise right into it: **Riser (noise)** (rushing noise
    opening up, the classic build before a drop or chorus), **Riser (synth)** (a synth chord rising
    an octave) and **Reverse swell** (a cymbal backwards).
  - Starting where you clicked: a **Downlifter** (noise falling away from the bar, 1 to 8 bars),
    an **Impact** (a big hit with a deep boom), a **Boom** (a sub drop) and a **Whoosh**.
  - They go on the track you clicked if it's an audio track, otherwise on a new "Sound effects"
    track below it, as ordinary audio clips: move, trim, fade, reverse or turn them down like any
    other. Each is made once and kept in the song's "Audio/Sound effects" folder. One Ctrl+Z takes
    it away again.
- **A Cinematic drum kit** (choose it in the drum editor, or add a drum track and pick it): taiko
  drums (low and high), big toms, a big snare, a stomp, an impact, a boom, a metal hit, a gong, a
  ticker, a reverse cymbal, a whoosh, hi-hats and a crash, with big rooms behind the drums. The 13
  new sounds are also in every pad's sound menu, under **Cinematic**.
- All of them are made by Aether's own code, so they're free to use in anything.

## [0.16.2] - 2026-11-13

### Added
- **Rename a song**: **File > Rename song...**, or right-click it on the start screen and choose
  **Rename...**. Its folder and file are renamed too (with its recordings, backups and autosave),
  so everything stays together; unsaved changes stay unsaved.
- **Reverse audio clips**: right-click an audio clip (or several selected) and choose **Reverse
  (play backwards)**, for reverse cymbals, swells and backwards vocals. The clip plays a reversed
  copy of its recording, made once in the song's Audio folder; your recording itself is never
  changed. **Play forwards again** (or Ctrl+Z) puts it back.
- **Fades on MIDI clips**, like on audio clips: drag the small dot at a MIDI clip's top left or top
  right corner. The instrument's sound fades (also notes already playing, and the end of a held
  chord), and the fade's length follows the bars if the tempo changes. After a fade-out, the notes'
  release and echoes stay silent too (until the next clip on the track, at most 4 seconds; playing
  live is always heard). The fade shapes (smooth, straight, fast, slow) are in the clip's menu.

### Changed
- The roadmap: **sound effects and cinematic sounds** (risers, impacts, booms, cinematic drums)
  come next, as Milestone 16; stretching and audio timing move to M17, and the later milestones
  one number on.

## [0.16.1] - 2026-11-13

From your M15 notes.

### Fixed
- **The horns and other recorded instruments distorted, more and more on high notes and
  chords.** The original libraries' own settings make many of their recordings much louder than
  they are (the trumpet up to 18 dB, the trombone up to 25 dB), so a single loud trumpet note
  already went over the maximum the sound card can play, and a chord went 5 to 8 dB over: the
  crunch you heard. On some high notes the *soft* recordings were even louder than the loud
  ones. Every recorded instrument is now set to the same safe level (its loudest note peaks at
  -8 dB, leaving room for chords), softer playing is always quieter than harder playing, and no
  single note jumps out above the rest. Measured on the trumpet: a chord that peaked at +5.4 dB
  now peaks at about -5 dB. Some instruments are now quieter than before (the trumpet and flute about
  10 dB): turn their tracks up if you need to.
- **Notes far above a recording got a harsh, metallic edge.** Played more than 3 semitones above
  the recording, the sampler now filters out what would fold back into tones that aren't in the
  sound. This helps your own recordings and 808s played high too. The highest notes of the
  Violins, the Brass section and the other sections were stretched up to 30 semitones past their
  last recording; now at most 7 (beyond that they're silent, as on the real instruments).
- **The Lo-fi kit sounded crunchy and clashing.** Its "lo-fi" treatment lowered the sample rate
  without filtering first, which folded the high sounds back as tones that clash, and cut the
  sound to 8 bits, which made every hit's tail crackle; and most of its pads (rim, clap, toms,
  congas, cymbals...) were the bright, clean sounds. It's now warm and soft (the top rolled off,
  with gentle tape saturation), and every pad has a lo-fi sound: 12 new ones (rim, clap, brush
  snare, three toms, shaker, two congas, crash, tambourine, ride). A kit you already chose keeps
  its pads: choose **Lo-fi** again in the drum editor to get the new ones.

### Added
- **Legato**, a new choice in the sampler's **Voices**: when a note starts while another is still
  sounding, it takes over smoothly (the old note fades out as the new one comes in, over 40 ms),
  instead of the two playing together and getting louder. Notes played together still make a
  chord. The solo instruments (trumpet, French horn, trombone, tuba, flute, oboe, clarinet,
  bassoon, tenor saxophone, solo violin) start in Legato when you choose them; a track that
  already has one keeps Poly until you change **Voices** (or choose the instrument again).

## [0.16.0] - 2026-11-12 - Milestone 15: timing for MIDI

### Added
- **Fix timing** for MIDI you played without the metronome: right-click a MIDI clip and choose
  **Fix timing (find the tempo you played)...** (or select several clips, or use the track's
  menu or **Edit > Fix timing**). Aether finds the beat of your playing, even if you sped up or
  slowed down, and shows it in a new window: your notes as you played them, with a line on every
  beat it found and numbered lines where bars start.
  - If the lines are wrong: **Half as fast** / **Twice as fast** (when you counted the other
    way), or type the tempo you meant and click **Look again**. The **<** and **>** buttons move
    where bar 1 starts, for a pick-up (notes before the first full bar).
  - **Make it steady** (the default): your notes move onto a steady beat, so they line up with
    the bars, the grid, drums and loops. Inside each beat they keep your feel. Choose the tempo
    you played (the song's tempo becomes it) or keep the song's tempo. **Strength** below 100%
    keeps some of your push and pull from beat to beat.
  - **Make the song's tempo follow your playing** (like Logic's Smart Tempo): your notes stay
    exactly as you played them, and the song gets tempo changes, every bar (smoother) or every
    beat (closer), so its bars, the metronome and other tracks follow you.
  - One Ctrl+Z puts everything back. Clips chosen together (e.g. a piano part and a bass part
    played over it) are fixed together.
  - Optionally the window opens by itself when you stop recording MIDI that you played without
    the metronome: tick **Offer this after recording without the metronome** in it (off at
    first).
- **Quantize from the timeline**: **Q** (or **Edit > Quantize...**, or a MIDI clip's or track's
  menu) quantizes the selected MIDI clips, or every clip on the selected track, without opening
  the piano roll: choose the grid (bar, beat, 1/8, 1/16, 1/32, triplets), strength, swing, and
  whether note ends move too. Notes go onto the song's grid even when a clip doesn't start on a
  grid line. Your choices are remembered.

## [0.15.2] - 2026-11-10

### Changed
- **A new logo and app icon** (your design): the "A" peak with a sound wave inside. It's on the
  start screen, the window and taskbar, the installer and the README. At the smallest sizes
  (title bar, taskbar) a simpler version with three thicker bars is used, so it stays sharp.

## [0.15.1] - 2026-11-09

From your M14 testing.

### Changed
- A lane's **x** now **removes** the lane and its points (Ctrl+Z brings them back), so you can
  start a take again without the old moves fighting you. Hiding a lane (its points keep
  working) is in the lane's **▾** menu, which also has **Clear all points** to draw or record it
  again.
- Recording automation while holding a control still now stretches one point instead of adding
  one every moment: a flat stretch is just two points.

### Added
- **Copy these points** / **Paste copied points onto the selected tracks** in a lane's **▾**
  menu: the same automation on several tracks at once (an effect's setting goes to the same kind
  of effect on each track). For one change to many tracks, you can also automate a bus they all
  play into.

## [0.15.0] - 2026-11-09 - Milestone 14: automation

### Added
- **Automation**: settings that change by themselves as the song plays, e.g. the volume fading out
  at the end, more reverb in the chorus, a filter opening during a build-up, a sound moving from
  left to right, an effect switching on for one section.
  - Click a track's new **A** button and choose a setting: its **lane** opens under the track,
    lined up with the clips. Several lanes per track; **Hide** keeps their points working.
  - Everything can be automated: **volume, pan, mute, sends**, every **effect** setting and
    switching effects **on/off**, and the **instrument's** settings (Synth, sampler, drum kit).
  - **Draw**: click in a lane to add a point (on the grid; hold Alt for anywhere), drag points to
    move them, **double-click** a point to remove it, **Ctrl+drag** a line to bend it into a curve.
    Right-click for more (clear the lane, remove it). Every change can be undone.
  - **Record your moves**: switch on **Write automation** (the new button after the metronome, or
    the Transport menu), play, and move any fader, knob or switch, in the track headers, the
    mixer, an effect's window or an instrument's window. Your moves are written into the lane
    (which opens by itself). Holding a control still keeps writing its value; let go, and the lane
    carries on as it was. One Ctrl+Z takes back a move.
  - Faders and knobs **follow** their automation as the song plays, so you can see it working.
  - Automation is saved with the project and included when you export.
- Effects can be removed together: right-click an effect that came with a ready-made chain and
  choose **Remove the whole chain**, or **Remove all effects** on a channel (both one undo step).

### Changed
- **Switching an effect on or off while playing fades** over 20 ms, so it no longer crackles
  (from your M13 testing). A switched-off effect keeps listening, so its echoes and reverb are
  ready when it comes back on. Adding an effect while playing fades it in too.
- **Changing the Delay's time or the Reverb's size or pre-delay while playing crossfades** from
  the old setting to the new one, instead of gliding, which bent the pitch (the short "laser"
  sound when switching presets, from your M13 testing).

## [0.14.0] - 2026-11-08 - Milestone 13: effects

### Added
- **Ten effects**, on any track, bus or the master (mixer > **+ Effect**):
  - **EQ**: a low cut, bass, three bands in the middle (low mids, mids, high mids), treble and a
    high cut: less mud, more clarity, more or less bass and treble.
  - **Compressor**: turns the loud parts down so a voice or instrument sits evenly in the mix,
    then brings it all back up (threshold, ratio, attack, release, knee, makeup, mix).
  - **Limiter**: a ceiling nothing goes above, for the end of the master: makes the whole song
    louder without clipping.
  - **Gate**: silences room noise and breaths between phrases.
  - **De-esser**: tames harsh "s" and "t" sounds in a voice.
  - **Saturation**: warmth and grit, from a gentle tape glow (Tape, Tube) to Fuzz and Hard clip.
  - **Filter**: darker (low-pass), thinner (high-pass), band-pass or notch, and it can sweep by
    itself for movement.
  - **Chorus**: a wavering copy mixed in, for wider, fuller pads, pianos and backing vocals.
  - **Delay** and **Reverb** (both rebuilt: see Changed).
- **Each effect opens in its own window** (click it in the mixer), with a **picture** of what it
  does, drawn from the same maths as the sound:
  - **EQ**: the curve, with a coloured point for each band. Drag a point left/right for the
    frequency and up/down to turn it up or down; scroll on it to make it narrower or wider;
    double-click to put it back to 0 dB.
  - **Compressor**: level in against level out (the bend is where it starts turning down), and
    **how much it's turning down as the song plays**, with the last five seconds as a moving line.
  - **Limiter** and **De-esser**: how much they're turning down, live. **Gate**: whether it's open.
  - **Reverb**: the sound, then the room's tail dying away (and how long it takes). **Delay**: the
    echoes on the beat grid, left and right for ping-pong. **Chorus**, **Saturation** and
    **Filter** (which moves when it sweeps) have their own pictures too.
  - Settings as knobs in labelled groups, an **On** switch to compare with and without, and the
    preset menu with **<** and **>**. Several effect windows can be open at once.
- **64 presets**, for **vocals**, **instruments** (piano, strings, pads, acoustic) and **the
  whole song** (on the master), plus a starting point for each effect.
- **16 ready-made chains**: several effects at once, set up for a common job, from **+ Effect >
  Ready-made chains**: for vocals (clear and up front, warm and smooth, rap / spoken, dreamy,
  recorded in a noisy room, telephone / radio), instruments (bright piano, warm piano, lush
  strings, wide synth pad, acoustic in a natural room) and the whole song (polished and louder,
  gentle, bright and punchy, warm, as loud as possible). One Ctrl+Z takes a whole chain off again.
- Up to **eight effects** per channel (was four). The mixer's strips make room when a channel has
  more than three.

### Changed
- **The Reverb is new**: a richer, smoother room (eight delay lines feeding each other, with
  diffusion and gentle movement), with two new settings: **Pre-delay** (a short gap before the
  room answers, which keeps a voice clear) and **Low cut** (keeps the reverb from getting muddy).
  Projects with the old Reverb keep their settings (size, damping, width, mix) but sound a little
  different: fuller and more natural.
- **The Delay** has a **Low cut** for its echoes, and gets a picture too.
- A new effect starts on its first preset ("... (start here)").
- **Soft to loud is gradual now** (from your M12 testing): the recorded instruments blend between
  their soft, medium and loud recordings as you play harder, instead of jumping between them. With
  your Kawai you'll hear every step in between.
- The on-screen keyboards reach full strength at the bottom of a key (they stopped at 80%).

### Fixed
- The Delay could read just past the end of its memory now and then (since 0.5.0), which could
  make a tiny click. The new Reverb and the Chorus are checked for the same thing by a test.

## [0.13.1] - 2026-11-08

From your M12 testing.

### Fixed
- **Held notes crackled and stopped after a moment** on strings, woodwinds, brass and organ (any
  instrument that holds a note): the sampler ended the note when it reached the end of its
  recording, a moment before it should have jumped back to repeat the loop. Held notes now last as
  long as you hold the key. A new test plays held chords on the real instruments for 8 seconds.

### Changed
- **Play it as notes...** (drum editor) now **moves** the pad onto its own track: the hits leave the
  drum pattern and become notes on the new track (in a pattern that repeats the same way, each note
  lasting until the next), and the **piano roll opens** on them at the 808's octave. Drag notes up
  and down to make the bass line; make a note overlap the next one to slide. One Ctrl+Z puts
  everything back.
- The on-screen keyboards now say what they could already do: click near the top of a key to play
  softly, near the bottom to play hard (so you can hear the instruments' soft and loud recordings
  without a MIDI keyboard).

## [0.13.0] - 2026-11-08 - Milestone 12: the sampler and recorded instruments

### Added
- **28 recorded instruments**, included in the installer (about 57 MB): pianos (grand, upright,
  harpsichord), strings (string ensemble, violins, cellos, pizzicato, tremolo, short notes, solo
  violin), woodwinds (flute, oboe, clarinet, bassoon, tenor saxophone), brass (brass section,
  trumpet, French horn, trombone, tuba), and mallets, harp and organ (marimba, vibraphone,
  glockenspiel, xylophone, tubular bells, kalimba, harp, pipe organ). They're real recordings by
  Versilian Studios, given away into the public domain (CC0), so they're free to use in anything.
  Strings, winds, brass and organ hold for as long as you hold the key.
- **+ MIDI** now offers a menu: the Synth, drums, or any recorded instrument (by kind), or your own
  sound. A track's menu (Instrument > Recorded instruments) switches it to one.
- **The sampler**, in its own window (click the track's **MIDI** badge): the instrument (with **<**
  and **>** to step through them), a picture of which keys each recording covers, its envelope
  (attack, decay, sustain, release), tone, touch (how much harder means louder), tuning, voices,
  glide, volume and pan, and a keyboard to play while you change them.
- **808 bass lines**: in the drum editor, a pad's **Play it as notes...** makes a new track that
  plays that sound across the keyboard, tuned to its own pitch, in **Slide** mode: notes that
  overlap glide from one pitch to the next, without starting again, like trap 808s. Separate notes
  play normally. Any sampler can use Poly, Mono or Slide. The sampler's sound menu also offers all
  of Aether's drum sounds as notes.
- **Your own sounds**: drop audio files on the sampler window, or use **Your own sound...**:
  - one recording plays across the keyboard; Aether works out which note it is (from its name,
    like "Bass C2.wav", or by listening to it) and you can adjust it;
  - several recordings, or a folder, named by their notes ("Piano C4 mf.wav", "Piano E4 f.wav"...)
    become one instrument: each covers the keys around its note, louder recordings answer harder
    playing, and takes marked rr1, rr2... alternate;
  - **SFZ instruments** (the common free format for sampled instruments) load directly.
  Your recordings are copied into the project's Audio folder, so the project keeps working if you
  move them.
- Recordings no track uses any more are let go after 20 seconds, so trying instruments one after
  another doesn't fill up your PC's memory.
- The installer's recorded instruments are built from the original recordings by
  `tools/sounds/build_pack.py` (GitHub builds and caches them), so anyone can rebuild them exactly.
- Tests for the sampler (pitch, layers, takes, envelope, loops, Slide and Mono, tone, real-time
  safety), SFZ files, laying out recordings by name, pitch detection, your own sounds, the window,
  and a check of every recorded instrument (345 tests).

## [0.12.1] - 2026-11-08

From your M11 testing.

### Fixed
- **808s got louder and louder until they peaked** (in the export too): every hit kept ringing
  under the next one, and long sounds like the 808 piled up, to 2.5 times the level of a single
  hit. Now a pad hit again cuts off its previous hit (as a real drum would), so a fast 808 pattern
  is never louder than one hit.

### Changed
- **Space in the drum editor plays just the pattern you're making**, over and over, until you
  press Space again (or close the editor); the playhead then goes back to where it was. Space in
  the main window still plays the whole song. New **Play** and **With song** buttons at the top
  of the drum editor: with **With song** on, you hear the rest of the song along with the
  pattern.
- **Shift+click an accented step again** to make it a normal hit (it used to stay accented).
  Right-click still removes a hit.

## [0.12.0] - 2026-11-07 - Milestone 11: drums

### Added
- **Drum tracks:** **+ Drums** in the toolbar (or Ctrl+Shift+D) adds a track with a **16-pad
  drum kit**. Any MIDI track can switch to it too (track menu > Instrument > Use the Drum kit).
  The pads follow the General MIDI drum notes (C2 = kick, D2 = snare, F#2 = closed hi-hat...), so
  a keyboard, MIDI files and the piano roll all play the right drums. With a drum track selected,
  the computer keys play the pads (A = kick, S = snare, T Y U = hi-hats).
- **37 drum sounds made by Aether** (kicks, snares, claps, rims, hi-hats, toms, cymbals, cowbell,
  shaker, tambourine, congas, clave, finger snap) and **5 kits**: Tight kit, 808, Electronic,
  Lo-fi and Deep. They're generated by Aether's own code, so there's nothing to download and
  they're free to share.
- **Your own sounds:** drop an audio file on a pad (or Choose sound... > From an audio file). It's
  copied into the project's Audio folder, like imported audio.
- **The drum editor** (double-click a drum pattern, or click a drum track's **DRUMS** badge), in
  its own window:
  - a **step sequencer**: click steps on and off (drag for several), Shift+click for an accent,
    right-click to clear; 1/8, 1/16 or 1/32 steps; patterns of 1, 2, 4 or 8 bars (a longer
    pattern starts as copies of the shorter one); **swing**; the playing step lights up;
  - a **velocity lane** under the steps: drag the bars to change how hard each hit is;
  - each pad's **sound, tune, volume, pan, attack, length** and **choke group** (a closed hi-hat
    cuts off an open one), and the kit's volume; click a pad to hear it.
- **Repeating patterns:** double-clicking an empty part of a drum track makes a one-bar pattern;
  drag its right edge and it repeats (dotted lines show where). Any MIDI clip can do this (clip
  menu > **Repeat pattern**). Turning it off writes the repeats out as ordinary notes, so nothing
  you hear changes. The piano roll shows the repeats faintly after the pattern's end line.
- Exports include pad sounds from your own files.
- Tests for the drum sounds, the kit's sound (levels, pan, tune, length, choke groups, real-time
  safety), patterns (steps, swing, step size, length, repeats, undo) and the drum editor (318
  tests).

### Fixed (from your 0.11.0 testing)
- **You couldn't play notes while adjusting the Synth:** clicking the keyboard in the main window
  brought it in front of the Synth's window. The Synth window now has its own keyboard at the
  bottom (computer keys work there too), so you can change a setting and play straight away.

- A clip's menu showed its file as "Audio/Guitar.wav" on Windows instead of "Guitar.wav".

### Changed
- The toolbar buttons are a little narrower, so the transport still fits at 1280 pixels wide with
  the new **+ Drums** button.

## [0.11.0] - 2026-11-07 - Milestone 10: the Synth

### Added
- **The Synth**, a full synthesizer for MIDI tracks, in its own window (click a MIDI track's
  **MIDI** badge, or right-click > Instrument settings...). New MIDI tracks use it.
  - **Simple page:** the settings that matter most, with live pictures: the waveform (both
    oscillators and their mix), the filter's curve, and the envelope's shape.
  - **Advanced page:** every setting, with plain-language tooltips: two oscillators (sine,
    triangle, saw, square with pulse width; octave, semitones, fine tuning), sub oscillator and
    noise, **unison** (up to 7 detuned copies spread across the stereo field, for big pads and
    strings), filter (low-pass 24 or 12, high-pass, band-pass) with resonance, drive, key tracking,
    velocity and its own envelope, the volume envelope, **two LFOs** (vibrato, filter sweeps,
    tremolo, auto-pan, pulse-width and mix movement, with fade-in), the **mod wheel**, glide,
    Poly / Mono / Legato, pitch-bend range, and built-in **chorus** and **reverb**.
  - **23 presets**, by kind: Pads (Warm, Glass, Dark, Choir, Evolving), Strings (String
    ensemble, Slow strings, String machine, Pizzicato), Keys (Soft, Warm, Bell, Vintage), Organ
    (Drawbar, Church, Rock, Soft), Bass, Leads, Plucks and a basic starting point. Previous/next
    buttons; "(changed)" shows when you've adjusted a preset.
- The track menu's **Instrument** submenu can switch a track between the Synth and the starter
  synth. Projects made before 0.11 keep the starter synth on their tracks (so they sound the
  same) until you switch.
- The mod wheel (MIDI controller 1) reaches instruments.
- Tests for the Synth's sound (tuning, filter, envelopes, stereo width, mono/legato and glide,
  mod wheel, clip notes, voice limits, every preset, real-time safety) and its window (294 tests).

### Fixed (from your 0.10.1 testing)
- After Exclusive Mode, switching back to Windows Audio kept the microphone in mono (one input,
  too loud) until it was chosen again. Every switch now asks for it in stereo.
- Dragging a chord's stem in the velocity lane changed only one note (the one selected after
  clicking it in). It now changes the whole chord; **Alt+drag** a dot to change just that note.

### Changed
- Settings that only make sense as whole numbers (octaves, semitones, unison, bend range) snap.
- The 50-track performance test now uses the Synth: about 25% of the time available per buffer
  (limit 50%).

## [0.10.1] - 2026-11-07

From your M9 testing.

### Fixed
- **Deleting many selected notes** left some behind: the selection changed while the notes were
  being deleted. All of them go now, in one undo step.
- **Exclusive Mode with your Fifine microphone:** Aether asked for the microphone in stereo, which
  many USB microphones refuse in Exclusive Mode (AUDCLNT_E_UNSUPPORTED_FORMAT). It now tries the
  microphone in mono, then at other sample rates, and only switches it off if nothing works.
- **The piano roll's keyboard** looked wrong: it now looks like a real piano (the seven white keys
  of each octave share its height, the black keys sit between them, with no white strip beside
  them), and clicking plays the key that's really under the mouse.

### Changed
- **Scale highlighting:** instead of darkening the notes outside the scale, the notes in it get a
  mark beside their key (a wider one for the key note) and a tinted row.
- **Velocity of chords:** dragging a chord's stem changes all its notes, keeping their
  differences. To change one note of a chord, select just that note first. The value is shown
  while you drag.
- **Dragging a note past the edge** of the piano roll scrolls to follow it.
- **Right-click** on a selected note deletes all the selected notes; **right-drag** deletes every
  note you pass over.
- **Key and note names as musicians write them:** Db, Eb, F#/Gb, Ab and Bb major (C#, G#, D#/Eb
  minor...), and notes spelled for the key (Bb in F major or D minor, A# in B major).
- The piano roll's keyboard is a little wider.

## [0.10.0] - 2026-11-06 - Milestone 9: piano roll

The first milestone of Phase 2 (making music).

### Added
- **The piano roll**, in its own window: **double-click a MIDI clip** to edit its notes, or
  double-click an empty part of a MIDI track to make a one-bar clip and open it (also in the
  clip's and the lane's right-click menus).
  - **Click an empty place to add a note** (keep the button down and drag to make it longer).
    New notes are as long as the last note you drew, resized or clicked, or a fixed length you
    choose. **Right-click a note to delete it.**
  - Drag notes to move them (in time and pitch), drag a note's right edge to change its length;
    hold Alt to place freely. Ctrl+click or Ctrl+drag a box to select several.
  - **You hear notes** as you add, move or change them (the **Sound** button turns this off).
    Click the keys on the left to hear any note.
  - **Velocity lane** at the bottom: drag a note's stem up or down (selected notes change
    together); drag across stems to set several.
  - **Quantize** (Q, or the button): moves notes onto the grid, with **strength** (how far: less
    than 100% keeps some of the feel), **swing** (pushes every second step later, up to a triplet
    shuffle) and optionally the note ends too.
  - **Humanize**: small random changes to timing and velocity, so notes sound less mechanical.
  - **Scale highlighting**: choose the song's key and a scale (major, minor, harmonic minor,
    dorian, mixolydian, pentatonic, blues): notes outside it are shaded, the key note tinted.
    It only shows them; every note can still be used. Saved with the project.
  - Keys: Up / Down (a semitone; Shift: an octave), Left / Right (a grid step), Ctrl+D (duplicate),
    Ctrl+C / X / V (copy, cut, paste at the playhead), Ctrl+A, Esc, Delete, Q. Space, undo and
    Aether's other shortcuts work in the piano roll window too.
  - Notes past the clip's end make the clip longer (to the end of that bar). Every edit is
    undoable, a whole drag in one step.
- A **1/32** grid (in the timeline too).
- Help > Keyboard shortcuts lists the piano roll's mouse actions and keys.
- Automated tests for every note edit (adding, moving, resizing, velocity, quantize with
  strength and swing, humanize, duplicating, pasting, scales), the piano roll driven by a
  simulated mouse and keyboard, note preview (only the track being edited hears it, and it's
  never recorded), and the timeline's double-click (270 tests in all).

### Changed
- Double-clicking a MIDI clip opens the piano roll (renaming it is in its right-click menu).

## [0.9.1] - 2026-11-06

From your M8 testing.

### Fixed
- **Exclusive Mode** didn't open ("the device can't run at this sample rate in this mode",
  AUDCLNT_E_UNSUPPORTED_FORMAT). When switching driver, JUCE always added your default
  microphone, even with "None" chosen, and one of the two devices refused. Aether now picks the
  devices itself (the same speakers; a microphone only if you had one), and if that doesn't open
  it tries without the microphone, then at other sample rates, and tells you what it changed
  (e.g. "your microphone can't be used with Exclusive Mode, so nothing can be recorded").
- **Tooltips** covered the bottom half of the button you were pointing at, and once one was
  showing, moving the mouse showed every other tooltip instantly. They now appear just below the
  control, and only after the mouse has rested on it for a moment, every time.
- **The on-screen keyboard** stretched its keys in a wide (full-screen) window. The keys now
  keep their size and more of them show; once all 88 fit, the keyboard is centred.

### Added
- **Delete project** (start screen: right-click a project). It moves the project's folder to the
  Recycle Bin, after asking. If the folder holds anything other than the project (say, your own
  lyrics file), only the project file is moved, so nothing else is ever deleted.
- **Effects on several tracks at once:** select tracks (Ctrl+click their names, in the mixer or
  the track list), then **+ Effect** on one of them adds it to all, in one undo step. Clicking an
  empty part of a mixer strip now selects it.

### Changed
- **New interface font: Inter**, built into Aether (it was Windows' Verdana). Clean and easy to
  read at small sizes. Licence: SIL Open Font License (see CREDITS.md).
- Audio settings: "Outputs 1 + 2" and "Input 1" instead of the drivers' longer names.

## [0.9.0] - 2026-11-06 - Milestone 8: Phase 1 hardening

Phase 1 (the core of a DAW) is complete with this version.

### Added
- **Aether's own Audio settings panel**, replacing the technical standard one: driver (with a
  plain explanation of each), speakers/headphones, microphone (or None), sample rate, and buffer
  size shown with its delay ("256 samples (5.3 ms)"), with a note when the driver doesn't let you
  change it. Tick which inputs to record from; choose the output pair on interfaces with more
  than two outputs; open an ASIO driver's own settings. MIDI keyboards are listed with tick boxes
  and a light that flashes when MIDI arrives.
- **Copy, cut and paste clips** (Ctrl+C, Ctrl+X, Ctrl+V): pasted at the playhead, onto the
  selected track (clips copied from several tracks keep their layout). One undo step each.
- **Box selection**: drag on an empty part of the timeline to select the clips inside the box
  (hold Ctrl to add to the selection). **Esc** deselects.
- **Drag and drop everywhere**: audio and MIDI files dropped on a track header go onto that track
  at the playhead; dropped anywhere else (below the tracks, the mixer...) they become new tracks.
  On the start screen they start a new project named after the first file. A `.aether` project
  file dropped anywhere opens (asking about unsaved changes first). A hint shows what a drop will
  do.
- **Help > Keyboard shortcuts** (F1): every shortcut, plus mouse actions, with a search box. It
  reads the shortcuts from Aether's command list, so it's always up to date.
- Performance tests with a 50-track project (36 audio tracks with 8 clips each, 10 MIDI tracks
  playing chords, 4 buses with reverb and delay). In an optimised build: the audio engine uses
  about 17% of the time available per buffer; updating the engine after an edit takes about
  0.6 ms; drawing the timeline, track list or mixer takes 1 to 4 ms (a 60 Hz screen allows 16).
  The time limits are checked automatically in the Windows (Release) build.
- A `linux-release` build preset (for developers measuring performance).
- Automated tests for the settings panel (with a simulated sound card), copy/paste, box
  selection, dropping files, the shortcut list, finishing a recording on close, and the fast
  sine (243 tests in all).

### Changed
- The starter synth is about 25% faster (a fast sine for its electric piano and organ sounds,
  and its filter settings worked out only when they change). It sounds the same.
- Tooltips added where a sweep of every window found none (the Export window's choices and
  buttons, the status bar, the mixer's resize bar). The start screen has a tip about dropping
  files.

### Fixed
- Closing the project, opening another one or quitting **while recording** stopped the
  recording and kept the file, but the take never appeared in the project. Aether now finishes
  the recording first, so the new clips are in the project and the "save changes?" question
  includes them.

## [0.8.1] - 2026-11-06

### Fixed
- While the Export window was open (also during an export), the main window couldn't be used:
  clicking it only made the Windows error sound. The Export window is now a separate window that
  doesn't block anything, so you can keep working while it exports, as intended.
- The Export window's own tooltips now show.

## [0.8.0] - 2026-11-06 - Milestone 7: export

### Added
- **Export audio** (File > Export audio, Ctrl+E): the mix as one file, and/or **stems** (one
  file per track, each as you'd hear it soloed, with its effects, sends and the master; played
  together they add up to the mix).
- Formats: **WAV** (16-bit, 24-bit, 32-bit floating point), **FLAC** (16 or 24-bit: smaller than
  WAV, same quality) and **MP3** (128 to 320 kbps). Sample rates 44.1, 48, 88.2 and 96 kHz (MP3:
  44.1 or 48 kHz).
- The whole song (to the end of the last clip) or just the loop region; effect tails (reverb,
  echoes, released notes) ring out at the end until silent; optional normalizing of the mix to
  -1 dB; 16-bit files are dithered.
- Exporting runs in the background with a progress bar and Cancel, from a copy of the project,
  so you can keep working. Files go in the project's Exports folder (or one you choose) and are
  never overwritten. A warning appears if the mix clipped (went over 0 dB).
- MP3 encoding uses LAME 3.100 (LGPL), built from its official source and shipped as a
  separate library next to Aether.exe (libmp3lame.dll).
- MIDI import: when the project already has music in it, Aether asks whether to use the MIDI
  file's tempo (from your M6 feedback). A MIDI file with one part, dropped on a MIDI track, goes
  onto that track.
- Automated tests: exports match the engine sample for sample; tails; other sample rates; WAV
  and FLAC at every bit depth read back to their precision; MP3 encodes and decodes at the same
  loudness; stems add up to the mix (with a reverb send); clipping warnings and normalizing;
  MIDI tracks; cancelling (226 tests).

### Changed
- The roadmap's time estimate: roughly 3 to 6 weeks to version 1.0 at the current pace.

## [0.7.0] - 2026-11-06 - Milestone 6: MIDI tracks

### Added
- MIDI tracks make sound: every MIDI track has the new **starter synth**, with presets
  (Electric piano, Soft pad, Bass, Lead, Organ, Pluck) and settings (tone, detune, sub, filter,
  envelope, touch sensitivity, volume). Click a MIDI track's **MIDI** badge (or right-click >
  Instrument) to choose a preset or change the sound. A fuller synth comes in M10.
- MIDI keyboards: every MIDI input (e.g. your Kawai over USB) is switched on automatically,
  including ones plugged in while Aether is running. They can be switched off in Audio settings
  (Aether remembers). A **MIDI** light in the toolbar flashes when notes arrive; its tooltip
  lists the keyboards.
- Playing: what you play goes to the selected MIDI track, and to armed (R) or "play through" (I)
  MIDI tracks. The sustain pedal and pitch bend work.
- The **on-screen keyboard** (View > On-screen keyboard, Ctrl+K): click its keys, or click it
  and play with the computer keyboard (A W S E D F T G Y H U J K; Z / X change octave).
- Recording MIDI: arm a MIDI track (R) and record as with audio: count-in, punch in/out,
  latency correction (by the output delay, i.e. what you heard while playing). With the loop
  on, every pass is added to the same clip. The sustain pedal is turned into note lengths.
- MIDI clips: shown with their notes, and moved, trimmed, split, duplicated, deleted and
  renamed like audio clips (only onto MIDI tracks). A piano roll to edit notes comes in M9.
- Importing MIDI files (.mid): drag them onto the timeline, or Track > Import (Ctrl+I). Each
  part becomes a MIDI track with a clip; a new project takes the file's tempo.
- Automated tests for MIDI clips and their edits, instruments and presets, the sustain pedal,
  MIDI file import, notes playing at exactly their sample, every preset, live playing, notes not
  hanging after stop or loop jumps, voice stealing, and recording MIDI (216 tests), plus a
  real-time safety test of MIDI playback, the synth and live/recorded MIDI.

### Fixed
- **Tooltips now appear** in the main window. They were written for every control but never
  shown, because the main window was missing the part that displays them (only the Audio
  settings window had it). Hover over any button or control to see its name and what it does.

### Changed
- Arming (R) and "play through" (I) work on MIDI tracks too.
- The roadmap now includes pitch correction ("auto-tune") in M19, with pitch-shifting.

## [0.6.0] - 2026-11-06 - Milestone 5: recording

### Added
- Recording: arm audio tracks with their **R** button and press **Record** (R key, or the red
  button). Several tracks can record at once, each from its own input. Recording starts after
  the count-in if one is set, or straight away if you press Record during playback. Press Space
  (or R again) to stop.
- Recordings are saved as 24-bit WAV files in the project's Audio folder ("Vocals 1.wav",
  "Vocals 2.wav"...). The files are updated every second while recording, so a crash or power cut
  loses at most a second. Nothing recorded is ever deleted by undo.
- Automatic latency compensation: what you record is moved back by the round-trip delay, so it
  lines up with what you heard while playing.
- Measure latency (Settings > Audio settings): Aether plays eight short chirps and listens for
  them through your input to measure the real delay (drivers often report it wrongly). The result
  is remembered for that exact setting (device, sample rate, buffer size) and used for
  recordings.
- An armed track's level meter shows its input (e.g. your microphone), so you can check the level
  before recording.
- Input monitoring: the **I** button on a track lets you hear its input through Aether, with the
  track's effects (use headphones).
- Choosing a track's input: right-click the track > Input (a single input, or a stereo pair).
- Punch in/out (the button next to Loop, or P): only what's inside the loop region is kept.
- Loop recording: with the loop on, every pass round the loop is kept as a take in one file. The
  clip plays the last take; right-click it > Takes to choose another. Split a clip and choose a
  different take for each part to combine the best bits.
- A recording replaces what was on the track where it lands (one undo step brings it all back).
- While recording, a red block grows on each recording track.
- Go to start (the |◀ button, Home, or Stop when already stopped) now also scrolls the timeline
  back to the start.
- Automated tests for where recordings land (latency, punch, loop takes), replacing and takes,
  recording a known signal end-to-end into WAV files and clips, stopping during the count-in,
  monitoring, the latency measurement through a simulated cable (exact to the sample, also with
  noise), remembering measurements per setting (198 tests), and a real-time safety test of
  recording, monitoring and measuring.

### Changed
- The latency panel shows both the driver's round trip and what recordings are corrected by.
- Track headers: the volume slider is a little shorter on audio tracks, to make room for R and I.

## [0.5.0] - 2026-11-06 - Milestone 4: mixer

### Added
- The mixer (View > Mixer, F9, or the Mixer button): a channel strip for every track and bus,
  and the master. Each strip has volume (fader), pan, mute, solo, a stereo level meter, up to
  four effects, up to three sends, and its output (the master or a bus). Drag the bar above it
  to make it taller or shorter.
- Buses (Track > Add bus, Ctrl+Shift+B, or "+ Bus"): channels other tracks play into, to share
  effects or set several tracks' volume at once. Buses can play into other buses. Aether
  refuses connections that would make a loop.
- Sends: copy some of a track's sound to a bus (e.g. one with a reverb), after the fader or
  before it (right-click the send).
- Solo works "in place": you hear the soloed tracks through their buses, their sends, and the
  master; soloing a bus lets you hear what plays into it.
- Two simple, temporary effects: Reverb (size, damping, width, mix) and Delay (time in note
  lengths that follow the tempo, feedback, tone, ping-pong, mix). Click an effect to change its
  settings. The full set of effects comes in M13.
- Track headers now have mute (M), solo (S), volume, pan and a level meter.
- Select several tracks: Ctrl+click adds or removes one, Shift+click (or Shift+Up/Down) selects
  a range. Moving one selected track's fader moves them all by the same amount (a quieter track
  stays quieter); mute, solo, colour, output and delete also work on all selected tracks.
- Volume, pan and mute changes fade over 10 ms, so they never click.
- The clip's right-click menu shows the file's sample rate and channels, and whether Aether is
  converting it for your sound card.
- Automated tests for the mixer settings and undo, routing and solo, the sound coming out of
  the mixer (volume, pan, mute, buses, sends, master, meters, echo timing, reverb tail), group
  fader moves, selection and the mixer panel (177 tests), plus a real-time safety test for the
  whole mixer with effects while settings change.

### Changed
- Fade handles on clips are now small dots that appear only when the mouse is over the clip.
- The toolbar buttons are shorter ("+ Audio", "+ MIDI", "+ Bus"), with a Mixer button on the
  right. Zoom and snap moved to a new View menu.
- Deleting a bus sends the tracks that played into it to the master (undo brings it all back).

### Fixed
- The Delay's note lengths were set up the first time the audio thread used them, which could
  allocate memory on the audio thread. Found by the RealtimeSanitizer tests before release.

## [0.4.0] - 2026-11-06 - Milestone 3: arrangement and audio clips

### Added
- Audio clips: drag audio files (WAV, AIFF, FLAC, MP3, Ogg Vorbis) from File Explorer onto a
  track, or use Track > Import audio file (Ctrl+I) to add them at the playhead. Several files at
  once go on separate tracks, all starting at the same place (handy for stems). Files are copied
  into the project's Audio folder.
- Clips show their waveform, and play back through the transport, sample-accurately, including
  looping. Files at a different sample rate are converted automatically (with Aether's own
  delay-free converter, so clips stay exactly in place).
- Clip editing, all undoable: select (Ctrl+click for several, Ctrl+A for all), move (also onto
  another track), trim either edge, split at the playhead (S), duplicate (Ctrl+D), delete
  (Delete), fade in/out (drag the small squares at the top corners; four fade shapes), loop
  (the content repeats; stretch the clip to repeat it more), volume and rename (right-click).
- Snap grid: Bar, Beat, 1/8, 1/16, 1/8 triplet, 1/16 triplet or Off (menu above the track list;
  G switches snapping off and on; hold Alt while dragging for exact positions). Grid lines follow
  time signature changes.
- Nudge: Left/Right arrows move the selected clips one grid step, Shift+Left/Right by 10 ms (the
  playhead, if no clips are selected).
- Edit menu entries for split, duplicate, delete, select all and nudge.
- Automated tests for clip editing and undo, audio file loading, waveform peaks, sample-rate
  conversion, importing, clip playback (timing, offsets, looping, fades, volume, mixing, edits
  that must not change the sound), and the timeline's clip commands (145 tests), plus a
  real-time safety test for clip playback.

### Changed
- Clicking positions now snap to the chosen grid (was: always the beat).
- The Delete key deletes the selected clips if any, otherwise the selected track.

### Fixed
- The Milestone 2 test checklist told you to undo the tempo/loop changes before saving, so they
  (correctly) weren't there after reopening. The steps are now in the right order.
- Some harmless Windows compiler warnings in the timeline.

## [0.3.0] - 2026-11-06 - Milestone 2: time and transport

### Added
- Transport bar: go to start, stop, play/pause, loop, metronome, count-in (off, 1 or 2 bars),
  the playhead position (bar.beat.sixteenth and minutes:seconds), the tempo and the time
  signature at the playhead.
- Keyboard shortcuts: Space (play/pause), Home (go to start), L (loop on/off), K (metronome on/off),
  Ctrl+= / Ctrl+- (zoom). Stop goes back to the start when pressed while stopped.
- Timeline with a ruler (bar numbers), a time signature lane, a tempo lane and one lane per
  track, lined up with the track headers. Click the ruler or a lane to move the playhead;
  Ctrl + mouse wheel zooms, Shift + mouse wheel scrolls sideways; the view follows playback.
- Tempo changes: double-click the tempo lane to add one, drag points to change tempo and
  position, right-click to make a change gradual ("ramp") or delete it. The tempo field can be
  dragged up/down or double-clicked to type.
- Time signature changes: click a signature to change it, right-click the lane to add one at a
  bar. Common signatures (2/4, 3/4, 4/4, 5/4, 6/8, 7/8, 9/8, 12/8) are offered.
- Loop region: drag in the top strip of the ruler to set it (this switches looping on), drag
  its middle to move it or its edges to resize it, double-click or press L to switch it on/off.
- Metronome: clicks generated by Aether (no sound files), a higher click on the first beat of
  each bar, volume Quiet/Normal/Loud (right-click the metronome button). On by default for now.
- Count-in: one or two bars of clicks before playback starts, at the tempo where you start.
- Tempo, time signatures and the loop are saved in the project and every change is undoable
  (a whole drag is one undo step). Older projects open with 120 BPM, 4/4.
- Audio engine: sample-accurate transport and metronome; clicks land on exactly the right
  sample whatever the block size, tempo changes, ramps or looping. Tempo and loop changes reach
  the audio thread through a lock-free snapshot exchange (no locks or memory allocation there).
- Automated tests for the tempo maths (jumps, ramps, signature changes, long songs), the
  transport and metronome timing, looping, count-in, the snapshot exchange, the timeline edits,
  and zoom/snapping (103 tests), plus real-time safety tests of the transport.

## [0.2.2] - 2026-11-06

### Fixed
- Text boxes (project name, track rename): selected text was drawn almost black and the typing
  cursor was dark grey on a dark background, so you couldn't see what you were typing. Selected
  text is now readable, the cursor is teal, and the box has a teal outline while you type.

### Added
- A log file, `%LOCALAPPDATA%\Aether\Logs\Aether.log` (Help > Open log folder), recording
  start-up, audio device changes and errors, to help diagnose problems.
- When a sound device can't be opened, the orange message now shows the exact reason Windows
  gave (for example "another program is using this device") and the error code, instead of
  only listing likely causes.

## [0.2.1] - 2026-11-05

### Fixed
- The installer no longer copies JUCE's developer files (about 4,000 C++ headers and build
  tools) into Aether's install folder. Updating removes them from earlier installs.

## [0.2.0] - 2026-11-05 - Milestone 1: projects, tracks, undo, autosave and crash recovery

### Added
- Start screen: create a new project (name and folder), or open a recent one. Recent projects
  can be removed from the list or shown in File Explorer.
- Projects are folders (`My Song/My Song.aether` plus `Audio`, `Backups` and `Autosave`), saved as
  versioned JSON (format version 1, documented in docs/FILE_FORMAT.md) with an upgrade system so
  old projects always open. Files from newer versions are refused without being changed.
- Safe saving: the old file is kept in `Backups/` (newest 10), and the new file replaces it in one
  step so a crash mid-save can't damage the project.
- Tracks: add audio and MIDI tracks, rename (double-click or F2), change colour, reorder (drag, or
  Ctrl+Up/Down), delete, select with the arrow keys, right-click menu.
- Undo and redo for every change (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z), with named steps
  ("Undo Rename track").
- Unsaved-changes prompt (Save / Don't save / Cancel) when closing, quitting, opening or starting
  another project; `*` in the title bar and "Unsaved changes" in the status bar.
- Autosave every 60 seconds to the project's `Autosave/` folder, in the background.
- Crash recovery: if Aether didn't close properly, the next start offers to recover the unsaved
  work, or to open the last saved version. Help > Test crash recovery crashes on purpose to try it.
- Crash reports (Windows minidumps) in `%LOCALAPPDATA%\Aether\CrashReports`.
- Menus (File, Edit, Track, Settings, Help) with keyboard shortcuts; toolbar buttons show their
  shortcut in their tooltip.
- File > Save as (copies the project folder's audio too), Show project folder, Open recent.
- Double-clicking a `.aether` file opens it in Aether (the installer registers the file type).
- Help > About, with the licence and a link to the source code.
- The main window remembers its size and position.
- Automated tests for the project model, undo/redo, saving/loading, format upgrades, backups,
  autosave, crash recovery, recent projects, and the track list (renaming, dragging).

### Changed
- The Milestone 0 screen is now Settings > Audio settings (Ctrl+,), in its own window.

## [0.1.1] - 2026-11-05 - Milestone 0 fixes from first test

### Fixed
- Choosing a device setting that can't be opened (for example a built-in microphone in
  Exclusive Mode) no longer leaves Aether with no sound device at all: it goes back to the last
  setting that worked.

### Added
- A warning banner that explains in plain language why a device couldn't be opened and what to
  try (another program using it, Windows' "exclusive control" setting, or using "Windows Audio").
- The hint under "Audio device" now explains the selected mode, including why the buffer size
  can't be changed in "Windows Audio" mode (Windows chooses it).
- Latency shows "No input selected" instead of "0 samples" when no microphone is chosen.
- Tests for the device fallback, using a fake sound card.

### Changed
- Pop-up messages have a cleaner design (small round icon instead of a large cut-off triangle).

## [0.1.0] - 2026-11-05 - Milestone 0: skeleton and build pipeline

### Added
- Aether application window with a dark theme and tooltips on every control.
- Audio device setup: choose Windows Audio (WASAPI shared, exclusive or low-latency mode) or
  ASIO, the output and input device, sample rate and buffer size. The choice is remembered.
- Latency readout: buffer size, output, input and round-trip latency in samples and milliseconds.
- "Play test tone" button (440 Hz at -18 dBFS, fades in and out so it never clicks).
- Output and input (microphone) level meters with peak hold, and an audio CPU meter.
- `--screenshot <file.png>` command-line option that saves a picture of the window and quits
  (used by automated checks).
- Automated tests (Catch2) for latency maths, the test tone, the audio engine and the lock-free
  queue, plus real-time safety tests run under Clang's RealtimeSanitizer on Linux.
- `build-windows.bat`: one-click Windows build, tests and installer.
- Inno Setup installer (`Aether-Setup-<version>.exe`) for 64-bit Windows 10/11, with no extra
  runtime downloads required.
- GitHub Actions workflow: builds and tests on Windows and Linux for every push, uploads
  Setup.exe and screenshots, and publishes a release for version tags.
- Documentation: README, architecture, roadmap, feature status, credits, Milestone 0 test checklist.
