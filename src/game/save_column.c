// Column persistence. Only columns the player modified are written; untouched terrain is
// regenerated from the seed (deterministic world generation), which keeps saves tiny.
// Format v2: a palette of block-state names ("oak_stairs:5") followed by RLE palette indices,
// so saves survive changes to the generated block id numbering.
#include "../world/world.h"

#define COL_MAGIC 0x33434355u   // "UCC3" (384-block columns)

static void col_path(char *out, size_t n, const char *dir, int cx, int cz) {
    snprintf(out, n, "%.300s/columns/c.%d.%d.bin", dir, cx, cz);
}

bool save_write_column(const char *dir, Column *c) {
    static __thread u16 pal_of[BLOCK_STATES];
    u16 palette[1024]; int np = 0;
    memset(pal_of, 0xFF, sizeof pal_of);
    const u16 *b = &c->blocks[0][0][0];
    size_t total = sizeof c->blocks / sizeof(u16);
    for (size_t i = 0; i < total; i++) if (pal_of[b[i]] == 0xFFFF && np < 1024) { pal_of[b[i]] = (u16)np; palette[np++] = b[i]; }
    size_t cap = 16 + np * 72 + total * 4 + 256, n = 0;
    u8 *buf = xmalloc(cap);
    u32 magic = COL_MAGIC;
    memcpy(buf + n, &magic, 4); n += 4;
    memcpy(buf + n, &c->cx, 4); n += 4;
    memcpy(buf + n, &c->cz, 4); n += 4;
    u16 npal = (u16)np; memcpy(buf + n, &npal, 2); n += 2;
    for (int i = 0; i < np; i++) {
        char name[96];
        const BlockDef *d = &BLOCKS[palette[i]];
        int len = snprintf(name, sizeof name, "%s:%d", d->key, d->var);
        buf[n++] = (u8)len; memcpy(buf + n, name, len); n += len;
    }
    for (size_t i = 0; i < total;) {
        size_t run = 1;
        while (i + run < total && run < 65535 && b[i + run] == b[i]) run++;
        u16 r16 = (u16)run, v = pal_of[b[i]];
        memcpy(buf + n, &r16, 2); n += 2; memcpy(buf + n, &v, 2); n += 2;
        i += run;
    }
    memcpy(buf + n, c->biome, 256); n += 256;
    char path[400];
    col_path(path, sizeof path, dir, c->cx, c->cz);
    bool ok = plat_write_file(path, buf, n);
    free(buf);
    if (!ok) LOG_WARN("could not save column %d,%d", c->cx, c->cz);
    return ok;
}

bool save_read_column(const char *dir, Column *c) {
    char path[400];
    col_path(path, sizeof path, dir, c->cx, c->cz);
    size_t len;
    u8 *buf = (u8 *)plat_read_file(path, &len);
    if (!buf) return false;
    bool ok = false;
    u32 magic; int cx, cz;
    if (len >= 14 + 256) {
        memcpy(&magic, buf, 4); memcpy(&cx, buf + 4, 4); memcpy(&cz, buf + 8, 4);
        if (magic == COL_MAGIC && cx == c->cx && cz == c->cz) {
            size_t i = 12;
            u16 np; memcpy(&np, buf + i, 2); i += 2;
            u16 *pal = xmalloc(sizeof(u16) * (np + 1));
            for (int k = 0; k < np && i < len; k++) {
                int l = buf[i++];
                char name[256]; memcpy(name, buf + i, l); name[l] = 0; i += l;
                char *colon = strrchr(name, ':'); int var = 0;
                if (colon) { var = atoi(colon + 1); *colon = 0; }
                Block base = block_by_key(name);
                pal[k] = base ? block_with_var(base, var) : (strcmp(name, "air") ? B_STONE : B_AIR);
            }
            u16 *b = &c->blocks[0][0][0];
            size_t total = sizeof c->blocks / sizeof(u16), o = 0;
            while (o < total && i + 4 <= len - 256) {
                u16 run, v; memcpy(&run, buf + i, 2); memcpy(&v, buf + i + 2, 2); i += 4;
                u16 blk = v < np ? pal[v] : B_STONE;
                for (size_t k = 0; k < run && o < total; k++) b[o++] = blk;
            }
            if (o == total && i + 256 <= len) { memcpy(c->biome, buf + i, 256); ok = true; }
            free(pal);
        }
    }
    free(buf);
    if (ok) { c->state = COL_GENERATED; c->modified = false; c->spawned_animals = true; }
    else LOG_WARN("column file %s is unreadable or from an older version; regenerating", path);
    return ok;
}
