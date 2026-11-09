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
 static void appendRaw(const char* text, int n) {
     char tmp[MAX_PATH] = {}, path[MAX_PATH] = {};
     if (!GetTempPathA(MAX_PATH, tmp)) return;
     std::snprintf(path, MAX_PATH, "%sraygun_dll.txt", tmp);
     HANDLE f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
     if (f != INVALID_HANDLE_VALUE) { DWORD w = 0; WriteFile(f, text, (DWORD) n, &w, nullptr); CloseHandle(f); }
 }
 static BOOL CALLBACK enumChild(HWND h, LPARAM idx) {
     int& i = *(int*) idx; RECT r{}; char cls[128] = {}; GetClassNameA(h, cls, 127); GetWindowRect(h, &r);
     char line[400]; int n = std::snprintf(line, sizeof(line), "    sibling z=%d hwnd=%p class=%s visible=%d rect=[%ld,%ld,%ld,%ld]\r\n", i++, (void*) h, cls, (int) IsWindowVisible(h), r.left, r.top, r.right, r.bottom);
     appendRaw(line, n); return TRUE; }
 extern "C" __declspec(dllexport) void rg_logWindowInfo(void* hwnd, const char* tag) {
     HWND h = (HWND) hwnd; char line[900]; RECT wr{}, cr{}, pr{}; char cls[128] = {}, pcls[128] = {};
     if (!h || !IsWindow(h)) { int n = std::snprintf(line, sizeof(line), "[%s] hwnd %p is NOT a window\r\n", tag, hwnd); appendRaw(line, n); return; }
     GetWindowRect(h, &wr); GetClientRect(h, &cr); GetClassNameA(h, cls, 127); HWND p = GetParent(h); if (p) { GetWindowRect(p, &pr); GetClassNameA(p, pcls, 127); }
     int n = std::snprintf(line, sizeof(line), "[%s] hwnd=%p class=%s visible=%d style=0x%lx ex=0x%lx wnd=[%ld,%ld,%ld,%ld] client=%ldx%ld | parent=%p class=%s visible=%d rect=[%ld,%ld,%ld,%ld]\r\n", tag, hwnd, cls, (int) IsWindowVisible(h), (unsigned long) GetWindowLongPtr(h, GWL_STYLE), (unsigned long) GetWindowLongPtr(h, GWL_EXSTYLE), wr.left, wr.top, wr.right, wr.bottom, cr.right, cr.bottom, (void*) p, pcls, p ? (int) IsWindowVisible(p) : 0, pr.left, pr.top, pr.right, pr.bottom);
     appendRaw(line, n); if (p) { int i = 0; EnumChildWindows(p, enumChild, (LPARAM) &i); } }
 BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
     if (reason == DLL_PROCESS_ATTACH) { juce::Process::setCurrentModuleInstanceHandle(h); breadcrumb("ATTACH", h); }
     else if (reason == DLL_PROCESS_DETACH) breadcrumb("DETACH", h);
     return TRUE; }
#endif
