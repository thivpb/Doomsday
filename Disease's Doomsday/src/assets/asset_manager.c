#include "../../include/asset_manager.h"
#include "../../include/sprite_manager.h"
#include "../entities/enemy.h"
#include <stdio.h>

GameAssets g_assets = {0};
static int g_scientistVoiceScope = -1;
static int g_scientistVoicePage = -1;
static float g_scientistVoiceDuration = 0.0f;

static Sound LoadGameSound(const char *path, float volume)
{
    Sound sound = LoadSound(path);
    if (sound.frameCount > 0) SetSoundVolume(sound, volume);
    return sound;
}

static void UnloadGameSound(Sound sound)
{
    if (sound.stream.buffer != NULL) UnloadSound(sound);
}

static void PlayLoadedSound(Sound sound)
{
    if (sound.frameCount > 0) PlaySound(sound);
}

static void SetLoadedSoundVolume(Sound sound, float volume)
{
    if (sound.frameCount > 0) SetSoundVolume(sound, volume);
}

void LoadGameAssets(void)
{
    InitAudioDevice();

    // Carrega Fonte com suporte a acentuação (PT-BR)
    // ASCII básico (32..126) + Latin-1 com acentos (À..ÿ, 192..255)
    int codepoints[95 + 64];
    int cpCount = 0;
    for (int c = 32; c <= 126; c++) codepoints[cpCount++] = c;
    for (int c = 192; c <= 255; c++) codepoints[cpCount++] = c;

    g_assets.font = LoadFontEx("Assets/font.ttf", 96, codepoints, cpCount);
    if (g_assets.font.texture.id == 0)
    {
        g_assets.font = GetFontDefault();
    }
    else
    {
        SetTextureFilter(g_assets.font.texture, TEXTURE_FILTER_BILINEAR);
    }

    // Carrega Música
    g_assets.musicMain = LoadMusicStream("Assets/Musica/DeepVoid.mp3");
    g_assets.musicB = LoadMusicStream("Assets/Musica/DeepVoid.mp3");
    // Efeitos nomeados por evento. Mantê-los em Assets/SFX separa efeitos
    // curtos das trilhas em Assets/Musica.
    g_assets.sfxSwordSlice        = LoadGameSound("Assets/SFX/Sword_Slice.mp3",                 0.72f);
    g_assets.sfxRifleShoot        = LoadGameSound("Assets/SFX/RifleShooting.mp3",               0.62f);
    g_assets.sfxGrenadeLaunch     = LoadGameSound("Assets/SFX/GranadeLauncher.mp3",             0.68f);
    g_assets.sfxGrenadeExplode    = LoadGameSound("Assets/SFX/Granade_exploding.mp3",           0.78f);
    g_assets.sfxBFGShoot          = LoadGameSound("Assets/SFX/BFGLauncher_Shoot.mp3",            0.78f);
    g_assets.sfxBFGDamage         = LoadGameSound("Assets/SFX/BFGProjectile_Damaging.mp3",       0.70f);
    g_assets.sfxHeroHurt          = LoadGameSound("Assets/SFX/DamageTaken_Hero.mp3",             0.72f);
    g_assets.sfxBacteriaHurt      = LoadGameSound("Assets/SFX/DamageTaken_Bacteria.mp3",         0.62f);
    g_assets.sfxVirusHurt         = LoadGameSound("Assets/SFX/DamageTaken_Virus.mp3",            0.62f);
    g_assets.sfxBacteriaBossHurt  = LoadGameSound("Assets/SFX/DamageTaken_BossBacteria.mp3",     0.72f);
    g_assets.sfxVirusBossHurt     = LoadGameSound("Assets/SFX/DamageTaking_BossVirus.mp3",       0.72f);
    g_assets.sfxEnemyShoot        = LoadGameSound("Assets/SFX/EnemiesShooting.mp3",              0.58f);
    g_assets.sfxMenuClick         = LoadGameSound("Assets/SFX/ClickinsoundMenu.mp3",             0.55f);
    g_assets.sfxMenuHover         = LoadGameSound("Assets/SFX/HoveringMenuButtons.mp3",          0.34f);
    g_assets.sfxQuizHover         = LoadGameSound("Assets/SFX/QuizPage_hoveringIN_OUT.mp3",      0.40f);
    g_assets.sfxArmorEquip        = LoadGameSound("Assets/SFX/ArmorEquip.mp3",                   0.24f);
    g_assets.sfxScientistVoice    = LoadGameSound("Assets/SFX/fala_cientista_maluco_serio.mp3",  0.80f);
    if (g_assets.sfxScientistVoice.frameCount > 0 &&
        g_assets.sfxScientistVoice.stream.sampleRate > 0)
    {
        g_scientistVoiceDuration =
            (float)g_assets.sfxScientistVoice.frameCount /
            (float)g_assets.sfxScientistVoice.stream.sampleRate;
    }

    // O pacote não contém sons próprios para coleta/level-up e morte.
    g_assets.sfxPickup = LoadGameSound("Assets/Musica/Ataque_Pulse.mp3", 0.45f);
    g_assets.sfxDeath  = LoadGameSound("Assets/Musica/Dano_Heroi.mp3",   0.75f);
    
    // Shader
    g_assets.biologicalShader = LoadShader(0, "Assets/Shaders/biological.fs");
    g_assets.shdLowHP = LoadShader(0, "Assets/Shaders/low_hp_vignette.fs");
    if (g_assets.biologicalShader.id != 0 && g_assets.shdLowHP.id != 0) {
        g_assets.shaderLoaded = true;
        g_assets.shdLowHPTimeLoc = GetShaderLocation(g_assets.shdLowHP, "time");
        g_assets.shdLowHPResLoc = GetShaderLocation(g_assets.shdLowHP, "resolution");
    }

    // Pipeline de sprites: carrega os PNGs que existirem (fallback procedural
    // automático para os que faltarem). A janela/contexto GL já existe aqui.
    LoadSprites();
}

void UnloadGameAssets(void)
{
    if (g_assets.font.texture.id != GetFontDefault().texture.id && g_assets.font.texture.id != 0)
    {
        UnloadFont(g_assets.font);
    }

    if (g_assets.musicMain.stream.buffer != NULL) UnloadMusicStream(g_assets.musicMain);
    if (g_assets.musicB.stream.buffer != NULL) UnloadMusicStream(g_assets.musicB);
    UnloadGameSound(g_assets.sfxSwordSlice);
    UnloadGameSound(g_assets.sfxRifleShoot);
    UnloadGameSound(g_assets.sfxGrenadeLaunch);
    UnloadGameSound(g_assets.sfxGrenadeExplode);
    UnloadGameSound(g_assets.sfxBFGShoot);
    UnloadGameSound(g_assets.sfxBFGDamage);
    UnloadGameSound(g_assets.sfxHeroHurt);
    UnloadGameSound(g_assets.sfxBacteriaHurt);
    UnloadGameSound(g_assets.sfxVirusHurt);
    UnloadGameSound(g_assets.sfxBacteriaBossHurt);
    UnloadGameSound(g_assets.sfxVirusBossHurt);
    UnloadGameSound(g_assets.sfxEnemyShoot);
    UnloadGameSound(g_assets.sfxMenuClick);
    UnloadGameSound(g_assets.sfxMenuHover);
    UnloadGameSound(g_assets.sfxQuizHover);
    UnloadGameSound(g_assets.sfxArmorEquip);
    UnloadGameSound(g_assets.sfxScientistVoice);
    UnloadGameSound(g_assets.sfxPickup);
    UnloadGameSound(g_assets.sfxDeath);

    if (g_assets.shaderLoaded) {
        UnloadShader(g_assets.biologicalShader);
        UnloadShader(g_assets.shdLowHP);
    }

    // Descarrega as texturas da pipeline de sprites (antes de fechar a janela).
    UnloadSprites();

    CloseAudioDevice();
}

void PlayWeaponAttackSfx(int weapon)
{
    switch (weapon)
    {
        case 1: PlayLoadedSound(g_assets.sfxSwordSlice);     break;
        case 2: PlayLoadedSound(g_assets.sfxRifleShoot);     break;
        case 3: PlayLoadedSound(g_assets.sfxGrenadeLaunch);  break;
        case 4: PlayLoadedSound(g_assets.sfxBFGShoot);       break;
        default: break;
    }
}

void PlayEnemyDamageSfx(int enemyType, int enemyTier)
{
    bool boss = (enemyTier == TIER_3_BOSS || enemyTier == TIER_MINIBOSS);
    bool virus = (enemyType == ETYPE_SARS || enemyType == ETYPE_DENGUE_OLD ||
                  enemyType == ETYPE_VIRUS_MELEE || enemyType == ETYPE_VIRUS_RANGED ||
                  enemyType == ETYPE_VIRUS_BOSS || enemyType == ETYPE_VIRUS_SWARM ||
                  enemyType == ETYPE_VIRUS_ELITE);

    if (boss)
        PlayLoadedSound(virus ? g_assets.sfxVirusBossHurt : g_assets.sfxBacteriaBossHurt);
    else
        PlayLoadedSound(virus ? g_assets.sfxVirusHurt : g_assets.sfxBacteriaHurt);
}

void ApplySfxVolume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    // Multiplicadores preservam o mix definido no carregamento dos assets.
    SetLoadedSoundVolume(g_assets.sfxSwordSlice,       0.72f * volume);
    SetLoadedSoundVolume(g_assets.sfxRifleShoot,       0.62f * volume);
    SetLoadedSoundVolume(g_assets.sfxGrenadeLaunch,    0.68f * volume);
    SetLoadedSoundVolume(g_assets.sfxGrenadeExplode,   0.78f * volume);
    SetLoadedSoundVolume(g_assets.sfxBFGShoot,         0.78f * volume);
    SetLoadedSoundVolume(g_assets.sfxBFGDamage,        0.70f * volume);
    SetLoadedSoundVolume(g_assets.sfxHeroHurt,         0.72f * volume);
    SetLoadedSoundVolume(g_assets.sfxBacteriaHurt,     0.62f * volume);
    SetLoadedSoundVolume(g_assets.sfxVirusHurt,        0.62f * volume);
    SetLoadedSoundVolume(g_assets.sfxBacteriaBossHurt, 0.72f * volume);
    SetLoadedSoundVolume(g_assets.sfxVirusBossHurt,    0.72f * volume);
    SetLoadedSoundVolume(g_assets.sfxEnemyShoot,       0.58f * volume);
    SetLoadedSoundVolume(g_assets.sfxMenuClick,        0.55f * volume);
    SetLoadedSoundVolume(g_assets.sfxMenuHover,        0.34f * volume);
    SetLoadedSoundVolume(g_assets.sfxQuizHover,        0.40f * volume);
    SetLoadedSoundVolume(g_assets.sfxArmorEquip,       0.24f * volume);
    SetLoadedSoundVolume(g_assets.sfxScientistVoice,   0.80f * volume);
    SetLoadedSoundVolume(g_assets.sfxPickup,           0.45f * volume);
    SetLoadedSoundVolume(g_assets.sfxDeath,            0.75f * volume);
}

static float ClampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

static float ScientistVoiceTargetDuration(int totalLen)
{
    if (totalLen <= 0) return 0.0f;

    // O arquivo tem uma fala de ~15s. Cada página usa um trecho do começo do MP3
    // com pitch ajustado para terminar junto do typewriter, em vez de continuar
    // falando depois que o texto acabou.
    float target = 0.65f + (float)totalLen * 0.043f;
    float maxDuration = (g_scientistVoiceDuration > 0.8f) ? g_scientistVoiceDuration - 0.35f : 12.0f;
    return ClampFloat(target, 2.4f, maxDuration);
}

float ScientistVoiceCharDelay(int totalLen, float fallbackDelay)
{
    if (g_assets.sfxScientistVoice.frameCount <= 0 || totalLen <= 0)
        return fallbackDelay;
    return ScientistVoiceTargetDuration(totalLen) / (float)totalLen;
}

void StopScientistVoice(void)
{
    if (g_assets.sfxScientistVoice.frameCount > 0)
    {
        StopSound(g_assets.sfxScientistVoice);
        SetSoundPitch(g_assets.sfxScientistVoice, 1.0f);
    }
    g_scientistVoiceScope = -1;
    g_scientistVoicePage = -1;
}

void SyncScientistVoice(int scope, int page, int charShown, int totalLen, float sfxVolume)
{
    if (g_assets.sfxScientistVoice.frameCount <= 0 || totalLen <= 0 || charShown >= totalLen)
    {
        StopScientistVoice();
        return;
    }

    if (sfxVolume < 0.0f) sfxVolume = 0.0f;
    if (sfxVolume > 1.0f) sfxVolume = 1.0f;

    float target = ScientistVoiceTargetDuration(totalLen);
    float pitch = (target > 0.01f && g_scientistVoiceDuration > 0.01f)
                ? g_scientistVoiceDuration / target : 1.0f;
    pitch = ClampFloat(pitch, 0.85f, 2.65f);

    bool newPage = (g_scientistVoiceScope != scope || g_scientistVoicePage != page);
    if (newPage || !IsSoundPlaying(g_assets.sfxScientistVoice))
    {
        StopSound(g_assets.sfxScientistVoice);
        SetSoundPitch(g_assets.sfxScientistVoice, pitch);
        SetSoundVolume(g_assets.sfxScientistVoice, 0.80f * sfxVolume);
        PlaySound(g_assets.sfxScientistVoice);
        g_scientistVoiceScope = scope;
        g_scientistVoicePage = page;
    }
    else
    {
        SetSoundPitch(g_assets.sfxScientistVoice, pitch);
        SetSoundVolume(g_assets.sfxScientistVoice, 0.80f * sfxVolume);
    }
}
