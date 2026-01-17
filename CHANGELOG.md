# Changelog

Rudimentary changelog of new features & fixes between recent versions.
A more complete list of all noteworthly changes can be found on the releases page.
https://github.com/Meowchestra/MidiEditor/releases


## [4.3.1] - 2026-01-17

<details>
<summary>Summary</summary>

* Fixed DataEditor & EventWidget strings not properly displaying. (i.e. SysEx events)
* Fixed MidiOutput channel instruments not resetting on file change.
* Fixed StrummerDialog End Timing tooltips not newlining.

</details>

## [4.3.0] - 2025-11-23

<details>
<summary>Summary</summary>

* Strummer Tool: strum / arpeggiate selected chord notes with ``ctrl`` + ``alt`` + ``s``
  - adjustable modes: ``start (timing)``, ``end (timing)``, ``velocity``
  - options: ``preserve end``, ``alternate direction``, ``relative strength (per note)``, ``strum across tracks``
* Added a simple update checker. Start of the application will now compare the app version with the github repo release tags. If there is a higher tag version available than your current version, you will be prompted with an update is available asking if you would like to update. Clicking yes will take you to the github releases page for the new version. Optionally, you can also force manual version checks under the Help dropdown.
* Improved in-place upgrading via the installer wizard for the default directory. If you use the installer for a new version but have an older version already installed, the installer will now ask to run the maintenancetool uninstaller for the previous version before continuing with the new installation.
* Improved custom keybinds ui to notify on duplicate shortcuts, special handling for actions with multiple default shortcuts, and minor ui adjustments for fitting.
* Control change number is now shown in the misc widget dropdown & "undefined" control changes no longer show the text undefined to make it easier to navigate.
* Can now set custom control change names (for specifying undefined out-of-spec # names used in other programs) & instrument definitions (via instrument .ins files or manually renaming).
* Settings window is now scrollable and resizable smaller for lower resolutions.
* Qt version bumped to 6.10.1 & rtmidi dependency updated.

</details>

## [4.2.0] - 2025-10-16
<details>
<summary>Summary</summary>

* Implemented Drag & Drop track reordering.
* Implemented the ability to set custom keybinds.
_Some hardcoded binds still remain._ -- https://github.com/Meowchestra/MidiEditor/issues/28#issuecomment-3567161623
_(i.e. ctrl drag note edge to adjust duration, shift + # to move selected events to specific track, holding modifier keys while dragging note(s) to lock in place, etc. etc.)_
* Convert Pitch Bends Tool:
  - ``ctrl`` + ``b`` selected notes to convert assigned pitch bend events into separate notes of the nearest semitone
  - options: ±2 semitones (general MIDI default), ±12 semitones (guitar/bass VSTs), ±24 semitones (extreme pitch modulation), custom (±1-96 semitones)
_Use of the glue tool (ctrl + g) afterwards is helpful to recombine notes of the same pitch back into a single note. Each individual pitch bend event (on a note's position) is converted into a separate note and then removed._
* Explode Chords to Tracks Tool:
  - ``ctrl`` + ``e`` to explode chords into separate tracks. Select / focus a track to operate across the entire track or highlight chords after track selection to minimize scope.
  - chord detection modes: same start tick, same start tick & note length
  - grouping modes: group equal parts (voices) into tracks, each chord into separate track, all chords into single track
  - options: minimum simultaneous notes, insert new tracks at end of track list, keep original notes on source track
* UI improvements for settings & toolbar separator inconsistencies.
* Keybinds for Measure tool, Time Signature tool, and Tempo tool changed to ``ctrl`` + ``F1``, ``ctrl`` + ``F2``, & ``ctrl`` + ``F3``.
_F13 and F14 keys are not commonly used in standard keyboards lol..._
* Compiled with Qt 6.10.0 release build. This seemingly fixes the MaintenanceTool not removing properly when uninstalling if using the standard Setup installer.
* Fixed metronome amplifying loudness oddities.

</details>

## [4.1.4] - 2025-08-29
<details>
<summary>Summary</summary>

* Fixed source instance notes not preserving musical tempo / duration when pasted into target instance if tempo was mismatched.
(i.e. pasting from midi 1 tempo 120 -> midi 2 tempo 700, the note position / duration will properly preserve original note intent)
* Fixed cross-pasted key signature events, tempo events, and time signature events pasting into the correct channels (16, 17, & 18).
* Fixed the midi not recalculating its tempo map if tempo events are pasted from the shared clipboard to the cursor location.
* Fixed weird edge case of shared clipboard notes becoming ghost notes (which don't delete on track remove and couldn't be manually deleted) not properly removing on protocol undo (ctrl + z) if meta events were included in the paste.

</details>

## [4.1.3] - 2025-08-15
<details>
<summary>Summary</summary>

- Horizontal scrollbar is now fixed in place and no longer collapses if the misc widget (velocity box) is collapsed. Allowing you to collapse the misc widget to have a larger view & still be able to scroll.
- Fixed a bunch of horizontal scrollbar oddities with it not properly aligned to the start at 0 (empty left space padding) and scaling / resizing issues.
- No longer caching note colors. Notes should immediately repaint accurate colors on track deletion or protocol undo/redo/jump-to actions.

</details>

## [4.1.2] - 2025-08-09
<details>
<summary>Summary</summary>

- Option to unlock widget minimum resizing before snapping closed.

</details>

## [4.1.1] - 2025-08-08
<details>
<summary>Summary</summary>

- Fixed application crash (hopefully) when changing app styles while a midi is loaded.
- Fixed some cached colors not clearing when changing app styles from light -> dark mode compatible theme or toggling windows dark mode on/off on a compatible theme while the app is running.

</details>

## [4.1.0] - 2025-08-08
<details>
<summary>Summary</summary>

* Modernized installer wizard, supporting windows dark mode.
* Windows dark mode compatibility for application styles windows11, windows, & fusion.
  - Channel/Track colors will automatically update to a darker shade if no custom colors are saved.
* Fully customizable toolbar header with support for compact single row and expanded double row.
* System options to ignore default Qt6 scaling with windows & changing scaling behavior.
* Performance improvements for rendering, batch selecting events, batch deleting events, & initial hardware acceleration for widgets.
  - Additional tweaks for visuals & performance available in settings.
* Quantize selection keybind changed to ``ctrl`` + ``q``.
* Glue tool: glue notes of the same pitch & track together.
  - ``ctrl`` + ``g`` (same channel) 
  - ``ctrl`` + ``shift`` + ``g`` (all channels)
* Scissors tool: cut notes into separate events from cursor position with ``ctrl`` + ``x``.
* Delete overlap tool: delete overlapping notes with ``ctrl`` + ``d``.
  - modes: delete overlaps (mono), delete overlaps (poly), delete doubles
  - options: respect track boundaries, respect channel boundaries
* Shared Clipboard: now able to copy & paste events across multiple MidiEditor instances. Cross-instance pasting at cursor location & selected track.
* Expanded raster & quantization options for triplets and other musical coverage.
* Fixed overlapping keybinds & updated default binds for ergonomics.
  - Large Increase/Decrease tweak now requires all 3 modifier keys (being the 3rd step of small, medium, large).
  - Zoom in actions no longer require holding shift (+ -> =) when it didn't for zoom out actions (-).
  - Vertical out/in updated to ``shift`` + ``-/=`` to match Qt vertical mouse scrolling on shift.
  - Scroll multiplier with ``ctrl`` + ``shift`` + ``mousewheel``. Default zoom now only scrolls 1 line at a time.
  - Full reset view with ``ctrl`` + ``shift`` + ``backspace`` and updated zoom reset to ``ctrl`` + ``backspace``.
* Numerous fixes such as sustain notes disappearing if the start & end times were outside the view, sustains partially outside the view visually clipping the note length when dragged back into view, dead scrolls when zoomed in too far, cancelling color selection setting color black instead of properly cancelling, opacity not applying to custom colors set after opacity adjustment, and more.

</details>

## [4.0.0] - 2025-07-22
<details>
<summary>Summary</summary>

Substantially updated MidiEditor that doesn't have as many memory leaks. Numerous changes and improvements made prior to v4.0, pre-changelog.

Some notable features:

* Support for all 3 main strip styling people may want. By keys, alternating, and by octave (c).
* Added an option to highlight c3/c6 range lines so you can more easily tell if notes are outside the range.
* You can quickly transpose notes up/down octaves by selecting them and then ``shift + up/down``.
* You can quickly update note on/off tick duration times by selecting notes and then holding ``ctrl + drag left/right edge``
* You can quickly move events to a different track by ``shift + #``
* You can lock position movement when dragging notes around while holding ``shift`` to go up/down or ``alt`` to go left/right.
* Support for different application styles if you have a specific preference.

Most noticeably should no longer see the app go to 1gb+ when making major edits such as bulk deleting and undoing over and over. Should be a bit more lightweight with less spikes overall from optimizations / qt6. Also less crashing when loading rare malformed midis / long track names.

Plus other improvements from various forks and imported changes to be discovered.

</details>
