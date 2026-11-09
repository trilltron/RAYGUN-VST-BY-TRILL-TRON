cmake_minimum_required(VERSION 3.22)
project(RAYGUN VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
# Static MSVC runtime: users need no Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(RAYGUN_VST3_MANIFEST "Generate VST3 moduleinfo.json (needs to run a helper; off when cross-compiling)" ON)
set(RAYGUN_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8 checkout (otherwise fetched)")
if(RAYGUN_JUCE_PATH)
  add_subdirectory(${RAYGUN_JUCE_PATH} juce EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(juce)
endif()

# JUCE 8.0.4 hardcodes new Windows peers to Direct2D (engine index 1). In host-embedded plugin windows (FL Studio) that gives a black
# window, so start every peer on the software renderer (index 0) instead. Idempotent; only touches the Windows peer source.
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(_rg_peer "${juce_SOURCE_DIR}/modules/juce_gui_basics/native/juce_Windowing_windows.cpp")
  if(RAYGUN_JUCE_PATH)
    set(_rg_peer "${RAYGUN_JUCE_PATH}/modules/juce_gui_basics/native/juce_Windowing_windows.cpp")
  endif()
  if(EXISTS "${_rg_peer}")
    file(READ "${_rg_peer}" _rg_src)
    string(REPLACE "(HWND) parentHWND, false, 1 }" "(HWND) parentHWND, false, 0 /*RAYGUN: software renderer*/ }" _rg_new "${_rg_src}")
    if(NOT _rg_new STREQUAL _rg_src)
      file(WRITE "${_rg_peer}" "${_rg_new}")
      message(STATUS "RAYGUN: patched JUCE Windows peer to default to the software renderer")
    endif()
  endif()
endif()

file(GLOB RG_WT   CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/data/wavetables/*.rgwt)
juce_add_binary_data(RaygunData SOURCES ${RG_WT} ${CMAKE_SOURCE_DIR}/presets/factory.json
  ${CMAKE_SOURCE_DIR}/data/fonts/Orbitron.ttf ${CMAKE_SOURCE_DIR}/data/fonts/Rajdhani-SemiBold.ttf ${CMAKE_SOURCE_DIR}/data/fonts/Rajdhani-Bold.ttf)
set_target_properties(RaygunData PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(RAYGUN
  COMPANY_NAME "Trill Tron"
  PLUGIN_MANUFACTURER_CODE TrTr
  PLUGIN_CODE Rgun
  PRODUCT_NAME "RAYGUN"
  FORMATS VST3 Standalone
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  VST3_CATEGORIES Instrument Synth
  VST3_AUTO_MANIFEST ${RAYGUN_VST3_MANIFEST}
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(RAYGUN PRIVATE
  Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(RAYGUN PRIVATE Source)
target_compile_definitions(RAYGUN PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
  JUCE_REPORT_APP_USAGE=0 JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(RAYGUN PRIVATE RaygunData
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
if(MSVC)
  target_compile_options(RAYGUN PRIVATE /O2 /fp:fast /MP /utf-8)
elseif(MINGW)
  target_compile_options(RAYGUN PRIVATE -O2 -fno-finite-math-only)
  target_link_options(RAYGUN PRIVATE -static -static-libgcc -static-libstdc++)
else()
  target_compile_options(RAYGUN PRIVATE -O3 -ffast-math -fno-finite-math-only)
endif()

# Engine tests / offline renderer (no JUCE needed)
option(RAYGUN_BUILD_TOOLS "Build rg_render + tests" ON)
if(RAYGUN_BUILD_TOOLS)
  add_executable(rg_render tools/render_cli.cpp)
  add_executable(rg_tests tests/dsp_tests.cpp)
  add_executable(rg_bench tests/bench.cpp)
  enable_testing()
  add_test(NAME dsp COMMAND rg_tests)
endif()

option(RAYGUN_BUILD_HARNESS "Headless plugin test + screenshots" OFF)
if(RAYGUN_BUILD_HARNESS)
  juce_add_console_app(rg_harness PRODUCT_NAME "rg_harness")
  target_sources(rg_harness PRIVATE tests/plugin_harness.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_include_directories(rg_harness PRIVATE Source)
  target_link_libraries(rg_harness PRIVATE RaygunData juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra juce::juce_recommended_config_flags)
  target_compile_definitions(rg_harness PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
endif()

# VST2 build (legacy hosts such as FL Studio 20.0-20.6). Own minimal VST2 ABI - no Steinberg SDK. Personal-use build.
option(RAYGUN_BUILD_VST2 "Build the VST2 wrapper DLL" ON)
if(RAYGUN_BUILD_VST2)
  add_library(RAYGUN_VST2 MODULE Source/vst2/Vst2Wrapper.cpp Source/vst2/Vst2DllMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  set_target_properties(RAYGUN_VST2 PROPERTIES PREFIX "" OUTPUT_NAME "RAYGUN_VST2" SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
  target_include_directories(RAYGUN_VST2 PRIVATE Source)
  target_compile_definitions(RAYGUN_VST2 PRIVATE RAYGUN_VST2_BUILD=1 JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
  target_link_libraries(RAYGUN_VST2 PRIVATE RaygunData juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
  if(MSVC)
    target_compile_options(RAYGUN_VST2 PRIVATE /O2 /fp:fast /MP /utf-8)
  endif()
endif()
