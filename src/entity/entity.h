// Entities: player, mobs, dropped items, XP orbs, arrows/tridents, primed TNT, generic
// projectiles (snowballs, fireballs, potions ...), evoker fangs and end crystals share one pool,
// one physics routine, one status-effect system and one damage pipeline.
#ifndef ENTITY_H
#define ENTITY_H
#include "../core/common.h"
#include "../item/item.h"

#define MAX_ENTITIES 1024

typedef enum { ENT_NONE, ENT_PLAYER, ENT_MOB, ENT_ITEM, ENT_XP, ENT_ARROW, ENT_TNT, ENT_PROJ, ENT_FANGS, ENT_CRYSTAL, ENT_VEHICLE, ENT_DECOR, ENT_FALLING } EntityKind;
typedef enum { DMG_GENERIC, DMG_MELEE, DMG_ARROW, DMG_FALL, DMG_LAVA, DMG_FIRE, DMG_DROWN, DMG_EXPLOSION, DMG_VOID, DMG_STARVE,
               DMG_CACTUS, DMG_MAGIC, DMG_WITHER, DMG_POISON, DMG_FREEZE, DMG_SONIC, DMG_WALL, DMG_FIREBALL, DMG_SUFFOCATE, DMG_DRAGON, DMG_VACUUM, DMG_STAR, DMG_CRAMMING } DamageType;
typedef enum { AI_IDLE, AI_WANDER, AI_PANIC, AI_CHASE, AI_ATTACK, AI_FLEE, AI_SIT, AI_SPECIAL } AIState;

typedef struct Entity {
    u32 id;                 // 0 = free slot; low 10 bits = slot index, high bits = generation
    u8 kind, mob;
    vec3 pos, prev_pos, vel;
    f32 yaw, pitch, head_yaw, body_yaw, prev_body_yaw, prev_head_yaw, prev_pitch;
    f32 width, height;
    bool on_ground, in_water, in_lava, h_collision, no_gravity, remove, persistent, no_clip;
    f32 health, max_health, absorption;
    int hurt_time, invuln, death_time, fire, air, age;
    f32 fall_dist, slippery;
    // status effects (EffectId): remaining ticks and amplifier
    int eff_time[EFFECT_COUNT];
    u8 eff_amp[EFFECT_COUNT];
    // animation
    f32 limb_swing, limb_amount, prev_limb_amount, attack_anim;
    // AI
    u8 ai;
    int ai_timer, attack_cooldown, fuse, stuck_ticks, jump_cooldown, special_timer, anger;
    u32 target, owner_id, partner;
    vec3 goal, anchor;
    f32 move_speed;          // current desired speed (blocks / second)
    f32 swell, prev_swell;   // creeper fuse / bow aim / guardian laser charge visual
    int strafe_dir;
    // mob life cycle
    bool baby, tamed, sitting, sheared, saddled, from_spawner;
    int grow_age;            // ticks until a baby grows up (negative counts down)
    int love;                // breeding mode ticks
    int breed_cooldown;
    int egg_timer;           // chicken egg laying, sheep regrow ...
    u8 size;                 // slime/magma cube size 1/2/4
    u8 phase;                // boss / special ability phase
    // item / xp / projectile
    ItemStack item;
    int pickup_delay;
    int xp;
    u32 owner;
    f32 damage;
    bool stuck, from_player;
    u8 proj;                 // ProjectileType for ENT_PROJ
    u8 color;                // sheep wool colour, item bob seed, variant
    u8 prof, vlevel;         // villagers: profession (PROF_*), level 1..5
    u16 vxp;                 // villager experience
    ivec3 jobsite;           // villager workstation (y = -1000: none)
    u8 vtype;                // vehicles: VEH_* kind
    u32 rider, mount;        // riding: passenger on this entity / entity this one sits on
    u32 tags;                // /tag bits (names in g_tag_names)
    u32 raid_id;             // raiders: the raid they belong to (0 none); patrol members: 1 << 31
    u8 raid_role;            // 1 patrol / raid captain
    ItemStack equip[4];      // armour stands: head, chest, legs, feet
    u8 pose;                 // armour stands: pose 0..5
    s8 rep;                  // villagers: reputation of the player (gossip), -30..30
    u8 sleeping;             // villagers: asleep in a bed
} Entity;

typedef struct {
    const char *name, *key;
    u8 model;                // ModelId (generated)
    const char *texture;
    f32 width, height, eye;
    f32 health, speed, attack;
    int armor;
    f32 kb_resist;
    u8 ai;                   // MobAI (generated)
    u8 projectile;           // ProjectileType or 255
    u32 flags;               // MF_* (generated)
    f32 range;
    int attack_cd;
    int xp_min, xp_max;
    struct { u16 item; u8 min, max, chance; } drops[6];
    u8 sound;                // MobSoundKind
    f32 pitch;
    u16 food[7];             // breeding / taming / tempting items
    u8 effect, effect_amp;   // applied on melee hit
    u16 effect_secs;
    f32 scale;
} MobDef;
typedef struct { u8 biome, mob, cat; u16 weight; u8 min, max; } SpawnEntry;

extern const MobDef MOB_DEFS[MOB_COUNT];
extern const SpawnEntry SPAWNS[SPAWN_COUNT];
static inline bool mob_has(int type, u32 flag) { return (MOB_DEFS[type].flags & flag) != 0; }
bool mob_hostile(int type);          // counts toward the monster cap / despawns
int mob_by_key(const char *key);

typedef struct {
    Entity e[MAX_ENTITIES];
    u32 gen;
    int count_mobs_hostile, count_mobs_passive, count_water, count_ambient;
    u32 boss;                // id of the active boss (for the boss bar)
} EntityPool;
extern EntityPool g_ents;

// pool
Entity *entity_spawn(int kind, vec3 pos);
Entity *entity_get(u32 id);
void entities_clear(void);
#define ENT_FOREACH(var) for (Entity *var = g_ents.e; var < g_ents.e + MAX_ENTITIES; var++) if (var->id && !var->remove)

// constructors
Entity *mob_spawn(int type, vec3 pos);
Entity *item_drop(vec3 pos, ItemStack s, vec3 vel, int pickup_delay);
void item_drop_scatter(vec3 pos, ItemStack s);
void xp_drop(vec3 pos, int amount);
Entity *arrow_spawn(vec3 pos, vec3 vel, u32 owner, f32 damage, bool from_player);
Entity *proj_spawn(int type, vec3 pos, vec3 vel, u32 owner);
Entity *tnt_spawn(vec3 pos, int fuse);
Entity *falling_spawn(vec3 pos, u16 block);                  // a gravity block (sand, gravel, anvil...) in flight; its block id is kept in xp
Entity *falling_try_spawn(vec3 pos, u16 block);              // same, but NULL when no one is near enough to see it fall or the pool is full
Entity *fangs_spawn(vec3 pos, int delay, u32 owner);
Entity *crystal_spawn(vec3 pos);
Entity *vehicle_spawn(int type, vec3 pos, f32 yaw);   // vehicle.c

// simulation
void entities_tick(void);
void entity_physics(Entity *e);
bool entity_damage(Entity *e, f32 amount, int type, Entity *source);
void entity_heal(Entity *e, f32 amount);
void entity_knockback(Entity *e, vec3 from, f32 strength);
static inline AABB entity_aabb(const Entity *e) { return aabb_at(e->pos, e->width, e->height); }
static inline vec3 entity_eye(const Entity *e) { return v3_add(e->pos, v3(0, e->height * 0.85f, 0)); }
bool entity_holy_source(const Entity *src);   // gold, Smite, soul items, flaming arrows (hurt ghosts double)
f32 entity_light(const Entity *e);   // 0..1 brightness at the entity (for rendering)
bool entity_can_see(const Entity *a, const Entity *b);
void explosion(vec3 pos, f32 power, Entity *source);
void explosion_ex(vec3 pos, f32 power, Entity *source, bool fire, bool breaks_blocks);
bool entity_is_undead(const Entity *e);
// Space boss (Zorvath, the Cosmic Tyrant) – counters used by the test scripts
extern int g_tyrant_pulls, g_tyrant_summons, g_tyrant_meteors, g_keeper_slams;
bool mob_is_space_minion(int type);
bool sift_interact(Entity *m);
// sculk (src/world/sculk.c): vibrations, sensors, shriekers, wardens, catalysts
void sculk_vibration(vec3 pos, int freq, bool by_player);
void sculk_tick(void);
void sculk_on_mob_death(Entity *e);
// decorations (decor.c): item frames, glow item frames, paintings, armour stands
enum { DECOR_FRAME, DECOR_GLOW_FRAME, DECOR_PAINTING, DECOR_STAND, DECOR_COUNT };
#define PAINTING_COUNT 16
typedef struct { const char *name; u8 w, h, style; } PaintingDef;
extern const PaintingDef PAINTINGS[PAINTING_COUNT];
u32 painting_color(int k, f32 u, f32 v);
const char *decor_key(const Entity *e);
int decor_type_by_key(const char *k);
AABB decor_aabb(const Entity *e);
Entity *decor_spawn(int type, vec3 pos, int face, int variant);
Entity *decor_place_at(int type, ivec3 cell, int face, f32 yaw);
bool decor_place(Entity *pl, ItemStack *h);
bool decor_interact(Entity *e, ItemStack *h);
void decor_hit(Entity *e, Entity *by);
void decor_tick(Entity *e);
int decor_comparator(ivec3 p);   // -1: no frame there
int fishing_reel(Entity *player);
void effects_on_death(Entity *e);
void villager_hurt_by_player(Entity *v, bool killed);
extern int g_villager_sleeps, g_gossip_swaps;
extern int g_blocks_shot, g_cramming_hits, g_effect_deaths, g_phantoms_spawned, g_traders_spawned;   // returns rod durability to spend
extern int g_fish_caught, g_fish_junk, g_fish_treasure, g_fish_hooked;
extern int g_vibrations, g_sensor_triggers, g_shrieks, g_wardens_summoned, g_sculk_spread;   // talk to a soul or a Luminary (right-click / script 'talk')
extern int g_soul_talks, g_avatar_eruptions, g_holy_splashes, g_ghost_repels, g_revenant_novas, g_pearl_phases, g_hell_ambience;

// status effects
void effect_add(Entity *e, int effect, int amplifier, int ticks);
static inline bool effect_has(const Entity *e, int eff) { return e->eff_time[eff] > 0; }
static inline int effect_level(const Entity *e, int eff) { return e->eff_time[eff] > 0 ? e->eff_amp[eff] + 1 : 0; }
void effects_clear(Entity *e);
void effects_tick(Entity *e);
void potion_apply(Entity *e, int potion, f32 scale);   // drink / splash

// mob AI & spawning (mob.c)
void mob_tick(Entity *e);
void mobs_spawn_tick(void);
void mob_on_death(Entity *e, Entity *killer);
void mob_on_hurt(Entity *e, Entity *source);
void spawn_animals_for_column(int cx, int cz);
bool mob_interact(Entity *m, ItemStack *held);          // player right-clicks a mob
void mob_set_size(Entity *e, int size);
void golem_check_build(ivec3 p);                         // pumpkin / skull placed: build golems / wither
f32 mob_scale(const Entity *e);

// rendering (entity_render.c)
extern vec3 g_cam_pos, g_cam_right, g_cam_up, g_cam_fwd;
#include "../gfx/gfx.h"
void entities_render_init(void);
GLuint entity_items_texture(void);
GLuint entity_particles_texture(void);
void render_block_cube(mat4 m, u16 block, u32 color);
void render_item_sprite(mat4 m, int item, u32 color);
void render_item_icon_sprite(mat4 m, ItemStack s, u32 color);
void render_stack(mat4 m, ItemStack s, u32 color);
void entities_render(f32 alpha, const mat4 *vp, vec4 fog, f32 fog_near, f32 fog_far);
#endif
