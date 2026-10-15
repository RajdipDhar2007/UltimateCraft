// The player: controller, survival stats (health, hunger, air, XP), inventory and block/entity interaction.
#ifndef PLAYER_H
#define PLAYER_H
#include "../entity/entity.h"
#include "../world/world.h"

#define PLAYER_WIDTH 0.6f
#define PLAYER_HEIGHT 1.8f
#define PLAYER_EYE 1.62f
#define PLAYER_REACH 4.5f
#define PLAYER_ATTACK_REACH 3.2f

typedef enum { GM_SURVIVAL, GM_CREATIVE, GM_ADVENTURE, GM_SPECTATOR } GameMode;   // Minecraft numbering
static inline bool gm_survival_like(int gm) { return gm == GM_SURVIVAL || gm == GM_ADVENTURE; }

typedef struct {
    u32 eid;
    Inventory inv;
    ItemStack cursor;            // stack held by the mouse in inventory screens
    ItemStack craft[9];
    ItemStack ender[27];         // ender chest contents (shared by every ender chest)          // 2x2 (inventory) or 3x3 (crafting table) grid
    int food;
    f32 saturation, exhaustion;
    int xp_level, xp_total;
    f32 xp_progress;
    int gamemode;
    bool flying, sneaking, sprinting;
    f32 eye_offset;              // smoothed sneak camera offset
    // interaction
    RayHit look;
    u32 look_entity;
    bool mining;
    ivec3 mine_pos;
    f32 mine_progress;
    int mine_sound_timer, place_cooldown, attack_ticks;
    f32 swing, prev_swing;       // hand swing animation 0..1
    int eating_ticks, bow_ticks;
    // survival timers
    int regen_timer, starve_timer, drown_timer, step_dist_timer, void_timer, lava_timer;
    f32 step_acc;
    // camera feel
    f32 bob, prev_bob, bob_amount, prev_bob_amount, hurt_tilt, fov_kick;
    ivec3 spawn;
    bool dead;
    int death_ticks;
    char death_msg[96];
    int score, kills;
    int last_space_tap;
    int rest_ticks;              // ticks since last sleep (phantoms)
    int portal_ticks, portal_cooldown;   // standing in a nether portal / after a dimension change
    bool sleeping, gliding, blocking, using_spyglass;
    int use_ticks;               // generic hold-to-use (shield, crossbow charge, drinking)
    int ender_pearl_cd;
    bool crossbow_loaded, dim_changed;
    ItemStack crossbow_ammo;
    u32 ench_seed;
    f32 ride_fwd, ride_strafe;        // riding input this tick
    bool ride_jump;                    // enchanting table offers (re-rolled after each enchant)
    u32 bobber;                  // fishing bobber entity id (0 = none)
    u8 recipes_known[(RECIPE_COUNT + 7) / 8];   // recipe book unlocks          // double-tap space to fly in creative
} Player;

extern Player g_player;
Entity *player_ent(void);
void player_reset(vec3 pos, bool new_world);
void player_tick(void);                       // movement & survival (game tick)
void player_frame(f32 dt);                    // mouse look & interaction (per frame)
void player_add_xp(int amount);
void player_spend_levels(int n);
int xp_for_level(int level);
void player_respawn(void);
void player_on_death(const char *msg);
void player_exhaust(f32 amount);
bool player_give(ItemStack s);                // inventory or drop at feet
void player_drop_selected(bool whole_stack);
f32 player_attack_strength(void);             // 0..1 cooldown indicator
f32 player_mine_speed(u16 block);
void player_hurt_feedback(f32 amount);
bool player_place_state(Entity *e, u16 kind, RayHit look);   // place a block kind using Minecraft orientation rules
bool try_light_nether_portal(ivec3 p);
bool try_light_sift_portal(ivec3 p);   // gold blocks / glowstone frame, lit with the Sift Key
#endif
