// Минимальный LoadLibrary-инжектор. Собирается mingw-gcc, без зависимостей.
//   gcc -O2 tools/inject.c -o inject.exe
//   ./inject.exe cs2.exe "D:\\path\\to\\ragemod_cs2.dll"
// LoadLibrary грузит через штатный загрузчик — api-set'ы резолвятся сами,
// в отличие от manual-map (0xC0000135/0138 в Xenos).
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static DWORD pid_by_name(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32 pe = { .dwSize = sizeof(pe) };
    DWORD pid = 0;
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: %s <process.exe> <full\\path\\to.dll>\n", argv[0]);
        return 1;
    }
    const char* proc = argv[1];
    char full[MAX_PATH];
    if (!GetFullPathNameA(argv[2], MAX_PATH, full, NULL)) {
        printf("bad dll path\n");
        return 1;
    }
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
        printf("dll not found: %s\n", full);
        return 1;
    }

    DWORD pid = pid_by_name(proc);
    if (!pid) { printf("process '%s' not running\n", proc); return 1; }
    printf("pid %lu, dll %s\n", pid, full);

    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                           PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!h) { printf("OpenProcess failed: %lu (запусти от админа)\n", GetLastError()); return 1; }

    size_t n = strlen(full) + 1;
    void* remote = VirtualAllocEx(h, NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { printf("VirtualAllocEx failed: %lu\n", GetLastError()); CloseHandle(h); return 1; }
    if (!WriteProcessMemory(h, remote, full, n, NULL)) {
        printf("WriteProcessMemory failed: %lu\n", GetLastError());
        VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); return 1;
    }

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadlib = GetProcAddress(k32, "LoadLibraryA");
    HANDLE th = CreateRemoteThread(h, NULL, 0, (LPTHREAD_START_ROUTINE)loadlib, remote, 0, NULL);
    if (!th) {
        printf("CreateRemoteThread failed: %lu\n", GetLastError());
        VirtualFreeEx(h, remote, 0, MEM_RELEASE); CloseHandle(h); return 1;
    }
    WaitForSingleObject(th, INFINITE);
    DWORD ret = 0;
    GetExitCodeThread(th, &ret);   // младшие 32 бита HMODULE: 0 => LoadLibrary не загрузил
    printf(ret ? "injected ok\n" : "LoadLibrary вернул 0 — dll не загрузилась (зависимость/DllMain)\n");

    VirtualFreeEx(h, remote, 0, MEM_RELEASE);
    CloseHandle(th); CloseHandle(h);
    return ret ? 0 : 2;
}
