// Riding: boats, minecarts on rails, and rideable mobs (horses, donkeys, mules, camels, skeleton /
// zombie horses, llamas, pigs with a carrot on a stick, striders with a warped fungus on a stick).
#include "entity.h"
#include "vehicle.h"
#include "../world/world.h"
#include "../game/player.h"
#include "../game/game.h"
#include "../game/fx.h"
#include "../audio/audio.h"

static const f32 VEH_W[VEH_COUNT] = {1.375f, 1.375f, 0.98f, 0.98f, 0.98f, 0.98f, 0.98f};
static const f32 VEH_H[VEH_COUNT] = {0.5625f, 0.5625f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f};

bool vehicle_is_cart(int t) { return t >= VEH_MINECART; }
int vehicle_type_of_item(int id) {
    if (!item_valid(id)) return -1;
    const char *k = ITEMS[id].key;
    if (!strcmp(k, "minecart")) return VEH_MINECART;
    if (!strcmp(k, "chest_minecart")) return VEH_CHEST_MINECART;
    if (!strcmp(k, "furnace_minecart")) return VEH_FURNACE_MINECART;
    if (!strcmp(k, "tnt_minecart")) return VEH_TNT_MINECART;
    if (!strcmp(k, "hopper_minecart")) return VEH_HOPPER_MINECART;
    if (strstr(k, "chest_boat") || strstr(k, "chest_raft")) return VEH_CHEST_BOAT;
    if (strstr(k, "_boat") || strstr(k, "_raft")) return VEH_BOAT;
    return -1;
}
ivec3 vehicle_storage_pos(Entity *v) { return iv3(v->special_timer & 0xFFFF, -500, (v->special_timer >> 16) & 0x7FFF); }

Entity *vehicle_spawn(int type, vec3 pos, f32 yaw) {
    if (type < 0 || type >= VEH_COUNT) return NULL;
    Entity *v = entity_spawn(ENT_VEHICLE, pos);
    if (!v) return NULL;
    v->vtype = (u8)type;
    v->width = VEH_W[type]; v->height = VEH_H[type];
    v->yaw = v->body_yaw = yaw;
    v->health = v->max_health = 6;
    v->persistent = true;
    v->special_timer = irand(1, 0x7FFFFFFF);   // key of the chest storage
    v->no_gravity = true;                       // vehicles do their own gravity / buoyancy
    return v;
}

static bool world_box_collides(AABB box) {
    ivec3 a = v3_floor(box.min), b = v3_floor(box.max);
    AABB bb[8];
    for (int y = a.y; y <= b.y; y++) for (int z = a.z; z <= b.z; z++) for (int x = a.x; x <= b.x; x++) {
        int n = block_boxes(world_get_block(x, y, z), x, y, z, bb, 8, true);
        for (int i = 0; i < n; i++) if (aabb_overlap(bb[i], box)) return true;
    }
    return false;
}

// ------------------------------------------------------------------ mounting
static Entity *rider_of(Entity *m) { Entity *r = m->rider ? entity_get(m->rider) : NULL; if (r && r->mount != m->id) { m->rider = 0; return NULL; } return r; }
bool ride_mount(Entity *rider, Entity *m) {
    if (!rider || !m || m->rider || rider->mount) return false;
    m->rider = rider->id; rider->mount = m->id;
    rider->vel = v3(0, 0, 0);
    if (rider->kind == ENT_PLAYER) { g_player.sneaking = false; g_player.flying = false; }
    return true;
}
void ride_dismount(Entity *rider) {
    Entity *m = rider ? entity_get(rider->mount) : NULL;
    if (!rider) return;
    rider->mount = 0;
    if (!m) return;
    m->rider = 0;
    // step off to the side where there is room
    for (int k = 0; k < 5; k++) {
        f32 a = m->yaw + PI_F / 2 + k * PI_F / 2;
        vec3 p = v3_add(m->pos, v3(sinf(a) * (m->width * 0.5f + 0.6f), k == 4 ? m->height : 0.1f, cosf(a) * (m->width * 0.5f + 0.6f)));
        AABB box = {v3(p.x - 0.3f, p.y, p.z - 0.3f), v3(p.x + 0.3f, p.y + 1.8f, p.z + 0.3f)};
        if (!world_box_collides(box)) { rider->pos = rider->prev_pos = p; rider->vel = v3(0, 0, 0); return; }
    }
    rider->pos = v3_add(m->pos, v3(0, m->height + 0.1f, 0));
}
vec3 ride_seat(Entity *m) {
    if (m->kind == ENT_VEHICLE) return v3_add(m->pos, v3(0, vehicle_is_cart(m->vtype) ? 0.05f : -0.15f, 0));
    f32 h = m->height * 0.72f;
    if (m->mob == MOB_PIG) h = m->height * 0.85f;
    if (m->mob == MOB_STRIDER) h = m->height * 0.95f;
    if (m->mob == MOB_CAMEL) h = m->height * 0.82f;
    return v3_add(m->pos, v3(0, h, 0));
}
// called after mounts tick: carry riders along
void ride_update_rider(Entity *m) {
    Entity *r = rider_of(m);
    if (!r) return;
    r->pos = ride_seat(m);
    r->vel = v3(0, 0, 0); r->fall_dist = 0; r->on_ground = true;
    if (r->kind == ENT_MOB) r->yaw = r->body_yaw = m->yaw;
}

// ------------------------------------------------------------------ boats
static void boat_tick(Entity *v, Entity *r) {
    ivec3 c = v3_floor(v3_add(v->pos, v3(0, 0.25f, 0)));
    Block in = world_get_block(c.x, c.y, c.z), below = world_get_block(c.x, c.y - 1, c.z), above = world_get_block(c.x, c.y + 1, c.z);
    bool water = block_is_water(in) || block_is_water(below);
    bool under = block_is_water(above);
    // steering
    if (r && r->kind == ENT_PLAYER) {
        f32 turn = -g_player.ride_strafe * 0.045f;
        v->yaw += turn; r->yaw += turn;
        f32 acc = g_player.ride_fwd > 0 ? 0.9f : g_player.ride_fwd < 0 ? -0.25f : 0;
        if (g_player.ride_strafe != 0 && g_player.ride_fwd == 0) acc = 0.1f;
        v->vel.x += sinf(v->yaw) * acc * DT * 9; v->vel.z += cosf(v->yaw) * acc * DT * 9;
    }
    if (water) {
        f32 surface = (f32)c.y + (block_is_water(in) ? fluid_height(in) : 0) - 0.12f;
        if (!block_is_water(in)) surface = (f32)c.y - 0.12f;
        if (under) v->vel.y += 14 * DT;   // pushed up through water
        else v->vel.y = (surface - v->pos.y) * 6.0f;
        vec3 flow = fluid_flow(c.x, c.y, c.z);
        v->vel.x += flow.x * 1.5f * DT; v->vel.z += flow.z * 1.5f * DT;
        v->vel.x *= 0.955f; v->vel.z *= 0.955f;
    } else {
        v->vel.y -= 20 * DT;
        Block g = world_get_block(c.x, (int)floorf(v->pos.y - 0.1f), c.z);
        f32 slip = (g == B_ICE || g == B_PACKED_ICE || g == B_FROSTED_ICE) ? 0.985f : g == B_BLUE_ICE ? 0.995f : 0.6f;
        v->vel.x *= slip; v->vel.z *= slip;
    }
    entity_physics(v);
    v->body_yaw = v->yaw;
}

// ------------------------------------------------------------------ minecarts
static bool rail_cell(vec3 p, ivec3 *cell, Block *b) {
    ivec3 c = v3_floor(p);
    Block x = world_get_block(c.x, c.y, c.z);
    if (BLOCKS[x].place == PL_RAIL) { *cell = c; *b = x; return true; }
    x = world_get_block(c.x, c.y - 1, c.z);
    if (BLOCKS[x].place == PL_RAIL) { *cell = iv3(c.x, c.y - 1, c.z); *b = x; return true; }
    return false;
}
static void rail_endpoints(ivec3 c, Block b, vec3 *p0, vec3 *p1) {
    int ends[2]; bool up[2];
    rs_rail_ends(rs_rail_shape(b), ends, up);
    vec3 m = v3(c.x + 0.5f, (f32)c.y, c.z + 0.5f);
    vec3 e[2];
    for (int i = 0; i < 2; i++) {
        int f = H4_FACE[ends[i]];
        e[i] = v3(m.x + FACE_DIR[f].x * 0.5f, m.y + (up[i] ? 1.0f : 0.0f), m.z + FACE_DIR[f].z * 0.5f);
    }
    *p0 = e[0]; *p1 = e[1];
}
static void cart_explode(Entity *v) {
    vec3 p = v->pos;
    v->remove = true;
    explosion(p, 4.0f, NULL);
}
static void cart_tick(Entity *v, Entity *r) {
    ivec3 c; Block b;
    bool on = rail_cell(v->pos, &c, &b);
    if (r && r->kind == ENT_PLAYER && g_player.ride_fwd > 0) {   // push in the look direction
        v->vel.x += sinf(r->yaw) * 2.5f * DT; v->vel.z += cosf(r->yaw) * 2.5f * DT;
    }
    if (v->vtype == VEH_FURNACE_MINECART && v->grow_age > 0) {   // fuelled: keeps pushing
        v->grow_age--;
        f32 sp = v3_len(v3(v->vel.x, 0, v->vel.z));
        vec3 d = sp > 0.01f ? v3_scale(v3(v->vel.x, 0, v->vel.z), 1 / sp) : v3(sinf(v->yaw), 0, cosf(v->yaw));
        v->vel.x += d.x * 3 * DT; v->vel.z += d.z * 3 * DT;
        if (v->age % 4 == 0) fx_smoke(v3_add(v->pos, v3(0, 0.9f, 0)), 1, 0.05f, rgba_u32(60, 60, 60, 255));
    }
    if (!on) {
        v->vel.y -= 20 * DT;
        if (v->on_ground) { v->vel.x *= 0.5f; v->vel.z *= 0.5f; }
        entity_physics(v);
        return;
    }
    int k = BLOCKS[b].base;
    bool powered = BLOCKS[b].var >= 6 && k != B_RAIL;
    vec3 p0, p1;
    rail_endpoints(c, b, &p0, &p1);
    vec3 dir = v3_sub(p1, p0);
    f32 len = v3_len(dir);
    dir = v3_scale(dir, 1 / len);
    // project velocity onto the rail (keep the sign of travel)
    f32 sp = v3_dot(v->vel, dir);
    sp -= dir.y * 20 * DT * 0.5f;   // slopes: gravity along the rail
    if (k == B_POWERED_RAIL) {
        if (powered) {
            if (fabsf(sp) < 0.05f) {   // start moving away from a solid block at the end
                vec3 a = v3_add(p0, v3_scale(dir, -0.6f)), bb = v3_add(p1, v3_scale(dir, 0.6f));
                if (BLOCKS[world_get_block((int)floorf(a.x), c.y, (int)floorf(a.z))].solid) sp = 2;
                else if (BLOCKS[world_get_block((int)floorf(bb.x), c.y, (int)floorf(bb.z))].solid) sp = -2;
            }
            sp += (sp >= 0 ? 1 : -1) * 18 * DT;
        } else { sp *= 0.5f; if (fabsf(sp) < 0.1f) sp = 0; }
    }
    if (k == B_ACTIVATOR_RAIL && powered) {
        if (v->vtype == VEH_TNT_MINECART) { cart_explode(v); return; }
        if (r) ride_dismount(r);
    }
    if (k == B_DETECTOR_RAIL) rs_entity_on(c, v);
    sp *= r ? 0.997f : 0.99f;
    sp = clampf(sp, -8.0f, 8.0f);
    // snap onto the rail line and advance
    vec3 rel = v3_sub(v->pos, p0);
    f32 t = clampf(v3_dot(rel, dir), 0, len);
    vec3 onl = v3_add(p0, v3_scale(dir, t));
    v->pos.x = onl.x; v->pos.z = onl.z; v->pos.y = onl.y + 0.0625f;
    vec3 step = v3_scale(dir, sp * DT);
    vec3 np = v3_add(v->pos, step);
    // leaving the cell: continue into the neighbour (if it is a rail the next tick redirects us)
    AABB box = {v3(np.x - 0.45f, np.y + 0.05f, np.z - 0.45f), v3(np.x + 0.45f, np.y + 0.7f, np.z + 0.45f)};
    if (world_box_collides(box)) { sp = -sp * 0.3f; np = v->pos; if (v->vtype == VEH_TNT_MINECART && fabsf(sp) > 2) { cart_explode(v); return; } }
    v->pos = np;
    v->vel = v3_scale(dir, sp);
    if (fabsf(dir.x) + fabsf(dir.z) > 0.01f && fabsf(sp) > 0.01f) v->yaw = atan2f(dir.x * (sp >= 0 ? 1 : -1), dir.z * (sp >= 0 ? 1 : -1));
    v->body_yaw = v->yaw;
    v->on_ground = true;
    // hopper minecart collects items
    if (v->vtype == VEH_HOPPER_MINECART) {
        BlockEntity *be = world_block_entity(vehicle_storage_pos(v), true, BE_HOPPER);
        ENT_FOREACH(o) if (o->kind == ENT_ITEM && o->pickup_delay <= 0 && v3_dist(o->pos, v->pos) < 1.2f) {
            for (int i = 0; i < 5 && !stack_empty(o->item); i++) {
                ItemStack *s = &be->slots[i];
                if (stack_empty(*s)) { *s = o->item; o->item = (ItemStack){0}; }
                else if (stack_can_merge(*s, o->item)) { int m = MINI(o->item.count, stack_max(*s) - s->count); s->count += (u8)m; o->item.count -= (u8)m; if (!o->item.count) o->item = (ItemStack){0}; }
            }
            if (stack_empty(o->item)) o->remove = true;
        }
    }
    // mobs walking into an empty cart get in
    if (!r && v->vtype == VEH_MINECART && fabsf(sp) > 0.5f) {
        ENT_FOREACH(o) if (o->kind == ENT_MOB && !o->mount && !mob_hostile(o->mob) && o->width < 1 && v3_dist(o->pos, v->pos) < 0.8f) { ride_mount(o, v); break; }
    }
}

void vehicle_tick(Entity *v) {
    Entity *r = rider_of(v);
    if (v->hurt_time == 0 && v->health < v->max_health) v->health = fminf(v->max_health, v->health + 0.05f);
    if (vehicle_is_cart(v->vtype)) cart_tick(v, r); else boat_tick(v, r);
    if (v->pos.y < -64) v->remove = true;
    if (!v->remove) ride_update_rider(v);
}

void vehicle_break(Entity *v, bool drop) {
    Entity *r = rider_of(v);
    if (r) ride_dismount(r);
    vec3 c = v3_add(v->pos, v3(0, 0.3f, 0));
    if (drop) {
        ItemStack it = v->item;
        if (stack_empty(it)) it = stack_of(v->vtype == VEH_BOAT ? I_OAK_BOAT : v->vtype == VEH_CHEST_BOAT ? I_OAK_CHEST_BOAT : I_MINECART, 1);
        item_drop_scatter(c, it);
    }
    if (v->vtype == VEH_CHEST_BOAT || v->vtype == VEH_CHEST_MINECART || v->vtype == VEH_HOPPER_MINECART) {
        BlockEntity *be = world_block_entity(vehicle_storage_pos(v), false, 0);
        if (be) { for (int i = 0; i < 27; i++) if (!stack_empty(be->slots[i])) { item_drop_scatter(c, be->slots[i]); be->slots[i] = (ItemStack){0}; } }
    }
    fx_poof(v->pos, v->width, v->height);
    sound_play_at(SND_DIG_WOOD, v->pos, 0.8f, 1.0f);
    v->remove = true;
}
// player hits a vehicle
void vehicle_hit(Entity *v, Entity *by) {
    (void)by;
    v->hurt_time = 10;
    v->health -= 2.5f;
    sound_play_at(SND_HIT, v->pos, 0.5f, 1.2f);
    if (g_player.gamemode == GM_CREATIVE) { vehicle_break(v, false); return; }
    if (v->health <= 0) vehicle_break(v, true);
}
// player right-clicks a vehicle
bool vehicle_interact(Entity *v, ItemStack *held) {
    Entity *pl = player_ent();
    if (!pl) return false;
    if (v->vtype == VEH_FURNACE_MINECART && (held->id == I_COAL || held->id == I_CHARCOAL)) {
        v->grow_age += 3600;
        v->yaw = pl->yaw;
        if (g_player.gamemode != GM_CREATIVE) { if (--held->count == 0) *held = (ItemStack){0}; }
        return true;
    }
    if (v->vtype == VEH_TNT_MINECART) { if (held->id == I_FLINT_AND_STEEL || held->id == I_FIRE_CHARGE) { cart_explode(v); return true; } return false; }
    bool storage = v->vtype == VEH_CHEST_MINECART || v->vtype == VEH_HOPPER_MINECART || (v->vtype == VEH_CHEST_BOAT && g_player.sneaking);
    if (storage) {
        world_block_entity(vehicle_storage_pos(v), true, v->vtype == VEH_HOPPER_MINECART ? BE_HOPPER : BE_CHEST);
        G.open_be = vehicle_storage_pos(v); G.open_ender = false;
        game_open_screen(SCR_CHEST);
        return true;
    }
    if (v->vtype == VEH_MINECART || v->vtype == VEH_BOAT || v->vtype == VEH_CHEST_BOAT) {
        if (v->rider || pl->mount) return false;
        ride_mount(pl, v);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ rideable mobs
bool mob_rideable(int m) {
    return m == MOB_HORSE || m == MOB_DONKEY || m == MOB_MULE || m == MOB_CAMEL || m == MOB_SKELETON_HORSE || m == MOB_ZOMBIE_HORSE ||
           m == MOB_LLAMA || m == MOB_TRADER_LLAMA || m == MOB_PIG || m == MOB_STRIDER;
}
static bool horse_like(int m) { return m == MOB_HORSE || m == MOB_DONKEY || m == MOB_MULE || m == MOB_CAMEL || m == MOB_SKELETON_HORSE || m == MOB_ZOMBIE_HORSE; }
// right-click on a rideable mob (empty hand / saddle). Returns true if handled.
bool ride_mob_interact(Entity *m, ItemStack *held) {
    Entity *pl = player_ent();
    if (!pl || m->baby || !mob_rideable(m->mob)) return false;
    if (held->id == I_SADDLE && !m->saddled && (m->mob == MOB_PIG || m->mob == MOB_STRIDER || (horse_like(m->mob) && m->tamed))) {
        m->saddled = true; m->persistent = true;
        if (g_player.gamemode != GM_CREATIVE) { if (--held->count == 0) *held = (ItemStack){0}; }
        sound_play_at(SND_CLICK, m->pos, 0.7f, 0.8f);
        return true;
    }
    if (g_player.sneaking || m->rider || pl->mount) return false;
    if (m->mob == MOB_PIG || m->mob == MOB_STRIDER) { if (!m->saddled) return false; ride_mount(pl, m); return true; }
    if (horse_like(m->mob) || m->mob == MOB_LLAMA || m->mob == MOB_TRADER_LLAMA) {
        bool empty = stack_empty(*held) || held->id == I_SADDLE;
        if (!empty && m->tamed) return false;
        if (m->mob == MOB_ZOMBIE_HORSE && !m->tamed) return false;
        ride_mount(pl, m);
        m->ai_timer = 0;
        return true;
    }
    return false;
}
// a mob carrying the player: taming attempts, steering. Returns true if the normal AI should be skipped.
bool ride_mob_tick(Entity *m) {
    Entity *r = rider_of(m);
    if (!r) return false;
    if (r->kind != ENT_PLAYER) { ride_update_rider(m); return false; }
    // untamed horse-likes buck the rider off unless their temper is won over
    if ((horse_like(m->mob) || m->mob == MOB_LLAMA || m->mob == MOB_TRADER_LLAMA) && !m->tamed) {
        if (++m->ai_timer % 40 == 0) {
            int temper = m->special_timer;
            if (irand(0, 99) < temper + 5 || m->mob == MOB_SKELETON_HORSE) {
                m->tamed = true; m->persistent = true; m->owner = r->id;
                fx_hearts(v3_add(m->pos, v3(0, m->height + 0.3f, 0)));
                sound_play_at(SND_LEVELUP, m->pos, 0.4f, 1.6f);
            } else if (irand(0, 2) == 0) {
                m->special_timer = MINI(100, temper + 5);
                ride_dismount(r);
                m->vel.y = 5;
                fx_smoke(v3_add(m->pos, v3(0, m->height, 0)), 6, 0.1f, COL_WHITE);
                return false;
            }
        }
        // wander while bucking
        m->vel.x += frand_range(-1, 1) * DT * 10; m->vel.z += frand_range(-1, 1) * DT * 10;
        entity_physics(m);
        m->vel.x *= 0.8f; m->vel.z *= 0.8f;
        ride_update_rider(m);
        return true;
    }
    const MobDef *d = &MOB_DEFS[m->mob];
    bool steer = false;
    f32 speed = d->speed * 1.6f;
    vec3 want = v3(0, 0, 0);
    ItemStack *h = inventory_held(&g_player.inv);
    if (m->mob == MOB_PIG || m->mob == MOB_STRIDER) {
        int stick = m->mob == MOB_PIG ? I_CARROT_ON_A_STICK : I_WARPED_FUNGUS_ON_A_STICK;
        if (h->id == stick || g_player.inv.offhand.id == stick) {
            steer = true;
            want = v3(sinf(r->yaw), 0, cosf(r->yaw));
            speed = d->speed * (m->mob == MOB_PIG ? 0.8f : 0.9f);
            if (m->special_timer > 0) { m->special_timer--; speed *= 2.2f; }   // boost
            if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_RIGHT] && m->special_timer <= 0 && G.screen == SCR_NONE) {
                ItemStack *s = h->id == stick ? h : &g_player.inv.offhand;
                m->special_timer = 140;
                if (g_player.gamemode != GM_CREATIVE && ++s->damage >= ITEMS[s->id].durability) *s = stack_of(m->mob == MOB_PIG ? I_FISHING_ROD : I_FISHING_ROD, 1);
            }
        }
    } else if (m->saddled && m->mob != MOB_LLAMA && m->mob != MOB_TRADER_LLAMA) {
        steer = true;
        // horses turn toward where the rider looks, keys drive
        m->yaw = r->yaw;
        f32 f = g_player.ride_fwd, s = g_player.ride_strafe * 0.5f;
        if (f < 0) f *= 0.25f;
        want = v3(sinf(m->yaw) * f + cosf(m->yaw) * s, 0, cosf(m->yaw) * f - sinf(m->yaw) * s);
        speed = d->speed * (m->mob == MOB_CAMEL ? 0.6f : 0.85f);
        if (g_player.ride_jump && m->on_ground) { m->vel.y = m->mob == MOB_CAMEL ? 7.0f : 9.0f; if (m->mob == MOB_CAMEL) { m->vel.x += sinf(m->yaw) * 12; m->vel.z += cosf(m->yaw) * 12; } }
    }
    if (!steer) { ride_update_rider(m); return false; }
    // walk
    f32 wl = v3_len(want);
    if (wl > 0.01f) {
        want = v3_scale(want, 1 / wl);
        m->vel.x += (want.x * speed * 20 - m->vel.x) * 0.25f;
        m->vel.z += (want.z * speed * 20 - m->vel.z) * 0.25f;
        if (m->mob != MOB_HORSE || g_player.ride_fwd >= 0) m->yaw = atan2f(want.x, want.z) * (m->mob == MOB_PIG || m->mob == MOB_STRIDER ? 1 : 0) + m->yaw * (m->mob == MOB_PIG || m->mob == MOB_STRIDER ? 0 : 1);
        m->limb_amount = fminf(1, m->limb_amount + 0.2f);
        m->limb_swing += speed * 2;
        // step up single blocks
        if (m->on_ground && m->h_collision) m->vel.y = 6.5f;
    } else { m->vel.x *= 0.6f; m->vel.z *= 0.6f; m->limb_amount *= 0.8f; }
    if (m->mob == MOB_STRIDER) {   // walk on lava
        ivec3 f = v3_floor(m->pos);
        if (block_is_lava(world_get_block(f.x, f.y, f.z))) { m->vel.y = fmaxf(m->vel.y, 3); }
        if (block_is_lava(world_get_block(f.x, f.y - 1, f.z)) && m->vel.y < 0) { m->vel.y = 0; m->pos.y = (f32)f.y; m->on_ground = true; }
    }
    entity_physics(m);
    m->body_yaw = m->head_yaw = m->yaw;
    ride_update_rider(m);
    return true;
}
