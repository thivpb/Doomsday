// charsel_preview.c — captura offline da TELA DE SELEÇÃO DE PERSONAGEM real
// (DrawTelaCharacterSelect), com o Anticorpo e o ANTICORPO-V selecionados, para
// validar layout + preview ao vivo de cada personagem. Linka o jogo (menos main.c).
#include "../include/game.h"
#include "../include/gameplay.h"
#include "../include/telas.h"
#include "../include/asset_manager.h"
#include <string.h>
#include <stdio.h>

// Globais que src/main.c normalmente define.
float g_scale = 1.0f;
Vector2 g_mouseOffset = { 0.0f, 0.0f };
Vector2 g_virtualMouse = { -100.0f, -100.0f };
Font g_gameFont;
Texture2D slotTextures[3] = { 0 };
bool slotTexturesLoaded[3] = { false };
GameScreen loadSelectBackScreen = SCREEN_MENU;
GameScreen settingsBackScreen = SCREEN_MENU;
Image screenshotTemp = { 0 };
bool hasScreenshotTemp = false;

static RenderTexture2D rt;

static void shot(const char *name)
{
    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    char path[160]; snprintf(path, sizeof(path), "tools/ui_%s.png", name);
    ExportImage(img, path); UnloadImage(img);
    printf("[charsel_preview] tools/ui_%s.png\n", name);
}

int main(void)
{
    // Rode com CWD = raiz do jogo (assets + tools/ resolvem relativos).
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "charsel preview");
    SetTargetFPS(60);
    LoadGameAssets();
    g_gameFont = g_assets.font;
    rt = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

    GameState g;
    memset(&g, 0, sizeof(g));
    strcpy(g.player.name, "DR. AB");
    g.player.hp = 90; g.player.maxHp = 100; g.player.level = 5; g.player.xpNeeded = 100;
    g.player.equippedWeapon = 1; g.player.speed = 300; g.musicVolume = 1.0f; g.sfxVolume = 1.0f;
    g.difficulty = DIFFICULTY_MEDIUM; g.screenAnim = 1.0f; // entrada concluida (cards assentados)
    g.currentScreen = SCREEN_CHARACTER_SELECT;

    // Seleção no ANTICORPO original.
    g.player.characterId = 0;
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaCharacterSelect(&g, g_gameFont); EndTextureMode();
    shot("charsel_anticorpo");

    // Seleção no ANTICORPO-V.
    g.player.characterId = 1;
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaCharacterSelect(&g, g_gameFont); EndTextureMode();
    shot("charsel_anticorpo_v");

    // PROVA: ANTICORPO refletindo a skin/cosmeticos escolhidos pelo jogador.
    g.player.characterId = 0;
    g.player.skinId = 2;            // Infectada (paleta roxa/acida)
    g.player.weaponSkinId = 1;      // arma Plasma
    g.player.cosmetics[0] = 1;      // capacete
    g.player.cosmetics[1] = 1;      // mascara
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaCharacterSelect(&g, g_gameFont); EndTextureMode();
    shot("charsel_skinned");
    g.player.skinId = 0; g.player.weaponSkinId = 0; g.player.cosmetics[0] = 0; g.player.cosmetics[1] = 0;

    // ---- Seam REAL de gameplay: DrawPlayerHero escolhe idle/walk/hurt ----
    // Câmera IDENTIDADE (target/offset 0, zoom 1): world == screen, cada estado
    // num quadrante distinto.
    extern void DrawPlayerHero(GameState *game, Vector2 pPos, float playerSize);
    g.camera.target = (Vector2){ 0.0f, 0.0f };
    g.camera.offset = (Vector2){ 0.0f, 0.0f };
    g.camera.zoom = 1.0f;
    BeginTextureMode(rt);
    ClearBackground((Color){ 24, 30, 36, 255 });
    const char *labels[4] = { "IDLE (V)", "ANDANDO (V)", "DANO (V)", "ANTICORPO (orig)" };
    for (int i = 0; i < 4; i++)
    {
        Vector2 worldPos = { 320.0f + (i % 2) * 640.0f, 250.0f + (i / 2) * 320.0f };
        g.player.position = worldPos;
        for (int t = 0; t < 10; t++) g.player.trail[t] = worldPos; // evita trail a partir de (0,0)
        g.player.characterId = (i == 3) ? 0 : 1;
        g.player.isMoving   = (i == 1);
        g.player.squashX = 1.0f; g.player.squashY = 1.0f; // o procedural usa squash
        g.hurtFlashTimer    = (i == 2) ? 0.3f : 0.0f;
        BeginMode2D(g.camera);
        DrawPlayerHero(&g, worldPos, 30.0f);
        EndMode2D();
        DrawTextEx(g_gameFont, labels[i],
                   (Vector2){ (i % 2) * 640.0f + 40.0f, (i / 2) * 320.0f + 30.0f },
                   20.0f, 1.0f, RAYWHITE);
    }
    EndTextureMode();
    shot("charsel_gameplay_seam");

    UnloadRenderTexture(rt);
    UnloadGameplayResources();
    UnloadGameAssets();
    CloseWindow();
    return 0;
}
