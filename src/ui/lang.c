// Localisation, gettext style: the English text is the key. res/lang/<code>.lang holds "English=Translation" lines
// (# comments, \n escapes). Missing keys fall back to English, so new UI text works before it is translated.
#include "lang.h"
#include "../core/platform.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const char *LANG_CODES[LANG_COUNT] = {"en_us", "de_de"};
const char *LANG_NAMES[LANG_COUNT] = {"English", "Deutsch"};
static char **keys, **vals; static int n, cap;
static int cur;
static unsigned hash_s(const char *s) { unsigned h = 2166136261u; while (*s) h = (h ^ (unsigned char)*s++) * 16777619u; return h; }
static int *tab; static int tabcap;
static char *unescape(const char *a, size_t len) {
    char *o = malloc(len + 1); size_t j = 0;
    for (size_t i = 0; i < len; i++) { if (a[i] == '\\' && i + 1 < len && a[i + 1] == 'n') { o[j++] = '\n'; i++; } else o[j++] = a[i]; }
    o[j] = 0; return o;
}
void lang_load(const char *code) {
    for (int i = 0; i < n; i++) { free(keys[i]); free(vals[i]); }
    n = 0; cur = 0;
    for (int i = 0; i < LANG_COUNT; i++) if (!strcmp(code, LANG_CODES[i])) cur = i;
    char path[128]; snprintf(path, sizeof path, "res/lang/%s.lang", LANG_CODES[cur]);
    size_t len; char *txt = plat_read_file(path, &len);
    if (txt) {
        for (char *l = txt, *nx; l && *l; l = nx) {
            nx = strchr(l, '\n'); if (nx) *nx++ = 0;
            size_t ll = strlen(l); if (ll && l[ll - 1] == '\r') l[--ll] = 0;
            if (!ll || l[0] == '#') continue;
            char *eq = strchr(l, '='); if (!eq) continue;
            if (n == cap) { cap = cap ? cap * 2 : 256; keys = realloc(keys, sizeof *keys * cap); vals = realloc(vals, sizeof *vals * cap); }
            keys[n] = unescape(l, (size_t)(eq - l)); vals[n] = unescape(eq + 1, strlen(eq + 1)); n++;
        }
        free(txt);
    }
    tabcap = 1; while (tabcap < n * 2 + 2) tabcap <<= 1;
    free(tab); tab = malloc(sizeof *tab * tabcap); memset(tab, 0xFF, sizeof *tab * tabcap);
    for (int i = 0; i < n; i++) { unsigned h = hash_s(keys[i]) & (tabcap - 1); while (tab[h] >= 0) h = (h + 1) & (tabcap - 1); tab[h] = i; }
}
const char *TR(const char *k) {
    if (!n || !k) return k;
    for (unsigned h = hash_s(k) & (tabcap - 1); tab[h] >= 0; h = (h + 1) & (tabcap - 1)) if (!strcmp(keys[tab[h]], k)) return vals[tab[h]];
    return k;
}
int lang_index(void) { return cur; }
