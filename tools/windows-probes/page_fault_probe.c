#include <windows.h>
#include <psapi.h>
#include <stdio.h>
static DWORD faults(void) { PROCESS_MEMORY_COUNTERS c; GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof c); return c.PageFaultCount; }
int main(void) {
    SIZE_T size = 64u << 20;
    char *p = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    DWORD before = faults();
    for (SIZE_T i = 0; i < size; i += 4096) p[i] = 1;
    DWORD after = faults();
    printf("touch_64MiB_pagefault_delta=%lu expected_4k_pages=%zu\n", after - before, size / 4096);
    SIZE_T large = GetLargePageMinimum();
    void *lp = VirtualAlloc(NULL, large, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    printf("large_page_minimum=%zu large_alloc=%s err=%lu\n", large, lp ? "ok" : "failed", lp ? 0 : GetLastError());
    return 0;
}
