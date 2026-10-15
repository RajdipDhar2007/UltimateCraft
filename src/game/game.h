// Global game state, screens and settings.
#ifndef GAME_H
#define GAME_H
#include "../core/common.h"
#include "../world/world.h"

typedef enum {
    SCR_NONE, SCR_TITLE, SCR_WORLDS, SCR_CREATE, SCR_SETTINGS, SCR_PAUSE, SCR_DEATH,
    SCR_INVENTORY, SCR_CRAFTING, SCR_FURNACE, SCR_CHEST, SCR_CHAT, SCR_LOADING, SCR_CONFIRM_DELETE,
    SCR_STONECUTTER, SCR_SMITHING, SCR_BREWING,
    SCR_ENCHANT, SCR_ANVIL, SCR_GRINDSTONE, SCR_LOOM, SCR_CARTOGRAPHY, SCR_TRADE, SCR_LECTERN, SCR_CREDITS, SCR_GUIDE, SCR_COMMAND_BLOCK, SCR_ADVANCEMENTS, SCR_STATS, SCR_CONTROLS
} Screen;
bool screen_is_container(Screen s);   // inventory-style screens (items return to the player on close)

enum { KB_FORWARD, KB_BACK, KB_LEFT, KB_RIGHT, KB_JUMP, KB_SNEAK, KB_SPRINT, KB_INVENTORY, KB_DROP, KB_SWAP, KB_CHAT, KB_COMMAND,
       KB_SCREENSHOT, KB_HIDE_HUD, KB_DEBUG, KB_COUNT };
extern const char *KB_NAMES[KB_COUNT], *KB_KEYS[KB_COUNT];
extern const int KB_DEFAULT[KB_COUNT];
#define KEY(kb) (G.set.keys[kb])
const char *key_name(int glfw_key);
typedef struct {
    int render_dist;
    f32 fov, sensitivity, master, sfx, music;
    int gui_scale;          // 0 = auto
    bool view_bob, vsync, show_fps, invert_y;
    bool backups;           // automatic world backups when a world opens
    bool clouds;
    int keys[KB_COUNT];     // rebindable controls (GLFW key codes)
    bool toggle_sneak, toggle_sprint, reduced_motion, fullscreen;
    bool subtitles, colorblind, high_contrast, narrator;
    char lang[8];           // UI language (res/lang/<code>.lang)
    bool controller;        // read a connected gamepad
    int sim_dist;           // simulation distance in chunks
    int tutorial;           // tutorial hints: next step (0 = start, 99 = done / off)
    int max_fps;            // 0 = unlimited (only applies with VSync off)
} Settings;

typedef struct { char text[160]; f64 time; u32 color; } ChatLine;

typedef struct {
    Settings set;
    Screen screen, return_screen;
    bool in_world;
    ivec3 open_be;          // position of the open chest/furnace
    bool open_ender;        // the open chest screen shows the player's ender chest
    bool jukebox_playing;
    int creative_tab, creative_scroll;
    char creative_search[48];
    int cutter_sel;
    int loom_sel, loom_scroll;
    char anvil_name[40];
    bool text_focus;            // a text field has keyboard focus (E doesn't close the screen)
    bool recipe_book, book_craftable;
    int book_page;
    char book_search[32];
    u32 trade_with;             // villager entity id being traded with
    int trade_sel, trade_scroll;
    bool freeze_mobs;
    bool snap_ground;       // after loading, put the player on the surface (locate ... tp)       // test helper: skip mob AI         // stonecutter selected recipe
    f64 tick_acc;
    f32 alpha;              // render interpolation factor
    u64 frames, ticks;
    f32 fps;
    bool debug, hide_hud;
    char chat[128];
    ChatLine msgs[10];
    int nmsgs;
    int autosave_timer;
    int loading_ticks;
    char selected_world[64];
    // world creation form
    char new_name[48], new_seed[48];
    int new_mode, focus_field;
    int new_difficulty;     // Difficulty
    bool new_cheats, new_hardcore;
    char chat_hist[16][128]; int chat_hist_n, chat_hist_pos;
    // test automation
    bool autotest;
    const char *autotest_script;
    bool quit;
    // /title
    char title[96], subtitle[96], actionbar[128];
    int title_in, title_stay, title_out, title_t, actionbar_t;
    int credits_t;             // frames since the credits screen opened
    int guide_page;            // in-game guide page
    ivec3 cmdblock_pos; char cmdblock_text[240];   // command block editor
    char chat_hint[256];       // Tab-completion suggestions
    char toast[96]; int toast_t;   // advancement toast
    int rebind;                    // controls screen: action waiting for a key (-1 none)
    char hint[160];                // tutorial hint
    bool autotest_quiet;           // scripts: no tutorial hints
    char world_msg[160];           // world list status line
    Screen controls_back;          // screen to return to
    int adv_scroll;
    // /tick
    f32 tick_rate;          // Minecraft ticks per second (20 = normal)
    bool tick_frozen;
    int tick_step;
} Game;

extern Game G;

void game_init(void);
void game_shutdown(void);
void game_frame(f64 dt);
void game_message(u32 color, const char *fmt, ...);
void game_open_screen(Screen s);
void game_close_screen(void);
bool game_start_world(const char *name, u64 seed, int gamemode, bool create);
void game_quit_to_title(void);
void game_save(void);
void boxes_reset(void); void boxes_save(void); void boxes_load(void);
u16 box_store(const ItemStack *slots27);
bool box_take(u16 id, ItemStack *out27);
int box_count(u16 id, ItemStack *first);
bool game_paused(void);           // world simulation frozen (pause / title)
bool screen_grabs_mouse(Screen s);
void settings_save(void);
void settings_apply(void);
u64 seed_from_string(const char *s);
bool command_execute(const char *line);   // chat commands, returns false if unknown
bool command_execute_chat(const char *line);   // typed by the player: respects the Allow Cheats setting
bool mc_command(const char *line, bool *handled);   // commands_mc.c: Minecraft-syntax commands
void title_show(const char *title, const char *subtitle);
void game_change_dimension(int dim, bool via_portal);
void brewing_tick(BlockEntity *be);   // travel between Overworld, Nether and End
void game_block_drops(ivec3 p, u16 block, ItemStack *tool); // drops of a block mined with tool (NULL = none)

// save.c
bool save_level(void);
// scoreboard, world border, forceload, data packs (commands_ext.inc)
#define SB_MAX_OBJ 64
#define SB_MAX_SCORES 2048
typedef struct { char name[32], crit[32], disp[48]; } SbObj;
typedef struct { char holder[40]; u8 obj; int value; } SbScore;
extern SbObj g_sb_obj[SB_MAX_OBJ]; extern int g_sb_nobj, g_sb_sidebar;
extern SbScore g_sb[SB_MAX_SCORES]; extern int g_sb_n;
int sb_obj(const char *name);
bool sb_get(const char *holder, int obj, int *v);
void sb_set(const char *holder, int obj, int v);
void scoreboard_event(const char *crit, const char *holder, int add);
void scoreboard_save(const char *dir);
void scoreboard_load(const char *dir);
typedef struct { f64 cx, cz, size, target; int lerp; f32 damage, buffer, warning; } WorldBorder;
extern WorldBorder g_wb;
bool forceload_has(int cx, int cz);
void forceload_save(const char *dim_dir);
void forceload_load(const char *dim_dir);
void datapacks_tick(void);
// statistics + advancements (progress.c)
enum { STAT_MINED, STAT_PLACED, STAT_KILLS, STAT_DEATHS, STAT_WALK, STAT_JUMPS, STAT_DMG_DEALT, STAT_DMG_TAKEN, STAT_CRAFTED, STAT_FISH, STAT_TIME, STAT_BOSSES, STAT_ADVANCEMENTS, STAT_COUNT };
extern const char *STAT_NAMES[STAT_COUNT];
extern long long g_stats[STAT_COUNT];
int adv_count(void); bool adv_done(int i); const char *adv_title(int i); const char *adv_desc(int i); int adv_by_id(const char *id);
void adv_grant(int i); void adv_revoke(int i); void progress_grant(const char *id);
void progress_tick(void); void progress_mob_killed(void *mob_entity); void progress_moved(f32 dist);
void progress_save(const char *dir); void progress_load(const char *dir);
int command_complete(char *line, size_t cap, char *hint, size_t hcap);   // Tab completion
const char *cmdblock_get(ivec3 p);
void cmdblock_set(ivec3 p, const char *cmd);
void cmdblocks_save(const char *dim_dir);
bool save_load_level(const char *name, u64 *seed, int *gamemode);
int save_level_dim(const char *name);
void save_player(void);
bool save_load_player(void);
void save_entities(void);
void save_load_entities(void);
#endif
