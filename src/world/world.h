// The voxel world: fixed-height columns, background generation, lighting, meshing and rendering.
#ifndef WORLD_H_INCLUDED
#define WORLD_H_INCLUDED
#include "../core/common.h"
#include "../core/platform.h"
#include "../gfx/gfx.h"
#include "../item/item.h"
#include "block.h"
#include "dimension.h"

#define CHUNK_W 16
// Minecraft's build range: the Overworld spans y -64 .. 319 (384 blocks). Columns are stored with
// internal y 0 .. 383; in the Overworld internal y = Minecraft y + 64. The Nether (0 .. 127, bedrock
// roof at 127) and the End (0 .. 255) use internal y = Minecraft y.
#define WORLD_H 384
#define SECTIONS (WORLD_H / 16)
#define OW_Y_OFFSET 64
#define MCY(y) ((y) + OW_Y_OFFSET)          // Overworld Minecraft y -> internal y
#define SEA_LEVEL MCY(63)
#define NETHER_H 128
#define NETHER_LAVA_LEVEL 31
#define END_H 256
#define COL_SLOTS 64           // ring-buffer of column slots per axis (render distance must stay < 30)
#define MAX_RENDER_DIST 16
#define DAY_TICKS (20 * 60 * TICKRATE)   // 20 minute day like Minecraft

// light word: 4 bits sky | 4 bits red | 4 bits green | 4 bits blue
#define L_SKY(l) (((l) >> 12) & 15)
#define L_R(l) (((l) >> 8) & 15)
#define L_G(l) (((l) >> 4) & 15)
#define L_B(l) ((l) & 15)
#define L_CH(l, c) (((l) >> (12 - 4 * (c))) & 15)   // channel 0 = sky, 1 = r, 2 = g, 3 = b

enum { COL_QUEUED = 0, COL_GENERATED, COL_READY };
typedef struct {
    const char *name, *key;
    u8 dim;
    f32 temperature, downfall;
    u8 precip;                    // 0 none, 1 rain, 2 snow
    u32 grass, foliage, water, sky, fog;
    u16 top, filler, under;       // surface block, sub-surface, underwater floor
    u8 trees[8];                  // TreeKind choices (TREE_NONE padded)
    f32 tree_density, grass_density, flower_density;
    u8 nflowers;
    u16 flowers[16];
    u64 feats;                    // 1 << FEAT_*
    u8 category;                  // 0 land 1 ocean 2 river 3 beach 4 cave 5 mushroom
} BiomeDef;
extern const BiomeDef BIOMES[BIOME_COUNT];
static inline bool biome_has(int b, int feat) { return (BIOMES[b].feats >> feat) & 1; }
int biome_by_key(const char *key);

typedef struct {
    GLuint vao, vbo;
    int n_opaque, n_trans;     // quad counts
    bool dirty, built;
} SectionMesh;

#define MAX_MARKERS 64
typedef struct { u8 lx, lz, kind; u16 y, arg; } StructMarker;   // structure chest / spawner / mob to create when ready
typedef struct Column {
    int cx, cz;
    volatile int state;
    volatile bool cancelled;
    bool modified;             // changed by the player since generation -> must be saved
    bool spawned_animals;      // initial passive mobs placed
    u16 blocks[WORLD_H][16][16];   // [y][z][x] block states
    u16 light[WORLD_H][16][16];
    u16 heightmap[16][16];          // first y above the highest light-blocking block
    u8 biome[16][16];
    SectionMesh sec[SECTIONS];
    struct Column *next_job;
    StructMarker markers[MAX_MARKERS];
    u8 nmarkers;
} Column;

typedef struct {
    ivec3 pos;
    u8 type;                   // BlockEntityType (generated enum)
    ItemStack slots[27];       // chest: 27, furnace-likes: 0 input 1 fuel 2 output, brewing: 0-2 bottles 3 ingredient 4 fuel
    int burn_time, burn_max, cook_time;
    f32 xp;                    // furnace: stored experience
    int data;                  // jukebox disc, brewing time ...
} BlockEntity;

typedef struct { ivec3 pos; u64 due; } ScheduledUpdate;

typedef struct {
    bool active;
    bool dragon_killed;
    bool tyrant_killed;        // Space: Zorvath defeated (Sift gateway open)
    ivec3 sift_gate;           // Space: where the Sift gateway was built (y 0 = none)
    u8 sift_trials;            // the Sift: trials passed (1 Dawn / Solenne, 2 Winds / Zephyrine, 4 Stone / Thalor)
    bool ascended;             // all three trials passed: the ending has been seen
    int warden_warning;        // sculk shrieker warning level (0..4; 4 summons a warden)
    bool old_height_save;      // loaded from a pre-v4 save: put the player back on the surface        // the End: exit portal open, no dragon respawn
    int dim;                   // Dimension: overworld / hell / space / sift (one dimension is loaded at a time)
    ivec3 sift_return;         // Overworld position of the Sift portal last used (where the Sift sends you back)
    u64 seed;
    char name[64];
    char dir[256];             // saves/<world>        (player, level data)
    char dim_dir[300];         // dimension folder (columns, entities)
    u64 time;                  // world ticks (game time)
    s64 day_shift;             // added to time for the time of day (doDaylightCycle false / /time set)
    ivec3 spawn;
    int sim_dist, sim_cx, sim_cz;   // simulation distance (chunks) and the player's chunk: ticking stops beyond it
    int render_dist;
    int center_cx, center_cz;
    Column *slots[COL_SLOTS * COL_SLOTS];
    int loaded;
    // worker threads
    pthread_t workers[8];
    int nworkers;
    Mutex lock;
    Column *job_head, *job_tail, *done_head;
    int jobs_pending;
    volatile bool quit;
    // block entities
    BlockEntity *bents;
    int nbents, cap_bents;
    // scheduled block updates
    ScheduledUpdate *updates;
    int nupdates, cap_updates;
    // rendering
    Shader chunk_shader;
    Texture atlas;
    int sections_drawn, sections_meshed_total;
} World;

extern World g_world;
static inline u64 world_daytime(void) { return g_world.time + (u64)g_world.day_shift; }
// Minecraft-facing y (F3 screen, commands): internal y minus the dimension's offset
static inline int world_y_offset(void) { return dim_info(g_world.dim)->y_offset; }
static inline int world_min_y(void) { return -world_y_offset(); }
static inline int world_build_top(void) { return dim_info(g_world.dim)->height - 1; }

// lifecycle
void world_gfx_init(void);
bool world_open(const char *name, u64 seed, bool create, int dim);
void world_close(void);   // saves everything and frees all columns
extern bool g_world_sync;   // scripted runs: deterministic column loading and meshing
void world_update(vec3 player_pos, f64 mesh_budget_ms);  // per frame: load/unload/light/mesh
void world_tick(void);    // per game tick: scheduled + random block updates
void world_save_all(void);
bool world_area_ready(vec3 pos, int radius_cols);

// columns
static inline Column *world_column(int cx, int cz) {
    Column *c = g_world.slots[(cz & (COL_SLOTS - 1)) * COL_SLOTS + (cx & (COL_SLOTS - 1))];
    return (c && c->cx == cx && c->cz == cz && c->state == COL_READY) ? c : NULL;
}
static inline Column *world_column_at(int x, int z) { return world_column(x >> 4, z >> 4); }

// blocks
static inline u16 world_get_block(int x, int y, int z) {
    if (y < 0 || y >= WORLD_H) return B_AIR;
    Column *c = world_column_at(x, z);
    return c ? c->blocks[y][z & 15][x & 15] : B_AIR;
}
static inline u16 world_get_block_or(int x, int y, int z, u16 fallback) {
    if (y < 0) return B_BEDROCK;
    if (y >= WORLD_H) return B_AIR;
    Column *c = world_column_at(x, z);
    return c ? c->blocks[y][z & 15][x & 15] : fallback;
}
static inline u16 world_get_light(int x, int y, int z) {
    if (y >= WORLD_H) return dim_info(g_world.dim)->skylight ? 0xF000 : 0;
    if (y < 0) return 0;
    Column *c = world_column_at(x, z);
    return c ? c->light[y][z & 15][x & 15] : (dim_info(g_world.dim)->skylight ? 0xF000 : 0);
}
static inline bool world_loaded(int x, int z) { return world_column_at(x, z) != NULL; }
bool world_set_block(int x, int y, int z, u16 block);
void world_set_block_silent(int x, int y, int z, u16 block); // no neighbour updates (used by worldgen tree growth)
int world_biome_at(int x, int y, int z);
bool world_block_supported(ivec3 p, u16 b);   // support rules (plants, torches, doors ...)
bool world_grow(ivec3 p, Rng *r, bool bonemeal); // crops/saplings/stems/canes; true if anything happened
int block_connect_mask(int x, int y, int z, u16 b); // fence/wall/pane arms: bit k = N,E,S,W   // biome id (cave biomes below the surface)
bool world_has_skylight(void);
void world_schedule_update(ivec3 p, int delay_ticks);
int world_top_y(int x, int z);   // y of the highest solid block, -1 if none
f32 world_daylight(void);        // 0.18 .. 1.0 sky brightness factor
bool world_is_night(void);
int world_light_level(ivec3 p);  // combined Minecraft-style light level 0..15 (current daylight applied)
void world_mark_dirty(int x, int y, int z);

// block entities
BlockEntity *world_block_entity(ivec3 p, bool create, int type);
void world_remove_block_entity(ivec3 p);

// raycast
typedef struct { bool hit; ivec3 pos, prev; int face; f32 dist; u16 block; vec3 point; } RayHit;
RayHit world_raycast(vec3 origin, vec3 dir, f32 max_dist, bool hit_liquids);
bool world_block_selection(ivec3 p, u16 b, AABB *out);   // false if not targetable

// collision helper: fills up to max boxes of solid blocks intersecting area
int world_collect_boxes(AABB area, AABB *out, int max);

// rendering
void world_render(const mat4 *vp, vec3 cam, const Frustum *fr, f32 daylight, vec4 fog_color, f32 fog_near, f32 fog_far, int pass);

// hooks implemented by the game layer
void game_hook_block_broken_naturally(ivec3 p, u16 block);   // e.g. torch losing support -> drop item
void game_hook_column_ready(Column *c);
void game_hook_tnt_ignite(ivec3 p);                         // fire / lightning lit a TNT block                     // first time a column becomes playable

// lighting (light.c)
void light_init_column(Column *c);
void light_block_changed(int x, int y, int z);

// meshing (mesher.c)
void mesher_init(void);
void mesh_section(Column *c, int s);

// world generation (worldgen.c), thread-safe
void worldgen_column(Column *c, u64 seed);
int worldgen_surface(u64 seed, int x, int z, int *biome);
ivec3 worldgen_find_spawn(u64 seed);
bool worldgen_grow_tree(int x, int y, int z, int kind, Rng *rng);   // kind = TreeKind; false if no room
int worldgen_biome(u64 seed, int dim, int x, int z);                // surface biome
int worldgen_cave_biome(u64 seed, int x, int y, int z);              // -1 if none
ivec3 worldgen_stronghold(u64 seed);
int worldgen_sift_surface(u64 seed, int x, int z);   // top of the Sift island at x,z or -1
void worldgen_end_column(u64 seed, int x, int z, int *lo, int *hi); // End island surface range (lo > hi: void)                                 // portal room of the nearest stronghold
void worldgen_set_dim(int dim);

// redstone (redstone.c)
void redstone_reset(void);
void redstone_update(ivec3 p);           // block update for components / mechanisms
void redstone_tick(void);                // hoppers, daylight detectors
bool rs_is_component(Block b);
int rs_input_power(ivec3 p);
bool tripwire_trip(ivec3 p);
void cmdblock_redstone(ivec3 p, bool powered);   // commands_ext.inc             // tripwire.c: trip the string at p (true if a hooked line)
void tripwire_tick(void);
extern int g_tripwire_trips;             // strongest power reaching a mechanism at p
bool rs_powered(ivec3 p);
int rs_wire_mask(int x, int y, int z);   // bits 0..3 connect N/E/S/W, 4..7 climb in that direction
int rs_wire_mask_with(u16 (*get)(int, int, int), int x, int y, int z);
int rs_container_signal(ivec3 p);        // comparator reading of a container (-1 = not a container)
void rs_toggle_lever(ivec3 p);
void rs_press_button(ivec3 p);
void rs_hit_target(ivec3 p, bool arrow);
struct Entity;
void rs_entity_on(ivec3 p, struct Entity *e);   // pressure plates / detector rails
void rs_block_changed(ivec3 p);          // observers
void rs_scan_column(Column *c);
int rs_rail_shape(Block b);              // 0 N-S, 1 E-W, 2..5 ascending E/W/N/S, 6..9 curves SE/SW/NW/NE
void rs_rail_ends(int shape, int out[2], bool up[2]);
void rs_rail_placed(ivec3 p, int look_h4);

// fluids (fluid.c)
void fluid_update(ivec3 p);
bool fluid_due_now(ivec3 p, int delay);   // (redstone.c) delayed fluid ticks
void fluid_arm(ivec3 p, int delay);
vec3 fluid_flow(int x, int y, int z);    // horizontal current of the fluid at a cell
f32 fluid_height(Block b);               // surface height of a fluid cell (0..1)

// persistence hooks (game/save.c)
bool save_read_column(const char *world_dir, Column *c);   // called from worker threads
bool save_write_column(const char *world_dir, Column *c);

#endif
