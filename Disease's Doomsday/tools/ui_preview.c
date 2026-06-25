// ui_preview.c — captura offline de telas de UI (arsenal, quiz, cenas do
// cientista) renderizando as funções REAIS do jogo numa RenderTexture virtual
// 1280x720. Linka o jogo inteiro exceto main.c. Saídas .gitignored em tools/.
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

// Hooks de preview expostos pelas telas (forçam estado p/ captura offline).
extern void ArsenalPreviewSet(float scroll, int sel);
extern void QuizPreviewForce(int qIdx, int selectedShown);

static RenderTexture2D rt;

static void shot(const char *name)
{
    Image img = LoadImageFromTexture(rt.texture);
    ImageFlipVertical(&img);
    char path[160]; snprintf(path, sizeof(path), "tools/ui_%s.png", name);
    ExportImage(img, path); UnloadImage(img);
    printf("[ui_preview] tools/ui_%s.png\n", name);
}

static void baseState(GameState *g)
{
    memset(g, 0, sizeof(*g));
    strcpy(g->player.name, "DR. AB");
    g->player.hp = 78; g->player.maxHp = 100; g->player.level = 5; g->player.xpNeeded = 100;
    g->player.equippedWeapon = 1; g->player.speed = 300; g->musicVolume = 1.0f; g->sfxVolume = 1.0f;
    g->player.score = 12450;
    g->maxWeaponUnlocked = 4;            // todas as armas de nível liberadas
    g->difficulty = DIFFICULTY_MEDIUM; g->screenAnim = 1.0f;
    g->currentWorld = WORLD_VIRUS;
}

int main(void)
{
    ChangeDirectory(GetApplicationDirectory());
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "ui preview");
    SetTargetFPS(60);
    LoadGameAssets();
    g_gameFont = g_assets.font;
    rt = LoadRenderTexture(SCREEN_WIDTH, SCREEN_HEIGHT);

    GameState g;

    // ---- ARSENAL: rolado até a Lâmina Bioelétrica BLOQUEADA (10 abates) ----
    baseState(&g);
    g.totalEnemiesKilled = 10;
    g.currentScreen = SCREEN_ARSENAL;
    ArsenalPreviewSet(9999.0f, WEAPON_BIOBLADE - 1); // rola ao fim, seleciona a arma 5
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaArsenal(&g, g_gameFont); EndTextureMode();
    shot("arsenal_locked");

    // ---- ARSENAL: rolado até a Lâmina Bioelétrica DESBLOQUEADA (40 abates) ----
    baseState(&g);
    g.totalEnemiesKilled = 40;
    g.currentScreen = SCREEN_ARSENAL;
    ArsenalPreviewSet(9999.0f, WEAPON_BIOBLADE - 1);
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaArsenal(&g, g_gameFont); EndTextureMode();
    shot("arsenal_unlocked");

    // ---- QUIZ: pergunta sem resposta (alternativas equilibradas, posicao sorteada) ----
    baseState(&g);
    g.currentScreen = SCREEN_QUIZ;
    QuizPreviewForce(2, -1); // pergunta da EQUIDADE, ainda nao respondida
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaQuiz(&g, g_gameFont); EndTextureMode();
    shot("quiz_question");

    // ---- QUIZ: respondido (correta=verde, escolha errada=vermelho) ----
    baseState(&g);
    g.currentScreen = SCREEN_QUIZ;
    QuizPreviewForce(40, -2); // pergunta do antibiotico, força uma alternativa errada
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaQuiz(&g, g_gameFont); EndTextureMode();
    shot("quiz_answered");

    // ---- CENA PÓS-MUNDO 1: cientista (página do antibiotico, texto revelado) ----
    baseState(&g);
    g.currentScreen = SCREEN_WORLD_TRANSITION;
    g.uiAnimTimer = 1.0f;                              // entrada concluida
    g.sceneDialog = (DialogState){ true, 1, 9999, 0 }; // pagina 1 (a mais longa), revelada
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaTransicao(&g, g_gameFont); EndTextureMode();
    shot("scene_world1");

    // ---- CENA FINAL (vitória): FASE 0 — cientista narra com o relatório ----
    baseState(&g);
    g.player.score = 28750; g.player.level = 9; g.totalEnemiesKilled = 214;
    g.currentScreen = SCREEN_VICTORY;
    g.sceneDialog = (DialogState){ true, 1, 9999, 0 }; // página do relatório, revelada
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaVitoria(&g, g_gameFont); EndTextureMode();
    shot("victory_dialog");

    // ---- CENA FINAL (vitória): FASE 1 — stats + opções (transmissão encerrada) ----
    baseState(&g);
    g.player.score = 28750; g.player.level = 9; g.totalEnemiesKilled = 214;
    g.currentScreen = SCREEN_VICTORY;
    g.sceneDialog.active = false;
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaVitoria(&g, g_gameFont); EndTextureMode();
    shot("victory_stats");

    // ---- TUTORIAL: captura de cada aba (verificacao visual offline) ----
    extern void TutorialPreviewSetTab(int tab);
    const char *tabNames[7] = { "basico", "armas", "inimigos", "chefe", "skins", "xp", "hordas" };
    for (int tb = 0; tb < 7; tb++)
    {
        baseState(&g);
        g.currentScreen = SCREEN_CONTROLS;
        TutorialPreviewSetTab(tb);
        BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaControles(&g, g_gameFont); EndTextureMode();
        char nm[48]; snprintf(nm, sizeof(nm), "tut_%d_%s", tb, tabNames[tb]);
        shot(nm);
    }
    // Avanca o relogio (~4.5s no loop de 6s) para pegar a fase de LEVEL-UP da aba
    // XP (barras de status cheias) e outra posicao da varredura de HORDAS.
    WaitTime(4.0);
    baseState(&g); g.currentScreen = SCREEN_CONTROLS; TutorialPreviewSetTab(5);
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaControles(&g, g_gameFont); EndTextureMode();
    shot("tut_5_xp_levelup");
    baseState(&g); g.currentScreen = SCREEN_CONTROLS; TutorialPreviewSetTab(6);
    BeginTextureMode(rt); ClearBackground(BLACK); DrawTelaControles(&g, g_gameFont); EndTextureMode();
    shot("tut_6_hordas_b");

    UnloadRenderTexture(rt);
    UnloadGameplayResources();
    UnloadGameAssets();
    CloseWindow();
    return 0;
}
