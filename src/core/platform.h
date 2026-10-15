// Thin platform layer: the ONLY place with OS specific code (time, sleep, files, threads).
#ifndef PLATFORM_H
#define PLATFORM_H
#include "common.h"
#include <pthread.h>

f64 plat_time(void);                 // seconds, monotonic
static inline f64 plat_time_wrap(void) { return plat_time(); }
void plat_sleep_ms(int ms);
bool plat_mkdir(const char *path);   // creates one level; true if it exists afterwards
bool plat_mkdirs(const char *path);  // creates all levels
bool plat_exists(const char *path);
bool plat_is_dir(const char *path);
bool plat_remove_tree(const char *path);
int plat_list_dirs(const char *path, char out[][64], int max);
char *plat_read_file(const char *path, size_t *len);                  // NUL terminated, caller frees
bool plat_write_file(const char *path, const void *data, size_t len); // atomic (tmp + fsync + rename)
typedef struct { char *d; size_t n, cap; } StrBuf;   // text built in memory, written atomically
void sbuf_printf(StrBuf *b, const char *fmt, ...);
bool sbuf_write(StrBuf *b, const char *path);         // frees the buffer
long long plat_copy_dir(const char *src, const char *dst);   // recursive copy
int plat_fuzz_copy(const char *src, const char *dst, unsigned long long seed);   // tests only
long long plat_backup_dir(const char *src, const char *backups, int keep, long long max_bytes);   // rotating copies; -1 = too big
int plat_cpu_count(void);

typedef pthread_mutex_t Mutex;
static inline void mutex_init(Mutex *m) { pthread_mutex_init(m, NULL); }
static inline void mutex_lock(Mutex *m) { pthread_mutex_lock(m); }
static inline void mutex_unlock(Mutex *m) { pthread_mutex_unlock(m); }
static inline void mutex_destroy(Mutex *m) { pthread_mutex_destroy(m); }
#endif
