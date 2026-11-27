#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

int wmain(int argc, wchar_t **argv) {
    HMODULE module;

    if (argc != 2) {
        fwprintf(stderr, L"usage: CxPackageRuntimeLoader <module.dll>\n");
        return 2;
    }

    module = LoadLibraryExW(argv[1], NULL,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (module == NULL) {
        fwprintf(stderr, L"LoadLibraryExW failed with error %lu\n",
            GetLastError());
        return 1;
    }

    if (!FreeLibrary(module)) {
        fwprintf(stderr, L"FreeLibrary failed with error %lu\n",
            GetLastError());
        return 1;
    }

    wprintf(L"PASS loaded %ls\n", argv[1]);
    return 0;
}
