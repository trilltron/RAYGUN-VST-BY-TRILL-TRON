RAYGUN by TRILL TRON  v1.0.0  -  Windows 64-bit  -  VST3 + Standalone
=====================================================================

WHAT YOU NEED
  * Windows 10 / 11, 64-bit. Nothing else - no Visual C++ redistributable, no extra DLLs.
  * FL Studio 20.7 or newer (FL 20, 21, 2024, 2025+). FL Studio loads VST3 natively from 20.7 on.
    (Versions 20.0-20.6 only load VST2 plugins; RAYGUN is VST3, so update FL to 20.7+, which is free.)
  * Any other VST3 host works too: Ableton, Reaper, Cubase, Bitwig, Studio One...

FL STUDIO 20.0 - 20.6 (OLD VERSIONS)  ->  USE THE VST2 FILE
  Old FL versions only load VST2. The install zip includes RAYGUN_VST2.dll and the installer copies it to
  C:\Program Files\VSTPlugins\RAYGUN.dll . Then in FL: Options > File settings > make sure
  C:\Program Files\VSTPlugins is listed under "Plugin search paths", then Options > Manage plugins > Find installed plugins.
  RAYGUN appears as a normal (VST) instrument. Personal-use build: it uses its own minimal VST2 interface,
  not the Steinberg SDK, so please do not redistribute it commercially.

INSTALL (30 seconds)
  1. Unzip this folder anywhere.
  2. Right-click  Install_RAYGUN.bat  ->  Run as administrator.
     (Manual alternative: copy the folder "RAYGUN.vst3" into  C:\Program Files\Common Files\VST3\ )

FL STUDIO: FIRST LOAD
  1. Options > Manage plugins.
  2. Make sure  C:\Program Files\Common Files\VST3  is listed under plugin search paths (it is by default).
  3. Click "Find installed plugins" (Start scan). RAYGUN appears in the list.
  4. Click the star/enable next to RAYGUN, then in the Channel rack:  + Add one > RAYGUN
     (or Browser > Plugin database > Installed > Generators > RAYGUN).
  5. Draw notes in the piano roll, or play your MIDI keyboard.

KEYBOARD / MIDI SAFETY
  RAYGUN never grabs your computer keyboard. FL's typing-keyboard piano, shortcuts and your MIDI controller
  keep working while the plugin window is open. Only the preset search box accepts typing: click it, type,
  press Enter or Esc and focus is handed straight back.

RESIZING
  Drag the plugin window edge in FL, or Settings tab > Display size (75% - 200%). The UI is vector - always sharp.

PRESETS
  595 factory presets are built into the plugin. Click the preset name (top-left) to open the Preset Arsenal:
  filter by Type / Character / Style / Genre, search, star favourites, load into Layer A or B.
  Your own presets (menu > Save preset) go to  Documents\Trill Tron\RAYGUN\User Presets  (.rgp files).

TROUBLESHOOTING
  * Not in the list after scanning: reinstall as administrator; in FL use "Start scan" with "Verify plugins" ticked.
  * No sound: check mode (top bar A / B / A+B / SPLIT / VEL) - in SPLIT, low keys play Barrel A and high keys Barrel B.
  * CPU: switch Quality to ECO (top bar). Typical patches use 1-6% of one core.
  * Uninstall: run Uninstall_RAYGUN.bat as administrator.
