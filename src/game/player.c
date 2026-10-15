#include "player.h"
#include "weather.h"
#include "../world/rules.h"
#include "maps.h"
#include "../ui/screens.h"
#include "../entity/vehicle.h"
#include "game.h"
#include "fx.h"
#include "../audio/audio.h"

Player g_player;

// movement tuning (blocks / second, Minecraft-like)
#define WALK_SPEED 4.317f
#define SPRINT_SPEED 5.612f
#define SNEAK_SPEED 1.31f
#define FLY_SPEED 10.9f
#define JUMP_VELOCITY 8.95f

Entity *player_ent(void) { return entity_get(g_player.eid); }

void player_spend_levels(int n) {
    if (g_player.gamemode == GM_CREATIVE) return;
    g_player.xp_level = MAXI(0, g_player.xp_level - n);
    g_player.ench_seed = (u32)hash64(g_player.ench_seed + 1234567ULL);
}
int xp_for_level(int l) { return l >= 30 ? 9 * l - 158 : l >= 15 ? 5 * l - 38 : 2 * l + 7; }

void player_reset(vec3 pos, bool new_world) {
    Entity *e = player_ent();
    if (!e) {
        e = entity_spawn(ENT_PLAYER, pos);
        g_player.eid = e->id;
    }
    e->pos = e->prev_pos = pos;
    e->vel = v3(0, 0, 0);
    e->width = PLAYER_WIDTH; e->height = PLAYER_HEIGHT;
    e->health = e->max_health = 20;
    e->air = 300 * 3; e->fire = 0; e->fall_dist = 0; e->hurt_time = 0; e->invuln = 60;
    g_player.food = 20; g_player.saturation = 5; g_player.exhaustion = 0;
    g_player.dead = false; g_player.death_ticks = 0;
    g_player.mining = false; g_player.mine_progress = 0;
    g_player.eating_ticks = g_player.bow_ticks = g_player.use_ticks = 0;
    g_player.gliding = g_player.blocking = g_player.sleeping = false;
    effects_clear(e);
    g_player.eye_offset = 0;
    if (new_world) {
        memset(&g_player.inv, 0, sizeof g_player.inv);
        g_player.xp_level = g_player.xp_total = 0; g_player.xp_progress = 0;
        g_player.score = g_player.kills = 0;
        e->yaw = 0; e->pitch = 0;
    }
}

void player_add_xp(int n) {
    if (n <= 0) return;
    g_player.score += n;
    g_player.xp_total += n;
    g_player.xp_progress += (f32)n / xp_for_level(g_player.xp_level);
    while (g_player.xp_progress >= 1.0f) {
        g_player.xp_progress = (g_player.xp_progress - 1.0f) * xp_for_level(g_player.xp_level) / xp_for_level(g_player.xp_level + 1);
        g_player.xp_level++;
        sound_play(SND_LEVELUP, 0.6f, 1.0f);
        game_message(rgba_u32(128, 255, 32, 255), "Level up! You are now level %d", g_player.xp_level);
    }
}

void player_exhaust(f32 a) { if (gm_survival_like(g_player.gamemode) && g_level.difficulty != DIFF_PEACEFUL) g_player.exhaustion += a; }

// recipe book: picking up an item unlocks every recipe that uses it (like Minecraft)
int g_recipes_unlocked;
static void unlock_recipes_for(int id) {
    int n = 0;
    for (int r = 0; r < RECIPE_COUNT; r++) {
        if (g_player.recipes_known[r / 8] & (1 << (r % 8))) continue;
        bool uses = false;
        for (int k = 0; k < 9 && !uses; k++) {
            int ing = RECIPES[r].ing[k]; if (!ing) continue;
            for (int a = 0; a < INGREDIENTS[ing].n; a++) if (ING_ITEMS[INGREDIENTS[ing].off + a] == id) { uses = true; break; }
        }
        if (uses) { g_player.recipes_known[r / 8] |= (u8)(1 << (r % 8)); n++; }
    }
    if (n) { g_recipes_unlocked += n; snprintf(G.actionbar, sizeof G.actionbar, "%d new recipe%s unlocked", n, n == 1 ? "" : "s"); G.actionbar_t = 60; }
}
bool player_give(ItemStack s) {
    if (!stack_empty(s)) unlock_recipes_for(s.id);
    int left = inventory_add(&g_player.inv, s);
    if (left > 0) {
        Entity *e = player_ent();
        s.count = (u8)left;
        if (e) item_drop(entity_eye(e), s, v3(0, 2, 0), 40);
        return false;
    }
    return true;
}

void player_drop_selected(bool whole) {
    Entity *e = player_ent();
    ItemStack *h = inventory_held(&g_player.inv);
    if (!e || stack_empty(*h)) return;
    ItemStack d = *h;
    d.count = whole ? h->count : 1;
    h->count -= d.count;
    if (!h->count) *h = (ItemStack){0};
    vec3 dir = v3(sinf(e->yaw) * cosf(e->pitch), sinf(e->pitch), cosf(e->yaw) * cosf(e->pitch));
    item_drop(v3_add(entity_eye(e), v3(0, -0.3f, 0)), d, v3_add(v3_scale(dir, 6.0f), v3(0, 1.5f, 0)), 40);
    g_player.swing = 1.0f;
}

f32 player_attack_strength(void) {
    ItemStack h = *inventory_held(&g_player.inv);
    f32 spd = (!stack_empty(h) && item_valid(h.id)) ? ITEMS[h.id].attack_speed : 4.0f;
    if (spd <= 0) spd = 4;
    return clampf(g_player.attack_ticks / (TICKRATE / spd), 0, 1);
}

void player_hurt_feedback(f32 amount) {
    (void)amount;
    g_player.hurt_tilt = 1.0f;
    sound_play(SND_PLAYER_HURT, 0.8f, frand_range(0.92f, 1.08f));
}

void player_on_death(const char *msg) {
    Entity *e = player_ent();
    if (g_player.dead || !e) return;
    g_player.dead = true;
    g_player.death_ticks = 0;
    snprintf(g_player.death_msg, sizeof g_player.death_msg, "%s", msg);
    if (rule(GR_SHOW_DEATH_MESSAGES)) game_message(rgba_u32(255, 80, 80, 255), "Player %s", msg);
    effects_on_death(e);
    scoreboard_event("deathCount", "Player", 1);
    g_stats[STAT_DEATHS]++;
    if (rule(GR_ENDER_PEARLS_VANISH)) ENT_FOREACH(o) if (o->kind == ENT_PROJ && o->proj == PROJ_ENDER_PEARL && o->owner == e->id) o->remove = true;   // enderPearlsVanishOnDeath
    if (rule(GR_FORGIVE_DEAD_PLAYERS)) ENT_FOREACH(o) if (o->kind == ENT_MOB && o->anger > 0 && o->target == e->id) { o->anger = 0; o->target = 0; }   // forgiveDeadPlayers
    if (rule(GR_KEEP_INVENTORY)) {   // keep items and levels
        g_player.gliding = false; g_player.sleeping = false; e->fire = 0;
        if (g_level.hardcore) g_player.gamemode = GM_SPECTATOR;
        game_open_screen(SCR_DEATH);
        if (rule(GR_DO_IMMEDIATE_RESPAWN) && !g_level.hardcore) player_respawn();
        return;
    }
    // drop everything like Minecraft (no keepInventory)
    // curse of vanishing: those items are destroyed instead of dropped
    for (int i = 0; i < INV_MAIN; i++) if (ench_level(g_player.inv.slots[i], ENCH_VANISHING_CURSE)) g_player.inv.slots[i] = (ItemStack){0};
    for (int i = 0; i < 4; i++) if (ench_level(g_player.inv.armor[i], ENCH_VANISHING_CURSE)) g_player.inv.armor[i] = (ItemStack){0};
    if (ench_level(g_player.inv.offhand, ENCH_VANISHING_CURSE)) g_player.inv.offhand = (ItemStack){0};
    for (int i = 0; i < INV_MAIN; i++) { if (!stack_empty(g_player.inv.slots[i])) item_drop_scatter(entity_eye(e), g_player.inv.slots[i]); g_player.inv.slots[i] = (ItemStack){0}; }
    for (int i = 0; i < 4; i++) { if (!stack_empty(g_player.inv.armor[i])) item_drop_scatter(entity_eye(e), g_player.inv.armor[i]); g_player.inv.armor[i] = (ItemStack){0}; }
    if (!stack_empty(g_player.inv.offhand)) item_drop_scatter(entity_eye(e), g_player.inv.offhand);
    g_player.inv.offhand = (ItemStack){0};
    g_player.gliding = false; g_player.sleeping = false;
    for (int i = 0; i < 9; i++) { if (!stack_empty(g_player.craft[i])) item_drop_scatter(entity_eye(e), g_player.craft[i]); g_player.craft[i] = (ItemStack){0}; }
    if (!stack_empty(g_player.cursor)) { item_drop_scatter(entity_eye(e), g_player.cursor); g_player.cursor = (ItemStack){0}; }
    xp_drop(e->pos, MINI(g_player.xp_level * 7, 100));
    g_player.xp_level = 0; g_player.xp_progress = 0; g_player.xp_total = 0;
    e->fire = 0;
    if (g_level.hardcore) g_player.gamemode = GM_SPECTATOR;   // hardcore: one life, then spectate
    game_open_screen(SCR_DEATH);
    if (rule(GR_DO_IMMEDIATE_RESPAWN) && !g_level.hardcore) player_respawn();
}

void player_respawn(void) {
    if (g_world.dim != DIM_OVERWORLD) game_change_dimension(DIM_OVERWORLD, false);
    ivec3 s = g_world.spawn;
    int top = world_top_y(s.x, s.z);
    vec3 p = v3(s.x + 0.5f, (f32)(top >= 0 ? top + 1 : s.y), s.z + 0.5f);
    int rad = rule_i(GR_SPAWN_RADIUS);   // spawnRadius: respawn somewhere around the world spawn (survival / adventure)
    if (rad > 0 && (g_player.gamemode == GM_SURVIVAL || g_player.gamemode == GM_ADVENTURE)) for (int t = 0; t < 10; t++) {
        int x = s.x + irand(-rad, rad), z = s.z + irand(-rad, rad), ty = world_top_y(x, z);
        if (ty < 0) continue;
        Block g = world_get_block(x, ty, z);
        if (BLOCKS[g].liquid || !BLOCKS[g].solid) continue;
        p = v3(x + 0.5f, (f32)(ty + 1), z + 0.5f); break;
    }
    player_reset(p, false);
    game_close_screen();
}

// ------------------------------------------------------------------------ movement
static bool ground_under(Entity *e, f32 dx, f32 dz) {
    AABB a = entity_aabb(e);
    a.min.x += dx; a.max.x += dx; a.min.z += dz; a.max.z += dz;
    a.min.y -= 0.6f; a.max.y = a.min.y + 0.55f;
    AABB boxes[64];
    int n = world_collect_boxes(a, boxes, 64);
    for (int i = 0; i < n; i++) if (aabb_overlap(a, boxes[i])) return true;
    return false;
}

static int sound_group_below(Entity *e) {
    u16 b = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y - 0.2f), (int)floorf(e->pos.z));
    if (!b) return -1;
    return BLOCKS[b].sound;
}


// ------------------------------------------------------------------------ mining
f32 player_mine_speed(u16 b) {
    const BlockDef *d = &BLOCKS[b];
    if (d->hardness < 0) return 0;
    if (d->hardness == 0) return 1000;
    ItemStack h = *inventory_held(&g_player.inv);
    const ItemDef *it = (!stack_empty(h) && item_valid(h.id)) ? &ITEMS[h.id] : NULL;
    bool correct = it && it->tool && it->tool == d->tool;
    f32 speed = correct ? it->dig_speed : 1.0f;
    if (it && it->tool == TOOL_SWORD) { if (b == B_COBWEB) speed = 15; else if (block_is_leaves(b) || d->mesh == MESH_CROSS || b == B_BAMBOO) speed = 1.5f; }
    if (it && it->tool == TOOL_SHEARS) { if (b == B_COBWEB || block_is_leaves(b)) speed = 15; else if (d->key && strstr(d->key, "_wool")) speed = 5; else if (b == B_VINE || b == B_GLOW_LICHEN) speed = 2; }
    if (it && it->tool == TOOL_HOE && block_is_leaves(b)) speed = it->dig_speed;
    if (it && it->tool == TOOL_AXE && (d->key && (strstr(d->key, "_door") || strstr(d->key, "ladder")))) correct = true;
    Entity *e = player_ent();
    if (e) {
        speed *= 1.0f + 0.2f * effect_level(e, EFF_HASTE);
        if (effect_has(e, EFF_MINING_FATIGUE)) speed *= powf(0.3f, (f32)MINI(4, effect_level(e, EFF_MINING_FATIGUE)));
    }
    bool can = d->tier == 0 || (correct && it->tier >= d->tier);
    int eff = ench_level(h, ENCH_EFFICIENCY);
    if (eff && speed > 1.0f) speed += (f32)(eff * eff + 1);
    f32 t = d->hardness * (can ? 1.5f : 5.0f) / speed;
    if (e) {
        u16 head = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + PLAYER_EYE), (int)floorf(e->pos.z));
        if (block_is_water(head) && !ench_level(g_player.inv.armor[0], ENCH_AQUA_AFFINITY)) t *= 5;
        if (!e->on_ground && !g_player.flying) t *= 5;
    }
    return 1.0f / t;
}

static void damage_held(ItemStack *h, int amount) {
    if (g_player.gamemode == GM_CREATIVE || stack_empty(*h) || !ITEMS[h->id].durability) return;
    int unb = ench_level(*h, ENCH_UNBREAKING);
    if (unb) { int kept = 0; for (int i = 0; i < amount; i++) if (irand(0, unb) != 0) kept++; amount -= kept; if (amount <= 0) return; }
    h->damage += (u16)amount;
    if (h->damage >= ITEMS[h->id].durability) { *h = (ItemStack){0}; sound_play(SND_BREAK_GLASS, 0.8f, 0.7f); game_message(COL_WHITE, "Your tool broke!"); }
}
static void consume_held(ItemStack *h, int n) {
    if (g_player.gamemode == GM_CREATIVE) return;
    if (h->count <= n) *h = (ItemStack){0}; else h->count -= (u8)n;
}
static void swap_held(ItemStack *h, int new_item) {   // e.g. bucket -> water bucket
    if (g_player.gamemode == GM_CREATIVE) { if (!inventory_count(&g_player.inv, new_item)) player_give(stack_of(new_item, 1)); return; }
    if (h->count == 1) *h = stack_of(new_item, 1);
    else { h->count--; player_give(stack_of(new_item, 1)); }
}

static void break_block(ivec3 p, u16 b) {
    const BlockDef *d = &BLOCKS[b];
    sculk_vibration(iv3_center(p), 2, true);
    g_stats[STAT_MINED]++;
    if (BLOCKS[b].base == B_TRIPWIRE && inventory_held(&g_player.inv)->id != I_SHEARS) tripwire_trip(p);   // cut without shears: it snaps and trips
    fx_block_break(p, b);
    sound_play_at(d->sound == SG_GLASS ? SND_BREAK_GLASS : SND_DIG_STONE + MINI(d->sound, SG_SNOW), iv3_center(p), 1.0f, 0.8f);
    // containers spill their contents (ender chests keep theirs)
    BlockEntity *be = world_block_entity(p, false, 0);
    ItemStack banner = {0};
    if (d->be == BE_BANNER) { banner = be && !stack_empty(be->slots[0]) ? be->slots[0] : stack_of(d->base, 1); banner.count = 1; }
    ItemStack shulker = {0};
    if (d->be == BE_SHULKER) { shulker = stack_of(d->base, 1); if (be) { shulker.data = box_store(be->slots); memset(be->slots, 0, sizeof be->slots); } }
    if (be && d->be != BE_ENDER && d->be != BE_BANNER && d->be != BE_SHULKER) {
        int n = d->be == BE_JUKEBOX || d->be == BE_COMPARATOR || d->be == BE_SPAWNER ? 0 : 27;
        for (int i = 0; i < n; i++) if (!stack_empty(be->slots[i])) item_drop_scatter(iv3_center(p), be->slots[i]);
        if (d->be == BE_JUKEBOX && be->data) item_drop_scatter(iv3_center(p), stack_of(be->data, 1));
    }
    world_set_block(p.x, p.y, p.z, B_AIR);
    if (g_player.gamemode == GM_CREATIVE) { if (shulker.data) item_drop_scatter(iv3_center(p), shulker); return; }
    if (!stack_empty(banner)) item_drop_scatter(iv3_center(p), banner);
    if (!stack_empty(shulker)) { item_drop_scatter(iv3_center(p), shulker); return; }
    ItemStack *h = inventory_held(&g_player.inv);
    const ItemDef *it = (!stack_empty(*h) && item_valid(h->id)) ? &ITEMS[h->id] : NULL;
    bool correct = it && it->tool && it->tool == d->tool;
    bool can = d->tier == 0 || (correct && it->tier >= d->tier);
    if (d->tool == TOOL_SHEARS && d->key && strstr(d->key, "_wool")) can = true;
    bool shears = it && it->tool == TOOL_SHEARS;
    if (shears && (block_is_leaves(b) || b == B_SHORT_GRASS || b == B_FERN || b == B_VINE || b == B_DEAD_BUSH || b == B_SEAGRASS || b == B_GLOW_LICHEN || b == B_HANGING_ROOTS || b == B_COBWEB || b == B_TALL_GRASS || b == B_LARGE_FERN)) {
        Block drop = (b == B_TALL_GRASS || b == B_LARGE_FERN) ? b : d->base;
        if (!block_upper_half(b)) item_drop_scatter(iv3_center(p), stack_of(drop == B_TALL_GRASS ? B_SHORT_GRASS : drop == B_LARGE_FERN ? B_FERN : drop, drop == B_TALL_GRASS || drop == B_LARGE_FERN ? 2 : 1));
    } else if (can && !block_upper_half(b)) {
        bool silk = it && ench_level(*h, ENCH_SILK_TOUCH) && item_valid(d->base) && d->base != B_SPAWNER && d->base != B_BUDDING_AMETHYST && d->mesh != MESH_CROP && d->place != PL_DOUBLE && d->place != PL_BED && d->place != PL_DOOR;
        if (silk) item_drop_scatter(iv3_center(p), stack_of(d->base == B_REDSTONE_ORE + 1 ? B_REDSTONE_ORE : d->base, 1));
        else game_block_drops(p, b, h);
    }
    if (it && it->durability && (it->tool != TOOL_SWORD || d->hardness > 0)) damage_held(h, it->tool == TOOL_SWORD ? 2 : 1);
    player_exhaust(0.005f);
}

static void mining_tick(Entity *e, bool held) {
    if (g_player.place_cooldown > 0) g_player.place_cooldown--;
    if (!held || !g_player.look.hit || g_player.look_entity) { g_player.mining = false; g_player.mine_progress = 0; return; }
    if (g_player.gamemode == GM_ADVENTURE || g_player.gamemode == GM_SPECTATOR) { g_player.mining = false; g_player.mine_progress = 0; return; }   // adventure / spectator can't break blocks
    ivec3 p = g_player.look.pos;
    if (!g_player.mining || !iv3_eq(p, g_player.mine_pos)) { g_player.mining = true; g_player.mine_pos = p; g_player.mine_progress = 0; }
    u16 b = world_get_block(p.x, p.y, p.z);
    if (!b || BLOCKS[b].liquid) { g_player.mining = false; return; }
    g_player.swing = fmaxf(g_player.swing, 0.01f);
    if (g_player.gamemode == GM_CREATIVE) {
        ItemStack h = *inventory_held(&g_player.inv);
        if (h.id && ITEMS[h.id].tool == TOOL_SWORD) return;   // swords don't break blocks in creative
        if (g_player.place_cooldown <= 0) { break_block(p, b); g_player.place_cooldown = 12; g_player.mining = false; }
        return;
    }
    if (g_player.place_cooldown > 0) return;
    g_player.mine_progress += player_mine_speed(b) * DT;
    if (++g_player.mine_sound_timer % 14 == 0) {
        sound_play_at(SND_STEP_STONE + MINI(BLOCKS[b].sound, SG_SNOW), iv3_center(p), 0.5f, 0.6f);
        fx_block_hit(p, g_player.look.face, b);
    }
    if (g_player.mine_progress >= 1.0f) {
        break_block(p, b);
        g_player.mining = false; g_player.mine_progress = 0;
        g_player.place_cooldown = 12;
    }
    (void)e;
}

static bool aabb_blocked_by_entity(AABB box) {
    ENT_FOREACH(o) {
        if (o->kind != ENT_MOB && o->kind != ENT_PLAYER) continue;
        if (o->death_time > 0) continue;
        if (aabb_overlap(entity_aabb(o), box)) return true;
    }
    return false;
}

// ------------------------------------------------------------------------ placing
static int look_dir4(Entity *e) {   // 0 N, 1 E, 2 S, 3 W: the way the player looks
    f32 dx = sinf(e->yaw), dz = cosf(e->yaw);
    if (fabsf(dz) >= fabsf(dx)) return dz > 0 ? 2 : 0;
    return dx > 0 ? 1 : 3;
}
static int dir4_of_face(int f) { for (int k = 0; k < 4; k++) if (H4_FACE[k] == f) return k; return -1; }
static bool cell_free(ivec3 p) {
    if (p.y < 0 || p.y >= WORLD_H) return false;
    u16 c = world_get_block(p.x, p.y, p.z);
    return c == B_AIR || c == B_CAVE_AIR || BLOCKS[c].replaceable;
}
static bool can_occupy(ivec3 p, u16 state) {
    if (!BLOCKS[state].solid) return true;
    AABB boxes[8];
    int n = block_boxes(state, p.x, p.y, p.z, boxes, 8, true);
    for (int i = 0; i < n; i++) if (aabb_blocked_by_entity(boxes[i])) return false;
    return true;
}
static void after_place(ivec3 p, u16 s) {
    const BlockDef *d = &BLOCKS[s];
    if (d->be != BE_NONE) world_block_entity(p, true, d->be);
    sound_play_at(SND_DIG_STONE + MINI(d->sound, SG_SNOW), iv3_center(p), 0.9f, 0.85f);
    if (d->base == B_CARVED_PUMPKIN || d->base == B_JACK_O_LANTERN || d->base == B_WITHER_SKELETON_SKULL) golem_check_build(p);
}

// wall-torch variants for torches placed on a wall
static Block wall_variant(Block kind) {
    if (kind == B_TORCH) return B_WALL_TORCH;
    if (kind == B_SOUL_TORCH) return B_SOUL_WALL_TORCH;
    if (kind == B_COPPER_TORCH) return B_COPPER_WALL_TORCH;
    if (kind == B_REDSTONE_TORCH) return B_REDSTONE_WALL_TORCH;
    return 0;
}

bool player_place_state(Entity *e, Block kind, RayHit L) {
    const BlockDef *kd = &BLOCKS[kind];
    ivec3 p = (BLOCKS[L.block].replaceable && L.block != kind) ? L.pos : L.prev;
    int face = L.face;
    f32 fy = L.point.y - floorf(L.point.y);
    bool upper_click = face == FACE_D || (face != FACE_U && fy > 0.5f);
    int look = look_dir4(e), facing = (look + 2) % 4;
    u16 s = kind;
    // slabs combine into a double slab
    if (kd->place == PL_SLAB) {
        u16 at = L.block;
        if (BLOCKS[at].base == kind && ((BLOCKS[at].var == 0 && face == FACE_U) || (BLOCKS[at].var == 1 && face == FACE_D))) {
            world_set_block(L.pos.x, L.pos.y, L.pos.z, kind + 2); after_place(L.pos, kind + 2); return true;
        }
        u16 cur = world_get_block(p.x, p.y, p.z);
        if (BLOCKS[cur].base == kind && BLOCKS[cur].var < 2) { world_set_block(p.x, p.y, p.z, kind + 2); after_place(p, kind + 2); return true; }
        s = kind + (upper_click ? 1 : 0);
    }
    if (kd->place == PL_LAYER) {   // snow layers stack
        if (BLOCKS[L.block].base == kind && BLOCKS[L.block].var < 7 && face == FACE_U) { world_set_block(L.pos.x, L.pos.y, L.pos.z, L.block + 1); after_place(L.pos, L.block + 1); return true; }
        u16 cur = world_get_block(p.x, p.y, p.z);
        if (BLOCKS[cur].base == kind && BLOCKS[cur].var < 7) { world_set_block(p.x, p.y, p.z, cur + 1); after_place(p, cur + 1); return true; }
    }
    if (!cell_free(p)) return false;
    switch (kd->place) {
    case PL_AXIS: s = kind + (face == FACE_U || face == FACE_D ? 0 : face == FACE_W || face == FACE_E ? 1 : 2); break;
    case PL_AXIS2: s = kind + ((look == 1 || look == 3) ? 1 : 0); break;
    case PL_FACING4: s = kind + ((kind == B_REPEATER || kind == B_COMPARATOR) ? look : facing) * (kd->nvar / 4); break;
    case PL_PISTON: {   // front faces the player, also up/down
        f32 dy = sinf(e->pitch);
        if (fabsf(dy) > 0.78f) s = kind + (dy < 0 ? FACE_U : FACE_D);
        else s = kind + H4_FACE[facing];
    } break;
    case PL_HOPPER: { int k = dir4_of_face(FACE_OPP[face]); s = kind + (face == FACE_U || face == FACE_D || k < 0 ? 0 : k + 1); } break;
    case PL_RAIL: s = kind; break;
    case PL_FACING6: s = kind + face; break;
    case PL_STAIRS: s = kind + look * 2 + (upper_click ? 1 : 0); break;
    case PL_TRAPDOOR: s = kind + facing * 4 + (upper_click ? 2 : 0); break;
    case PL_GATE: s = kind + look * 2; break;
    case PL_ATTACH: s = kind + FACE_OPP[face]; break;
    case PL_HANGING: s = kind + (face == FACE_D ? 1 : 0); break;
    case PL_WALLONLY: {
        int k = dir4_of_face(FACE_OPP[face]);
        if (k < 0) return false;
        s = kind + k;
    } break;
    case PL_SIGN: {
        int k = dir4_of_face(FACE_OPP[face]);
        s = (face == FACE_U || k < 0) ? kind + facing : kind + 4 + k;
    } break;
    case PL_DOOR: {
        ivec3 up = iv3(p.x, p.y + 1, p.z);
        if (!cell_free(up)) return false;
        u16 lo = kind + look * 4, hi = lo + 2;
        if (!BLOCKS[world_get_block(p.x, p.y - 1, p.z)].solid || !can_occupy(p, lo) || !can_occupy(up, hi)) return false;
        world_set_block_silent(up.x, up.y, up.z, hi);
        world_set_block(p.x, p.y, p.z, lo);
        after_place(p, lo);
        return true;
    }
    case PL_DOUBLE: {
        ivec3 up = iv3(p.x, p.y + 1, p.z);
        if (!cell_free(up) || !world_block_supported(p, kind + 0) ) {
            // supported check fails while the upper half is missing: test the plant's own support rule
            if (!cell_free(up)) return false;
        }
        world_set_block_silent(up.x, up.y, up.z, kind + 1);
        world_set_block(p.x, p.y, p.z, kind);
        if (!world_block_supported(p, kind)) { world_set_block_silent(up.x, up.y, up.z, B_AIR); world_set_block(p.x, p.y, p.z, B_AIR); return false; }
        after_place(p, kind);
        return true;
    }
    case PL_BED: {
        ivec3 head = iv3(p.x + FACE_DIR[H4_FACE[look]].x, p.y, p.z + FACE_DIR[H4_FACE[look]].z);
        if (!cell_free(head) || !BLOCKS[world_get_block(head.x, head.y - 1, head.z)].solid || !BLOCKS[world_get_block(p.x, p.y - 1, p.z)].solid) return false;
        world_set_block_silent(head.x, head.y, head.z, kind + look * 2 + 1);
        world_set_block(p.x, p.y, p.z, kind + look * 2);
        after_place(p, kind);
        return true;
    }
    default: break;
    }
    if ((kd->flags & BF_LEAVES) && kd->nvar >= 2) s = kind + 1;   // leaves a player places are persistent: they never decay
    // torches on walls become wall torches
    if (kd->mesh == MESH_TORCH && face != FACE_U) {
        int k = dir4_of_face(FACE_OPP[face]);
        Block wv = wall_variant(kind);
        if (k < 0 || !wv) return false;
        s = wv + k;
    }
    if (!world_block_supported(p, s)) return false;
    if (!can_occupy(p, s)) return false;
    world_set_block(p.x, p.y, p.z, s);
    if (kd->place == PL_RAIL) rs_rail_placed(p, look);
    if (BLOCKS[s].be == BE_SHULKER) {   // a shulker box brings its contents back
        ItemStack *h = inventory_held(&g_player.inv);
        if (h->data) { BlockEntity *be = world_block_entity(p, true, BE_SHULKER); box_take(h->data, be->slots); h->data = 0; }
    }
    if (BLOCKS[s].be == BE_BANNER) {   // the banner keeps its pattern layers
        BlockEntity *be = world_block_entity(p, true, BE_BANNER);
        ItemStack *h = inventory_held(&g_player.inv);
        be->slots[0] = item_is_block(h->id) && ITEMS[h->id].block == kind ? *h : stack_of(kind, 1);
        be->slots[0].count = 1;
    }
    after_place(p, s);
    return true;
}

static bool place_block_inner(Entity *e);
static bool place_block(Entity *e) {   // every placement is a vibration (frequency 3)
    bool ok = place_block_inner(e);
    if (ok) { sculk_vibration(iv3_center(g_player.look.prev), 3, true); g_stats[STAT_PLACED]++; }
    return ok;
}
static bool place_block_inner(Entity *e) {
    if (g_player.gamemode == GM_ADVENTURE || g_player.gamemode == GM_SPECTATOR) return false;
    ItemStack *h = inventory_held(&g_player.inv);
    if (stack_empty(*h) || !item_valid(h->id)) return false;
    const ItemDef *it = &ITEMS[h->id];
    if (!it->block || (it->use != USE_NONE && it->use != USE_PLACE && it->use != USE_FOOD)) return false;
    if (!g_player.look.hit) return false;
    if (it->use == USE_FOOD && !item_is_block(h->id)) {
        // seeds-like foods (carrot, potato, berries) only place on fitting ground
        ivec3 p = g_player.look.prev;
        if (!world_block_supported(p, it->block)) return false;
    }
    if (!player_place_state(e, it->block, g_player.look)) return false;
    consume_held(h, 1);
    g_player.swing = 1.0f;
    return true;
}

// ------------------------------------------------------------------------ hell / space / sift portals
static bool frame_obsidian(u16 b) { return b == B_OBSIDIAN; }
static bool frame_sift(u16 b) { return b == B_GOLD_BLOCK || b == B_GLOWSTONE; }
static bool try_light_portal(ivec3 p, bool (*isframe)(u16), u16 portal) {
    for (int axis = 0; axis < 2; axis++) {
        int dx = axis ? 0 : 1, dz = axis ? 1 : 0;
        // find the bottom-left interior corner
        ivec3 c = p;
        while (c.y > 0 && cell_free(iv3(c.x, c.y - 1, c.z))) c.y--;
        if (!isframe(world_get_block(c.x, c.y - 1, c.z))) continue;
        int guard = 0;
        while (cell_free(iv3(c.x - dx, c.y, c.z - dz)) && guard++ < 21) { c.x -= dx; c.z -= dz; }
        if (!isframe(world_get_block(c.x - dx, c.y, c.z - dz))) continue;
        int w = 0; while (w < 22 && cell_free(iv3(c.x + dx * w, c.y, c.z + dz * w))) w++;
        int h = 0; while (h < 22 && cell_free(iv3(c.x, c.y + h, c.z))) h++;
        if (w < 2 || h < 3 || w > 21 || h > 21) continue;
        bool ok = true;
        for (int i = 0; i < w && ok; i++) ok = isframe(world_get_block(c.x + dx * i, c.y - 1, c.z + dz * i)) && isframe(world_get_block(c.x + dx * i, c.y + h, c.z + dz * i));
        for (int j = 0; j < h && ok; j++) ok = isframe(world_get_block(c.x - dx, c.y + j, c.z - dz)) && isframe(world_get_block(c.x + dx * w, c.y + j, c.z + dz * w));
        for (int i = 0; i < w && ok; i++) for (int j = 0; j < h && ok; j++) ok = cell_free(iv3(c.x + dx * i, c.y + j, c.z + dz * i));
        if (!ok) continue;
        for (int i = 0; i < w; i++) for (int j = 0; j < h; j++) world_set_block_silent(c.x + dx * i, c.y + j, c.z + dz * i, portal + axis);
        sound_play_at(SND_FIRE, iv3_center(p), 1, 0.5f);
        return true;
    }
    return false;
}
bool try_light_nether_portal(ivec3 p) { return try_light_portal(p, frame_obsidian, B_NETHER_PORTAL); }
bool try_light_sift_portal(ivec3 p) { return try_light_portal(p, frame_sift, B_SIFT_PORTAL); }

static void check_end_portal(ivec3 p) {
    // look for a complete 5x5 ring of eyed frames around a 3x3 hole near p
    for (int ox = -4; ox <= 0; ox++) for (int oz = -4; oz <= 0; oz++) {
        int x0 = p.x + ox, z0 = p.z + oz;
        bool ok = true;
        for (int i = 1; i <= 3 && ok; i++) {
            ok = world_get_block(x0 + i, p.y, z0) == B_END_PORTAL_FRAME + 1 && world_get_block(x0 + i, p.y, z0 + 4) == B_END_PORTAL_FRAME + 1 &&
                 world_get_block(x0, p.y, z0 + i) == B_END_PORTAL_FRAME + 1 && world_get_block(x0 + 4, p.y, z0 + i) == B_END_PORTAL_FRAME + 1;
        }
        if (!ok) continue;
        // UltimateCraft: the eyes only align the frame; a Void Sigil (from the Underground City) must be offered to open it
        if (g_player.gamemode != GM_CREATIVE) {
            if (inventory_count(&g_player.inv, I_VOID_SIGIL) <= 0) {
                game_message(rgba_u32(170, 140, 255, 255), "The frame hums but stays dark: it needs a Void Sigil from the Underground City");
                return;
            }
            inventory_remove(&g_player.inv, I_VOID_SIGIL, 1);
        }
        for (int i = 1; i <= 3; i++) for (int j = 1; j <= 3; j++) world_set_block(x0 + i, p.y, z0 + j, B_END_PORTAL);
        sound_play(SND_LEVELUP, 1, 0.5f);
        game_message(rgba_u32(200, 120, 255, 255), "The Space portal has opened!");
        return;
    }
}

// ------------------------------------------------------------------------ right-click on blocks
static Block stripped_of(Block b) {
    const char *k = BLOCKS[b].key;
    if (!k || strncmp(k, "stripped_", 9) == 0 || BLOCKS[b].place != PL_AXIS) return 0;
    if (!(strstr(k, "_log") || strstr(k, "_wood") || strstr(k, "_stem") || strstr(k, "_hyphae") || !strcmp(k, "bamboo_block"))) return 0;
    char buf[64]; snprintf(buf, sizeof buf, "stripped_%s", k);
    Block s = block_by_key(buf);
    return s ? s + BLOCKS[b].var : 0;
}
static Block copper_scraped(Block b, bool *unwax) {
    const char *k = BLOCKS[b].key;
    if (!k || (!strstr(k, "copper") && !strstr(k, "lightning_rod"))) return 0;
    char buf[64];
    if (!strncmp(k, "waxed_", 6)) { *unwax = true; snprintf(buf, sizeof buf, "%s", k + 6); if (!strcmp(buf, "copper_block")) {} Block s = block_by_key(buf); return s ? s + BLOCKS[b].var : 0; }
    *unwax = false;
    static const char *ST[3][2] = {{"exposed_", ""}, {"weathered_", "exposed_"}, {"oxidized_", "weathered_"}};
    for (int i = 0; i < 3; i++) if (!strncmp(k, ST[i][0], strlen(ST[i][0]))) {
        const char *rest = k + strlen(ST[i][0]);
        if (i == 0 && !strcmp(rest, "copper")) snprintf(buf, sizeof buf, "copper_block");
        else snprintf(buf, sizeof buf, "%s%s", ST[i][1], rest);
        Block s = block_by_key(buf);
        return s ? s + BLOCKS[b].var : 0;
    }
    return 0;
}
static Block waxed_of(Block b) {
    const char *k = BLOCKS[b].key;
    if (!k || !strncmp(k, "waxed_", 6) || (!strstr(k, "copper") && !strstr(k, "lightning_rod")) || strstr(k, "ore") || strstr(k, "raw_")) return 0;
    char buf[64];
    if (!strcmp(k, "copper_block")) snprintf(buf, sizeof buf, "waxed_copper_block"); else snprintf(buf, sizeof buf, "waxed_%s", k);
    Block s = block_by_key(buf);
    return s ? s + BLOCKS[b].var : 0;
}

static bool use_item_on_block(Entity *e, ItemStack *h) {
    RayHit L = g_player.look;
    ivec3 p = L.pos;
    Block b = L.block;
    const BlockDef *d = &BLOCKS[b];
    int id = h->id;
    if (!id) return false;
    const ItemDef *it = &ITEMS[id];
    // tools
    if (it->tool == TOOL_HOE && (d->base == B_GRASS_BLOCK || b == B_DIRT || b == B_DIRT_PATH || b == B_COARSE_DIRT || b == B_ROOTED_DIRT) && L.face != FACE_D && block_is_air(world_get_block(p.x, p.y + 1, p.z))) {
        world_set_block(p.x, p.y, p.z, b == B_COARSE_DIRT || b == B_ROOTED_DIRT ? B_DIRT : B_FARMLAND);
        if (b == B_ROOTED_DIRT) item_drop_scatter(v3(p.x + 0.5f, p.y + 1.1f, p.z + 0.5f), stack_of(I_HANGING_ROOTS, 1));
        sound_play_at(SND_DIG_GRAVEL, iv3_center(p), 1, 0.9f); damage_held(h, 1); return true;
    }
    if (it->tool == TOOL_SHOVEL && (d->base == B_GRASS_BLOCK || b == B_DIRT || b == B_COARSE_DIRT || b == B_PODZOL || b == B_MYCELIUM || b == B_ROOTED_DIRT) && L.face != FACE_D && block_is_air(world_get_block(p.x, p.y + 1, p.z))) {
        world_set_block(p.x, p.y, p.z, B_DIRT_PATH); sound_play_at(SND_DIG_GRASS, iv3_center(p), 1, 0.9f); damage_held(h, 1); return true;
    }
    if (it->tool == TOOL_SHOVEL && (b == B_CAMPFIRE || b == B_SOUL_CAMPFIRE)) { fx_smoke(iv3_center(p), 8, 0.3f, COL_WHITE); return true; }
    if (it->tool == TOOL_AXE) {
        Block s = stripped_of(b);
        bool unwax = false;
        if (!s) s = copper_scraped(b, &unwax);
        if (s) { world_set_block(p.x, p.y, p.z, s); sound_play_at(SND_DIG_WOOD, iv3_center(p), 1, 0.8f); damage_held(h, 1); if (unwax || BLOCKS[b].sound == SG_METAL) fx_crit(iv3_center(p), 6); return true; }
    }
    if (id == I_HONEYCOMB) { Block w = waxed_of(b); if (w) { world_set_block(p.x, p.y, p.z, w); consume_held(h, 1); fx_crit(iv3_center(p), 6); return true; } }
    if (id == I_SHEARS && (d->base == B_PUMPKIN)) {
        world_set_block(p.x, p.y, p.z, B_CARVED_PUMPKIN + (look_dir4(e) + 2) % 4);
        item_drop_scatter(v3(p.x + 0.5f, p.y + 1.0f, p.z + 0.5f), stack_of(I_PUMPKIN_SEEDS, 4)); damage_held(h, 1); return true;
    }
    if (id == I_SHEARS && d->base == B_BEE_NEST) { item_drop_scatter(iv3_center(p), stack_of(I_HONEYCOMB, 3)); damage_held(h, 1); return true; }
    if (id == I_GLASS_BOTTLE && (d->base == B_BEEHIVE || d->base == B_BEE_NEST)) { swap_held(h, I_HONEY_BOTTLE); return true; }
    if (it->use == USE_BONE_MEAL) {
        static Rng r = {12345};
        if (world_grow(p, &r, true)) { consume_held(h, 1); for (int i = 0; i < 8; i++) fx_crit(v3(p.x + frand(), p.y + frand(), p.z + frand()), 1); return true; }
        // underwater: seagrass
        if (block_is_water(world_get_block(p.x, p.y + 1, p.z))) { world_set_block(p.x, p.y + 1, p.z, B_SEAGRASS); consume_held(h, 1); return true; }
        return false;
    }
    if (it->use == USE_FLINT_STEEL || it->use == USE_FIRE_CHARGE) {
        if (b == B_TNT) { world_set_block(p.x, p.y, p.z, B_AIR); tnt_spawn(v3(p.x + 0.5f, (f32)p.y, p.z + 0.5f), 4 * TICKRATE); }
        else if (b == B_CAMPFIRE || b == B_SOUL_CAMPFIRE || (d->key && strstr(d->key, "candle"))) {}
        else {
            ivec3 q = L.prev;
            if (!try_light_nether_portal(q)) {
                if (!cell_free(q)) return false;
                bool soul = world_get_block(q.x, q.y - 1, q.z) == B_SOUL_SAND || world_get_block(q.x, q.y - 1, q.z) == B_SOUL_SOIL;
                world_set_block(q.x, q.y, q.z, soul ? B_SOUL_FIRE : B_FIRE);
            }
        }
        sound_play_at(SND_FIRE, iv3_center(p), 1, 1.2f);
        if (it->use == USE_FLINT_STEEL) damage_held(h, 1); else consume_held(h, 1);
        return true;
    }
    if (h->id == I_SIFT_KEY) {   // the key to the Sift opens a gold-and-glowstone frame (Overworld only)
        if (g_world.dim != DIM_OVERWORLD) { game_message(rgba_u32(255, 220, 120, 255), "The Sift can only be reached from the Overworld"); return true; }
        if (try_light_sift_portal(L.prev)) { sound_play_at(SND_LEVELUP, iv3_center(p), 1, 1.4f); game_message(rgba_u32(255, 235, 140, 255), "A gate to the Sift opens"); return true; }
        return false;
    }
    if (it->use == USE_SHEARS && BLOCKS[b].base == B_TRIPWIRE) { world_set_block(p.x, p.y, p.z, B_TRIPWIRE + 2); sound_play_at(SND_CLICK, iv3_center(p), 0.6f, 1.5f); return true; }   // disarm
    if (h->id == I_VOID_SIGIL && BLOCKS[b].base == B_END_PORTAL_FRAME) { check_end_portal(p); return true; }   // offer the sigil to a full frame
    if (it->use == USE_ENDER_EYE && b == B_END_PORTAL_FRAME) {
        world_set_block(p.x, p.y, p.z, B_END_PORTAL_FRAME + 1);
        consume_held(h, 1);
        sound_play_at(SND_CLICK, iv3_center(p), 1, 0.6f);
        check_end_portal(p);
        return true;
    }
    if (it->use == USE_END_CRYSTAL && (b == B_OBSIDIAN || b == B_BEDROCK) && cell_free(iv3(p.x, p.y + 1, p.z))) {
        crystal_spawn(v3(p.x + 0.5f, p.y + 1.0f, p.z + 0.5f)); consume_held(h, 1); return true;
    }
    if (it->use == USE_SPAWN_EGG) {
        ivec3 q = cell_free(p) ? p : L.prev;
        if (b == B_SPAWNER) return false;
        Entity *m = mob_spawn(it->use_arg, v3(q.x + 0.5f, (f32)q.y, q.z + 0.5f));
        if (m) { m->persistent = true; m->from_spawner = true; m->yaw = m->body_yaw = e->yaw + PI_F; }
        consume_held(h, 1);
        return true;
    }
    if (it->use == USE_BUCKET) {
        RayHit wl = world_raycast(entity_eye(e), v3(sinf(e->yaw) * cosf(e->pitch), sinf(e->pitch), cosf(e->yaw) * cosf(e->pitch)), PLAYER_REACH, true);
        if (wl.hit && (wl.block == B_WATER || wl.block == B_LAVA || wl.block == B_POWDER_SNOW)) {
            world_set_block(wl.pos.x, wl.pos.y, wl.pos.z, B_AIR);
            swap_held(h, wl.block == B_WATER ? I_WATER_BUCKET : wl.block == B_LAVA ? I_LAVA_BUCKET : I_POWDER_SNOW_BUCKET);
            sound_play(SND_SPLASH, 0.6f, 1.2f);
            return true;
        }
        return false;
    }
    if (it->use == USE_FLUID_BUCKET || it->use == USE_MOB_BUCKET) {
        ivec3 q = cell_free(p) && !BLOCKS[b].liquid ? p : L.prev;
        if (!cell_free(q) && !BLOCKS[world_get_block(q.x, q.y, q.z)].liquid) return false;
        Block fl = it->use == USE_MOB_BUCKET ? B_WATER : it->block;
        if (fl == B_WATER && g_world.dim == DIM_NETHER) { fx_smoke(iv3_center(q), 10, 0.4f, COL_WHITE); sound_play(SND_FIRE, 0.7f, 1.6f); }
        else world_set_block(q.x, q.y, q.z, fl);
        if (it->use == USE_MOB_BUCKET) { Entity *m = mob_spawn(it->use_arg, v3(q.x + 0.5f, q.y + 0.2f, q.z + 0.5f)); if (m) m->persistent = true; }
        if (g_player.gamemode != GM_CREATIVE) *h = stack_of(I_BUCKET, 1);
        sound_play(SND_SPLASH, 0.6f, 1.0f);
        return true;
    }
    if (id == I_GLASS_BOTTLE) {
        RayHit wl = world_raycast(entity_eye(e), v3(sinf(e->yaw) * cosf(e->pitch), sinf(e->pitch), cosf(e->yaw) * cosf(e->pitch)), PLAYER_REACH, true);
        if (wl.hit && block_is_water(wl.block)) { ItemStack w = stack_of(I_POTION, 1); w.damage = POT_WATER; if (g_player.gamemode != GM_CREATIVE) { if (--h->count == 0) *h = (ItemStack){0}; } player_give(w); sound_play(SND_SPLASH, 0.4f, 1.5f); return true; }
    }
    return false;
}

static bool interact_block(Entity *e) {
    if (!g_player.look.hit) return false;
    ivec3 p = g_player.look.pos;
    u16 b = g_player.look.block;
    const BlockDef *d = &BLOCKS[b];
    if ((d->base == B_COMMAND_BLOCK || d->base == B_REPEATING_COMMAND_BLOCK || d->base == B_CHAIN_COMMAND_BLOCK) && g_player.gamemode == GM_CREATIVE) {   // command block editor
        G.cmdblock_pos = p; snprintf(G.cmdblock_text, sizeof G.cmdblock_text, "%s", cmdblock_get(p)); G.chat_hint[0] = 0;
        game_open_screen(SCR_COMMAND_BLOCK); return true;
    }
    ItemStack *h = inventory_held(&g_player.inv);
    if (g_player.sneaking && !stack_empty(*h)) return false;
    // harvesting
    if (d->base == B_SWEET_BERRY_BUSH && d->var >= 2 && h->id != I_BONE_MEAL) {
        item_drop_scatter(iv3_center(p), stack_of(I_SWEET_BERRIES, d->var == 3 ? irand(2, 3) : 1));
        world_set_block(p.x, p.y, p.z, B_SWEET_BERRY_BUSH + 1); return true;
    }
    if (b == B_CAVE_VINES + 1) { item_drop_scatter(iv3_center(p), stack_of(I_GLOW_BERRIES, 1)); world_set_block(p.x, p.y, p.z, B_CAVE_VINES); return true; }
    // redstone controls
    if (d->base == B_LEVER) { rs_toggle_lever(p); return true; }
    if (d->place == PL_ATTACH && d->key && strstr(d->key, "_button")) { rs_press_button(p); return true; }
    if (d->base == B_REPEATER) { int dl = ((d->var >> 1) & 3) + 1; world_set_block(p.x, p.y, p.z, (Block)(d->base + (d->var & ~7) + ((dl & 3) << 1) + (d->var & 1))); sound_play_at(SND_CLICK, iv3_center(p), 0.5f, 0.6f + dl * 0.1f); return true; }
    if (d->base == B_COMPARATOR) { world_set_block(p.x, p.y, p.z, (Block)(d->base + (d->var ^ 2))); sound_play_at(SND_CLICK, iv3_center(p), 0.5f, (d->var & 2) ? 0.55f : 0.65f); return true; }
    if (d->base == B_DAYLIGHT_DETECTOR) { world_set_block(p.x, p.y, p.z, (Block)(d->base + !d->var)); return true; }
    switch (d->ui) {
    case UI_CRAFTING: game_open_screen(SCR_CRAFTING); return true;
    case UI_FURNACE: world_block_entity(p, true, d->be); G.open_be = p; game_open_screen(SCR_FURNACE); return true;
    case UI_CHEST: case UI_LECTERN:
        if (d->ui == UI_LECTERN) { world_block_entity(p, true, BE_LECTERN); G.open_be = p; game_open_screen(SCR_LECTERN); return true; }
        if (d->be == BE_ENDER) { G.open_be = p; G.open_ender = true; game_open_screen(SCR_CHEST); return true; }
        world_block_entity(p, true, d->be ? d->be : BE_CHEST); G.open_be = p; G.open_ender = false; game_open_screen(SCR_CHEST);
        sound_play_at(SND_CHEST_OPEN, iv3_center(p), 0.8f, frand_range(0.95f, 1.05f));
        return true;
    case UI_STONECUTTER: game_open_screen(SCR_STONECUTTER); return true;
    case UI_SMITHING: game_open_screen(SCR_SMITHING); return true;
    case UI_BREWING: world_block_entity(p, true, BE_BREWING); G.open_be = p; game_open_screen(SCR_BREWING); return true;
    case UI_TOGGLE: {
        if (d->sound == SG_METAL && (d->place == PL_DOOR || d->place == PL_TRAPDOOR) && strncmp(d->key, "copper", 6) && !strstr(d->key, "copper")) return false;   // iron needs redstone
        bool open = !block_open(b);
        world_set_block(p.x, p.y, p.z, block_set_open(b, open));
        if (d->place == PL_DOOR) {   // move the other half too
            int dy = block_upper_half(b) ? -1 : 1;
            u16 o = world_get_block(p.x, p.y + dy, p.z);
            if (BLOCKS[o].base == d->base) world_set_block_silent(p.x, p.y + dy, p.z, block_set_open(o, open));
        }
        sound_play_at(open ? SND_DOOR_OPEN : SND_DOOR_CLOSE, iv3_center(p), 0.8f, frand_range(0.94f, 1.06f));
        return true;
    }
    case UI_BED: {
        if (g_world.dim != DIM_OVERWORLD) { world_set_block(p.x, p.y, p.z, B_AIR); explosion_ex(iv3_center(p), 5, NULL, true, true); return true; }
        g_world.spawn = p;
        game_message(COL_WHITE, "Respawn point set");
        if (!world_is_night() && !g_level.thundering) { game_message(COL_WHITE, "You can only sleep at night or during thunderstorms"); return true; }
        bool monsters = false;
        ENT_FOREACH(m) if (m->kind == ENT_MOB && mob_hostile(m->mob) && v3_dist(m->pos, e->pos) < 8) monsters = true;
        if (monsters) { game_message(COL_WHITE, "You may not rest now; there are monsters nearby"); return true; }
        g_player.sleeping = true; g_player.use_ticks = 0;
        return true;
    }
    case UI_CAKE: {
        if (g_player.food >= 20 && gm_survival_like(g_player.gamemode)) return false;
        BlockEntity *be = world_block_entity(p, true, BE_NONE);
        g_player.food = MINI(20, g_player.food + 2); g_player.saturation = fminf((f32)g_player.food, g_player.saturation + 0.4f);
        sound_play(SND_EAT, 0.6f, 1);
        if (++be->data >= 7) { world_set_block(p.x, p.y, p.z, B_AIR); world_remove_block_entity(p); }
        return true;
    }
    case UI_JUKEBOX: {
        BlockEntity *be = world_block_entity(p, true, BE_JUKEBOX);
        if (be->data) { item_drop(v3(p.x + 0.5f, p.y + 1.1f, p.z + 0.5f), stack_of(be->data, 1), v3(0, 3, 0), 10); be->data = 0; G.jukebox_playing = false; return true; }
        if (ITEMS[h->id].use == USE_DISC) {
            be->data = h->id; consume_held(h, 1);
            game_message(rgba_u32(255, 120, 255, 255), "Now Playing: %s", ITEMS[be->data].name);
            G.jukebox_playing = true;
            return true;
        }
        return false;
    }
    case UI_NOTE: {
        BlockEntity *be = world_block_entity(p, true, BE_NONE);
        be->data = (be->data + 1) % 25;
        sound_play_at(SND_CLICK, iv3_center(p), 1, powf(2.0f, (be->data - 12) / 12.0f));
        fx_crit(v3(p.x + 0.5f, p.y + 1.2f, p.z + 0.5f), 2);
        return true;
    }
    case UI_COMPOSTER: {
        BlockEntity *be = world_block_entity(p, true, BE_NONE);
        if (be->data >= 8) { item_drop(v3(p.x + 0.5f, p.y + 1.1f, p.z + 0.5f), stack_of(I_BONE_MEAL, 1), v3(0, 3, 0), 10); be->data = 0; return true; }
        const ItemDef *it = &ITEMS[h->id];
        bool compostable = h->id && (it->category == CAT_FOOD || (item_is_block(h->id) && (block_is_leaves(it->block) || BLOCKS[it->block].mesh == MESH_CROSS || BLOCKS[it->block].mesh == MESH_CROP)) ||
                                     h->id == I_WHEAT_SEEDS || h->id == I_WHEAT || h->id == I_MELON_SLICE || h->id == I_KELP || h->id == I_SUGAR_CANE || h->id == I_CACTUS);
        if (!compostable) return false;
        consume_held(h, 1);
        if (frand() < 0.5f) be->data++;
        if (be->data >= 7) be->data = 8;
        fx_crit(v3(p.x + 0.5f, p.y + 1.0f, p.z + 0.5f), 3);
        return true;
    }
    case UI_RESPAWN:
        if (h->id == I_GLOWSTONE) { consume_held(h, 1); fx_crit(iv3_center(p), 8); return true; }
        if (g_world.dim == DIM_NETHER) { g_world.spawn = p; game_message(COL_WHITE, "Respawn point set"); return true; }
        world_set_block(p.x, p.y, p.z, B_AIR); explosion_ex(iv3_center(p), 5, NULL, true, true); return true;
    case UI_BELL: sound_play_at(SND_LEVELUP, iv3_center(p), 1, 2.0f); ENT_FOREACH(m) if (m->kind == ENT_MOB && (m->mob == MOB_PILLAGER || m->mob == MOB_VINDICATOR || m->mob == MOB_EVOKER) && v3_dist(m->pos, iv3_center(p)) < 48) effect_add(m, EFF_GLOWING, 0, 3 * TICKRATE); return true;
    case UI_ENCHANT: G.open_be = p; game_open_screen(SCR_ENCHANT); return true;
    case UI_ANVIL: G.open_be = p; G.anvil_name[0] = 0; game_open_screen(SCR_ANVIL); return true;
    case UI_LOOM: G.open_be = p; game_open_screen(SCR_LOOM); return true;
    case UI_GRINDSTONE: G.open_be = p; game_open_screen(SCR_GRINDSTONE); return true;
    case UI_CARTOGRAPHY: G.open_be = p; game_open_screen(SCR_CARTOGRAPHY); return true;
    default: break;
    }
    return false;
}

// ------------------------------------------------------------------------ combat
static void attack(Entity *e, Entity *t) {
    f32 strength = player_attack_strength();
    ItemStack *h = inventory_held(&g_player.inv);
    f32 base = (!stack_empty(*h) && item_valid(h->id)) ? ITEMS[h->id].attack_damage : 1.0f;
    base += 3 * effect_level(e, EFF_STRENGTH) - 4 * effect_level(e, EFF_WEAKNESS);
    {   // damage enchantments
        int sh = ench_level(*h, ENCH_SHARPNESS), sm = ench_level(*h, ENCH_SMITE), ba = ench_level(*h, ENCH_BANE_OF_ARTHROPODS), im = ench_level(*h, ENCH_IMPALING);
        if (sh) base += 0.5f * sh + 0.5f;
        if (sm && t->kind == ENT_MOB && mob_has(t->mob, MF_UNDEAD)) base += 2.5f * sm;
        if (ba && t->kind == ENT_MOB && mob_has(t->mob, MF_ARTHROPOD)) { base += 2.5f * ba; effect_add(t, EFF_SLOWNESS, 3, irand(20, 20 + 10 * ba) * 3); }
        if (im && t->kind == ENT_MOB && (mob_has(t->mob, MF_AQUATIC) || t->in_water)) base += 2.5f * im;
    }
    f32 dmg = fmaxf(0, base) * (0.2f + strength * strength * 0.8f);
    bool crit = strength > 0.9f && e->vel.y < -0.5f && !e->on_ground && !e->in_water && !g_player.gliding;
    if (crit) dmg *= 1.5f;
    if (h->id == I_MACE && e->fall_dist > 1.5f) {   // smash attack
        f32 fd = e->fall_dist;
        dmg += fd <= 3 ? 4 * fd : fd <= 8 ? 12 + 2 * (fd - 3) : 22 + (fd - 8);
        dmg += 0.5f * ench_level(*h, ENCH_DENSITY) * fd;
        e->fall_dist = 0; e->vel.y = fmaxf(e->vel.y, 4);
        if (ench_level(*h, ENCH_WIND_BURST)) e->vel.y = 8.0f + 4.0f * ench_level(*h, ENCH_WIND_BURST);
        ENT_FOREACH(o) if (o != t && o != e && o->kind == ENT_MOB && v3_dist(o->pos, t->pos) < 3.5f) entity_knockback(o, t->pos, 0.8f);
        fx_explosion(t->pos, 0.5f);
    }
    g_player.swing = 1.0f;
    g_player.attack_ticks = 0;
    if (t->kind == ENT_PROJ || t->kind == ENT_CRYSTAL) { entity_damage(t, dmg, DMG_MELEE, e); return; }
    if (entity_damage(t, dmg, DMG_MELEE, e)) {
        f32 kb = 0.35f + (g_player.sprinting && strength > 0.9f ? 0.5f : 0.0f) + 0.5f * ench_level(*h, ENCH_KNOCKBACK);
        if (ench_level(*h, ENCH_FIRE_ASPECT) && !(t->kind == ENT_MOB && mob_has(t->mob, MF_FIRE_IMMUNE))) t->fire = MAXI(t->fire, 4 * TICKRATE * ench_level(*h, ENCH_FIRE_ASPECT));
        entity_knockback(t, e->pos, kb);
        if (g_player.sprinting) { e->vel.x *= 0.6f; e->vel.z *= 0.6f; g_player.sprinting = false; }
        sound_play_at(crit ? SND_CRIT : SND_HIT, t->pos, 0.9f, frand_range(0.9f, 1.1f));
        if (crit) fx_crit(v3_add(t->pos, v3(0, t->height * 0.7f, 0)), 12);
        // sword sweep
        if (ITEMS[h->id].tool == TOOL_SWORD && strength > 0.9f && e->on_ground && !g_player.sprinting)
            ENT_FOREACH(o) if (o != t && o->kind == ENT_MOB && !o->tamed && v3_dist(o->pos, t->pos) < 1.5f) {
                int se = ench_level(*h, ENCH_SWEEPING_EDGE);
                entity_damage(o, 1 + dmg * se / (se + 1.0f), DMG_MELEE, e); entity_knockback(o, e->pos, 0.4f);
            }
        if (item_valid(h->id) && ITEMS[h->id].durability && !stack_empty(*h)) damage_held(h, ITEMS[h->id].tool == TOOL_SWORD || h->id == I_MACE || h->id == I_TRIDENT ? 1 : 2);
        player_exhaust(0.1f);
    } else sound_play(SND_SWING, 0.4f, 1.0f);
}

static vec3 look_dir(Entity *e) { return v3(sinf(e->yaw) * cosf(e->pitch), sinf(e->pitch), cosf(e->yaw) * cosf(e->pitch)); }
static int find_ammo(void) {
    ItemStack *off = &g_player.inv.offhand;
    if (off->id == I_ARROW || off->id == I_SPECTRAL_ARROW || off->id == I_TIPPED_ARROW) return -2;
    for (int i = 0; i < INV_MAIN; i++) { int id = g_player.inv.slots[i].id; if (id == I_ARROW || id == I_SPECTRAL_ARROW || id == I_TIPPED_ARROW) return i; }
    return -1;
}
static ItemStack take_ammo(void) {
    int s = find_ammo();
    if (g_player.gamemode == GM_CREATIVE && s < 0) return stack_of(I_ARROW, 1);
    if (s == -1) return (ItemStack){0};
    ItemStack *st = s == -2 ? &g_player.inv.offhand : &g_player.inv.slots[s];
    ItemStack one = *st; one.count = 1;
    if (g_player.gamemode != GM_CREATIVE) { if (--st->count == 0) *st = (ItemStack){0}; }
    return one;
}
static Entity *fire_arrow_dir(Entity *e, ItemStack ammo, f32 power, bool crit, vec3 dir) {
    ItemStack *bow = inventory_held(&g_player.inv);
    bool infinite = ench_level(*bow, ENCH_INFINITY) && ammo.id == I_ARROW;
    Entity *a = arrow_spawn(v3_add(entity_eye(e), v3_scale(dir, 0.5f)), v3_scale(dir, 48.0f * power), e->id, 2.0f, g_player.gamemode != GM_CREATIVE && !infinite);
    if (!a) return NULL;
    if (crit) a->color = 1;
    if (ammo.id != I_ARROW) a->item = ammo; else a->item = stack_of(I_ARROW, 1);
    int pw = ench_level(*bow, ENCH_POWER);
    if (pw) a->damage *= 1.0f + 0.25f * (pw + 1);
    a->size = (u8)ench_level(*bow, ENCH_PUNCH);
    a->special_timer = ench_level(*bow, ENCH_PIERCING);
    if (ench_level(*bow, ENCH_FLAME)) a->fire = 100 * TICKRATE;
    return a;
}
static void fire_arrow(Entity *e, ItemStack ammo, f32 power, bool crit) {
    vec3 dir = look_dir(e);
    fire_arrow_dir(e, ammo, power, crit, dir);
    if (ench_level(*inventory_held(&g_player.inv), ENCH_MULTISHOT)) {   // two extra arrows at +-10 degrees
        for (int s = -1; s <= 1; s += 2) {
            f32 a = s * 0.17f, c = cosf(a), sn = sinf(a);
            vec3 d2 = v3(dir.x * c - dir.z * sn, dir.y, dir.x * sn + dir.z * c);
            Entity *x = fire_arrow_dir(e, ammo, power, crit, d2);
            if (x) x->from_player = false;
        }
    }
}
static void shoot_bow(Entity *e) {
    f32 c = fminf(1.0f, g_player.bow_ticks / (f32)TICKRATE);
    f32 f = (c * c + 2 * c) / 3.0f;
    g_player.bow_ticks = 0;
    if (f < 0.1f) return;
    bool inf = ench_level(*inventory_held(&g_player.inv), ENCH_INFINITY) && find_ammo() != -1 && g_player.inv.slots[find_ammo() < 0 ? 0 : find_ammo()].id == I_ARROW;
    ItemStack ammo = inf ? stack_of(I_ARROW, 1) : take_ammo();
    if (stack_empty(ammo)) return;
    fire_arrow(e, ammo, f, f >= 1.0f);
    sound_play(SND_BOW, 0.8f, 1.0f / frand_range(1.0f, 1.3f) + f * 0.5f);
    damage_held(inventory_held(&g_player.inv), 1);
}

// ------------------------------------------------------------------------ using items in the air (hold or click)
static bool is_consumable(int id) {
    const ItemDef *it = &ITEMS[id];
    return it->food > 0 || it->use == USE_POTION || it->use == USE_MILK || id == I_HONEY_BOTTLE || id == I_OMINOUS_BOTTLE;
}
static void finish_consume(Entity *e, ItemStack *h) {
    const ItemDef *it = &ITEMS[h->id];
    int id = h->id;
    if (it->food > 0) {
        g_player.food = MINI(20, g_player.food + it->food);
        g_player.saturation = fminf((f32)g_player.food, g_player.saturation + it->saturation10 / 10.0f);
        for (int i = 0; i < it->fx_n; i++) if (irand(0, 99) < it->fx[i][3]) effect_add(e, it->fx[i][0], it->fx[i][1], it->fx[i][2] * TICKRATE);
        if (id == I_CHORUS_FRUIT) for (int i = 0; i < 16; i++) {   // random teleport
            int x = (int)e->pos.x + irand(-8, 8), z = (int)e->pos.z + irand(-8, 8);
            int y = world_top_y(x, z);
            if (y > 0 && fabsf(y - e->pos.y) < 8) { e->pos = e->prev_pos = v3(x + 0.5f, y + 1.0f, z + 0.5f); e->fall_dist = 0; sound_play(SND_POP, 1, 0.5f); break; }
        }
        if (id == I_HONEY_BOTTLE) e->eff_time[EFF_POISON] = 0;
        sound_play(SND_BURP, 0.5f, frand_range(0.9f, 1.1f));
    }
    if (it->use == USE_POTION) potion_apply(e, h->damage, 1);
    if (it->use == USE_MILK) effects_clear(e);
    if (id == I_OMINOUS_BOTTLE) { effect_add(e, EFF_BAD_OMEN, clampi(h->damage, 0, 4), 120000 * MC_TICK_DIV); sound_play(SND_LEVELUP, 0.8f, 0.5f); }   // Bad Omen I-V for 100 minutes
    int rem = it->remainder ? it->remainder : (it->use == USE_POTION ? I_GLASS_BOTTLE : 0);
    if (g_player.gamemode != GM_CREATIVE) {
        if (--h->count == 0) *h = rem ? stack_of(rem, 1) : (ItemStack){0};
        else if (rem) player_give(stack_of(rem, 1));
    }
}

static int consume_time(int id) {
    if (id == I_DRIED_KELP) return 16 * 3;
    if (ITEMS[id].use == USE_POTION || ITEMS[id].use == USE_MILK || id == I_HONEY_BOTTLE) return 32 * 3;
    return 32 * 3;
}

static void use_tick(Entity *e, bool held, bool pressed) {
    ItemStack *h = inventory_held(&g_player.inv);
    int id = h->id;
    const ItemDef *it = id ? &ITEMS[id] : NULL;
    g_player.blocking = false; g_player.using_spyglass = false;
    // shield in either hand
    bool shield = held && (id == I_SHIELD || (g_player.inv.offhand.id == I_SHIELD && !(it && (is_consumable(id) || it->use == USE_BOW)))) && !g_player.look.hit;
    if (held && (id == I_SHIELD || g_player.inv.offhand.id == I_SHIELD) && !(it && is_consumable(id))) shield = true;
    if (shield && !(id && it->block && g_player.look.hit)) { g_player.blocking = ++g_player.use_ticks > 5 * 3; return; }
    if (!it) { g_player.eating_ticks = 0; g_player.use_ticks = 0; return; }
    // eating / drinking (hold)
    if (is_consumable(id)) {
        bool can = it->food == 0 || g_player.food < 20 || g_player.gamemode == GM_CREATIVE || id == I_GOLDEN_APPLE || id == I_ENCHANTED_GOLDEN_APPLE || id == I_CHORUS_FRUIT || id == I_SUSPICIOUS_STEW || id == I_HONEY_BOTTLE;
        if (!held || !can) { g_player.eating_ticks = 0; return; }
        if (g_player.look.hit && it->block && it->use == USE_PLACE) { g_player.eating_ticks = 0; return; }
        g_player.eating_ticks++;
        if (g_player.eating_ticks % 12 == 0) {
            sound_play(it->food ? SND_EAT : SND_SWIM, 0.5f, frand_range(0.8f, 1.2f));
            if (it->food) fx_smoke(v3_add(entity_eye(e), v3(sinf(e->yaw) * 0.4f, -0.25f, cosf(e->yaw) * 0.4f)), 2, 0.05f, rgba_u32(200, 120, 90, 255));
        }
        if (g_player.eating_ticks >= consume_time(id)) { finish_consume(e, h); g_player.eating_ticks = 0; }
        return;
    }
    g_player.eating_ticks = 0;
    switch (it->use) {
    case USE_BOW:
        if (held && (g_player.gamemode == GM_CREATIVE || find_ammo() != -1)) g_player.bow_ticks++;
        else if (g_player.bow_ticks > 0) shoot_bow(e);
        return;
    case USE_CROSSBOW:
        if (g_player.crossbow_loaded) {
            if (pressed) {
                fire_arrow(e, g_player.crossbow_ammo, 1.3f, false);
                g_player.crossbow_loaded = false; sound_play(SND_BOW, 0.9f, 1.4f); damage_held(h, 1);
            }
            return;
        }
        if (held && (g_player.gamemode == GM_CREATIVE || find_ammo() != -1)) {
            if (++g_player.bow_ticks >= (25 - 5 * ench_level(*h, ENCH_QUICK_CHARGE)) * 3) { g_player.crossbow_ammo = take_ammo(); g_player.crossbow_loaded = !stack_empty(g_player.crossbow_ammo); g_player.bow_ticks = 0; sound_play(SND_CLICK, 0.8f, 0.6f); }
        } else g_player.bow_ticks = 0;
        return;
    case USE_TRIDENT:
        if (held) g_player.bow_ticks++;
        else if (g_player.bow_ticks > 10 * 3 && ench_level(*h, ENCH_RIPTIDE)) {
            if (e->in_water || block_is_water(world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + 1), (int)floorf(e->pos.z)))) {
                e->vel = v3_add(e->vel, v3_scale(look_dir(e), 14.0f + 8.0f * ench_level(*h, ENCH_RIPTIDE)));
                damage_held(h, 1); sound_play(SND_SWIM, 1, 0.6f);
            }
            g_player.bow_ticks = 0;
        } else if (g_player.bow_ticks > 10 * 3) {
            vec3 dir = look_dir(e);
            Entity *a = arrow_spawn(v3_add(entity_eye(e), v3_scale(dir, 0.5f)), v3_scale(dir, 50.0f), e->id, 8, true);
            if (a) { a->item = *h; a->item.count = 1; a->item.damage++; a->phase = (u8)ench_level(*h, ENCH_LOYALTY); a->damage = 8 + 2.5f * ench_level(*h, ENCH_IMPALING); }
            if (g_player.gamemode != GM_CREATIVE) *h = (ItemStack){0};
            sound_play(SND_SWING, 1, 0.6f);
            g_player.bow_ticks = 0;
        } else g_player.bow_ticks = 0;
        return;
    case USE_SPYGLASS: g_player.using_spyglass = held; return;
    default: break;
    }
    if (!pressed) return;
    // click-to-use items
    if (id == I_MAP) {   // empty map -> new filled map of this area
        int mid = map_create_at(e->pos);
        if (!mid) return;
        ItemStack fm = stack_of(I_FILLED_MAP, 1); fm.data = (u16)mid;
        if (g_player.gamemode != GM_CREATIVE && h->count == 1) *h = fm;
        else { consume_held(h, 1); player_give(fm); }
        sound_play(SND_CLICK, 0.6f, 0.8f);
        return;
    }
    switch (it->use) {
    case USE_THROW: {
        if (it->use_arg == PROJ_ENDER_PEARL && g_player.ender_pearl_cd > 0) return;
        vec3 dir = look_dir(e);
        f32 sp = it->use_arg == PROJ_POTION || it->use_arg == PROJ_XP_BOTTLE ? 18 : 30;
        Entity *pr = proj_spawn(it->use_arg, v3_add(entity_eye(e), v3_scale(dir, 0.4f)), v3_add(v3_scale(dir, sp), v3(0, it->use_arg == PROJ_XP_BOTTLE ? 4 : 0, 0)), e->id);
        if (pr && it->use_arg == PROJ_POTION) pr->item = *h;
        if (it->use_arg == PROJ_ENDER_PEARL) g_player.ender_pearl_cd = TICKRATE;
        consume_held(h, 1);
        sound_play(SND_SWING, 0.6f, 0.8f);
        g_player.swing = 1;
    } break;
    case USE_FISH:   // cast or reel in
        if (g_player.bobber && entity_get(g_player.bobber)) {
            int wear = fishing_reel(e);
            if (wear && g_player.gamemode != GM_CREATIVE) { h->damage += (u16)(wear * (irand(0, ench_level(*h, ENCH_UNBREAKING)) == 0)); if (h->damage >= ITEMS[h->id].durability) { *h = (ItemStack){0}; sound_play(SND_BREAK_GLASS, 0.6f, 0.8f); } }
            sound_play(SND_SWING, 0.5f, 1.4f);
        } else {
            vec3 d = look_dir(e);
            Entity *b = proj_spawn(PROJ_BOBBER, v3_add(entity_eye(e), v3_scale(d, 0.5f)), v3_add(v3_scale(d, 12.0f), v3(0, 2.0f, 0)), e->id);
            if (b) { b->no_gravity = true; b->color = (u8)ench_level(*h, ENCH_LURE); b->anger = ench_level(*h, ENCH_LUCK_OF_THE_SEA); b->special_timer = 0; g_player.bobber = b->id; }
            sound_play(SND_BOW, 0.4f, 1.2f);
        }
        break;
    case USE_INSTANT:
        if (id == I_GHOST_SIGHT_TONIC) { effect_add(e, EFF_GHOST_SIGHT, 0, 180 * TICKRATE); consume_held(h, 1); sound_play(SND_EAT, 0.6f, 1.3f); }
        else if (id == I_PHASING_PEARL) {   // step through up to 10 blocks of wall to the farthest free spot ahead
            vec3 d = look_dir(e); d.y = 0; d = v3_norm(d);
            for (f32 dist = 10; dist >= 1.5f; dist -= 0.5f) {
                vec3 t = v3_add(e->pos, v3_scale(d, dist));
                ivec3 f = v3_floor(t);
                u16 feet = world_get_block(f.x, f.y, f.z), head = world_get_block(f.x, f.y + 1, f.z), below = world_get_block(f.x, f.y - 1, f.z);
                if (!BLOCKS[feet].solid && !BLOCKS[head].solid && BLOCKS[below].solid) {
                    fx_smoke(v3_add(e->pos, v3(0, 1, 0)), 10, 0.2f, rgba_u32(184, 240, 228, 255));
                    e->pos = e->prev_pos = v3(f.x + 0.5f, (f32)f.y, f.z + 0.5f); e->vel = v3(0, 0, 0); e->fall_dist = 0;
                    g_pearl_phases++; consume_held(h, 1); sound_play(SND_SWING, 0.8f, 0.5f);
                    break;
                }
            }
        }
        break;
    case USE_OXYGEN:   // Oxygen Canister: 3 minutes of breathable air anywhere
        effect_add(e, EFF_OXYGEN, 0, 180 * TICKRATE);
        e->air = 900;
        consume_held(h, 1);
        sound_play(SND_SWING, 0.5f, 1.6f);
        break;
    case USE_ENDER_EYE: {
        if (g_world.dim != DIM_OVERWORLD) return;
        ivec3 sh = worldgen_stronghold(g_world.seed);
        vec3 to = v3_norm(v3(sh.x + 0.5f - e->pos.x, 0, sh.z + 0.5f - e->pos.z));
        Entity *pr = proj_spawn(PROJ_EYE, entity_eye(e), v3_scale(to, 8), e->id);
        (void)pr;
        consume_held(h, 1);
        sound_play(SND_SWING, 0.6f, 0.6f);
    } break;
    case USE_FIREWORK:
        if (g_player.gliding) { vec3 d = look_dir(e); e->vel = v3_add(e->vel, v3_scale(d, 12)); consume_held(h, 1); proj_spawn(PROJ_FIREWORK, e->pos, v3(0, 0, 0), e->id); }
        else if (g_player.look.hit) { ivec3 q = g_player.look.prev; proj_spawn(PROJ_FIREWORK, v3(q.x + 0.5f, (f32)q.y, q.z + 0.5f), v3(frand_range(-0.5f, 0.5f), 10, frand_range(-0.5f, 0.5f)), e->id); consume_held(h, 1); }
        break;
    case USE_WIND: break;
    case USE_GOAT_HORN: sound_play(SND_COW_IDLE, 1.0f, 0.5f); g_player.bow_ticks = 0; break;
    case USE_VEHICLE: {
        int vt = vehicle_type_of_item(id);
        if (vt < 0) break;
        RayHit hit = world_raycast(entity_eye(e), look_dir(e), PLAYER_REACH, true);
        if (!hit.hit) break;
        vec3 at;
        if (vehicle_is_cart(vt)) {
            if (BLOCKS[hit.block].place != PL_RAIL) { game_message(COL_WHITE, "Minecarts go on rails"); break; }
            at = v3(hit.pos.x + 0.5f, (f32)hit.pos.y + 0.0625f, hit.pos.z + 0.5f);
        } else {
            ivec3 q = hit.prev;
            at = v3(q.x + 0.5f, (f32)q.y, q.z + 0.5f);
            if (block_is_water(hit.block)) at.y = (f32)hit.pos.y + 0.75f;
        }
        Entity *v = vehicle_spawn(vt, at, e->yaw);
        if (!v) break;
        v->item = *h; v->item.count = 1;
        consume_held(h, 1);
        sound_play(SND_DIG_WOOD, 0.6f, 1.2f);
    } break;
    default:
        if (it->armor_slot >= 0) {   // right-click equips armour (swaps)
            ItemStack old = g_player.inv.armor[it->armor_slot];
            g_player.inv.armor[it->armor_slot] = *h;
            *h = old;
            sound_play(SND_CLICK, 0.6f, 0.8f);
        } else if (id == I_SHIELD || id == I_TOTEM_OF_UNDYING) {}
        break;
    }
}

// ------------------------------------------------------------------------ survival
// Space: is an Oxygen Bloom within 3 blocks? (re-scanned every 10 ticks)
static bool near_oxygen_bloom(Entity *e) {
    static bool cached; static int when = -100;
    if (e->age - when < 10 && e->age >= when) return cached;
    when = e->age; cached = false;
    ivec3 c = v3_floor(e->pos);
    for (int dy = -2; dy <= 3 && !cached; dy++) for (int dz = -3; dz <= 3 && !cached; dz++) for (int dx = -3; dx <= 3; dx++)
        if (world_get_block(c.x + dx, c.y + dy, c.z + dz) == B_OXYGEN_BLOOM) { cached = true; break; }
    return cached;
}
static void survival_tick(Entity *e) {
    if (g_player.ender_pearl_cd > 0) g_player.ender_pearl_cd--;
    g_player.rest_ticks++;
    effects_tick(e);
    if (g_player.gamemode == GM_CREATIVE || g_player.gamemode == GM_SPECTATOR) { e->air = 900; return; }
    // air
    u16 head = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + PLAYER_EYE), (int)floorf(e->pos.z));
    bool turtle = g_player.inv.armor[0].id == I_TURTLE_HELMET && !block_is_water(world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + PLAYER_EYE + 1), (int)floorf(e->pos.z)));
    if (block_is_water(head) && !effect_has(e, EFF_WATER_BREATHING) && !effect_has(e, EFF_CONDUIT_POWER)) {   // Conduit Power: breathe underwater
        if (turtle) effect_add(e, EFF_WATER_BREATHING, 0, 10 * TICKRATE);
        int resp = ench_level(g_player.inv.armor[0], ENCH_RESPIRATION);
        if (!resp || irand(0, resp) == 0) e->air--;
        if (e->air % 40 == 0) fx_bubbles(entity_eye(e), 2);
        if (e->air <= -40) { e->air = 0; entity_damage(e, 2, DMG_DROWN, NULL); }
    } else if (g_world.dim == DIM_END && (g_player.inv.armor[0].id != I_SPACE_HELMET && g_player.inv.armor[0].id != I_DIVINE_HELMET) && !effect_has(e, EFF_OXYGEN) && near_oxygen_bloom(e)) {
        e->air = MINI(900, e->air + 3);   // Oxygen Blooms breathe out air around themselves
    } else if (g_world.dim == DIM_END && (g_player.inv.armor[0].id != I_SPACE_HELMET && g_player.inv.armor[0].id != I_DIVINE_HELMET) && !effect_has(e, EFF_OXYGEN)) {
        // Space is a vacuum: without a Space Helmet or the Oxygen effect your air runs out (twice as fast as underwater)
        e->air -= 2;
        if (e->air <= -40) { e->air = 0; entity_damage(e, 2, DMG_VACUUM, NULL); }
        if (e->age % 60 == 0) { snprintf(G.actionbar, sizeof G.actionbar, "No air in space! Wear a Space Helmet"); G.actionbar_t = 70; }
    } else e->air = MINI(900, e->air + 6);
    if (effect_has(e, EFF_DOLPHINS_GRACE) && block_is_water(world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + 0.5f), (int)floorf(e->pos.z)))) {   // Dolphin's Grace: swim faster
        f32 hs = sqrtf(e->vel.x * e->vel.x + e->vel.z * e->vel.z);
        if (hs > 0.5f && hs < 9) { e->vel.x *= 1.05f; e->vel.z *= 1.05f; }
    }
    if (e->age % 20 == 0 && effect_has(e, EFF_BAD_OMEN)) {   // Bad Omen turns into Trial Omen next to a trial spawner
        ivec3 c = v3_floor(e->pos); bool near = false;
        for (int dy = -4; dy <= 4 && !near; dy++) for (int dz = -8; dz <= 8 && !near; dz++) for (int dx = -8; dx <= 8; dx++) if (BLOCKS[world_get_block(c.x + dx, c.y + dy, c.z + dz)].base == B_TRIAL_SPAWNER) { near = true; break; }
        if (near) { int lvl = effect_level(e, EFF_BAD_OMEN); e->eff_time[EFF_BAD_OMEN] = 0; effect_add(e, EFF_TRIAL_OMEN, 0, (lvl + 1) * 15 * 60 * TICKRATE); game_message(rgba_u32(120, 200, 255, 255), "The trial chamber senses your omen..."); }
    }
    if (effect_has(e, EFF_HAUNTED)) player_exhaust(0.004f * (1 + effect_level(e, EFF_HAUNTED)));   // the chill of the grave
    // Health Boost and Blessing of Vigor: +2 hearts per level each; Vigor also regenerates
    {   f32 want = 20.0f + 4.0f * effect_level(e, EFF_HEALTH_BOOST) + 4.0f * effect_level(e, EFF_BLESSING_OF_VIGOR);
        if (e->max_health != want) { e->max_health = want; if (e->health > want) e->health = want; } }
    if (effect_has(e, EFF_BLESSING_OF_VIGOR) && e->age % (50 * MC_TICK_DIV) == 0 && e->health > 0 && e->health < e->max_health) entity_heal(e, 1);
    // Blessing of Light: purges Haunted, Darkness and Blindness and sears hostile undead close by
    if (effect_has(e, EFF_BLESSING_OF_LIGHT)) {
        e->eff_time[EFF_HAUNTED] = e->eff_time[EFF_DARKNESS] = e->eff_time[EFF_BLINDNESS] = 0;
        if (e->age % (40 * MC_TICK_DIV) == 0) ENT_FOREACH(o) if (o->kind == ENT_MOB && o->death_time == 0 && entity_is_undead(o) && mob_hostile(o->mob) && v3_dist(o->pos, e->pos) < 6) {
            entity_damage(o, 1.0f + effect_level(e, EFF_BLESSING_OF_LIGHT), DMG_MAGIC, e);
            fx_smoke(v3_add(o->pos, v3(0, o->height * 0.6f, 0)), 4, 0.12f, rgba_u32(255, 240, 160, 255));
        }
    }
    // suffocation inside opaque blocks
    if (BLOCKS[head].opaque && BLOCKS[head].solid && e->age % 30 == 0) entity_damage(e, 1, DMG_SUFFOCATE, NULL);
    // hunger
    while (g_player.exhaustion >= 4.0f) {
        g_player.exhaustion -= 4.0f;
        if (g_player.saturation > 0) g_player.saturation = fmaxf(0, g_player.saturation - 1);
        else g_player.food = MAXI(0, g_player.food - 1);
    }
    if (g_level.difficulty == DIFF_PEACEFUL) {   // peaceful: health and hunger refill on their own
        if (e->age % 20 == 0 && e->health < e->max_health && rule(GR_NATURAL_REGENERATION)) e->health = fminf(e->max_health, e->health + 1);
        if (e->age % 40 == 0 && g_player.food < 20) g_player.food++;
    }
    if (g_player.food >= 18 && e->health < e->max_health && rule(GR_NATURAL_REGENERATION) && !effect_has(e, EFF_HAUNTED)) {   // Haunted: no natural healing
        if (++g_player.regen_timer >= (g_player.food >= 20 && g_player.saturation > 0 ? 30 : 240)) {
            g_player.regen_timer = 0;
            e->health = fminf(e->max_health, e->health + 1);
            player_exhaust(6.0f);
        }
    } else g_player.regen_timer = 0;
    if (g_player.food == 0) {
        // starvation stops at 10 health on Easy, 1 on Normal and kills on Hard
        f32 floor_hp = g_level.difficulty == DIFF_EASY ? 10 : g_level.difficulty == DIFF_NORMAL ? 1 : g_level.difficulty == DIFF_HARD ? 0 : 20;
        if (++g_player.starve_timer >= 240) { g_player.starve_timer = 0; if (e->health > floor_hp) entity_damage(e, 1, DMG_STARVE, NULL); }
    } else g_player.starve_timer = 0;
}

static void portal_tick(Entity *e) {
    if (g_player.portal_cooldown > 0) { g_player.portal_cooldown--; g_player.portal_ticks = 0; return; }
    ivec3 f = v3_floor(v3_add(e->pos, v3(0, 0.3f, 0)));
    u16 b = world_get_block(f.x, f.y, f.z), b2 = world_get_block(f.x, f.y + 1, f.z);
    if (BLOCKS[b].base == B_END_PORTAL || BLOCKS[b2].base == B_END_PORTAL) {
        g_player.portal_cooldown = 3 * TICKRATE;
        game_change_dimension(g_world.dim == DIM_END ? DIM_OVERWORLD : DIM_END, true);
        return;
    }
    if (BLOCKS[b].base == B_SIFT_PORTAL || BLOCKS[b2].base == B_SIFT_PORTAL) {
        if (++g_player.portal_ticks >= (g_player.gamemode == GM_CREATIVE ? 1 : 2 * TICKRATE)) {
            g_player.portal_ticks = 0; g_player.portal_cooldown = 5 * TICKRATE;
            game_change_dimension(g_world.dim == DIM_SIFT ? DIM_OVERWORLD : DIM_SIFT, true);
        }
        return;
    }
    if (BLOCKS[b].base == B_NETHER_PORTAL || BLOCKS[b2].base == B_NETHER_PORTAL) {
        if (++g_player.portal_ticks % 40 == 0) sound_play(SND_FIRE, 0.4f, 0.4f);
        if (g_player.portal_ticks >= MAXI(1, (g_player.gamemode == GM_CREATIVE ? rule_i(GR_PLAYERS_NETHER_PORTAL_CREATIVE_DELAY) : rule_i(GR_PLAYERS_NETHER_PORTAL_DEFAULT_DELAY)) * MC_TICK_DIV)) {   // playersNetherPortal*Delay (Hell portals)
            g_player.portal_ticks = 0; g_player.portal_cooldown = 5 * TICKRATE;
            game_change_dimension(g_world.dim == DIM_NETHER ? DIM_OVERWORLD : DIM_NETHER, true);
        }
    } else g_player.portal_ticks = 0;
}

// ------------------------------------------------------------------------ movement
static bool climbable_at(Entity *e) {
    ivec3 f = v3_floor(e->pos);
    for (int dy = 0; dy < 2; dy++) {
        u16 b = world_get_block(f.x, f.y + dy, f.z);
        if (BLOCKS[b].flags & BF_CLIMBABLE) return true;
    }
    return false;
}
static void movement(Entity *e, bool input) {
    bool fwd = input && key_down(KEY(KB_FORWARD)), back = input && key_down(KEY(KB_BACK));
    bool left = input && key_down(KEY(KB_LEFT)), right = input && key_down(KEY(KB_RIGHT));
    bool jump = input && key_down(KEY(KB_JUMP));
    static bool sneak_t, sprint_t;   // toggle mode: a press switches it on or off (once per frame, not per tick)
    static unsigned long sneak_f, sprint_f;
    if (G.set.toggle_sneak && input && key_pressed(KEY(KB_SNEAK)) && sneak_f != g_win.frame) { sneak_t = !sneak_t; sneak_f = g_win.frame; }
    if (G.set.toggle_sprint && input && key_pressed(KEY(KB_SPRINT)) && sprint_f != g_win.frame) { sprint_t = !sprint_t; sprint_f = g_win.frame; }
    if (!G.set.toggle_sprint) sprint_t = false;
    bool sneak_in = G.set.toggle_sneak ? sneak_t : key_down(KEY(KB_SNEAK));
    bool sprint_in = G.set.toggle_sprint ? sprint_t : key_down(KEY(KB_SPRINT));
    g_player.sneaking = input && sneak_in && !g_player.flying;
    if (input && sprint_in && fwd && !g_player.sneaking && (g_player.food > 6 || !gm_survival_like(g_player.gamemode))) g_player.sprinting = true;
    if (!fwd || g_player.sneaking || e->h_collision || (g_player.food <= 6 && gm_survival_like(g_player.gamemode))) g_player.sprinting = false;

    f32 fx = sinf(e->yaw), fz = cosf(e->yaw);
    vec3 f = v3(fx, 0, fz), r = v3(-fz, 0, fx);
    vec3 dir = v3(0, 0, 0);
    if (fwd) dir = v3_add(dir, f);
    if (back) dir = v3_sub(dir, f);
    if (left) dir = v3_sub(dir, r);
    if (right) dir = v3_add(dir, r);
    dir = v3_norm(dir);

    if (g_player.gamemode == GM_SPECTATOR) g_player.flying = true;
    e->no_clip = g_player.gamemode == GM_SPECTATOR;
    e->no_gravity = g_player.flying;
    if (g_player.flying) {
        f32 sp = g_player.sprinting ? FLY_SPEED * 2 : FLY_SPEED;
        vec3 want = v3_scale(dir, sp);
        e->vel.x += (want.x - e->vel.x) * 0.15f;
        e->vel.z += (want.z - e->vel.z) * 0.15f;
        f32 vy = (jump ? 1.0f : 0.0f) - (input && key_down(KEY(KB_SNEAK)) ? 1.0f : 0.0f);
        e->vel.y += (vy * 8.0f - e->vel.y) * 0.2f;
        if (e->on_ground && vy < 0 && g_player.gamemode != GM_SPECTATOR) g_player.flying = false;
        return;
    }
    // elytra gliding
    if (g_player.gliding) {
        if (e->on_ground || e->in_water || g_player.inv.armor[1].id != I_ELYTRA) { g_player.gliding = false; }
        else {
            vec3 look = look_dir(e);
            f32 hs = sqrtf(e->vel.x * e->vel.x + e->vel.z * e->vel.z), sp = v3_len(e->vel);
            f32 pitch = e->pitch;
            e->vel.y += (-32.0f + cosf(pitch) * cosf(pitch) * 24.0f) * DT;
            if (e->vel.y < 0 && hs > 0.1f) { f32 lift = -e->vel.y * 0.1f * cosf(pitch) * cosf(pitch); e->vel.y += lift; e->vel.x += look.x / fmaxf(0.01f, sqrtf(look.x * look.x + look.z * look.z)) * lift; e->vel.z += look.z / fmaxf(0.01f, sqrtf(look.x * look.x + look.z * look.z)) * lift; }
            if (pitch > 0) { f32 climb = sp * sinf(pitch) * 0.04f; e->vel.y += climb * 3.2f; e->vel.x -= look.x * climb; e->vel.z -= look.z * climb; }
            f32 hl = sqrtf(look.x * look.x + look.z * look.z);
            if (hl > 0.01f) { e->vel.x += (look.x / hl * hs - e->vel.x) * 0.1f; e->vel.z += (look.z / hl * hs - e->vel.z) * 0.1f; }
            e->vel = v3_scale(e->vel, 0.995f);
            e->fall_dist = 0;
            if (e->h_collision && hs > 10) entity_damage(e, (hs - 10) * 0.5f, DMG_WALL, NULL);
            if (e->age % TICKRATE == 0) { ItemStack *el = &g_player.inv.armor[1]; if (g_player.gamemode != GM_CREATIVE && ++el->damage >= ITEMS[I_ELYTRA].durability - 1) { el->damage = ITEMS[I_ELYTRA].durability - 1; g_player.gliding = false; } }
            return;
        }
    }
    f32 speed = g_player.sneaking ? SNEAK_SPEED * (1.0f + 0.45f * ench_level(g_player.inv.armor[2], ENCH_SWIFT_SNEAK)) : g_player.sprinting ? SPRINT_SPEED : WALK_SPEED;
    {
        u16 under = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y - 0.2f), (int)floorf(e->pos.z));
        int ss = ench_level(g_player.inv.armor[3], ENCH_SOUL_SPEED);
        if (ss && (under == B_SOUL_SAND || under == B_SOUL_SOIL)) speed *= 1.6f + 0.25f * ss;
        int fw = ench_level(g_player.inv.armor[3], ENCH_FROST_WALKER);
        if (fw && e->on_ground) for (int dz = -1 - fw; dz <= 1 + fw; dz++) for (int dx = -1 - fw; dx <= 1 + fw; dx++) {
            if (dx * dx + dz * dz > (1 + fw) * (1 + fw)) continue;
            int x = (int)floorf(e->pos.x) + dx, y = (int)floorf(e->pos.y - 0.2f), z = (int)floorf(e->pos.z) + dz;
            if (world_get_block(x, y, z) == B_WATER && block_is_air(world_get_block(x, y + 1, z))) { world_set_block_silent(x, y, z, B_FROSTED_ICE); world_schedule_update(iv3(x, y, z), irand(60, 120)); }
        }
    }
    speed *= 1.0f + 0.2f * effect_level(e, EFF_SPEED) - 0.15f * effect_level(e, EFF_SLOWNESS);
    if (g_player.blocking || g_player.eating_ticks > 0 || g_player.bow_ticks > 0) speed *= 0.35f;
    if (speed < 0) speed = 0;
    if (e->in_water || e->in_lava) {
        speed *= e->in_lava ? 0.35f : (g_player.sprinting ? 1.1f : 0.55f) * (1.0f + 0.33f * ench_level(g_player.inv.armor[3], ENCH_DEPTH_STRIDER));
        vec3 want = v3_scale(dir, speed);
        if (g_player.sprinting && e->in_water) want = v3_scale(look_dir(e), speed);
        e->vel.x += (want.x - e->vel.x) * 0.12f;
        e->vel.z += (want.z - e->vel.z) * 0.12f;
        if (g_player.sprinting && e->in_water) e->vel.y += (want.y - e->vel.y) * 0.12f;
        else if (jump) e->vel.y = fminf(e->vel.y + 26.0f * DT, e->h_collision ? 5.0f : 3.0f);
        else if (g_player.sneaking) e->vel.y -= 10.0f * DT;
        if (e->age % 30 == 0 && v3_len(e->vel) > 1) sound_play(SND_SWIM, 0.15f, frand_range(0.8f, 1.2f));
        return;
    }
    vec3 want = v3_scale(dir, speed);
    f32 k = e->on_ground ? (e->slippery > 0 ? 0.05f : 0.3f) : 0.045f;
    e->vel.x += (want.x - e->vel.x) * k;
    e->vel.z += (want.z - e->vel.z) * k;
    // ladders, vines, scaffolding
    if (climbable_at(e)) {
        e->fall_dist = 0;
        if ((e->h_collision && (fwd || jump)) || (jump && world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z)) == B_SCAFFOLDING)) e->vel.y = 2.35f;
        else if (g_player.sneaking) e->vel.y = 0;
        else e->vel.y = fmaxf(e->vel.y, -2.35f);
    }
    if (jump && e->on_ground && g_player.place_cooldown >= 0) {
        g_stats[STAT_JUMPS]++;
        e->vel.y = JUMP_VELOCITY + 1.9f * effect_level(e, EFF_JUMP_BOOST) + (effect_has(e, EFF_BLESSING_OF_THE_SKY) ? 3.0f : 0.0f);
        u16 below = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y - 0.1f), (int)floorf(e->pos.z));
        if (below == B_HONEY_BLOCK) e->vel.y *= 0.5f;
        if (g_player.sprinting) { e->vel.x += fx * 1.6f; e->vel.z += fz * 1.6f; player_exhaust(0.2f); }
        else player_exhaust(0.05f);
    }
    // start gliding: jump while falling with an elytra
    if (input && key_pressed(KEY(KB_JUMP)) && !e->on_ground && e->vel.y < 0 && g_player.inv.armor[1].id == I_ELYTRA && ITEMS[I_ELYTRA].durability - g_player.inv.armor[1].damage > 1) g_player.gliding = true;
    if (g_player.sneaking && e->on_ground) {
        f32 dx = e->vel.x * DT, dz = e->vel.z * DT;
        if (dx != 0 && !ground_under(e, dx, 0)) e->vel.x = 0;
        if (dz != 0 && !ground_under(e, 0, dz)) e->vel.z = 0;
        if (e->vel.x != 0 && e->vel.z != 0 && !ground_under(e, e->vel.x * DT, e->vel.z * DT)) e->vel.x = e->vel.z = 0;
    }
}

void player_tick(void) {
    Entity *e = player_ent();
    if (!e) return;
    e->prev_pos = e->pos;
    e->prev_pitch = e->pitch;
    g_player.prev_swing = g_player.swing;
    g_player.prev_bob = g_player.bob;
    g_player.prev_bob_amount = g_player.bob_amount;
    e->age++;
    if (e->hurt_time > 0) e->hurt_time--;
    if (e->invuln > 0) e->invuln--;
    g_player.hurt_tilt *= 0.9f;
    if (g_player.dead) { g_player.death_ticks++; return; }
    g_player.attack_ticks++;
    if (g_player.swing > 0) { g_player.swing += 0.06f; if (g_player.swing >= 1.0f) g_player.swing = 0; }
    if (g_player.sleeping) {   // fast-forward the night
        e->vel = v3(0, 0, 0);
        if (++g_player.use_ticks > 2 * TICKRATE && rule_i(GR_PLAYERS_SLEEPING_PERCENTAGE) <= 100) {   // playersSleepingPercentage > 100: nobody can skip the night
            if (world_is_night()) g_world.day_shift += (s64)((world_daytime() / DAY_TICKS + 1) * DAY_TICKS - world_daytime());
            weather_sleep_reset();
            g_player.sleeping = false; g_player.rest_ticks = 0; g_player.use_ticks = 0;
            game_message(COL_WHITE, "Good morning!");
            progress_grant("bed");
        }
        return;
    }

    bool input = G.screen == SCR_NONE && (g_win.focused || G.autotest);   // scripted runs must not depend on which window has the focus
    if (!world_loaded((int)floorf(e->pos.x), (int)floorf(e->pos.z))) return;
    if (e->mount) {   // riding: the mount moves, we only read the controls
        Entity *m = entity_get(e->mount);
        g_player.ride_fwd = g_player.ride_strafe = 0; g_player.ride_jump = false;
        if (!m || m->remove || m->health <= 0) { e->mount = 0; }
        else {
            if (input) {
                g_player.ride_fwd = (f32)(key_down(KEY(KB_FORWARD)) - key_down(KEY(KB_BACK)));
                g_player.ride_strafe = (f32)(key_down(KEY(KB_LEFT)) - key_down(KEY(KB_RIGHT)));
                g_player.ride_jump = key_down(KEY(KB_JUMP));
                if (key_pressed(KEY(KB_SNEAK))) { ride_dismount(e); return; }
            }
            g_player.sneaking = false; g_player.sprinting = false;
            e->vel = v3(0, 0, 0); e->fall_dist = 0;
            return;
        }
    }
    movement(e, input);
    vec3 before = e->pos;
    entity_physics(e);
    if (g_player.flying) e->fall_dist = 0;
    portal_tick(e);
    if (g_player.dim_changed) { g_player.dim_changed = false; return; }

    f32 moved = sqrtf((e->pos.x - before.x) * (e->pos.x - before.x) + (e->pos.z - before.z) * (e->pos.z - before.z));
    if (e->on_ground && !g_player.flying) {
        g_player.step_acc += moved;
        progress_moved(moved);
        if (g_player.sprinting) player_exhaust(0.1f * moved);
        if (g_player.step_acc > (g_player.sneaking ? 1.6f : 2.1f)) {
            g_player.step_acc = 0;
            int sg = sound_group_below(e);
            if (sg >= 0 && !g_player.sneaking) sound_play(SND_STEP_STONE + MINI(sg, SG_SNOW), 0.25f, frand_range(0.85f, 1.15f));
            if (!g_player.sneaking && gm_survival_like(g_player.gamemode)) sculk_vibration(e->pos, 1, true);   // sneaking is silent
        }
        g_player.bob += moved * 1.3f;
        g_player.bob_amount += (fminf(1.0f, moved * TICKRATE / WALK_SPEED) - g_player.bob_amount) * 0.25f;
    } else g_player.bob_amount *= 0.85f;
    g_player.eye_offset += ((g_player.sneaking ? 0.25f : 0.0f) - g_player.eye_offset) * 0.3f;

    bool lmb = input && g_win.mouse_down[GLFW_MOUSE_BUTTON_LEFT];
    bool rmb = input && g_win.mouse_down[GLFW_MOUSE_BUTTON_RIGHT];
    mining_tick(e, lmb);
    use_tick(e, rmb, false);
    ItemStack *h = inventory_held(&g_player.inv);
    // holding right click keeps placing every 1/5 second like Minecraft
    if (rmb && g_player.look.hit && !g_player.eating_ticks && !g_player.bow_ticks && ITEMS[h->id].block && !g_player.blocking) {
        static int rep;
        if (++rep >= 12) { rep = 0; place_block(e); }
    }
    survival_tick(e);
    f32 fov_target = g_player.sprinting ? 0.15f : 0;
    if (effect_has(e, EFF_SPEED)) fov_target += 0.05f * effect_level(e, EFF_SPEED);
    if (g_player.bow_ticks > 0 && ITEMS[h->id].use == USE_BOW) fov_target -= fminf(1.0f, g_player.bow_ticks / (f32)TICKRATE) * 0.15f;
    if (g_player.using_spyglass) fov_target = -0.8f;
    g_player.fov_kick += (fov_target - g_player.fov_kick) * 0.15f;
}

// per-frame: look, targeting and click actions
void player_frame(f32 dt) {
    (void)dt;
    Entity *e = player_ent();
    if (!e) return;
    bool input = G.screen == SCR_NONE && (g_win.grabbed || G.autotest) && !g_player.dead && !g_player.sleeping;
    if (input) {
        f32 s = 0.0022f * (0.3f + G.set.sensitivity * 1.4f) * (g_player.using_spyglass ? 0.15f : 1.0f);
        e->yaw -= (f32)g_win.mouse_dx * s;
        e->pitch -= (f32)g_win.mouse_dy * s * (G.set.invert_y ? -1 : 1);
        e->pitch = clampf(e->pitch, -1.55f, 1.55f);
        e->yaw = wrap_angle(e->yaw);
        for (int i = 0; i < 9; i++) if (key_pressed(GLFW_KEY_1 + i)) { g_player.inv.selected = i; g_player.crossbow_loaded = false; }
        if (g_win.scroll != 0) {
            int d = g_win.scroll > 0 ? -1 : 1;
            g_player.inv.selected = (g_player.inv.selected + d + 9) % 9;
        }
        if (key_pressed(KEY(KB_DROP))) player_drop_selected(key_down(KEY(KB_SPRINT)));
        if (key_pressed(KEY(KB_SWAP))) { ItemStack t = g_player.inv.offhand; g_player.inv.offhand = *inventory_held(&g_player.inv); *inventory_held(&g_player.inv) = t; }
        if (key_pressed(KEY(KB_JUMP)) && g_player.gamemode == GM_CREATIVE) {
            if ((int)(G.ticks - g_player.last_space_tap) < 18) { g_player.flying = !g_player.flying; e->vel.y = 0; }
            g_player.last_space_tap = (int)G.ticks;
        }
    }
    vec3 eye = v3_add(e->pos, v3(0, PLAYER_EYE - g_player.eye_offset, 0));
    vec3 dir = look_dir(e);
    f32 reach = g_player.gamemode == GM_CREATIVE ? 6.0f : PLAYER_REACH;
    g_player.look = world_raycast(eye, dir, reach, false);
    g_player.look_entity = 0;
    f32 best = g_player.look.hit ? g_player.look.dist : PLAYER_ATTACK_REACH + 1;
    ENT_FOREACH(o) {
        if ((o->kind != ENT_MOB && o->kind != ENT_CRYSTAL && o->kind != ENT_VEHICLE && o->kind != ENT_DECOR && !(o->kind == ENT_PROJ && o->proj == PROJ_FIREBALL)) || o->death_time > 0) continue;
        if (o->id == e->mount || o->mount == e->id) continue;
        AABB box = o->kind == ENT_DECOR ? decor_aabb(o) : entity_aabb(o);
        box.min = v3_sub(box.min, v3(0.1f, 0.1f, 0.1f)); box.max = v3_add(box.max, v3(0.1f, 0.1f, 0.1f));
        f32 t = aabb_ray(box, eye, dir);
        if (t >= 0 && t < best && t <= PLAYER_ATTACK_REACH + (o->width > 3 ? 3 : 0)) { best = t; g_player.look_entity = o->id; }
    }
    if (!input) return;
    if (g_player.gamemode == GM_SPECTATOR) { g_player.look.hit = false; g_player.look_entity = 0; return; }   // spectators only watch
    if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) {
        Entity *t = entity_get(g_player.look_entity);
        if (t && t->kind == ENT_VEHICLE) { vehicle_hit(t, e); g_player.swing = 1.0f; }
        else if (t && t->kind == ENT_DECOR) { decor_hit(t, e); g_player.swing = 1.0f; }
        else if (t) attack(e, t);
        else if (!g_player.look.hit) { g_player.swing = 1.0f; g_player.attack_ticks = 0; }
    }
    if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_RIGHT]) {
        Entity *t = entity_get(g_player.look_entity);
        ItemStack *h = inventory_held(&g_player.inv);
        bool done = false;
        if (t && t->kind == ENT_MOB) done = mob_interact(t, h) || ride_mob_interact(t, h);
        if (t && t->kind == ENT_VEHICLE) done = vehicle_interact(t, h);
        if (t && t->kind == ENT_DECOR) done = decor_interact(t, h);
        if (!done && !g_player.sneaking) done = interact_block(e);
        if (!done && g_player.look.hit && h->id && ITEMS[h->id].use == USE_DECOR) done = decor_place(e, h);
        if (!done && g_player.look.hit) done = use_item_on_block(e, h);
        if (!done && g_player.look.hit) done = place_block(e);
        if (!done) use_tick(e, true, true);
        if (done) g_player.swing = 1.0f;
    }
    if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_MIDDLE] && g_player.look.hit) {
        u16 b = g_player.look.block;
        int id = BLOCKS[b].base;
        if (BLOCKS[b].drop != DROP_NOTHING && !item_valid(id)) id = BLOCKS[b].drop;
        if (b >= B_WALL_TORCH && b < B_WALL_TORCH + 4) id = B_TORCH;
        if (item_valid(id)) {
            int found = -1;
            for (int i = 0; i < 9; i++) if (g_player.inv.slots[i].id == id) found = i;
            if (found >= 0) g_player.inv.selected = found;
            else if (g_player.gamemode == GM_CREATIVE) g_player.inv.slots[g_player.inv.selected] = stack_of(id, ITEMS[id].max_stack);
        }
    }
}

// scripted tests: right-click the held item in the air
void player_use_held_for_tests(void) { Entity *e = player_ent(); if (e) use_tick(e, true, true); }

void player_break_block_for_tests(ivec3 p) { Block b = world_get_block(p.x, p.y, p.z); if (!block_is_air(b)) break_block(p, b); }
