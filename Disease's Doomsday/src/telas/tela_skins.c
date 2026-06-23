// tela_skins.c
// GUARDA-ROUPA modular do Anticorpo: personalização por partes (capacete, facial,
// peitoral, braços, botas, efeito) + variações de material (paleta do
// corpo) e cor da arma. Preview ao vivo, itens bloqueados com explicação,
// equipar/remover/restaurar, persistência, mouse + teclado.
//
// É DATA-DRIVEN: as peças vêm do catálogo (Assets/@models/cosmetics.c) e o estado
// equipado vive em player->cosmetics[]. O layout usa CONSTANTES centralizadas
// (sem coordenadas frágeis espalhadas). A categoria "Traseiro" (COS_BACK) foi
// removida do jogo; "Calças" também saiu da UI por não encaixar bem no modelo.
#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include "../../include/asset_manager.h"
#include "../../Assets/@models/player_model.h"
#include "../../Assets/@models/cosmetics.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

extern Vector2 g_virtualMouse; // mouse já corrigido pelo letterbox (main.c)

// ---- Slots VISÍVEIS na UI (ordem de exibição; exclui COS_BACK e COS_LEGS) ----
static const CosmeticSlot WB_VISIBLE[] = {
    COS_HELMET, COS_FACE, COS_CHEST, COS_ARMS, COS_BOOTS, COS_FX
};
#define WB_VIS_COUNT ((int)(sizeof(WB_VISIBLE) / sizeof(WB_VISIBLE[0])))

// ---- Layout CENTRALIZADO (coordenadas virtuais 1280x720, letterboxed) ----
#define WB_LX          44.0f    // coluna esquerda: x e largura
#define WB_LW         380.0f
#define WB_MAT_Y      512.0f
#define WB_MAT_H       44.0f
#define WB_PV_Y       176.0f
#define WB_PV_H       298.0f
#define WB_WPN_Y      584.0f
#define WB_WPN_H       40.0f
#define WB_TAB_X      486.0f
#define WB_TAB_W      112.0f
#define WB_TAB_Y      184.0f
#define WB_TAB_H       44.0f
#define WB_TAB_PITCH  120.0f
#define WB_LIST_X     486.0f
#define WB_LIST_W     340.0f
#define WB_LIST_Y     262.0f
#define WB_CARD_H     132.0f
#define WB_CARD_PITCH 150.0f

// ---- Estado de UI (transitório) ----
static int   wbSel    = 0;     // índice em WB_VISIBLE (categoria selecionada)
static int   wbFocus  = 0;     // item em foco dentro do slot
static float wbDenied = 0.0f;  // feedback "bloqueado"
static float wbToast  = 0.0f;  // timer do toast
static float wbEquipSfxTimer = 0.0f;
static char  wbToastMsg[56] = { 0 };

static CosmeticSlot CurSlot(void) { return WB_VISIBLE[(wbSel < 0 || wbSel >= WB_VIS_COUNT) ? 0 : wbSel]; }
static int EffLevel(GameState *game)
{
    if (game->adminMode && game->adminUnlockSkins) return 99;
    return (game->player.level > 1) ? game->player.level : 1;
}
static void Toast(const char *m) { snprintf(wbToastMsg, sizeof(wbToastMsg), "%s", m); wbToast = 1.6f; }
static void PlaySkinEquipSfx(void)
{
    if (g_assets.sfxArmorEquip.frameCount <= 0) return;
    if (IsSoundPlaying(g_assets.sfxArmorEquip)) StopSound(g_assets.sfxArmorEquip);
    wbEquipSfxTimer = 0.0f;
    PlaySound(g_assets.sfxArmorEquip);
}

// ---- Geometria ÚNICA (desenho == hitbox) ----
static Rectangle PreviewRect(void)       { return (Rectangle){ WB_LX, WB_PV_Y, WB_LW, WB_PV_H }; }
static Rectangle MaterialSwatch(int i)   { return (Rectangle){ WB_LX + 12.0f + i * 121.0f, WB_MAT_Y, 112.0f, WB_MAT_H }; }
static Rectangle WeaponSwatchRect(int i) { return (Rectangle){ WB_LX + 12.0f + i * 121.0f, WB_WPN_Y, 112.0f, WB_WPN_H }; }
static Rectangle SlotTabRect(int i)      { return (Rectangle){ WB_TAB_X + i * WB_TAB_PITCH, WB_TAB_Y, WB_TAB_W, WB_TAB_H }; }
static Rectangle ItemCardRect(int i)
{
    int col = i % 2;
    int row = i / 2;
    return (Rectangle){ WB_LIST_X + col * 362.0f, WB_LIST_Y + row * WB_CARD_PITCH, WB_LIST_W, WB_CARD_H };
}
static Rectangle BtnRestoreRect(void)    { return (Rectangle){ 470, 648, 260, 44 }; }
static Rectangle BtnBackRect(void)       { return (Rectangle){ 780, 648, 360, 44 }; }

void DrawTelaSkins(GameState *game, Font font)
{
    float time = (float)GetTime();
    DrawThemedBackground(SCREEN_SKINS, time, game->screenAnim / 0.4f);
    float entry = UIEase(game->screenAnim / 0.4f);

    DrawUIScreenTitle(font, "GUARDA-ROUPA DO ANTICORPO", (Color){ 200, 110, 255, 255 }, entry);
    const char *sub = "Personalize o Anticorpo.";
    Vector2 s = MeasureTextEx(font, sub, 16.0f, 1.0f);
    DrawTextEx(font, sub, (Vector2){ SCREEN_WIDTH / 2.0f - s.x / 2.0f, 100.0f }, 16.0f, 1.0f, Fade(WHITE, 0.78f));

    int lvl = EffLevel(game);
    Color accent = (Color){ 200, 110, 255, 255 };
    Rectangle leftPanel = { 44, 138, 380, 500 };
    Rectangle rightPanel = { 462, 138, 776, 500 };
    DrawPanel(leftPanel, Fade(accent, entry), 0.62f * entry);
    DrawPanel(rightPanel, Fade(accent, entry), 0.62f * entry);

    // ============================ COLUNA ESQUERDA ============================
    DrawTextEx(font, "PREVIEW", (Vector2){ leftPanel.x + 22, leftPanel.y + 10 }, 16.0f, 1.0f, Fade(WHITE, 0.72f));
    Rectangle pv = PreviewRect();
    DrawRectangleRounded(pv, 0.06f, 8, Fade((Color){ 8, 16, 22, 255 }, 0.74f));
    DrawRectangleRoundedLines(pv, 0.06f, 8, Fade(accent, 0.58f));
    DrawCircleGradient((int)(pv.x + pv.width / 2), (int)(pv.y + pv.height / 2 + 24),
                       132.0f, Fade(accent, 0.18f), BLANK);
    DrawEllipse((int)(pv.x + pv.width / 2.0f), (int)(pv.y + pv.height - 54.0f), 96.0f, 22.0f, Fade(THEME_COLOR_MAIN, 0.10f));

    Player tmp = game->player; // cópia: reflete skin + cosméticos equipados
    tmp.position = (Vector2){ pv.x + pv.width / 2.0f, pv.y + pv.height / 2.0f + 38.0f };
    tmp.isMoving = false; tmp.facingDir = 1;
    tmp.squashX = 1.0f; tmp.squashY = 1.0f; tmp.attackBoostTimer = 0.0f;
    DrawPlayerModel(&tmp, 92.0f, THEME_COLOR_MAIN, time, 0.0f);

    // Nome da skin atual (canto inferior do quadro, sem invadir).
    DrawTextEx(font, PlayerSkinName(game->player.skinId),
               (Vector2){ pv.x + 14, pv.y + pv.height - 26 }, 18.0f, 1.0f, GOLD);

    DrawTextEx(font, "VESTIMENTA", (Vector2){ WB_LX + 12, WB_MAT_Y - 20 }, 13.0f, 1.0f, Fade(WHITE, 0.62f));
    for (int i = 0; i < SKIN_COUNT; i++)
    {
        Rectangle sw = MaterialSwatch(i);
        bool active = (game->player.skinId == i);
        bool hov = CheckCollisionPointRec(g_virtualMouse, sw);
        DrawRectangleRounded(sw, 0.18f, 6, active ? Fade(THEME_COLOR_MAIN, 0.22f)
                                               : hov ? Fade((Color){ 40, 60, 80, 255 }, 0.72f)
                                                     : Fade((Color){ 12, 12, 22, 255 }, 0.80f));
        DrawRectangleRoundedLines(sw, 0.18f, 6, active ? THEME_COLOR_MAIN : Fade(THEME_COLOR_BORDER, 0.76f));
        DrawTextFitCentered(font, PlayerSkinName(i), sw, 15.0f, active ? THEME_COLOR_MAIN : Fade(WHITE, 0.82f), true);
    }

    DrawTextEx(font, "DISPARO", (Vector2){ WB_LX + 12, WB_WPN_Y - 20 }, 13.0f, 1.0f, Fade(WHITE, 0.62f));
    for (int i = 0; i < WEAPON_SKIN_COUNT; i++)
    {
        Rectangle sw = WeaponSwatchRect(i);
        bool active = (game->player.weaponSkinId == i);
        bool hov = CheckCollisionPointRec(g_virtualMouse, sw);
        DrawRectangleRounded(sw, 0.25f, 6, active ? Fade(WeaponSkinPrimary(i), 0.3f)
                                                : hov ? Fade((Color){ 40, 60, 80, 255 }, 0.7f)
                                                      : Fade((Color){ 12, 12, 22, 255 }, 0.85f));
        DrawRectangleRoundedLines(sw, 0.25f, 6, active ? WeaponSkinPrimary(i) : THEME_COLOR_BORDER);
        DrawCircleV((Vector2){ sw.x + 18, sw.y + sw.height / 2 }, 9.0f, WeaponSkinPrimary(i));
        DrawCircleLines((int)(sw.x + 18), (int)(sw.y + sw.height / 2), 9.0f, WeaponSkinSecondary(i));
        DrawTextFitCentered(font, WeaponSkinName(i), (Rectangle){ sw.x + 30, sw.y, sw.width - 34, sw.height }, 13.0f, active ? WeaponSkinPrimary(i) : WHITE, true);
    }

    // ============================ COLUNA DIREITA ============================
    DrawTextEx(font, "PECAS", (Vector2){ rightPanel.x + 24, rightPanel.y + 14 }, 16.0f, 1.0f, Fade(WHITE, 0.72f));
    for (int i = 0; i < WB_VIS_COUNT; i++)
    {
        Rectangle tab = SlotTabRect(i);
        CosmeticSlot slot = WB_VISIBLE[i];
        bool sel = (i == wbSel);
        bool hov = CheckCollisionPointRec(g_virtualMouse, tab);
        DrawRectangleRounded(tab, 0.25f, 5, sel ? Fade((Color){ 190, 110, 255, 255 }, 0.25f)
                                               : hov ? Fade((Color){ 120, 80, 160, 255 }, 0.18f)
                                                     : Fade((Color){ 12, 12, 22, 255 }, 0.8f));
        DrawRectangleRoundedLines(tab, 0.25f, 5, sel ? (Color){ 200, 130, 255, 255 } : THEME_COLOR_BORDER);
        DrawTextFitCentered(font, CosmeticSlotShort(slot), tab, 15.0f, sel ? WHITE : Fade(WHITE, 0.8f), true);
        if (game->player.cosmetics[slot] > 0)
            DrawCircleV((Vector2){ tab.x + tab.width - 14, tab.y + tab.height / 2 }, 4.0f, (Color){ 0, 220, 120, 255 });
    }

    // Cabeçalho do slot + itens.
    CosmeticSlot cur = CurSlot();
    DrawTextEx(font, CosmeticSlotName(cur), (Vector2){ WB_LIST_X, 238 }, 18.0f, 1.0f, (Color){ 200, 130, 255, 255 });
    int count = CosmeticItemCount(cur);
    int equipped = game->player.cosmetics[cur];
    for (int i = 0; i < count; i++)
    {
        Rectangle card = ItemCardRect(i);
        const CosmeticItem *it = CosmeticGet(cur, i);
        if (!it) continue;
        bool unlocked = CosmeticUnlocked(cur, i, lvl);
        bool isEquip = (i == equipped);
        bool isFocus = (i == wbFocus);

        Color border = isEquip ? (Color){ 0, 220, 120, 255 } : isFocus ? (Color){ 200, 130, 255, 255 } : THEME_COLOR_BORDER;
        Color bg = isEquip ? Fade((Color){ 0, 220, 120, 255 }, 0.12f)
                 : isFocus ? Fade((Color){ 190, 110, 255, 255 }, 0.12f)
                           : Fade((Color){ 12, 12, 22, 255 }, 0.82f);
        DrawRectangleRounded(card, 0.08f, 6, bg);
        DrawRectangleRoundedLines(card, 0.08f, 6, border);
        if (isFocus) DrawRectangleRoundedLines(card, 0.08f, 6, Fade(WHITE, 0.3f + 0.3f * sinf(time * 6.0f)));

        DrawCircleGradient((int)(card.x + 46), (int)(card.y + 47), 30.0f, Fade(it->tint, unlocked ? 0.42f : 0.16f), BLANK);
        DrawRectangleRounded((Rectangle){ card.x + 20, card.y + 22, 52, 52 }, 0.25f, 5, Fade(it->tint, unlocked ? 0.9f : 0.3f));
        DrawRectangleRoundedLines((Rectangle){ card.x + 20, card.y + 22, 52, 52 }, 0.25f, 5, Fade(WHITE, 0.4f));

        DrawTextEx(font, it->name, (Vector2){ card.x + 88, card.y + 20 }, 18.0f, 1.0f, unlocked ? WHITE : Fade(WHITE, 0.5f));
        DrawTextWrapped(font, it->desc, (Rectangle){ card.x + 88, card.y + 48, card.width - 110, 58 }, 13.0f, 1.0f, Fade(WHITE, unlocked ? 0.72f : 0.4f));

        if (!unlocked)
        {
            Rectangle tag = { card.x + 18, card.y + card.height - 34, 88, 22 };
            DrawRectangleRounded(tag, 0.45f, 6, Fade(RED, 0.18f));
            DrawTextFitCentered(font, TextFormat("Nivel %d", it->unlockLevel), tag, 13.0f, (Color){ 255, 130, 130, 255 }, true);
        }
        else if (isEquip)
        {
            Rectangle tag = { card.x + 18, card.y + card.height - 34, 98, 22 };
            DrawRectangleRounded(tag, 0.45f, 6, Fade((Color){ 0, 220, 120, 255 }, 0.18f));
            DrawTextFitCentered(font, "EQUIPADO", tag, 13.0f, (Color){ 0, 230, 130, 255 }, true);
        }
        else
        {
            Rectangle tag = { card.x + 18, card.y + card.height - 34, 84, 22 };
            DrawRectangleRounded(tag, 0.45f, 6, Fade(WHITE, 0.06f));
            DrawTextFitCentered(font, "EQUIPAR", tag, 13.0f, Fade(WHITE, 0.68f), true);
        }
    }

    Vector2 mp = g_virtualMouse;
    UIButton bRestore = { BtnRestoreRect(), "RESTAURAR PADRAO", CheckCollisionPointRec(mp, BtnRestoreRect()), false };
    UIButton bBack    = { BtnBackRect(),    "VOLTAR E SALVAR",  CheckCollisionPointRec(mp, BtnBackRect()), false };
    DrawButton(bRestore, font, true);
    DrawButton(bBack, font, true);

    // Toast / feedback.
    if (wbToast > 0.0f)
    {
        float a = wbToast > 1.4f ? (1.6f - wbToast) / 0.2f : (wbToast < 0.3f ? wbToast / 0.3f : 1.0f);
        Vector2 tz = MeasureTextEx(font, wbToastMsg, 18.0f, 1.0f);
        Rectangle tb = { SCREEN_WIDTH / 2.0f - tz.x / 2.0f - 16, 686, tz.x + 32, 30 };
        DrawRectangleRounded(tb, 0.5f, 6, Fade((Color){ 10, 10, 20, 255 }, 0.9f * a));
        DrawRectangleRoundedLines(tb, 0.5f, 6, Fade((Color){ 255, 120, 120, 255 }, a));
        DrawTextEx(font, wbToastMsg, (Vector2){ SCREEN_WIDTH / 2.0f - tz.x / 2.0f, 691 }, 18.0f, 1.0f, Fade(WHITE, a));
    }
}

// Equipa o item em foco (se liberado); feedback visual + sonoro.
static void EquipFocused(GameState *game)
{
    CosmeticSlot cur = CurSlot();
    if (!CosmeticUnlocked(cur, wbFocus, EffLevel(game)))
    {
        wbDenied = 0.5f;
        Toast("Item bloqueado: suba de nivel para liberar");
        return;
    }
    if (game->player.cosmetics[cur] != wbFocus)
    {
        game->player.cosmetics[cur] = wbFocus;
        SavePlayerConfig(game);
        PlaySkinEquipSfx();
    }
}

static void RemoveCurrent(GameState *game)
{
    game->player.cosmetics[CurSlot()] = 0;
    SavePlayerConfig(game);
    PlaySkinEquipSfx();
    Toast("Peca removida");
}

void UpdateTelaSkins(GameState *game, Vector2 mouse)
{
    float dt = GetFrameTime();
    if (wbToast > 0.0f) wbToast -= dt;
    if (wbDenied > 0.0f) wbDenied -= dt;
    if (IsSoundPlaying(g_assets.sfxArmorEquip))
    {
        wbEquipSfxTimer += dt;
        if (wbEquipSfxTimer >= 1.0f)
        {
            StopSound(g_assets.sfxArmorEquip);
            wbEquipSfxTimer = 0.0f;
        }
    }
    else
    {
        wbEquipSfxTimer = 0.0f;
    }
    if (wbSel < 0 || wbSel >= WB_VIS_COUNT) wbSel = 0;

    CosmeticSlot cur = CurSlot();
    int count = CosmeticItemCount(cur);
    if (count < 1) count = 1;
    if (wbFocus >= count) wbFocus = count - 1;
    if (wbFocus < 0) wbFocus = 0;

    // ---- Teclado: navegação + ações ----
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S))
    { wbSel = (wbSel + 1) % WB_VIS_COUNT; wbFocus = game->player.cosmetics[CurSlot()]; }
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))
    { wbSel = (wbSel + WB_VIS_COUNT - 1) % WB_VIS_COUNT; wbFocus = game->player.cosmetics[CurSlot()]; }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) wbFocus = (wbFocus + 1) % count;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))  wbFocus = (wbFocus + count - 1) % count;
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) EquipFocused(game);
    if (IsKeyPressed(KEY_R)) RemoveCurrent(game);

    // ---- Mouse: abas de categoria ----
    for (int i = 0; i < WB_VIS_COUNT; i++)
        if (CheckCollisionPointRec(mouse, SlotTabRect(i)) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        { wbSel = i; wbFocus = game->player.cosmetics[WB_VISIBLE[i]]; }

    // ---- Mouse: cards de item (hover foca; clique equipa) ----
    int cnt = CosmeticItemCount(CurSlot());
    for (int i = 0; i < cnt; i++)
        if (CheckCollisionPointRec(mouse, ItemCardRect(i)))
        {
            wbFocus = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) EquipFocused(game);
        }

    // ---- Material (paleta) + arma ----
    for (int i = 0; i < SKIN_COUNT; i++)
        if (CheckCollisionPointRec(mouse, MaterialSwatch(i)) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        { game->player.skinId = i; SavePlayerConfig(game); PlaySkinEquipSfx(); }
    for (int i = 0; i < WEAPON_SKIN_COUNT; i++)
        if (CheckCollisionPointRec(mouse, WeaponSwatchRect(i)) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        { game->player.weaponSkinId = i; SavePlayerConfig(game); PlaySkinEquipSfx(); }

    // ---- Botões ----
    if (CheckCollisionPointRec(mouse, BtnRestoreRect()) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        for (int s = 0; s < COS_SLOT_COUNT; s++) game->player.cosmetics[s] = 0;
        game->player.skinId = 0; game->player.weaponSkinId = 0;
        wbFocus = 0;
        SavePlayerConfig(game);
        PlaySkinEquipSfx();
        Toast("Aparencia restaurada ao padrao");
    }

    bool backClick = CheckCollisionPointRec(mouse, BtnBackRect()) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (backClick || IsKeyPressed(KEY_ESCAPE))
    {
        SavePlayerConfig(game);
        game->currentScreen = SCREEN_MENU;
    }
}
