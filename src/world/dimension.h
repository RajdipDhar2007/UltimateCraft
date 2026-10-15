// Dimension registry: one table describing every dimension (ids, names, save folder, height, physics).
// The Dimension enum itself is generated (src/gen/content.h); its indices are stored in level.txt,
// so new dimensions are only ever appended. Internally DIM_NETHER is Hell and DIM_END is Space.
#ifndef DIMENSION_H
#define DIMENSION_H
#include "../core/common.h"
#include "../gen/content.h"

#define SIFT_H 256

typedef struct {
    const char *id;           // primary command id ("hell")
    const char *aliases[3];   // accepted alternatives ("the_nether", "nether"), NULL padded
    const char *name;         // shown to players ("Hell")
    const char *the_name;     // in sentences ("Hell", "the Overworld")
    const char *folder;       // save sub-folder ("" = world root)
    int height;               // build height (internal y 0 .. height-1)
    int y_offset;             // internal y = displayed y + y_offset
    bool skylight;            // sky light propagates from the top
    bool has_day_cycle;       // sun, moon, weather, night spawning
    f32 gravity;              // multiplier on entity gravity
    f32 ambient;              // minimum entity brightness
} DimInfo;

extern const DimInfo DIM_INFO[DIM_COUNT];
int dim_by_id(const char *s);   // accepts ids, aliases and "minecraft:"/"ultimatecraft:" prefixes; -1 if unknown
static inline const DimInfo *dim_info(int d) { return &DIM_INFO[d >= 0 && d < DIM_COUNT ? d : 0]; }
#endif
