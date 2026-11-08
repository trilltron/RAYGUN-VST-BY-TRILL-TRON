// Windows DLL entry: tells JUCE which module handle to use for window classes/resources.
// Kept in its own file because <windows.h> clashes with JUCE's Rectangle in GUI translation units.
#if defined(_WIN32)
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
 #include <juce_core/juce_core.h>
 BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) { if (reason == DLL_PROCESS_ATTACH) juce::Process::setCurrentModuleInstanceHandle(h); return TRUE; }
#endif
