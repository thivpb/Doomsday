#ifndef SPRITE_MANAGER_H
#define SPRITE_MANAGER_H

#include "raylib.h"
#include <stdbool.h>

// ============================================================================
// GERENCIADOR DE SPRITES (pipeline de texturas com fallback procedural)
// ----------------------------------------------------------------------------
// Os sprites em PNG ainda estão em produção. Este módulo prepara a infraestrutura
// para recebê-los: cada categoria tem um SpriteID e um caminho esperado em
// Game/Assets/Sprites/<...>. No carregamento, só carregamos o PNG se ele EXISTIR
// (FileExists). Se não existir, a textura fica inválida (id == 0) e o desenho
// continua 100% procedural (player_model.c, enemy_model.c, weapons_model.c).
//
// Uso típico no render:
//   if (SpriteAvailable(SPR_PLAYER_DEFAULT))
//       DrawSpriteCentered(SPR_PLAYER_DEFAULT, center, size, rot, tint);
//   else
//       DrawPlayerModel(...);  // fallback procedural existente
//
// Ou, de forma compacta, com o helper DrawSpriteOrFallback().
// ============================================================================

typedef enum SpriteID
{
    // ---- Player (Anticorpo) — uma textura por skin ----
    SPR_PLAYER_DEFAULT = 0,  // skinId 0 (Padrão)
    SPR_PLAYER_MEDIC,        // skinId 1 (Médica)
    SPR_PLAYER_INFECTED,     // skinId 2 (Infectada)

    // ---- Player 2 (ANTICORPO-V) — folhas de sprite ANIMADAS (4/4/3 frames) ----
    // Folhas em tira horizontal: idle/walk = 4 frames; hurt = 3 frames. Fundo já
    // transparente (geradas por tools/build_player2_sprites.sh).
    SPR_HERO2_IDLE,
    SPR_HERO2_WALK,
    SPR_HERO2_HURT,

    // ---- Inimigos — Mundo 1 (Bactérias) ----
    SPR_BACT_MELEE,
    SPR_BACT_RANGED,
    SPR_BACT_BOSS,

    // ---- Inimigos — Mundo 2 (Vírus) ----
    SPR_VIRUS_MELEE,
    SPR_VIRUS_RANGED,
    SPR_VIRUS_BOSS,
    SPR_VIRUS_SHIELD,        // overlay do escudo de capsídeo
    SPR_VIRUS_SWARM,         // vírus de enxame (rinovírus)
    SPR_VIRUS_ELITE,         // vírus elite/mutante (sarampo)

    // ---- Armas ----
    SPR_WEAPON_SYRINGE_SWORD, // Espada-Seringa (Mundo 1, melee)
    SPR_WEAPON_PHAGE_RIFLE,   // Rifle de Bacteriófagos (Mundo 1, ranged)
    SPR_WEAPON_VACCINE_RIFLE, // Rifle de Vacina (Mundo 2, ranged)
    SPR_WEAPON_SCALPEL,       // Escalpelizador Estático (Mundo 2, melee anti-escudo)

    // ---- Itens ----
    SPR_ITEM_MASK,            // Máscara Hospitalar
    SPR_ITEM_DISTANCING,      // Distanciamento Social
    SPR_ITEM_RNA_GRENADE,     // Desestabilizador de Ácidos Ribonucleicos
    SPR_ITEM_CYTOKINE,        // Citocina de Estabilização

    // ---- Projéteis ----
    SPR_PROJ_PLAYER,
    SPR_PROJ_ENEMY,

    // ---- Mapa (corpo humano) ----
    SPR_MAP_BODY,
    SPR_MAP_SILHOUETTE,
    SPR_MAP_ORGANS,

    // ---- UI (ícones de arma/item para HUD e hotbar) ----
    SPR_UI_ICONS,
    SPR_UI_MENU_BANNER,       // Banner/hero do menu principal (arte larga; legado)

    // ---- Elementos do MENU recortados da arte original (Assets/Sprites/UI/Menu) ----
    // (os micróbios do menu agora vêm do CATÁLOGO de organismos — ver abaixo.)
    SPR_MENU_SYRINGE,
    SPR_MENU_BIOHAZARD,

    SPRITE_COUNT
} SpriteID;

// Carrega todas as texturas que existirem em disco (silencioso se faltarem).
void LoadSprites(void);

// Descarrega todas as texturas carregadas.
void UnloadSprites(void);

// ---- Glifos do título do menu (recortados da arte; carregados uma única vez) ----
// Quantidade de glifos disponíveis (0 se não houver PNGs/arte).
int MenuTitleGlyphCount(void);
// Textura do glifo i (id == 0 se indisponível).
Texture2D GetTitleGlyph(int i);
// true se TODOS os glifos do título foram carregados com sucesso.
bool MenuTitleGlyphsReady(void);

// ---- Catálogo de ORGANISMOS do menu (18 recortes de virus_bacterias.png) ----
// Carregados uma única vez junto com os sprites; descarregados no encerramento.
int MenuOrganismCount(void);          // total de organismos do catálogo
Texture2D GetMenuOrganism(int i);     // textura do organismo i (id 0 se ausente)
bool MenuOrganismsReady(void);        // true se TODOS carregaram

// true se a textura do sprite foi carregada com sucesso (PNG presente e válido).
bool SpriteAvailable(SpriteID id);

// Retorna a textura do sprite (id == 0 se não carregada).
Texture2D GetSprite(SpriteID id);

// Desenha o sprite centralizado em `center`, escalado para caber em `destSize`
// (largura x altura, em pixels de mundo), com `rotation` (graus) e `tint`.
// Não faz nada se o sprite não estiver disponível.
void DrawSpriteCentered(SpriteID id, Vector2 center, Vector2 destSize, float rotation, Color tint);

// Se o sprite estiver disponível, desenha-o (via DrawSpriteCentered) e retorna true.
// Caso contrário, chama o desenho procedural `fallbackProc(userData)` e retorna false.
// Isso permite manter o jogo 100% jogável mesmo sem nenhum PNG.
bool DrawSpriteOrFallback(SpriteID id, Vector2 center, Vector2 destSize, float rotation, Color tint,
                          void (*fallbackProc)(void *userData), void *userData);

// ---- Folhas de sprite ANIMADAS (tira horizontal de `frameCount` frames) ----
// Desenha o frame `frameIndex` (recorta a fração frameWidth = tex.width/frameCount)
// centralizado em `center`, escalado para `destSize`, com `rotation` (graus) e `tint`.
// `flipX` espelha horizontalmente (para virar o personagem conforme a direção).
// Não faz nada se o sprite não estiver disponível.
void DrawSpriteFrame(SpriteID id, int frameIndex, int frameCount, Vector2 center,
                     Vector2 destSize, float rotation, Color tint, bool flipX);

// true se TODAS as folhas do ANTICORPO-V (idle/walk/hurt) carregaram. Se false, o
// render cai no Anticorpo procedural (o jogo nunca quebra por PNG ausente).
bool Player2SpritesReady(void);

#endif // SPRITE_MANAGER_H
