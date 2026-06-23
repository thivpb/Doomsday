#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include <stdio.h>
#include <string.h>

static UIButton upgradeBtns[3];
static UIButton btnNextWave;

void DrawTelaUpgrade(GameState *game, Font font)
{
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, THEME_COLOR_BG_DARK, THEME_COLOR_BG_LIGHT);

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
            // Ir para a próxima onda
            StartNextWave(game);
            game->currentScreen = SCREEN_GAMEPLAY;
        }
    } else {
        btnNextWave.hover = false;
    }
}
