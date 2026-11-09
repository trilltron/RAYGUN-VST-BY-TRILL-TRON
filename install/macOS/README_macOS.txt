RAYGUN by TRILL TRON  v1.0.0  -  macOS (Apple Silicon + Intel)  -  VST3, Audio Unit, Standalone
===========================================================================================

INSTALL
  1. Unzip RAYGUN_macOS.zip.
  2. Double-click  Install_RAYGUN.command .
     If macOS says it "cannot be opened because it is from an unidentified developer":
        System Settings > Privacy & Security > scroll down > "Open Anyway"  (or right-click the file > Open).
     Alternative, in Terminal:   xattr -cr ~/Downloads/RAYGUN_macOS   then double-click the installer again.
  3. Restart your DAW and rescan plugins.

WHERE THINGS GO (your user Library - no admin password)
  VST3        ~/Library/Audio/Plug-Ins/VST3/RAYGUN.vst3         Ableton Live, FL Studio, Reaper, Bitwig, Cubase, Studio One
  Audio Unit  ~/Library/Audio/Plug-Ins/Components/RAYGUN.component   Logic Pro, GarageBand, MainStage
  Standalone  ~/Applications/RAYGUN.app

REQUIREMENTS  macOS 11 or newer. Nothing else.

NOTES
  * These builds are not notarized by Apple (no paid developer account), so macOS shows the warning above once.
  * 595 factory presets are built in. Your own presets: menu > Save preset  (~/Documents/Trill Tron/RAYGUN/User Presets).
  * RAYGUN never takes your computer keyboard; only the preset search box accepts typing.
  * Logic Pro: if RAYGUN is missing, Logic > Settings > Plug-in Manager > Reset & Rescan Selection.
  * Uninstall: double-click Uninstall_RAYGUN.command.
