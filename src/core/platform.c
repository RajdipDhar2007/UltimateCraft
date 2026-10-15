#include "platform.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define RMDIR(p) _rmdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)
#define RMDIR(p) rmdir(p)
#endif

Rng g_rng = {0x1234567ULL};

void log_msg(const char *level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[%s] ", level);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    fflush(stderr);
}

void *xmalloc(size_t n) { void *p = malloc(n ? n : 1); if (!p) { LOG_ERR("out of memory (%zu bytes)", n); exit(1); } return p; }
void *xcalloc(size_t n, size_t s) { void *p = calloc(n ? n : 1, s ? s : 1); if (!p) { LOG_ERR("out of memory"); exit(1); } return p; }
void *xrealloc(void *p, size_t n) { void *q = realloc(p, n ? n : 1); if (!q) { LOG_ERR("out of memory"); exit(1); } return q; }

f64 plat_time(void) { return glfwGetTime(); }

void plat_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

bool plat_is_dir(const char *path) { struct stat st; return stat(path, &st) == 0 && S_ISDIR(st.st_mode); }
bool plat_exists(const char *path) { struct stat st; return stat(path, &st) == 0; }
bool plat_mkdir(const char *path) { if (plat_is_dir(path)) return true; MKDIR(path); return plat_is_dir(path); }

bool plat_mkdirs(const char *path) {
    char buf[512];
    snprintf(buf, sizeof buf, "%s", path);
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/' || *p == '\\') { char c = *p; *p = 0; plat_mkdir(buf); *p = c; }
    }
    return plat_mkdir(buf);
}

bool plat_remove_tree(const char *path) {
    DIR *d = opendir(path);
    if (!d) return remove(path) == 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char sub[600];
        snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
        if (plat_is_dir(sub)) plat_remove_tree(sub); else remove(sub);
    }
    closedir(d);
    return RMDIR(path) == 0;
}

int plat_list_dirs(const char *path, char out[][64], int max) {
    DIR *d = opendir(path);
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] == '.') continue;
        char sub[600];
        snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
        if (!plat_is_dir(sub)) continue;
        snprintf(out[n++], 64, "%.63s", e->d_name);
    }
    closedir(d);
    return n;
}

int g_plat_recovered;   // files recovered from <file>.tmp (crash between write and rename)
char *plat_read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    if (!f) {   // a crash between writing "<file>.tmp" and renaming it: recover the finished temp file
        char tmp[600]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
        f = fopen(tmp, "rb");
        if (f) { LOG_WARN("recovered %s from its temporary file", path); g_plat_recovered++; }
    }
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = xmalloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    if (len) *len = got;
    return buf;
}

bool plat_write_file(const char *path, const void *data, size_t len) {
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, len, f) == len;
    ok = fflush(f) == 0 && ok;
#ifndef _WIN32
    ok = fsync(fileno(f)) == 0 && ok;   // on disk before it replaces the old file
#endif
    ok = (fclose(f) == 0) && ok;
    if (!ok) { remove(tmp); return false; }
#ifdef _WIN32
    return MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;   // atomic replace
#else
    return rename(tmp, path) == 0;   // atomic: the old file stays intact until the new one is complete
#endif
}

// ---- small text builder for files written through plat_write_file
void sbuf_printf(StrBuf *b, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    va_list ap2; va_copy(ap2, ap);
    int need = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (need < 0) { va_end(ap2); return; }
    if (b->n + (size_t)need + 1 > b->cap) { b->cap = (b->n + (size_t)need + 1) * 2; b->d = xrealloc(b->d, b->cap); }
    vsnprintf(b->d + b->n, b->cap - b->n, fmt, ap2); va_end(ap2);
    b->n += (size_t)need;
}
bool sbuf_write(StrBuf *b, const char *path) { bool ok = plat_write_file(path, b->d ? b->d : "", b->n); free(b->d); *b = (StrBuf){0}; return ok; }

// ---- recursive directory copy (world backups); returns bytes copied, -1 if over the limit
static long long dir_size(const char *dir, const char *skip) {
    DIR *d = opendir(dir); if (!d) return 0;
    long long total = 0; struct dirent *e; char p[700];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || (skip && !strcmp(e->d_name, skip))) continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st; if (stat(p, &st)) continue;
        total += S_ISDIR(st.st_mode) ? dir_size(p, NULL) : (long long)st.st_size;
    }
    closedir(d); return total;
}
static long long copy_tree(const char *src, const char *dst, const char *skip) {
    DIR *d = opendir(src); if (!d) return 0;
    plat_mkdir(dst);
    long long total = 0; struct dirent *e; char a[700], b[700];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..") || (skip && !strcmp(e->d_name, skip))) continue;
        snprintf(a, sizeof a, "%s/%s", src, e->d_name); snprintf(b, sizeof b, "%s/%s", dst, e->d_name);
        struct stat st; if (stat(a, &st)) continue;
        if (S_ISDIR(st.st_mode)) { total += copy_tree(a, b, NULL); continue; }
        size_t len; char *data = plat_read_file(a, &len);
        if (data) { plat_write_file(b, data, len); total += (long long)len; free(data); }
    }
    closedir(d); return total;
}
static void remove_tree(const char *dir) {
    DIR *d = opendir(dir); if (!d) return;
    struct dirent *e; char p[700];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
        struct stat st; if (stat(p, &st)) continue;
        if (S_ISDIR(st.st_mode)) remove_tree(p); else remove(p);
    }
    closedir(d);
#ifdef _WIN32
    _rmdir(dir);
#else
    rmdir(dir);
#endif
}
// tests: copy a directory tree, flipping random bytes (about 1 in 400, deterministic) in every file
int plat_fuzz_copy(const char *src, const char *dst, unsigned long long seed) {
    DIR *d = opendir(src); if (!d) return 0;
    plat_mkdir(dst);
    int files = 0; struct dirent *e; char a[700], b[700];
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        snprintf(a, sizeof a, "%s/%s", src, e->d_name); snprintf(b, sizeof b, "%s/%s", dst, e->d_name);
        struct stat st; if (stat(a, &st)) continue;
        if (S_ISDIR(st.st_mode)) { files += plat_fuzz_copy(a, b, seed * 31 + 7); continue; }
        size_t len; char *data = plat_read_file(a, &len); if (!data) continue;
        unsigned long long x = seed ^ (unsigned long long)len * 0x9E3779B97F4A7C15ull;
        for (size_t i = 0; i < len; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; if (x % 400 == 0) data[i] = (char)(x >> 24); }
        if (len > 8 && (seed & 1)) len = len * (2 + seed % 5) / 8;   // and sometimes truncate
        plat_write_file(b, data, len); free(data); files++;
    }
    closedir(d); return files;
}

long long plat_copy_dir(const char *src, const char *dst) { return copy_tree(src, dst, NULL); }
// keeps the last `keep` copies: <backups>/1 is the newest
long long plat_backup_dir(const char *src, const char *backups, int keep, long long max_bytes) {
    long long size = dir_size(src, NULL);
    if (size > max_bytes) return -1;
    plat_mkdir(backups);
    char a[700], b[700];
    snprintf(a, sizeof a, "%s/%d", backups, keep); remove_tree(a);
    for (int i = keep - 1; i >= 1; i--) { snprintf(a, sizeof a, "%s/%d", backups, i); snprintf(b, sizeof b, "%s/%d", backups, i + 1); rename(a, b); }
    snprintf(a, sizeof a, "%s/1", backups);
    return copy_tree(src, a, NULL);
}

int plat_cpu_count(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (int)si.dwNumberOfProcessors;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 2;
#endif
}
