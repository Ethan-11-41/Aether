# Milestone 1: what to test

About 15 minutes. Note anything that doesn't match "you should see".

Install **Aether-Setup-0.2.1.exe** from the newest green run on the
[Actions page](https://github.com/Ethan-11-41/aether/actions) (under **Artifacts**). It installs
over the old version.

## 1. Start screen and a new project

1. Start Aether.
2. Type a name (e.g. `Test Song`) and click **Create project** (or press Enter).

**You should see:** the project window: a toolbar (Undo, Redo, + Audio track, + MIDI track), an
empty track list on the left saying "No tracks yet", and an empty timeline area on the right.
The window title says `Test Song - Aether`. In File Explorer, `Documents\Aether Projects\Test Song\`
now exists with `Test Song.aether` and the folders `Audio`, `Backups` and `Autosave`.

## 2. Tracks

1. Click **+ Audio track** twice and **+ MIDI track** once (or Ctrl+T, Ctrl+T, Ctrl+Shift+T).
2. Double-click a track name, type a new name, press Enter. (Or select it and press F2.)
3. Click the coloured strip on the left of a track and pick another colour.
4. Drag a track up or down by its header and drop it somewhere else.
5. Select a track and press Ctrl+Up / Ctrl+Down. Use the Up/Down arrow keys to change the selection.
6. Right-click a track: try the menu items.
7. Select a track and press Delete.

**You should see:** each change happen immediately. New tracks are called Audio 1, Audio 2,
MIDI 1… with different colours, and appear below the selected track. A teal line shows where a
dragged track will land.

## 3. Undo and redo

1. Press Ctrl+Z repeatedly, then Ctrl+Y (or Ctrl+Shift+Z) repeatedly.
2. Open the **Edit** menu between presses.

**You should see:** every change from step 2 undone one at a time, in reverse order, and redone.
The Edit menu says what will be undone, e.g. "Undo Rename track".

## 4. Saving and the title bar

1. Make a change: the window title gets a `*` and the status bar says "Unsaved changes".
2. Press Ctrl+S: the `*` disappears and the status bar says "Saved".
3. Close Aether, start it again: `Test Song` is listed under **Recent projects**. Double-click it.

**You should see:** the project exactly as you saved it (names, colours, order).

## 5. Unsaved-changes prompt

1. Make a change, then close the window (X button).

**You should see:** "Save changes?" with **Save**, **Don't save** and **Cancel**. Cancel keeps
Aether open; Don't save closes without saving; Save saves and closes.

## 6. Crash recovery (the important one)

1. Open the project and make a few changes, but **don't save**.
2. Choose **Help > Test crash recovery...** and click **Crash now**. Aether disappears
   (Windows may show a "stopped working" message).
3. Start Aether again.

**You should see:** "Recover unsaved work?" saying Aether didn't close properly. Click
**Recover**: your unsaved changes are back (the title shows `*`). Then save.

Also try it the other way: crash again, choose **Open last saved version**: the changes are
gone, and you're not asked again.

Optional: instead of the menu item, make a change, wait until the status bar says
"autosaved at …" (up to a minute), then end Aether with Task Manager (Ctrl+Shift+Esc > Aether >
End task). Recovery should be offered the same way.

## 7. Files

1. **File > Save as...**: give a new name. A new project folder is made, and the title changes.
2. **File > Show project folder** opens File Explorer at the project.
3. **File > Close project** goes back to the start screen. **File > Open project...** opens a
   `.aether` file.
4. Double-click a `.aether` file in File Explorer.

**You should see:** Aether opens that project (if Aether is already running, the existing
window switches to it, asking about unsaved changes first).

## 8. Audio settings still work

**Settings > Audio settings...** (Ctrl+,) opens the Milestone 0 screen in its own window: test
tone, meters, latency. Escape closes it.

## Reporting back

Tell me which steps worked and which didn't, and send a screenshot of anything that looks wrong.
Crash recovery (step 6) is the most important to report on.
