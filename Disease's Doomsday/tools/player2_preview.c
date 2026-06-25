// player2_preview.c — VERIFICAÇÃO VISUAL OFFLINE do 2º personagem (ANTICORPO-V).
// Renderiza um "contact sheet" (headless, exporta PNG) com:
//   linha 1: ANTICORPO-V IDLE  frames 0..3
//   linha 2: ANTICORPO-V WALK  frames 0..3
//   linha 3: ANTICORPO-V HURT  frames 0..2 + um frame ESPELHADO (flip)
//   linha 4: skins por TINTA (Padrao/Medica/Infectada) + boost
//   linha 5: encaixe de ARMAS (espada/rifle/BFG/lamina)
//   linha 6: ANTICORPO ORIGINAL (procedural) idle/walk — checagem de regressao
// Em cada célula desenha-se uma cruz magenta na POSIÇÃO lógica do jogador
// (centro de colisão) para avaliar o alinhamento corpo/arma.
#include "raylib.h"
#include "../include/game.h"
#include "../include/sprite_manager.h"
#include "../Assets/@models/player_model.h"
#include "../Assets/@models/player2_model.h"
#include <string.h>
#include <stdio.h>

// Stubs das cores de skin de arma (evita linkar update_gameplay.c inteiro).
Color WeaponSkinPrimary(int id)   { (void)id; return (Color){ 120, 200, 255, 255 }; }
Color WeaponSkinSecondary(int id) { (void)id; return (Color){ 235, 245, 255, 255 }; }

#define CELL 230
#define COLS 4
#define ROWS 6

static Player basePlayer(void)
{
    Player p; memset(&p, 0, sizeof(p));
    p.equippedWeapon = 1; p.facingDir = 1; p.isMoving = false;
    p.squashX = 1.0f; p.squashY = 1.0f;
    p.skinId = 0; p.weaponSkinId = 0; p.characterId = 1;
    return p;
}

static Vector2 cellCenter(int col, int row)
{
    return (Vector2){ col * CELL + CELL * 0.5f, row * CELL + CELL * 0.55f };
}

static void crosshair(Vector2 c)
{
    DrawLineEx((Vector2){ c.x - 8, c.y }, (Vector2){ c.x + 8, c.y }, 1.0f, Fade(MAGENTA, 0.8f));
    DrawLineEx((Vector2){ c.x, c.y - 8 }, (Vector2){ c.x, c.y + 8 }, 1.0f, Fade(MAGENTA, 0.8f));
}

int main(void)
{
    // NÃO faz ChangeDirectory: rode este preview com CWD = raiz do jogo, para
    // que "Assets/Sprites/..." e "tools/..." resolvam corretamente.
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(COLS * CELL, ROWS * CELL, "player2 preview");
    SetTargetFPS(60);
    LoadSprites(); // carrega as folhas anticorpo_v_*

    if (!Player2SpritesReady())
        printf("[player2_preview] AVISO: folhas do ANTICORPO-V nao carregaram!\n");

    RenderTexture2D rt = LoadRenderTexture(COLS * CELL, ROWS * CELL);
    BeginTextureMode(rt);
    ClearBackground((Color){ 70, 74, 82, 255 }); // cinza neutro

    // Grade leve.
    for (int c = 1; c < COLS; c++) DrawLineEx((Vector2){ c*CELL, 0 }, (Vector2){ c*CELL, ROWS*CELL }, 1.0f, Fade(BLACK, 0.25f));
    for (int r = 1; r < ROWS; r++) DrawLineEx((Vector2){ 0, r*CELL }, (Vector2){ COLS*CELL, r*CELL }, 1.0f, Fade(BLACK, 0.25f));

    Player p = basePlayer();

    // Linha 0: IDLE frames 0..3 (fps 6 -> time = (f+0.5)/6)
    for (int f = 0; f < 4; f++) {
        Vector2 c = cellCenter(f, 0); p.position = c; p.isMoving = false;
        DrawPlayer2Model(&p, 60.0f, (Color){0,229,255,255}, (f + 0.5f) / 6.0f, 0.0f, 0);
        crosshair(c);
    }
    // Linha 1: WALK frames 0..3 (fps 12)
    for (int f = 0; f < 4; f++) {
        Vector2 c = cellCenter(f, 1); p.position = c; p.isMoving = true;
        DrawPlayer2Model(&p, 60.0f, (Color){0,229,255,255}, (f + 0.5f) / 12.0f, 0.0f, 1);
        crosshair(c);
    }
    // Linha 2: HURT frames 0..2 + frame 0 espelhado (col 3, facingDir -1)
    for (int f = 0; f < 3; f++) {
        Vector2 c = cellCenter(f, 2); p.position = c; p.isMoving = false; p.facingDir = 1;
        DrawPlayer2Model(&p, 60.0f, (Color){0,229,255,255}, (f + 0.5f) / 14.0f, 0.0f, 2);
        crosshair(c);
    }
    { Vector2 c = cellCenter(3, 2); p.position = c; p.facingDir = -1;
      DrawPlayer2Model(&p, 60.0f, (Color){0,229,255,255}, 0.5f / 14.0f, 0.0f, 2);
      crosshair(c); p.facingDir = 1; }

    // Linha 3: skins por tinta (0,1,2) + boost (col 3)
    for (int s = 0; s < 3; s++) {
        Vector2 c = cellCenter(s, 3); p.position = c; p.skinId = s; p.attackBoostTimer = 0.0f; p.isMoving = false;
        DrawPlayer2Model(&p, 60.0f, (Color){0,229,255,255}, 0.5f / 6.0f, 0.0f, 0);
        crosshair(c);
    }
    { Vector2 c = cellCenter(3, 3); p.position = c; p.skinId = 0; p.attackBoostTimer = 1.0f;
      DrawPlayer2Model(&p, 60.0f, GOLD, 0.5f / 6.0f, 0.0f, 0); crosshair(c); p.attackBoostTimer = 0.0f; }

    // Linha 4: encaixe de armas (espada=1, rifle=2, BFG=4, lamina=5) — idle
    int wpns[4] = { 1, 2, 4, 5 };
    for (int i = 0; i < 4; i++) {
        Vector2 c = cellCenter(i, 4); p.position = c; p.equippedWeapon = wpns[i]; p.skinId = 0;
        DrawPlayer2Model(&p, 60.0f, (Color){0,229,255,255}, 0.5f / 6.0f, 0.0f, 0);
        crosshair(c);
    }
    p.equippedWeapon = 1;

    // Linha 5: ANTICORPO ORIGINAL (procedural) — regressao (idle col0, walk col1)
    { Vector2 c = cellCenter(0, 5); p.position = c; p.isMoving = false; p.characterId = 0;
      DrawPlayerModel(&p, 60.0f, (Color){0,229,255,255}, 0.3f, 0.0f); crosshair(c); }
    { Vector2 c = cellCenter(1, 5); p.position = c; p.isMoving = true;
      DrawPlayerModel(&p, 60.0f, (Color){0,229,255,255}, 0.3f, 0.0f); crosshair(c); }
    // col2: original idle com arma rifle; col3: original com skin infectada
    { Vector2 c = cellCenter(2, 5); p.position = c; p.isMoving = false; p.equippedWeapon = 2;
      DrawPlayerModel(&p, 60.0f, (Color){0,229,255,255}, 0.3f, 0.0f); crosshair(c); p.equippedWeapon = 1; }
    { Vector2 c = cellCenter(3, 5); p.position = c; p.skinId = 2;
      DrawPlayerModel(&p, 60.0f, (Color){0,229,255,255}, 0.3f, 0.0f); crosshair(c); p.skinId = 0; }

    EndTextureMode();

    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    ExportImage(img, "tools/preview_player2_contact.png");
    UnloadImage(img);
    printf("[player2_preview] tools/preview_player2_contact.png\n");

    // ---- Detalhe em ALTA resolução: V (esq) vs ORIGINAL (dir), mesma escala ----
    RenderTexture2D big = LoadRenderTexture(900, 460);
    BeginTextureMode(big);
    ClearBackground((Color){ 70, 74, 82, 255 });
    DrawLineEx((Vector2){ 450, 0 }, (Vector2){ 450, 460 }, 1.0f, Fade(BLACK, 0.3f));
    Player q = basePlayer();
    // ANTICORPO-V idle + espada
    q.position = (Vector2){ 225, 250 }; q.characterId = 1; q.equippedWeapon = 1;
    DrawPlayer2Model(&q, 60.0f, (Color){0,229,255,255}, 0.5f / 6.0f, 0.0f, 0);
    crosshair(q.position);
    DrawCircleLines((int)q.position.x, (int)q.position.y, 20.0f, Fade(RED, 0.6f)); // hitbox r=20
    // ANTICORPO original idle + espada
    q.position = (Vector2){ 675, 250 }; q.characterId = 0;
    DrawPlayerModel(&q, 60.0f, (Color){0,229,255,255}, 0.3f, 0.0f);
    crosshair(q.position);
    DrawCircleLines((int)q.position.x, (int)q.position.y, 20.0f, Fade(RED, 0.6f));
    EndTextureMode();
    Image bimg = LoadImageFromTexture(big.texture);
    ImageFlipVertical(&bimg);
    ExportImage(bimg, "tools/preview_player2_compare.png");
    UnloadImage(bimg);
    UnloadRenderTexture(big);
    printf("[player2_preview] tools/preview_player2_compare.png\n");

    UnloadRenderTexture(rt);
    UnloadSprites();
    CloseWindow();
    return 0;
}
