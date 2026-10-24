#include <windows.h>
#include <stdio.h>
int main(void) {
    HANDLE tok; TOKEN_PRIVILEGES tp = { 1 };
    OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok);
    LookupPrivilegeValueA(NULL, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid);
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(tok, FALSE, &tp, 0, NULL, NULL);
    DWORD adj = GetLastError();  /* ERROR_NOT_ALL_ASSIGNED (1300) means the token lacks it */
    SIZE_T large = GetLargePageMinimum();
    for (int n = 1; n <= 64; n *= 2) { SIZE_T size = n * large;
    void *p = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    printf("adjust_err=%lu large_page_minimum=%zu alloc_%zuMiB=%s err=%lu\n", adj, large, size >> 20, p ? "ok" : "failed", p ? 0 : GetLastError());
    if (p) VirtualFree(p, 0, MEM_RELEASE); }
    return 0;
}
