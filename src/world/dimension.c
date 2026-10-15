#include "dimension.h"
#include <string.h>

const DimInfo DIM_INFO[DIM_COUNT] = {
    [DIM_OVERWORLD] = {"overworld", {"minecraft:overworld", NULL}, "Overworld", "the Overworld", "", 384, 64, true, true, 1.0f, 0.06f},
    [DIM_NETHER] = {"hell", {"the_nether", "nether", NULL}, "Hell", "Hell", "DIM-1", 128, 0, false, false, 1.0f, 0.40f},
    [DIM_END] = {"space", {"the_end", "end", NULL}, "Space", "Space", "DIM1", 256, 0, true, false, 0.4f, 0.45f},
    [DIM_SIFT] = {"sift", {"heaven", NULL}, "Sift", "the Sift", "DIM2", SIFT_H, 0, true, false, 1.0f, 0.55f},
};

int dim_by_id(const char *s) {
    if (!s) return -1;
    const char *c = strchr(s, ':');
    if (c && (!strncmp(s, "minecraft:", 10) || !strncmp(s, "ultimatecraft:", 14))) s = c + 1;
    for (int d = 0; d < DIM_COUNT; d++) {
        if (!strcmp(s, DIM_INFO[d].id)) return d;
        for (int a = 0; a < 3 && DIM_INFO[d].aliases[a]; a++) {
            const char *al = DIM_INFO[d].aliases[a];
            const char *ac = strchr(al, ':');
            if (!strcmp(s, ac ? ac + 1 : al)) return d;
        }
    }
    return -1;
}
