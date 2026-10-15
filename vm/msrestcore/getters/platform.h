/* platform.h */
#ifndef PLATFORM_H
#define PLATFORM_H

#ifdef _WIN32
    #define MSREST_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define MSREST_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #define MSREST_PLATFORM_MACOS 1
#else
    #error "Unsupported platform"
#endif

/* dll/so */
#ifdef MSREST_PLATFORM_WINDOWS
    #include <windows.h>
    typedef HMODULE LibHandle;
    #define LIB_EXT ".dll"
    static inline LibHandle lib_open(const char *path) { return LoadLibraryA(path); }
    static inline void* lib_sym(LibHandle h, const char *name) { return (void*)GetProcAddress(h, name); }
    static inline void lib_close(LibHandle h) { FreeLibrary(h); }
#else
    #include <dlfcn.h>
    typedef void* LibHandle;
    #define LIB_EXT ".so"
    static inline LibHandle lib_open(const char *path) { return dlopen(path, RTLD_NOW | RTLD_LOCAL); }
    static inline void* lib_sym(LibHandle h, const char *name) { return dlsym(h, name); }
    static inline void lib_close(LibHandle h) { dlclose(h); }
#endif

#endif