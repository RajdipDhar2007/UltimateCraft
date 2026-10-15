// Crash logging: on a fatal signal (or SIGUSR1, which only dumps) a backtrace is appended to crash.log in the
// game folder and printed to stderr. Offsets can be resolved with `addr2line -e ultimatecraft <+offset>`.
// Windows records the exception code and address. Nothing is ever sent anywhere: see PRIVACY.md.
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "common.h"
#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
static void put(int fd, const char *s) { if (write(fd, s, strlen(s)) < 0) {} }
static void crash_handler(int sig) {
    void *bt[64];
    int n = backtrace(bt, 64);
    char head[160];
    snprintf(head, sizeof head, "\n=== UltimateCraft " UC_VERSION " %s (signal %d, time %ld) ===\n", sig == SIGUSR1 ? "backtrace on request" : "crashed", sig, (long)time(NULL));
    int fd = open("crash.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { put(fd, head); backtrace_symbols_fd(bt, n, fd); close(fd); }
    put(2, head); backtrace_symbols_fd(bt, n, 2);
    if (sig != SIGUSR1) { signal(sig, SIG_DFL); raise(sig); }
}
void crash_install(void) {
    int sigs[] = {SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGILL, SIGUSR1};
    for (size_t i = 0; i < sizeof sigs / sizeof sigs[0]; i++) signal(sigs[i], crash_handler);
}
#elif defined(_WIN32)
#include <windows.h>
static LONG WINAPI win_crash(EXCEPTION_POINTERS *ep) {
    FILE *f = fopen("crash.log", "a");
    if (f) { fprintf(f, "\n=== UltimateCraft " UC_VERSION " crashed: exception 0x%08lx at %p (time %ld) ===\n", (unsigned long)ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress, (long)time(NULL)); fclose(f); }
    return EXCEPTION_CONTINUE_SEARCH;
}
void crash_install(void) { SetUnhandledExceptionFilter(win_crash); }
#else
void crash_install(void) {}
#endif
