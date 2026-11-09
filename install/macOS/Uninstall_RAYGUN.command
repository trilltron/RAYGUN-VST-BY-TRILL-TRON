#!/bin/bash
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/RAYGUN.vst3" "$HOME/Library/Audio/Plug-Ins/Components/RAYGUN.component" "$HOME/Applications/RAYGUN.app"
echo "RAYGUN removed. Your presets in ~/Documents/Trill Tron/RAYGUN were left alone."
read -n 1 -s -r -p "Press any key to close..."
