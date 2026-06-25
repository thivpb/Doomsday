// tela_character_select.c
// Tela dedicada de SELEÇÃO DE PERSONAGEM: dois cards com PREVIEW AO VIVO —
// "ANTICORPO" (herói procedural clássico) e "ANTICORPO-V" (2º personagem, em
// sprites animados). Abre ao clicar JOGAR no menu; ao confirmar, segue para a
// seleção de dificuldade (fluxo normal de início de partida). A escolha é
// salva em Saves/config.txt (campo characterId) e preservada pelo InitGame.
//
// Fundo ANIMADO próprio (cultura/petri viva): reaproveita o catálogo de
// organismos do menu (GetMenuOrganism) — micróbios à deriva, em camadas de
// profundidade, que se acendem perto do card selecionado. Os previews reusam os
// MESMOS renderizadores do gameplay (DrawPlayerModel / DrawPlayer2Model) e
// refletem as skins/cosméticos que o jogador está usando.
#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include "../../include/sprite_manager.h"
#include "../../Assets/@models/player_model.h"
#include "../../Assets/@models/player2_model.h"
#include <math.h>

extern Vector2 g_virtualMouse; // mouse já corrigido pelo letterbox (main.c)

// ---- Layout (coordenadas virtuais 1280x720) ----
#define CS_CARD_W   360.0f
#define CS_CARD_H   404.0f
#define CS_CARD_GAP  80.0f
#define CS_CARD_Y   158.0f

// Fundo animado.
#define CS_ORG_COUNT   48   // micróbios à deriva (bactérias/vírus do catálogo)
#define CS_SPORE_COUNT 34   // esporos/partículas pequenas

static Rectangle CSCard(int i)
{
    float x0 = (SCREEN_WIDTH - (CS_CARD_W * 2.0f + CS_CARD_GAP)) * 0.5f;
    return (Rectangle){ x0 + i * (CS_CARD_W + CS_CARD_GAP), CS_CARD_Y, CS_CARD_W, CS_CARD_H };
}
static Rectangle CSConfirmRect(void) { return (Rectangle){ SCREEN_WIDTH * 0.5f - 330.0f, 602.0f, 320.0f, 46.0f }; }
static Rectangle CSBackRect(void)    { return (Rectangle){ SCREEN_WIDTH * 0.5f +  10.0f, 602.0f, 320.0f, 46.0f }; }

static const char *CS_NAME[CHARACTER_COUNT] = { "ANTICORPO", "ANTICORPO-V" };
static const char *CS_DESC[CHARACTER_COUNT] = {
    "Heroi classico do sistema imune, com todas as skins e cosmeticos.",
    "Variante mutante do sistema imune, com seu proprio estilo."
};
// Arma exibida no preview de cada personagem (apenas visual — não muda o
// equipamento real). Fácil de trocar: 1=Espada-Seringa, 2=Rifle, 3=Granada,
// 4=BFG, 5=Lamina Bioeletrica, 6=Rifle Replicante, 7=Lanca-Minas, 8=BFG Omega.
static const int CS_PREVIEW_WEAPON[CHARACTER_COUNT] = { 2, 5 };

// ----------------------------------------------------------------------------
// FUNDO ANIMADO
// ----------------------------------------------------------------------------
static Color CSLerpColor(Color a, Color b, float t)
{
    return (Color){
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        (unsigned char)(a.a + (b.a - a.a) * t)
    };
}

static void DrawCSOrganism(Texture2D tex, Vector2 c, float size, float rot, Color tint)
{
    if (tex.id == 0) { DrawCircleV(c, size * 0.42f, tint); return; } // fallback sem sprite
    float h = size * ((float)tex.height / (float)tex.width);
    Rectangle src = { 0.0f, 0.0f, (float)tex.width, (float)tex.height };
    Rectangle dst = { c.x, c.y, size, h };
    DrawTexturePro(tex, src, dst, (Vector2){ size * 0.5f, h * 0.5f }, rot, tint);
}

static void DrawCharSelectBackground(float time, float entry, Color accent, Vector2 selCenter)
{
    // 1) Gradiente base bio (verde-petróleo escuro).
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                           (Color){ 6, 20, 16, 255 }, (Color){ 3, 9, 11, 255 });

    // 2) Halo radial pulsante (a "cultura" respira) centrado na tela.
    float pulse = 0.5f + 0.5f * sinf(time * 1.2f);
    DrawCircleGradient(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 30,
                       430.0f + pulse * 34.0f, Fade(accent, 0.07f * entry), BLANK);

    int n = MenuOrganismsReady() ? MenuOrganismCount() : 0;

    // 3) Micróbios à deriva em 2 camadas (fundo menor/escuro; frente maior/claro).
    for (int i = 0; i < CS_ORG_COUNT; i++)
    {
        bool back = (i % 2 == 0);
        float depth     = back ? 0.45f : 1.0f;
        float size      = back ? 34.0f : 62.0f;
        float baseAlpha = (back ? 0.08f : 0.17f) * entry;
        float dir       = (i % 3) ? 1.0f : -1.0f;
        float speed     = (back ? 11.0f : 18.0f) * dir;
        float phase     = i * 0.7f;

        float x = fmodf((float)(i * 173) + time * speed + 260.0f, SCREEN_WIDTH + 520.0f) - 260.0f;
        float y = fmodf((float)(i * 97), (float)SCREEN_HEIGHT)
                  + sinf(time * (0.4f + 0.05f * i) + phase) * 28.0f * depth;
        float rot = time * (back ? 9.0f : 15.0f) * dir + phase * 40.0f;
        Vector2 c = { x, y };

        // Reação à escolha: micróbios perto do card selecionado tingem de accent
        // e brilham um pouco mais (vida pulsando ao redor do herói escolhido).
        float dx = c.x - selCenter.x, dy = c.y - selCenter.y;
        float dist = sqrtf(dx * dx + dy * dy);
        float near = 1.0f - fminf(dist / 360.0f, 1.0f);
        Color base = (Color){ 200, 235, 215, 255 };
        Color col  = CSLerpColor(base, accent, near * 0.85f);
        Color tint = Fade(col, baseAlpha * (0.7f + 0.6f * near));

        Texture2D tex = (n > 0) ? GetMenuOrganism(i % n) : (Texture2D){ 0 };
        DrawCSOrganism(tex, c, size, rot, tint);
    }

    // 4) Esporos minúsculos subindo (partículas de cultura).
    for (int i = 0; i < CS_SPORE_COUNT; i++)
    {
        float x = fmodf((float)(i * 71) + time * (6.0f + (i % 5)), (float)SCREEN_WIDTH);
        float y = fmodf((float)(i * 53) - time * 5.0f + 4.0f * SCREEN_HEIGHT, (float)SCREEN_HEIGHT);
        float r = 1.5f + (i % 3);
        float a = (0.10f + 0.06f * sinf(time * 2.0f + i)) * entry;
        DrawCircleV((Vector2){ x, y }, r, Fade(accent, a));
    }

    // 5) Vinhetas (topo p/ título, base p/ botões) — legibilidade.
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, 150, Fade(BLACK, 0.5f * entry), BLANK);
    DrawRectangleGradientV(0, SCREEN_HEIGHT - 130, SCREEN_WIDTH, 130, BLANK, Fade(BLACK, 0.4f * entry));
}

// Desenha o preview de um personagem dentro do retângulo `box`, centralizado.
// Usa as skins/cosméticos ATUAIS do jogador (cópia de game->player); apenas a
// arma exibida é trocada por CS_PREVIEW_WEAPON (showcase, sem afetar o jogo).
static void DrawCharPreview(GameState *game, int charId, Rectangle box, float time)
{
    Player tmp = game->player; // reflete skin/cosméticos escolhidos pelo jogador
    tmp.isMoving = false;
    tmp.facingDir = 1;
    tmp.squashX = 1.0f; tmp.squashY = 1.0f;
    tmp.attackBoostTimer = 0.0f;
    tmp.equippedWeapon = CS_PREVIEW_WEAPON[charId]; // arma de vitrine
    tmp.position = (Vector2){ box.x + box.width * 0.5f, box.y + box.height * 0.5f };

    if (charId == 1 && Player2SpritesReady())
    {
        // ANTICORPO-V (sprite). Leve ajuste vertical no preview.
        tmp.position.y = box.y + box.height * 0.46f;
        DrawPlayer2Model(&tmp, 60.0f, THEME_COLOR_MAIN, time, 0.0f, 0);
    }
    else
    {
        // ANTICORPO procedural (também é o fallback caso falte o sprite do V).
        DrawPlayerModel(&tmp, 78.0f, THEME_COLOR_MAIN, time, 0.0f);
    }
}

void DrawTelaCharacterSelect(GameState *game, Font font)
{
    float time = (float)GetTime();
    float entry = UIEase(game->screenAnim / 0.5f);
    Color accent = (Color){ 80, 230, 140, 255 }; // verde biológico

    int sel = (game->player.characterId >= 0 && game->player.characterId < CHARACTER_COUNT)
              ? game->player.characterId : 0;
    Rectangle selCard = CSCard(sel);
    Vector2 selCenter = { selCard.x + selCard.width * 0.5f, selCard.y + selCard.height * 0.5f };

    // Fundo animado próprio (micróbios à deriva que reagem à escolha).
    DrawCharSelectBackground(time, entry, accent, selCenter);

    DrawUIScreenTitle(font, "SELECIONE O PERSONAGEM", accent, entry);
    const char *sub = "Escolha quem entra em campo.";
    Vector2 ss = MeasureTextEx(font, sub, 16.0f, 1.0f);
    DrawTextEx(font, sub, (Vector2){ SCREEN_WIDTH / 2.0f - ss.x / 2.0f, 104.0f }, 16.0f, 1.0f, Fade(WHITE, 0.8f * entry));

    for (int i = 0; i < CHARACTER_COUNT; i++)
    {
        Rectangle r = CSCard(i);
        bool selected = (sel == i);
        bool hover = CheckCollisionPointRec(g_virtualMouse, r);
        bool pressed = hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        float ce = UIEase((game->screenAnim - 0.1f - i * 0.08f) / 0.4f);
        float dy = (1.0f - ce) * 50.0f + ((hover || selected) ? -6.0f : 0.0f) + (pressed ? 3.0f : 0.0f);
        Rectangle dr = { r.x, r.y + dy, r.width, r.height };

        // Fundo + borda por estado.
        DrawRectangleRounded(dr, 0.06f, 10, Fade((Color){ 10, 16, 18, 255 }, (selected ? 0.95f : 0.82f) * ce));
        if (selected)
        {
            float gp = 0.5f + 0.5f * sinf(time * 5.0f);
            DrawRectangleRoundedLines((Rectangle){ dr.x - 3, dr.y - 3, dr.width + 6, dr.height + 6 }, 0.06f, 10, Fade(accent, (0.4f + 0.5f * gp) * ce));
            DrawRectangleRoundedLines(dr, 0.06f, 10, Fade(accent, ce));
        }
        else
        {
            DrawRectangleRoundedLines(dr, 0.06f, 10, Fade(accent, (hover ? 0.9f : 0.5f) * ce));
        }
        // Faixa de acento no topo.
        DrawRectangleRounded((Rectangle){ dr.x + 14, dr.y + 12, dr.width - 28, 6 }, 0.8f, 4, Fade(accent, ce));

        // Nome.
        DrawTextFitCentered(font, CS_NAME[i], (Rectangle){ dr.x, dr.y + 24, dr.width, 38 }, 30.0f, Fade(accent, ce), true);

        // Caixa de preview (modelo ao vivo).
        Rectangle pv = { dr.x + 28, dr.y + 70, dr.width - 56, 232 };
        DrawRectangleRounded(pv, 0.06f, 8, Fade((Color){ 6, 12, 14, 255 }, 0.7f * ce));
        DrawRectangleRoundedLines(pv, 0.06f, 8, Fade(accent, 0.4f * ce));
        DrawCircleGradient((int)(pv.x + pv.width / 2.0f), (int)(pv.y + pv.height / 2.0f + 20.0f), 96.0f, Fade(accent, 0.16f * ce), BLANK);
        if (ce > 0.85f) // só desenha o modelo quando o card já "assentou" (evita tremor na entrada)
            DrawCharPreview(game, i, pv, time);

        // Descrição.
        DrawTextWrapped(font, CS_DESC[i], (Rectangle){ dr.x + 24, dr.y + 312, dr.width - 48, 60 }, 14.0f, 1.0f, Fade(WHITE, 0.82f * ce));

        // Selecionado.
        if (selected)
        {
            const char *z = "> SELECIONADO <";
            Vector2 zs = MeasureTextEx(font, z, 14.0f, 1.0f);
            DrawTextEx(font, z, (Vector2){ dr.x + dr.width / 2.0f - zs.x / 2.0f, dr.y + dr.height - 28 }, 14.0f, 1.0f, Fade(accent, ce));
        }
    }

    // Botões: CONFIRMAR (segue p/ dificuldade) + VOLTAR (menu).
    UIButton confirmBtn = { CSConfirmRect(), "CONFIRMAR E AVANCAR", CheckCollisionPointRec(g_virtualMouse, CSConfirmRect()), false };
    DrawButton(confirmBtn, font, true);
    UIButton backBtn = { CSBackRect(), "VOLTAR", CheckCollisionPointRec(g_virtualMouse, CSBackRect()), false };
    DrawButton(backBtn, font, true);

    const char *nav = "Setas / A-D para escolher   -   ENTER confirma   -   ESC volta";
    Vector2 ns = MeasureTextEx(font, nav, 14.0f, 1.0f);
    DrawTextEx(font, nav, (Vector2){ SCREEN_WIDTH / 2.0f - ns.x / 2.0f, SCREEN_HEIGHT - 26.0f }, 14.0f, 1.0f, Fade(THEME_COLOR_TEXT, 0.6f));
}

void UpdateTelaCharacterSelect(GameState *game, Vector2 mouse)
{
    if (game->player.characterId < 0 || game->player.characterId >= CHARACTER_COUNT)
        game->player.characterId = 0;

    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // Seleção por clique no card.
    for (int i = 0; i < CHARACTER_COUNT; i++)
        if (clicked && CheckCollisionPointRec(mouse, CSCard(i)) && game->player.characterId != i)
        {
            game->player.characterId = i;
            SavePlayerConfig(game); // persiste imediatamente (igual ao guarda-roupa)
        }

    // Navegação por teclado (alterna entre os dois personagens).
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) ||
        IsKeyPressed(KEY_LEFT)  || IsKeyPressed(KEY_A))
    {
        game->player.characterId = (game->player.characterId + 1) % CHARACTER_COUNT;
        SavePlayerConfig(game);
    }

    // VOLTAR / ESC -> menu.
    bool backClick = clicked && CheckCollisionPointRec(mouse, CSBackRect());
    if (backClick || IsKeyPressed(KEY_ESCAPE))
    {
        game->currentScreen = SCREEN_MENU;
        return;
    }

    // CONFIRMAR / ENTER -> segue para a seleção de dificuldade (fluxo de início).
    bool confirmClick = clicked && CheckCollisionPointRec(mouse, CSConfirmRect());
    if (confirmClick || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
    {
        SavePlayerConfig(game);
        game->pendingDifficulty = game->difficulty;
        game->diffReturnScreen  = SCREEN_MENU; // mantém o fluxo "novo jogo" (tutorial)
        game->currentScreen     = SCREEN_DIFFICULTY_SELECT;
    }
}
