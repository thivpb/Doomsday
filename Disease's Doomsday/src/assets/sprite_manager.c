#include "../../include/sprite_manager.h"
#include "../../Assets/@models/menu_title_glyphs.h"
#include "../../Assets/@models/menu_organisms.h"
#include <stddef.h>

// ============================================================================
// GERENCIADOR DE SPRITES — implementação
// ----------------------------------------------------------------------------
// Mantém um array de Texture2D indexado por SpriteID e a tabela de caminhos
// esperados. O carregamento é tolerante: se o PNG não existir, a textura fica
// com id == 0 (inválida) e o jogo recorre ao desenho procedural (fallback).
// ============================================================================

// Caminhos esperados de cada sprite (relativos à pasta Game/, onde o jogo roda).
// Mantenha em sincronia com o enum SpriteID em sprite_manager.h.
static const char *SPRITE_PATHS[SPRITE_COUNT] = {
    // Player (por skin)
    [SPR_PLAYER_DEFAULT]  = "Assets/Sprites/Player/anticorpo_default.png",
    [SPR_PLAYER_MEDIC]    = "Assets/Sprites/Player/anticorpo_medica.png",
    [SPR_PLAYER_INFECTED] = "Assets/Sprites/Player/anticorpo_infectada.png",

    // Player 2 (ANTICORPO-V): folhas animadas (idle/walk 4 frames; hurt 3 frames)
    [SPR_HERO2_IDLE] = "Assets/Sprites/Player/anticorpo_v_idle.png",
    [SPR_HERO2_WALK] = "Assets/Sprites/Player/anticorpo_v_walk.png",
    [SPR_HERO2_HURT] = "Assets/Sprites/Player/anticorpo_v_hurt.png",

    // Inimigos — Bactéria
    [SPR_BACT_MELEE]  = "Assets/Sprites/Enemies/Bacteria/melee.png",
    [SPR_BACT_RANGED] = "Assets/Sprites/Enemies/Bacteria/ranged.png",
    [SPR_BACT_BOSS]   = "Assets/Sprites/Enemies/Bacteria/boss.png",

    // Inimigos — Vírus
    [SPR_VIRUS_MELEE]  = "Assets/Sprites/Enemies/Virus/melee.png",
    [SPR_VIRUS_RANGED] = "Assets/Sprites/Enemies/Virus/ranged.png",
    [SPR_VIRUS_BOSS]   = "Assets/Sprites/Enemies/Virus/boss.png",
    [SPR_VIRUS_SHIELD] = "Assets/Sprites/Enemies/Virus/shield.png",
    [SPR_VIRUS_SWARM]  = "Assets/Sprites/Enemies/Virus/swarm.png",
    [SPR_VIRUS_ELITE]  = "Assets/Sprites/Enemies/Virus/elite.png",

    // Armas
    [SPR_WEAPON_SYRINGE_SWORD] = "Assets/Sprites/Weapons/espada_seringa.png",
    [SPR_WEAPON_PHAGE_RIFLE]   = "Assets/Sprites/Weapons/rifle_bacteriofagos.png",
    [SPR_WEAPON_VACCINE_RIFLE] = "Assets/Sprites/Weapons/rifle_vacina.png",
    [SPR_WEAPON_SCALPEL]       = "Assets/Sprites/Weapons/escalpelizador.png",

    // Itens
    [SPR_ITEM_MASK]        = "Assets/Sprites/Items/mascara.png",
    [SPR_ITEM_DISTANCING]  = "Assets/Sprites/Items/distanciamento.png",
    [SPR_ITEM_RNA_GRENADE] = "Assets/Sprites/Items/granada_rna.png",
    [SPR_ITEM_CYTOKINE]    = "Assets/Sprites/Items/citocina.png",

    // Projéteis
    [SPR_PROJ_PLAYER] = "Assets/Sprites/Projectiles/player_proj.png",
    [SPR_PROJ_ENEMY]  = "Assets/Sprites/Projectiles/enemy_proj.png",

    // Mapa
    [SPR_MAP_BODY]       = "Assets/Sprites/Map/corpo.png",
    [SPR_MAP_SILHOUETTE] = "Assets/Sprites/Map/silhueta.png",
    [SPR_MAP_ORGANS]     = "Assets/Sprites/Map/orgaos.png",

    // UI
    [SPR_UI_ICONS] = "Assets/Sprites/UI/icones.png",
    [SPR_UI_MENU_BANNER] = "Assets/Sprites/UI/menu_background.png",

    // Elementos do menu recortados da arte original (título/biohazard/seringa)
    [SPR_MENU_SYRINGE]      = "Assets/Sprites/UI/Menu/syringe_cyan.png",
    [SPR_MENU_BIOHAZARD]    = "Assets/Sprites/UI/Menu/biohazard_red.png",
};

static Texture2D s_sprites[SPRITE_COUNT] = {0};
static bool s_loaded[SPRITE_COUNT] = {0};

// Glifos do título do menu (carregados uma única vez junto com os sprites).
static Texture2D s_glyphs[MENU_TITLE_GLYPH_COUNT] = {0};
static bool s_glyphLoaded[MENU_TITLE_GLYPH_COUNT] = {0};
static bool s_glyphsReady = false;

// Catálogo de organismos do menu (18 recortes de virus_bacterias.png).
static Texture2D s_org[MENU_ORGANISM_COUNT] = {0};
static bool s_orgLoaded[MENU_ORGANISM_COUNT] = {0};
static bool s_orgReady = false;

void LoadSprites(void)
{
    for (int i = 0; i < SPRITE_COUNT; i++)
    {
        const char *path = SPRITE_PATHS[i];
        s_loaded[i] = false;
        s_sprites[i] = (Texture2D){0};

        // Só tenta carregar se o arquivo existir (evita avisos e falhas da raylib).
        if (path != NULL && FileExists(path))
        {
            Texture2D tex = LoadTexture(path);
            if (tex.id != 0)
            {
                SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
                s_sprites[i] = tex;
                s_loaded[i] = true;
            }
        }
    }

    // Glifos do título (uma vez). s_glyphsReady só é true se TODOS carregarem.
    s_glyphsReady = (MENU_TITLE_GLYPH_COUNT > 0);
    for (int i = 0; i < MENU_TITLE_GLYPH_COUNT; i++)
    {
        s_glyphLoaded[i] = false;
        s_glyphs[i] = (Texture2D){0};
        const char *p = MENU_TITLE_GLYPHS[i].path;
        if (p != NULL && FileExists(p))
        {
            Texture2D t = LoadTexture(p);
            if (t.id != 0)
            {
                SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
                s_glyphs[i] = t;
                s_glyphLoaded[i] = true;
            }
        }
        if (!s_glyphLoaded[i]) s_glyphsReady = false;
    }

    // Catálogo de organismos do menu (uma vez). Ready só se TODOS carregarem.
    s_orgReady = (MENU_ORGANISM_COUNT > 0);
    for (int i = 0; i < MENU_ORGANISM_COUNT; i++)
    {
        s_orgLoaded[i] = false;
        s_org[i] = (Texture2D){0};
        const char *p = MENU_ORGANISMS[i].path;
        if (p != NULL && FileExists(p))
        {
            Texture2D t = LoadTexture(p);
            if (t.id != 0)
            {
                SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
                s_org[i] = t;
                s_orgLoaded[i] = true;
            }
        }
        if (!s_orgLoaded[i]) s_orgReady = false;
    }
}

void UnloadSprites(void)
{
    for (int i = 0; i < SPRITE_COUNT; i++)
    {
        if (s_loaded[i] && s_sprites[i].id != 0)
        {
            UnloadTexture(s_sprites[i]);
        }
        s_loaded[i] = false;
        s_sprites[i] = (Texture2D){0};
    }
    for (int i = 0; i < MENU_TITLE_GLYPH_COUNT; i++)
    {
        if (s_glyphLoaded[i] && s_glyphs[i].id != 0) UnloadTexture(s_glyphs[i]);
        s_glyphLoaded[i] = false;
        s_glyphs[i] = (Texture2D){0};
    }
    s_glyphsReady = false;
    for (int i = 0; i < MENU_ORGANISM_COUNT; i++)
    {
        if (s_orgLoaded[i] && s_org[i].id != 0) UnloadTexture(s_org[i]);
        s_orgLoaded[i] = false;
        s_org[i] = (Texture2D){0};
    }
    s_orgReady = false;
}

int MenuTitleGlyphCount(void) { return MENU_TITLE_GLYPH_COUNT; }
Texture2D GetTitleGlyph(int i)
{
    if (i < 0 || i >= MENU_TITLE_GLYPH_COUNT) return (Texture2D){0};
    return s_glyphs[i];
}
bool MenuTitleGlyphsReady(void) { return s_glyphsReady; }

int MenuOrganismCount(void) { return MENU_ORGANISM_COUNT; }
Texture2D GetMenuOrganism(int i)
{
    if (i < 0 || i >= MENU_ORGANISM_COUNT) return (Texture2D){0};
    return s_org[i];
}
bool MenuOrganismsReady(void) { return s_orgReady; }

bool SpriteAvailable(SpriteID id)
{
    if (id < 0 || id >= SPRITE_COUNT) return false;
    return s_loaded[id];
}

Texture2D GetSprite(SpriteID id)
{
    if (id < 0 || id >= SPRITE_COUNT) return (Texture2D){0};
    return s_sprites[id];
}

void DrawSpriteCentered(SpriteID id, Vector2 center, Vector2 destSize, float rotation, Color tint)
{
    if (!SpriteAvailable(id)) return;

    Texture2D tex = s_sprites[id];
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle dst = { center.x, center.y, destSize.x, destSize.y };
    Vector2 origin = { destSize.x * 0.5f, destSize.y * 0.5f }; // pivô no centro
    DrawTexturePro(tex, src, dst, origin, rotation, tint);
}

bool DrawSpriteOrFallback(SpriteID id, Vector2 center, Vector2 destSize, float rotation, Color tint,
                          void (*fallbackProc)(void *userData), void *userData)
{
    if (SpriteAvailable(id))
    {
        DrawSpriteCentered(id, center, destSize, rotation, tint);
        return true;
    }

    // Sem PNG: mantém o desenho procedural atual.
    if (fallbackProc != NULL) fallbackProc(userData);
    return false;
}

void DrawSpriteFrame(SpriteID id, int frameIndex, int frameCount, Vector2 center,
                     Vector2 destSize, float rotation, Color tint, bool flipX)
{
    if (!SpriteAvailable(id)) return;
    if (frameCount < 1) frameCount = 1;
    // Clamp do índice ao range válido (evita recortar fora da textura).
    if (frameIndex < 0) frameIndex = 0;
    if (frameIndex >= frameCount) frameIndex = frameCount - 1;

    Texture2D tex = s_sprites[id];
    float fw = (float)tex.width / (float)frameCount;
    Rectangle src = { fw * frameIndex, 0.0f, fw, (float)tex.height };
    if (flipX) src.width = -src.width; // espelha horizontalmente

    Rectangle dst = { center.x, center.y, destSize.x, destSize.y };
    Vector2 origin = { destSize.x * 0.5f, destSize.y * 0.5f }; // pivô central
    DrawTexturePro(tex, src, dst, origin, rotation, tint);
}

bool Player2SpritesReady(void)
{
    return SpriteAvailable(SPR_HERO2_IDLE)
        && SpriteAvailable(SPR_HERO2_WALK)
        && SpriteAvailable(SPR_HERO2_HURT);
}
