// Audio: every sound is synthesised at start-up (no asset files needed), mixed in software with
// 3D attenuation/panning, and played through a tiny per-platform backend (WinMM / ALSA / silent).
#ifndef AUDIO_H
#define AUDIO_H
#include "../core/common.h"

typedef enum {
    SND_NONE,
    SND_DIG_STONE, SND_DIG_WOOD, SND_DIG_GRASS, SND_DIG_GRAVEL, SND_DIG_SAND, SND_DIG_GLASS, SND_DIG_WOOL, SND_DIG_SNOW,
    SND_STEP_STONE, SND_STEP_WOOD, SND_STEP_GRASS, SND_STEP_GRAVEL, SND_STEP_SAND, SND_STEP_GLASS, SND_STEP_WOOL, SND_STEP_SNOW,
    SND_BREAK_GLASS, SND_HURT, SND_POP, SND_XP, SND_LEVELUP, SND_CLICK, SND_EAT, SND_BURP, SND_SPLASH, SND_SWIM,
    SND_EXPLODE, SND_FUSE, SND_BOW, SND_ARROW_HIT, SND_SWING, SND_HIT, SND_CRIT, SND_FIRE, SND_LAND,
    SND_ZOMBIE_IDLE, SND_ZOMBIE_HURT, SND_ZOMBIE_DEATH, SND_SKELETON_IDLE, SND_SKELETON_HURT, SND_SKELETON_DEATH,
    SND_CREEPER_HURT, SND_CREEPER_DEATH, SND_SPIDER_IDLE, SND_SPIDER_DEATH,
    SND_COW_IDLE, SND_COW_HURT, SND_PIG_IDLE, SND_PIG_HURT, SND_SHEEP_IDLE, SND_CHICKEN_IDLE, SND_CHICKEN_HURT,
    SND_HELL_RUMBLE, SND_HELL_WAIL, SND_RAIN, SND_THUNDER, SND_CAVE,
    SND_PLAYER_HURT, SND_PLAYER_DEATH, SND_DOOR_OPEN, SND_DOOR_CLOSE, SND_CHEST_OPEN, SND_CHEST_CLOSE, SND_LEVER, SND_PISTON_OUT, SND_PISTON_IN,
    SND_ANVIL_LAND, SND_PLATE,
    SND_MOB_FIRST,                                   // creature bank: idle / hurt / death for each of the MOB_SOUND_KINDS voices
    SND_MOB_LAST = SND_MOB_FIRST + 35 * 3 - 1,
    SND_COUNT
} SoundId;
#define MOB_SOUND_KINDS 35                           // must match SOUND_KINDS in tools/content/mobs.py
#define SND_MOB(kind, which) (SND_MOB_FIRST + (kind) * 3 + (which))   // which: 0 idle, 1 hurt, 2 death

bool audio_init(void);
void audio_shutdown(void);
void audio_set_volumes(f32 master, f32 sfx, f32 music);
void audio_set_listener(vec3 pos, f32 yaw);
void sound_play(int id, f32 volume, f32 pitch);                 // 2D (UI, player)
void sound_play_at(int id, vec3 pos, f32 volume, f32 pitch);    // positional
void audio_update(bool in_game, bool night);
void audio_set_sift(bool sift);
typedef struct { const char *text; f32 pan; bool positional; double time; } Caption;
int audio_captions(Caption *out, int max);   // subtitles
extern bool g_narrate;                       // narration hook: chat, titles and captions to stdout
extern int g_captions_total;                                    // the Sift plays its own music                     // schedules ambient music
bool audio_backend_active(void);
// Renders a sound into a 16-bit mono WAV file (used by tests / to audition the synthesiser).
bool audio_export_wav(int id, const char *path);

#endif
