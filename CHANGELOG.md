# Changelog

Rudimentary changelog of new features & fixes between recent versions.
A more complete list of all noteworthly changes can be found on the releases page.
https://github.com/Meowchestra/MidiEditor/releases


## [4.4.1] - 2026-04-12

<details>
<summary>Summary</summary>

* Implemented Game Support Option - Use FFXIV Instrument Names.
  - Sets custom instrument names in the Instruments settings (overriding channel instrument names) to match with instrument names used by FFXIV.
* Implemented Status Bar customization options for align direction, offset, separator type, and spacing size.
* Status Bar "Show Track / Channel Info" section now displays the instrument name alongside the channel number, matching track number including track name. Info section Track Name / Channel Instrument display is fully customizable if you do not wish to show the full details.
* Status Bar no longer merges note & event types together as note count if both were in a selection.
* SoundFonts downloader now includes source info buttons hyperlinking to soundfont source / download mirror so you can read more about that specific soundfont.
* Updated SoundFonts - SGM Pro 12, Microsoft GS Wavetable Synth, Final Fantasy XIV (Standard / Sample Range).
* Expanded Chord Detector to cover a wider range of chord types & chord root note.
* Fixed Standard Tool & Select Events (Box) tool not unselecting events if a modifier key is held. Holding Ctrl or Shift while making a box selection will now properly toggle selection state based on if the event is already selected or not, matching the newer behavior of Select Events (Single) tool.
* Constrained Cursor to Measure Timeline to prevent it from stretching into marker row if shown.
* Extended Color Support to 17 tracks. 
_Appearance color customization only had 16 tracks shown while there were 17 actual channels (16 note channels + 1 general event/meta channel). Track 0 is commonly used as a system/meta track for things like Tempo & Time/Key Signature events (which is why its color matches General Events channel). This change now allows for the full complete color preset list in both Tracks & Channels before repeating._
* Numerous fixes and improvements to Smooth Playback Scrolling rendering performance, locked screen toggle jumping, elastic catch-up and forward timeline page turning.
* Numerous fixes and improvements to FFXIV Channel Fixer.
* Guard against infinite recursion during event loading fallback & small protocol memory leak improvement.
* Small fix to GuitarPro importer.
* More UI cleanup.
  - Reordered Track operations before Channel in Edit / View / Playback, Appearance settings, context menu, & event widget property order to follow Track -> Channel display order used by Tracks/Channel widget, Status Bar display, & Add new events to... section.
  - Removed redundant text in Add new events section.
  - Various cleanup everywhere to use "Track #: Name" / "Channel #: Instrument" instead of abbreviated numbers or missing info.
  - Renamed Percussion to Channel 9 and description "Drums and percussion events" to just Percussion to match with other Channel # / Instrument labeling.
  - Event widget channel selection is now a dropdown instead of a number box to match track selection.

</details>

## [4.4.0] - 2026-04-07

<details>
<summary>Summary</summary>

* Implemented built-in FluidSynth synthesizer with support for multiple reverb engines, soundfonts (sf2, sf3, & dls), and exporting midi workspace to wav audio.
* Implemented SoundFont downloader with support for the latest developed soundfonts. The default list of included soundfonts are organized under General, Games, & Legacy categories, alongside a hyperlink to find more fonts from musical-artifacts.com.
* Implemented note duration presets for the New Note Tool in the tools menu, with remappable keybinds. Activating a preset will immediately switch to the New Note Tool and show a visual preview of the fixed note duration for placement.
  - ``alt`` + `` ` `` - drag mode, ``alt`` + ``1-7`` - standard notes, ``alt`` + ``shift`` + ``1-8`` - tuplets
* Implemented Split Channels to Tracks Tool. Supports splitting type 0 midis (1 track, multiple channels) into different tracks & splitting drumkits (channel 9 percussion) into track groups mapped to specific target notes. Fully customizable mapping matrix with 3 default community presets available and previewable target note audio.
* Implemented mml & guitarpro format converters to be able to load additional music formats into midi.
* Implemented Context Menu for Standard Tool & Select Events (Single / Box) Tool. Context Menu can be accessed with ``Middle Click`` or ``Ctrl`` + ``Right Click``
  - Easy access to Copy, Paste, & Delete operations. Paste occurs at mouse position so you don't have to move the measure cursor before pasting.
  - Includes commonly used operations:
    * Quantize Selection, Move Events to Channel, Move Events to Track, Transpose (Selection, Octave Up, Octave Down), & Scale Events.
  - New options for note duration adjustment:
    * Stretch to Fill Neighbors, Extend Start to Previous End, Extend End to Next Start
  - New options for note position adjustment:
	* Snap Start to Previous End, Snap End to Next Start
* Implemented timeline marker display bar with options to show program change (PC) markers, control change (CC) markers, and text event (txt) markers. If enabled, a new row will show below the measure, allowing you to visualize where events appear in the midi & drag reposition with ease without needing to scroll to the bottom of the piano roll.
* Implemented customizable Status Bar with options to Show Track / Channel Info, Show Note Info (Name / Count), Show Selection Range, & Show Chord Name, alongside multiple chord detection strategies.
* Implemented option for alternative continuous smooth playback scrolling.
* Implemented color presets for track / channel colors.
* Implemented Game Support settings section with initial tool support for Final Fantasy XIV.
  - ``Fix XIV Channels`` will enable a new UI option in the Tracks / Channels widget that will try & fix note channels / program change events based on track title instrument matching and velocity normalizing.
* Updated Magnet with new option to snap to notes. Both ``Snap to Grid`` & ``Snap to Notes`` can be independently enabled or disabled for behavior preferences.
* Updated Glue Tool so only a single tool / icon exists in the tools menu & customizable toolbar. Dual functionality still exists with separate ``ctrl`` + ``g`` & ``ctrl`` + ``shift`` + ``g`` remappable keybinds, but now you can also hold shift while clicking on the toolbar icon for the all-channels behavior.
* Updated StandardTool to allow resizing notes by dragging from the start or end edges without needing to hold ctrl & replaced held ctrl behavior with freeform movement which prevents resizing if dragging from the edge (unifying behavior of held modifier keys (shift / alt / ctrl) controlling all movement locking).
* Improved note resizing behavior to show a previewable ghost duration while dragging an edge & updated Resize Events Tool to be less problematic. Left click controls Note OnTick position & right click controls Note OffTick position.
* Improved Transpose Tool to prevent operation if notes would collapse in on itself above maximum height (note 127) or minimum height (note 0).
* Improved Select Events (Single) Tool so multiple notes can be individually selected & unselected while holding shift or ctrl when clicking a note.
* Improved github workflows & installer setup. Qt plugin binaries are now neatly organized in a plugins subdirectory and no longer include unnecessary Qt modules (QtXml, QtMultimedia / ffmpeg libs), reducing total application size. The uninstaller additionally attempts further cleanup of files post-uninstall to work around Windows sometimes file locking MaintenanceTool, preventing it from deleting itself / app directory during uninstall routines.
* Overhauled metronome to use native midi note events metronome click (33) and metronome bell (34). Allowing for metronome to be more in sync with midi playback and optionally styled by soundfonts.
* Appearance changes now reflect immediately instead of when settings closes (strip style, c3-6 range lines, opacity, track / channel color).
* Fixed channel 16 (displayed channel 15) not being available in the misc widget.
* Fixed terminator null bytes showing [] characters in track name & causing copy truncation for some midi files. Null characters are now properly removed.
* Numerous UI improvements across the board for missing icons in protocol history, clipped track/channel text height, clipped color previews, updated settings window, file picker filter names, relocated / reordered menu options, option renames (quantify -> quantize, quantization fraction -> quantize resolution, raster -> grid division), standardizing title case/sentence case, and much more...
* Updated dependencies to Qt 6.11, QtIFW 4.11, & newer rtmidi submodule.

_FluidSynth is built from upstream source compiled with the latest multi-reverb engine branch changes._ 
_libsndfile, libogg, libvorbis, libflac, & libopus dependencies are also source compiled for latest improvements._

</details>

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
