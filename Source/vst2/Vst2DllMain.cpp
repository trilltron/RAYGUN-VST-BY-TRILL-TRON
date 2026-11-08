// Windows DLL entry: tells JUCE which module handle to use for window classes/resources, and leaves a breadcrumb.
// Kept in its own file because <windows.h> clashes with JUCE's Rectangle in GUI translation units.
#if defined(_WIN32)
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
 #include <juce_core/juce_core.h>
 #include <cstdio>
 static void breadcrumb(const char* what, HINSTANCE h) {   // raw Win32 append; safe under loader lock
     char tmp[MAX_PATH] = {}, path[MAX_PATH] = {}, mod[MAX_PATH] = {}, exe[MAX_PATH] = {}, line[1200] = {};
     if (!GetTempPathA(MAX_PATH, tmp)) return;
     std::snprintf(path, MAX_PATH, "%sraygun_dll.txt", tmp);
     GetModuleFileNameA(h, mod, MAX_PATH); GetModuleFileNameA(nullptr, exe, MAX_PATH);
     int n = std::snprintf(line, sizeof(line), "%s pid=%lu host=%s dll=%s\r\n", what, (unsigned long) GetCurrentProcessId(), exe, mod);
     HANDLE f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
     if (f != INVALID_HANDLE_VALUE) { DWORD w = 0; WriteFile(f, line, (DWORD) n, &w, nullptr); CloseHandle(f); }
 }
 BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
     if (reason == DLL_PROCESS_ATTACH) { juce::Process::setCurrentModuleInstanceHandle(h); breadcrumb("ATTACH", h); }
     else if (reason == DLL_PROCESS_DETACH) breadcrumb("DETACH", h);
     return TRUE; }
#endif
