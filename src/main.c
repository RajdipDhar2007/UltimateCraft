// UltimateCraft - a Minecraft-inspired voxel survival game.
// Foundation: minecraft-weekend (C/OpenGL). Ideas ported from Craft (AO, culling, worker threads,
// delta saves, commands) and the Python/pyglet Minecraft (brick texture). By RajdipDhar2007.
#include "game/game.h"
#include "game/player.h"
#include "game/fx.h"
#include "world/sky.h"
#include "ui/ui.h"
#include "audio/audio.h"
#include "core/platform.h"
#include <time.h>
#include <unistd.h>

void game_set_script(const char *path);
void crash_install(void);   // core/crash.c

int main(int argc, char **argv) {
#if defined(_WIN32) && !defined(UC_CONSOLE)
    // the Windows build is a GUI app (no console window): logs and narration go to latest.log next to the exe
    freopen("latest.log", "w", stderr); freopen("latest.log", "a", stdout);
#endif
    crash_install();
    const char *script = NULL;
    bool no_audio = false;
    int w = 1280, h = 720;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--script") && i + 1 < argc) script = argv[++i];
        else if (!strcmp(argv[i], "--no-audio")) no_audio = true;
        else if (!strcmp(argv[i], "--version")) { printf("UltimateCraft %s\n", UC_VERSION); return 0; }
        else if (!strcmp(argv[i], "--size") && i + 2 < argc) { w = atoi(argv[++i]); h = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--export-sounds") && i + 1 < argc) {   // writes every synthesised sound as a WAV
            const char *dir = argv[++i];
            glfwInit();
            plat_mkdirs(dir);
            audio_init();
            for (int s = 1; s < SND_COUNT; s++) { char p[300]; snprintf(p, sizeof p, "%s/sound_%02d.wav", dir, s); audio_export_wav(s, p); }
            audio_shutdown();
            return 0;
        }
    }
    // assets are loaded relative to the working directory: allow launching from build folders too
    if (!plat_exists("res/shaders/chunk.vs")) {
        if (plat_exists("../res/shaders/chunk.vs")) { if (chdir("..") != 0) {} }
        else if (plat_exists("../../res/shaders/chunk.vs")) { if (chdir("../..") != 0) {} }
    }
    g_rng.s = (u64)time(NULL);
    if (!window_init(w, h, "UltimateCraft " UC_VERSION)) {
        fprintf(stderr, "UltimateCraft needs OpenGL 3.3. Please update your graphics drivers.\n");
        return 1;
    }
    blocks_init();
    items_init();
    b2d_init();
    b3d_init();
    world_gfx_init();
    sky_init();
    entities_render_init();
    fx_init();
    ui_init();
    if (!no_audio) audio_init();
    game_init();
    settings_apply();
    if (script) game_set_script(script);
    { int nitems = 0; for (int i = 1; i < ITEM_MAX; i++) nitems += item_valid(i); LOG_INFO("ready: %d block states, %d items, %d recipes, %d mobs, %d biomes", BLOCK_STATES, nitems, recipe_count(), MOB_COUNT, BIOME_COUNT); }

    f64 last = plat_time();
    while (!window_should_close() && !G.quit) {
        window_begin_frame();
        f64 now = plat_time(), dt = now - last;
        last = now;
        if (dt > 0.25) dt = 0.25;
        if (G.autotest) dt = DT;   // scripted runs use virtual time: one frame is exactly one tick, whatever the machine load
        game_frame(dt);
        window_swap();
        if (G.set.max_fps > 0 && !G.set.vsync) {   // frame cap (Options)
            f64 left = 1.0 / G.set.max_fps - (plat_time() - now);
            if (left > 0.001) plat_sleep_ms((int)(left * 1000));
        }
    }
    game_shutdown();
    audio_shutdown();
    window_shutdown();
    return 0;
}
