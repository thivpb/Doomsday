// tela_admin.c
// Modo Administrador / Dev protegido por senha ("cyberbullies").
// Permite configurar atributos do jogador e ativa atalhos de teste no gameplay.
#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ADMIN_PASSWORD "cyberbullies"

static char pwInput[40] = "";
static bool pwFocused = true;
static bool showError = false;
static float pwShake = 0.0f;    // > 0 = tremor do input (senha incorreta)
static float pwSuccess = 0.0f;  // > 0 = animação "ACESSO AUTORIZADO" antes de abrir
static UIButton adminBack = { { 490, 650, 300, 44 }, "VOLTAR", false, false };

// Linhas configuráveis (label, ponteiro, passo, min, max). Geometria compartilhada
// entre Draw e Update.
static Rectangle RowRect(int row)      { return (Rectangle){ 360, 250.0f + row * 56.0f, 560, 44 }; }
static Rectangle MinusRect(int row)    { return (Rectangle){ 760, 250.0f + row * 56.0f, 44, 44 }; }
static Rectangle PlusRect(int row)     { return (Rectangle){ 876, 250.0f + row * 56.0f, 44, 44 }; }
static Rectangle UnlockWeaponsRect(void) { return (Rectangle){ 360, 520, 270, 38 }; }
static Rectangle UnlockSkinsRect(void)   { return (Rectangle){ 650, 520, 270, 38 }; }
static Rectangle AdminOffRect(void)      { return (Rectangle){ 440, 604, 400, 36 }; }

static void DrawAdminToggle(Font font, Rectangle r, const char *label, bool active)
{
    Color fill = active ? Fade((Color){ 45, 190, 110, 255 }, 0.38f) : Fade((Color){ 120, 120, 130, 255 }, 0.22f);
    Color line = active ? (Color){ 95, 255, 165, 255 } : Fade(WHITE, 0.45f);
    const char *state = active ? "ON" : "OFF";
    DrawRectangleRounded(r, 0.22f, 6, fill);
    DrawRectangleRoundedLines(r, 0.22f, 6, line);
    DrawTextEx(font, label, (Vector2){ r.x + 14, r.y + 10 }, 16.0f, 1.0f, WHITE);
    DrawTextEx(font, state, (Vector2){ r.x + r.width - 46, r.y + 10 }, 16.0f, 1.0f, line);
}

static void ActivateAdminDefaults(GameState *game)
{
    game->adminMode = true;
    game->adminApply = true;
    game->adminUnlockWeapons = true;
    game->adminUnlockSkins = true;
    if (game->adminMaxHp <= 0)  game->adminMaxHp = 300;
    if (game->adminDamage <= 0) game->adminDamage = 50;
    if (game->adminSpeed <= 0)  game->adminSpeed = 340.0f;
    if (game->adminLevel <= 0)  game->adminLevel = 4;
    if (game->adminSus <= 0)    game->adminSus = 200;
}

void DrawTelaAdmin(GameState *game, Font font)
{
    DrawThemedBackground(SCREEN_ADMIN, (float)GetTime(), game->screenAnim / 0.4f);

    const char *title = "MODO ADMINISTRADOR / DEV";
    Vector2 tSz = MeasureTextEx(font, title, 38.0f, 1.2f);
    DrawTextEx(font, title, (Vector2){ SCREEN_WIDTH / 2.0f - tSz.x / 2.0f, 50.0f }, 38.0f, 1.2f, (Color){ 255, 210, 60, 255 });

    if (!game->adminMode)
    {
        // ---- Tela de SENHA ----
        const char *info = "Area restrita. Digite a senha de desenvolvedor e tecle ENTER.";
        Vector2 iSz = MeasureTextEx(font, info, 18.0f, 1.0f);
        DrawTextEx(font, info, (Vector2){ SCREEN_WIDTH / 2.0f - iSz.x / 2.0f, 200.0f }, 18.0f, 1.0f, Fade(WHITE, 0.85f));

        // Input centralizado, com tremor (erro) e cor de borda por estado.
        float shakeX = (pwShake > 0.0f) ? sinf((float)GetTime() * 50.0f) * 9.0f * (pwShake / 0.45f) : 0.0f;
        Rectangle box = { 440 + shakeX, 280, 400, 50 };
        Color border = (pwSuccess > 0.0f) ? (Color){ 90, 255, 160, 255 }
                     : showError            ? (Color){ 255, 80, 80, 255 }
                     : (pwFocused ? (Color){ 255, 210, 60, 255 } : THEME_COLOR_BORDER);
        DrawRectangleRounded(box, 0.2f, 6, Fade((Color){ 10, 10, 14, 255 }, 0.9f));
        DrawRectangleRoundedLines(box, 0.2f, 6, border);

        // Senha mascarada + cursor piscando.
        char masked[40];
        int len = (int)strlen(pwInput);
        for (int i = 0; i < len && i < 39; i++) masked[i] = '*';
        masked[(len < 39) ? len : 39] = '\0';
        char shown[44];
        snprintf(shown, sizeof(shown), "%s%s", masked, (pwFocused && (int)(GetTime() * 2) % 2 == 0) ? "|" : "");
        DrawTextEx(font, shown, (Vector2){ box.x + 16, box.y + 14 }, 24.0f, 1.0f, WHITE);

        if (pwSuccess > 0.0f)
        {
            // Animação de confirmação: verde/ciano + pulso.
            float p = 1.0f - (pwSuccess / 0.9f);
            const char *ok = "ACESSO AUTORIZADO";
            float fs = 26.0f + 6.0f * sinf((float)GetTime() * 10.0f);
            Vector2 oSz = MeasureTextEx(font, ok, fs, 1.5f);
            DrawTextEx(font, ok, (Vector2){ SCREEN_WIDTH / 2.0f - oSz.x / 2.0f, 352.0f }, fs, 1.5f, (Color){ 100, 255, 170, 255 });
            DrawCircleLines(SCREEN_WIDTH / 2, 300, 60.0f + p * 220.0f, Fade((Color){ 100, 255, 170, 255 }, 1.0f - p));
        }
        else if (showError)
        {
            const char *err = "Senha incorreta. Tente novamente.";
            Vector2 eSz = MeasureTextEx(font, err, 18.0f, 1.0f);
            DrawTextEx(font, err, (Vector2){ SCREEN_WIDTH / 2.0f - eSz.x / 2.0f, 352.0f }, 18.0f, 1.0f, (Color){ 255, 90, 90, 255 });
        }
    }
    else
    {
        // ---- Painel de CONFIGURACAO (admin ativo) ----
        const char *active = "ADMIN ATIVO — estes valores serao aplicados ao iniciar o jogo.";
        Vector2 aSz = MeasureTextEx(font, active, 18.0f, 1.0f);
        DrawTextEx(font, active, (Vector2){ SCREEN_WIDTH / 2.0f - aSz.x / 2.0f, 180.0f }, 18.0f, 1.0f, (Color){ 120, 255, 160, 255 });

        const char *labels[5] = { "Vida Maxima", "Dano (ataque)", "Velocidade", "Nivel inicial", "Pontos SUS" };
        int values[5] = { game->adminMaxHp, game->adminDamage, (int)game->adminSpeed, game->adminLevel, game->adminSus };
        for (int r = 0; r < 5; r++)
        {
            Rectangle row = RowRect(r);
            DrawRectangleRounded(row, 0.2f, 6, Fade((Color){ 12, 12, 18, 255 }, 0.85f));
            DrawRectangleRoundedLines(row, 0.2f, 6, THEME_COLOR_BORDER);
            DrawTextEx(font, labels[r], (Vector2){ row.x + 16, row.y + 12 }, 20.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("%d", values[r]), (Vector2){ row.x + 330, row.y + 12 }, 20.0f, 1.0f, (Color){ 255, 210, 60, 255 });

            Rectangle mn = MinusRect(r), pl = PlusRect(r);
            DrawRectangleRounded(mn, 0.3f, 4, Fade(THEME_COLOR_BORDER, 0.8f));
            DrawRectangleRounded(pl, 0.3f, 4, Fade(THEME_COLOR_BORDER, 0.8f));
            DrawTextEx(font, "-", (Vector2){ mn.x + 16, mn.y + 8 }, 26.0f, 1.0f, WHITE);
            DrawTextEx(font, "+", (Vector2){ pl.x + 12, pl.y + 8 }, 26.0f, 1.0f, WHITE);
        }

        DrawAdminToggle(font, UnlockWeaponsRect(), "DESBLOQUEAR ARMAS", game->adminUnlockWeapons);
        DrawAdminToggle(font, UnlockSkinsRect(), "DESBLOQUEAR SKINS", game->adminUnlockSkins);

        const char *hint = "No jogo: [.] limpar fase  [H] cura  [L] +nivel  [P] +SUS  [K] so comuns  [ [ ] [ ] ] wave";
        Vector2 hSz = MeasureTextEx(font, hint, 15.0f, 1.0f);
        DrawTextEx(font, hint, (Vector2){ SCREEN_WIDTH / 2.0f - hSz.x / 2.0f, 574.0f }, 15.0f, 1.0f, Fade(WHITE, 0.7f));

        // Botao desativar
        Rectangle off = AdminOffRect();
        DrawRectangleRounded(off, 0.25f, 6, Fade(RED, 0.25f));
        DrawRectangleRoundedLines(off, 0.25f, 6, RED);
        const char *offt = "DESATIVAR MODO ADMIN";
        Vector2 oSz = MeasureTextEx(font, offt, 18.0f, 1.0f);
        DrawTextEx(font, offt, (Vector2){ SCREEN_WIDTH / 2.0f - oSz.x / 2.0f, off.y + 8 }, 18.0f, 1.0f, RED);
    }

    DrawButton(adminBack, font, true);
    DrawTextEx(font, "ESC para voltar", (Vector2){ 20, SCREEN_HEIGHT - 28 }, 14.0f, 1.0f, DARKGRAY);
}

static void Adjust(int *v, int step, int lo, int hi)
{
    *v += step;
    if (*v < lo) *v = lo;
    if (*v > hi) *v = hi;
}

void UpdateTelaAdmin(GameState *game, Vector2 mouse)
{
    adminBack.hover = CheckCollisionPointRec(mouse, adminBack.bounds);
    if ((adminBack.hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || IsKeyPressed(KEY_ESCAPE))
    {
        game->currentScreen = SCREEN_MENU;
        return;
    }

    if (!game->adminMode)
    {
        float dt = GetFrameTime();
        if (pwShake > 0.0f) pwShake -= dt;

        // Animação de sucesso em andamento: abre o painel ao terminar.
        if (pwSuccess > 0.0f)
        {
            pwSuccess -= dt;
            if (pwSuccess <= 0.0f)
            {
                pwSuccess = 0.0f;
                ActivateAdminDefaults(game);
                pwInput[0] = '\0';
            }
            return; // ignora digitação durante a confirmação
        }

        // Digitação da senha (limpa o erro ao editar — input continua utilizável).
        int key = GetCharPressed();
        while (key > 0)
        {
            if (key >= 32 && key <= 125 && strlen(pwInput) < sizeof(pwInput) - 1)
            {
                int l = (int)strlen(pwInput);
                pwInput[l] = (char)key;
                pwInput[l + 1] = '\0';
                showError = false;
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE))
        {
            int l = (int)strlen(pwInput);
            if (l > 0) pwInput[l - 1] = '\0';
            showError = false;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            if (strcmp(pwInput, ADMIN_PASSWORD) == 0)
            {
                pwSuccess = 0.9f;   // inicia a confirmação animada (verde/ciano)
                showError = false;
            }
            else
            {
                showError = true;   // feedback vermelho + tremor; mantém o texto editável
                pwShake = 0.45f;
            }
        }
        return;
    }

    // Admin ativo: ajustar valores
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        for (int r = 0; r < 5; r++)
        {
            bool minus = CheckCollisionPointRec(mouse, MinusRect(r));
            bool plus  = CheckCollisionPointRec(mouse, PlusRect(r));
            if (!minus && !plus) continue;
            int s = plus ? 1 : -1;
            switch (r)
            {
                case 0: Adjust(&game->adminMaxHp, s * 25, 50, 9999); break;
                case 1: Adjust(&game->adminDamage, s * 5, 5, 9999); break;
                case 2: { int sp = (int)game->adminSpeed; Adjust(&sp, s * 20, 100, 1200); game->adminSpeed = (float)sp; } break;
                case 3: Adjust(&game->adminLevel, s * 1, 1, 99); break;
                case 4: Adjust(&game->adminSus, s * 50, 0, 9999); break;
            }
            game->adminApply = true;
        }

        if (CheckCollisionPointRec(mouse, UnlockWeaponsRect()))
        {
            game->adminUnlockWeapons = !game->adminUnlockWeapons;
            game->adminApply = true;
            if (game->adminUnlockWeapons)
            {
                game->maxWeaponUnlocked = 4;
                if (game->totalEnemiesKilled < BIOBLADE_UNLOCK_KILLS)
                    game->totalEnemiesKilled = BIOBLADE_UNLOCK_KILLS;
                game->bioBladeAnnounced = true;
            }
        }

        if (CheckCollisionPointRec(mouse, UnlockSkinsRect()))
        {
            game->adminUnlockSkins = !game->adminUnlockSkins;
            game->adminApply = true;
        }

        // Botao desativar
        Rectangle off = AdminOffRect();
        if (CheckCollisionPointRec(mouse, off))
        {
            game->adminMode = false;
            game->adminApply = false;
            game->adminUnlockWeapons = false;
            game->adminUnlockSkins = false;
        }
    }
}
