#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include <stdio.h>
#include <string.h>

static UIButton upgradeBtns[3];
static UIButton btnNextWave;
static UIButton btnStageContinue = { { 490, 560, 300, 52 }, "CONTINUAR", false, false };
extern Vector2 g_virtualMouse;

static const char *StageCompleteMessage(int completed)
{
    static const char *msgs[] = {
        "Aproveite a pausa antes da pergunta: uma boa resposta pode render Pontos do SUS para fortalecer o anticorpo.",
        "Antes da proxima decisao, revise o folego: acertar o quiz garante recursos para evoluir sua defesa.",
        "A infeccao recuou por enquanto. A pergunta seguinte pode virar melhoria real para o anticorpo.",
        "Momento de triagem concluido. Responder bem no quiz aumenta suas chances na proxima onda.",
        "O organismo ganhou tempo. Use a pergunta do SUS para buscar pontos e preparar a proxima resposta imune."
    };
    int n = (int)(sizeof(msgs) / sizeof(msgs[0]));
    return msgs[(completed - 1) % n];
}

static void DrawTransitionPanel(GameState *game, Font font, const char *title, const char *body, Color accent)
{
    float entry = UIEase(game->screenAnim / 0.35f);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.55f * entry));
    Rectangle panel = { 310, 190, 660, 360 };
    DrawPanel(panel, accent, 0.82f * entry);
    DrawTitleText(font, title, SCREEN_WIDTH / 2.0f, panel.y + 34.0f, 34.0f, accent);
    DrawTextWrapped(font, body, (Rectangle){ panel.x + 52, panel.y + 118, panel.width - 104, 110 },
                    21.0f, 1.0f, Fade(WHITE, 0.9f));

    const char *hint = (game->screenAnim < 0.65f) ? "Aguarde..." : "Clique ou pressione ENTER";
    Vector2 hs = MeasureTextEx(font, hint, 16.0f, 1.0f);
    DrawTextEx(font, hint, (Vector2){ SCREEN_WIDTH / 2.0f - hs.x / 2.0f, panel.y + 256.0f },
               16.0f, 1.0f, Fade(WHITE, 0.65f));
    btnStageContinue.hover = game->screenAnim >= 0.65f && CheckCollisionPointRec(g_virtualMouse, btnStageContinue.bounds);
    DrawButton(btnStageContinue, font, game->screenAnim >= 0.65f);
}

void DrawTelaUpgrade(GameState *game, Font font)
{
    DrawMenuFXBackground((float)GetTime(), game->screenAnim / 0.35f);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade((Color){ 4, 6, 16, 255 }, 0.48f));

    const char *title = "MELHORIAS DO SUS";
    Vector2 titleSz = MeasureTextEx(font, title, 36.0f, 1.0f);
    DrawTextEx(font, title, (Vector2){ (SCREEN_WIDTH / 2.0f) - (titleSz.x / 2.0f), 80.0f }, 36.0f, 1.0f, THEME_COLOR_MAIN);

    char ptsTxt[32];
    snprintf(ptsTxt, sizeof(ptsTxt), "Pontos do SUS Disponiveis: %d", game->player.susPoints);
    Vector2 ptsSz = MeasureTextEx(font, ptsTxt, 20.0f, 1.0f);
    DrawTextEx(font, ptsTxt, (Vector2){ (SCREEN_WIDTH / 2.0f) - (ptsSz.x / 2.0f), 130.0f }, 20.0f, 1.0f, GOLD);

    // Initialize buttons if they are empty
    if (upgradeBtns[0].bounds.width == 0) {
        upgradeBtns[0] = (UIButton){ { 340, 220, 600, 70 }, "Upgrade Vida Maxima (+100) - Custo: 50 pts", false, false };
        upgradeBtns[1] = (UIButton){ { 340, 320, 600, 70 }, "Upgrade Velocidade (+15%) - Custo: 50 pts", false, false };
        upgradeBtns[2] = (UIButton){ { 340, 420, 600, 70 }, "Upgrade Dano (+10) - Custo: 50 pts", false, false };
        btnNextWave = (UIButton){ { 490, 580, 300, 50 }, "PROXIMA ONDA", false, false };
    }

    for (int i = 0; i < 3; i++) {
        DrawButton(upgradeBtns[i], font, game->player.susPoints >= 50);
    }
    DrawButton(btnNextWave, font, true);
}

void UpdateTelaUpgrade(GameState *game, Vector2 mouse)
{
    for (int i = 0; i < 3; i++) {
        if (game->player.susPoints >= 50 && CheckCollisionPointRec(mouse, upgradeBtns[i].bounds)) {
            upgradeBtns[i].hover = true;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                game->player.susPoints -= 50;
                if (i == 0) {
                    game->player.maxHp += 100;
                    game->player.hp += 100;
                } else if (i == 1) {
                    game->player.speed *= 1.15f;
                } else if (i == 2) {
                    game->player.attackPower += 10;
                }
            }
        } else {
            upgradeBtns[i].hover = false;
        }
    }

    if (CheckCollisionPointRec(mouse, btnNextWave.bounds)) {
        btnNextWave.hover = true;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            game->currentScreen = SCREEN_STAGE_PROLOGUE;
        }
    } else {
        btnNextWave.hover = false;
    }
}

void DrawTelaStageComplete(GameState *game, Font font)
{
    int completed = game->wave - 1;
    if (completed < 1) completed = 1;
    DrawTransitionPanel(game, font, TextFormat("ESTAGIO %d CONCLUIDO", completed), StageCompleteMessage(completed), GOLD);
}

void UpdateTelaStageComplete(GameState *game, Vector2 mouse)
{
    btnStageContinue.hover = game->screenAnim >= 0.65f && CheckCollisionPointRec(mouse, btnStageContinue.bounds);
    if (game->screenAnim >= 0.65f &&
        (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
         (btnStageContinue.hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))))
    {
        game->currentScreen = SCREEN_QUIZ;
    }
}

void DrawTelaStagePrologue(GameState *game, Font font)
{
    char title[64];
    snprintf(title, sizeof(title), "ONDA %d", game->wave);
    float entry = UIEase(game->screenAnim / 0.28f);
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.48f * entry));
    Rectangle panel = { 470, 290, 340, 120 };
    DrawPanel(panel, THEME_COLOR_MAIN, 0.86f * entry);
    DrawTitleText(font, title, SCREEN_WIDTH / 2.0f, panel.y + 34.0f, 42.0f, THEME_COLOR_MAIN);
}

void UpdateTelaStagePrologue(GameState *game, Vector2 mouse)
{
    (void)mouse;
    if (game->screenAnim >= 1.2f ||
        (game->screenAnim >= 0.45f &&
         (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))))
    {
        StartNextWave(game);
        game->currentScreen = SCREEN_GAMEPLAY;
    }
}
