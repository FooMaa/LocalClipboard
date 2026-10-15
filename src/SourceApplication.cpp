#include "SourceApplication.h"
#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
bool SourceApplication::supported() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}
QString SourceApplication::current() {
#ifdef Q_OS_WIN
    DWORD pid = 0; GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};
    wchar_t buffer[32768]; DWORD size = 32768;
    const bool ok = QueryFullProcessImageNameW(process, 0, buffer, &size);
    CloseHandle(process);
    return ok ? QString::fromWCharArray(buffer, int(size)) : QString();
#else
    return {};
#endif
}
