// tela_arsenal.c
// Tela de Arsenal: lista selecionável de armas + um grande painel de detalhes com
// PREVIEW REAL (mesmo renderer do gameplay: DrawHeldWeapon/DrawScalpel), barras
// de dano/cadência/cooldown, descrição com wrap e requisito de nível.
// O preview respeita o Mundo e a skin atuais; nenhum texto escapa do painel.
#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include "../../Assets/@models/weapons_model.h"
#include "raymath.h"
#include <stdio.h>
#include <math.h>

static UIButton arsenalBack = { { 60, 648, 300, 48 }, "VOLTAR", false, false };
static int g_arsenalSel = 0;          // arma selecionada (0..WEAPON_COUNT-1 => armas 1..N)
static float g_unlockPulse[WEAPON_COUNT] = {0}; // destaque de "recém-selecionada/desbloqueada"
static float g_arsenalScroll = 0.0f;  // deslocamento de rolagem da coluna de cards

// Coluna esquerda rolável: viewport (recorte) + layout dos cards. Os cards são
// desenhados deslocados por g_arsenalScroll dentro de um BeginScissorMode, então
// o arsenal comporta MAIS armas do que cabe na tela (rola com roda/setas).
#define ARS_VIEW_X      12.0f    // largo o bastante p/ não cortar o slide de entrada
#define ARS_VIEW_Y      140.0f
#define ARS_VIEW_W      420.0f
#define ARS_VIEW_H      482.0f   // base em 622 (acima do botão VOLTAR, y=648)
#define ARS_CARD_X      60.0f
#define ARS_CARD_W      360.0f
#define ARS_CARD_H      96.0f
#define ARS_CARD_PITCH  110.0f
#define ARS_CONTENT_TOP 150.0f

// Rolagem máxima (0 se todos os cards já cabem no viewport).
static float ArsenalMaxScroll(void)
{
    float contentBottom = ARS_CONTENT_TOP + (WEAPON_COUNT - 1) * ARS_CARD_PITCH + ARS_CARD_H;
    float m = contentBottom - (ARS_VIEW_Y + ARS_VIEW_H);
    return (m > 0.0f) ? m : 0.0f;
}

// Retângulo do seletor i (coluna esquerda), já deslocado pela rolagem.
static Rectangle ArsenalCardRect(int i)
{
    return (Rectangle){ ARS_CARD_X, ARS_CONTENT_TOP + i * ARS_CARD_PITCH - g_arsenalScroll, ARS_CARD_W, ARS_CARD_H };
}

// Mantém a rolagem dentro de [0, maxScroll].
static void ArsenalClampScroll(void)
{
    float mx = ArsenalMaxScroll();
    if (g_arsenalScroll < 0.0f) g_arsenalScroll = 0.0f;
    if (g_arsenalScroll > mx)   g_arsenalScroll = mx;
}

// Rola o mínimo necessário para que o card selecionado fique inteiro no viewport
// (usado quando a seleção muda por teclado/setas).
static void ArsenalScrollToSelection(void)
{
    float cardTop    = ARS_CONTENT_TOP + g_arsenalSel * ARS_CARD_PITCH;
    float showBottom = cardTop + ARS_CARD_H - (ARS_VIEW_Y + ARS_VIEW_H);
    float showTop    = cardTop - ARS_VIEW_Y;
    if (g_arsenalScroll < showBottom) g_arsenalScroll = showBottom;
    if (g_arsenalScroll > showTop)    g_arsenalScroll = showTop;
    ArsenalClampScroll();
}

// Barra de atributo com rótulo e valor 0..1, contida no painel (texto medido).
static void DrawStatBar(Font font, float x, float y, float w, const char *label,
                        float value01, const char *valueTxt, Color col)
{
    if (value01 < 0.0f) value01 = 0.0f;
    if (value01 > 1.0f) value01 = 1.0f;
    DrawTextEx(font, label, (Vector2){ x, y }, 16.0f, 1.0f, Fade(WHITE, 0.85f));
    float bx = x, by = y + 22.0f, bw = w, bh = 14.0f;
    DrawRectangleRounded((Rectangle){ bx, by, bw, bh }, 0.6f, 6, Fade(BLACK, 0.5f));
    DrawRectangleRounded((Rectangle){ bx, by, bw * value01, bh }, 0.6f, 6, col);
    DrawRectangleRoundedLines((Rectangle){ bx, by, bw, bh }, 0.6f, 6, Fade(col, 0.7f));
    Vector2 vs = MeasureTextEx(font, valueTxt, 14.0f, 1.0f);
    DrawTextEx(font, valueTxt, (Vector2){ bx + bw - vs.x, y }, 14.0f, 1.0f, col);
}

// Desenha o PREVIEW da arma (modelo real) centrado em `center`, com flutuação,
// pulso de energia, partículas e brilho/silhueta conforme desbloqueio.
static void DrawWeaponPreview(int weapon, Rectangle area, bool unlocked, float highlight,
                              Color primary, Color secondary, float time)
{
    Vector2 center = { area.x + area.width * 0.5f, area.y + area.height * 0.5f };
    float bob = sinf(time * 2.0f) * 7.0f;
    float sway = sinf(time * 1.3f) * 8.0f;            // leve balanço (graus)
    Vector2 c = { center.x, center.y + bob };
    float ring = 92.0f + sinf(time * 3.0f) * 8.0f;     // pulso de energia

    Color aura = unlocked ? primary : (Color){ 90, 96, 110, 255 };

    // Halo / pulso de energia atrás da arma.
    DrawCircleGradient((int)c.x, (int)c.y, ring, Fade(aura, (unlocked ? 0.22f : 0.10f) + highlight * 0.2f), BLANK);
    DrawCircleLines((int)c.x, (int)c.y, ring, Fade(aura, 0.25f + highlight * 0.4f));

    // Partículas orbitando (determinísticas por tempo).
    int np = unlocked ? 6 : 3;
    for (int i = 0; i < np; i++)
    {
        float a = time * 1.4f + i * (2.0f * PI / np);
        float rr = ring * (0.78f + 0.12f * sinf(time * 2.0f + i));
        Vector2 p = { c.x + cosf(a) * rr, c.y + sinf(a) * rr * 0.8f };
        DrawCircleV(p, unlocked ? 4.0f : 2.5f, Fade(secondary, 0.5f + 0.3f * sinf(time * 4.0f + i)));
    }

    // MODELO REAL da arma, ENQUADRADO no retângulo útil do preview: centralizado
    // e deslocado p/ baixo conforme a extensão real da arma — a agulha/cano nunca
    // vazam do painel. Acompanha a flutuação (bob) junto da aura.
    Rectangle wframe = { area.x, area.y + bob, area.width, area.height };
    DrawHeldWeaponFramed(weapon, wframe, 96.0f, sway, primary, secondary);

    // Bloqueada: escurece (silhueta) e desenha um cadeado.
    if (!unlocked)
    {
        DrawCircleV(c, ring * 0.92f, Fade((Color){ 4, 6, 10, 255 }, 0.55f));
        Vector2 lk = { c.x, c.y };
        DrawRectangleRounded((Rectangle){ lk.x - 22, lk.y - 4, 44, 40 }, 0.3f, 6, Fade((Color){ 200, 200, 210, 255 }, 0.9f));
        DrawRing((Vector2){ lk.x, lk.y - 6 }, 14.0f, 19.0f, 180.0f, 360.0f, 24, Fade((Color){ 200, 200, 210, 255 }, 0.9f));
        DrawCircleV((Vector2){ lk.x, lk.y + 12 }, 5.0f, (Color){ 40, 44, 54, 255 });
    }
}

void DrawTelaArsenal(GameState *game, Font font)
{
    float time = (float)GetTime();
    // Sincroniza nomes E modelos das armas com o Mundo atual (Mundo 1/2).
    SetWeaponWorld(game->currentWorld);

    DrawThemedBackground(SCREEN_ARSENAL, time, game->screenAnim / 0.4f);

    DrawTitleText(font, "ARSENAL DO ANTICORPO", SCREEN_WIDTH / 2.0f, 30.0f, 40.0f, THEME_COLOR_TEXT);
    const char *sub = "Selecione uma arma. Cada slot evolui com abates proprios; em jogo, a tecla do slot alterna a evolucao.";
    Vector2 sSz = MeasureTextEx(font, sub, 16.0f, 1.0f);
    DrawTextEx(font, sub, (Vector2){ SCREEN_WIDTH / 2.0f - sSz.x / 2.0f, 84.0f }, 16.0f, 1.0f, Fade(WHITE, 0.8f));

    Color skinPrim = WeaponSkinPrimary(game->player.weaponSkinId);
    Color skinSec  = WeaponSkinSecondary(game->player.weaponSkinId);

    // ---- COLUNA ESQUERDA: seletores das armas (ROLÁVEL, com mini-preview real) ----
    Rectangle arsView = { ARS_VIEW_X, ARS_VIEW_Y, ARS_VIEW_W, ARS_VIEW_H };
    BeginVirtualScissorMode(arsView);
    for (int s = 0; s < WEAPON_COUNT; s++)
    {
        WeaponInfo wi = GetWeaponInfo(s + 1);
        bool unlocked = WeaponUnlocked(game, s + 1);
        bool selected = (g_arsenalSel == s);
        Rectangle card = ArsenalCardRect(s);
        // Pula cards totalmente fora do viewport (recortados pela rolagem).
        if (card.y + card.height < arsView.y || card.y > arsView.y + arsView.height) continue;

        float entry = UIEase((game->screenAnim - s * 0.06f) / 0.4f);
        Rectangle drawc = card; drawc.x -= (1.0f - entry) * 40.0f; // slide de entrada

        Color accent = unlocked ? wi.color : (Color){ 120, 120, 130, 255 };
        DrawRectangleRounded(drawc, 0.18f, 8, Fade((Color){ 10, 14, 20, 255 }, (selected ? 0.92f : 0.7f) * entry));
        DrawRectangleRoundedLines(drawc, 0.18f, 8, Fade(accent, (selected ? 1.0f : 0.5f) * entry));
        if (selected)
            DrawRectangleRoundedLines(drawc, 0.18f, 8, Fade(THEME_COLOR_MAIN, 0.5f + 0.5f * sinf(time * 5.0f)));
        // acento lateral
        DrawRectangle((int)drawc.x, (int)drawc.y + 8, 5, (int)drawc.height - 16, Fade(accent, entry));

        // mini-preview real da arma, enquadrado na zona esquerda do card (sem
        // vazar o topo: a agulha/cano são contidos pelo enquadramento).
        DrawHeldWeaponFramed(s + 1, (Rectangle){ drawc.x + 10, drawc.y + 8, 74, drawc.height - 16 }, 30.0f,
                       sinf(time * 1.5f + s) * 6.0f,
                       unlocked ? skinPrim : (Color){ 120,124,134,255 },
                       unlocked ? skinSec : (Color){ 90,94,104,255 });

        DrawTextEx(font, TextFormat("[%d]", wi.key), (Vector2){ drawc.x + 86, drawc.y + 12 }, 18.0f, 1.0f, accent);
        // nome ajustado para nunca estourar o card
        Rectangle nameArea = { drawc.x + 86, drawc.y + 34, drawc.width - 96, 30 };
        DrawTextFitCentered(font, wi.name, nameArea, 20.0f, unlocked ? WHITE : Fade(GRAY, 0.85f), true);
        // Status: DISPONIVEL, ou requisito por abates/nivel.
        const char *status = unlocked ? "DISPONIVEL"
                           : WeaponIsEvolution(s + 1) ? TextFormat("%d ABATES S%d", WEAPON_EVOLVE_KILLS, WeaponSlotForId(s + 1))
                           : TextFormat("NIVEL %d", wi.unlockLevel);
        DrawTextEx(font, status, (Vector2){ drawc.x + 86, drawc.y + 66 }, 14.0f, 1.0f,
                   unlocked ? (Color){ 120, 220, 140, 255 } : (Color){ 230, 120, 120, 255 });
    }
    EndVirtualScissorMode();

    // Barra de rolagem (apenas quando há overflow) à direita do viewport.
    float maxScroll = ArsenalMaxScroll();
    if (maxScroll > 0.0f)
    {
        float trackX = arsView.x + arsView.width + 4.0f;
        DrawRectangleRounded((Rectangle){ trackX, arsView.y, 5.0f, arsView.height }, 0.8f, 4, Fade(BLACK, 0.4f));
        float thumbH = arsView.height * (arsView.height / (arsView.height + maxScroll));
        float thumbY = arsView.y + (g_arsenalScroll / maxScroll) * (arsView.height - thumbH);
        DrawRectangleRounded((Rectangle){ trackX, thumbY, 5.0f, thumbH }, 0.8f, 4, Fade(THEME_COLOR_MAIN, 0.7f));
    }

    // ---- PAINEL DE DETALHES (direita) com PREVIEW grande ----
    WeaponInfo wi = GetWeaponInfo(g_arsenalSel + 1);
    bool unlocked = WeaponUnlocked(game, g_arsenalSel + 1);
    Rectangle panel = { 450, 150, 770, 470 };
    float panelEntry = UIEase(game->screenAnim / 0.5f);
    DrawPanel(panel, Fade(wi.color, panelEntry), 0.72f * panelEntry);

    // Área de preview (metade esquerda do painel).
    Rectangle prevArea = { panel.x + 20, panel.y + 20, 300, panel.height - 40 };
    DrawRectangleRounded(prevArea, 0.08f, 8, Fade((Color){ 6, 8, 12, 255 }, 0.55f));
    DrawRectangleRoundedLines(prevArea, 0.08f, 8, Fade(wi.color, 0.35f));
    g_unlockPulse[g_arsenalSel] = Lerp(g_unlockPulse[g_arsenalSel], 0.0f, 3.0f * GetFrameTime());
    DrawWeaponPreview(g_arsenalSel + 1, prevArea,
                      unlocked, g_unlockPulse[g_arsenalSel], skinPrim, skinSec, time);

    // Coluna de texto/estatísticas (metade direita).
    float tx = panel.x + 344;
    float tw = panel.x + panel.width - tx - 24;
    float ty = panel.y + 26;
    DrawTextFitCentered(font, wi.name, (Rectangle){ tx, ty, tw, 30 }, 26.0f, unlocked ? WHITE : Fade(GRAY, 0.9f), true);
    ty += 40;
    // descrição + especial com WRAP (nunca escapa do painel)
    ty += DrawTextWrapped(font, wi.desc, (Rectangle){ tx, ty, tw, 56 }, 17.0f, 1.0f, Fade(WHITE, 0.85f)) + 6;
    ty += DrawTextWrapped(font, TextFormat("Especial: %s", wi.special), (Rectangle){ tx, ty, tw, 56 }, 16.0f, 1.0f, (Color){ 120, 230, 140, 255 }) + 6;
    ty += DrawTextWrapped(font, TextFormat("Estilo: %s", wi.playstyle), (Rectangle){ tx, ty, tw, 60 }, 15.0f, 1.0f, Fade(WHITE, 0.7f)) + 10;

    // Barras: Dano / Cadência / Cooldown (valores normalizados).
    float dano01 = (float)wi.baseDamage / 100.0f;
    float cad01  = (0.22f / wi.cooldown);                 // referência: arma mais rápida (0.22s)
    if (cad01 > 1.0f) cad01 = 1.0f;
    float cd01   = (wi.cooldown / 5.0f);                  // recarga longa => barra cheia
    DrawStatBar(font, tx, ty, tw, "Dano base", dano01, TextFormat("%d (+ATK)", wi.baseDamage), GOLD); ty += 46;
    DrawStatBar(font, tx, ty, tw, "Cadencia", cad01, wi.speedTxt, THEME_COLOR_MAIN); ty += 46;
    DrawStatBar(font, tx, ty, tw, "Recarga (cooldown)", cd01, TextFormat("%.2fs", wi.cooldown), (Color){ 255, 150, 80, 255 }); ty += 46;
    // Alcance real (somente armas de projétil com limite de alcance — Fase 5).
    if (wi.maxRange > 0.0f)
    {
        DrawTextEx(font, TextFormat("Alcance efetivo: %.0f px", wi.maxRange),
                   (Vector2){ tx, ty }, 15.0f, 1.0f, (Color){ 120, 200, 255, 255 }); ty += 24;
    }
    else ty += 4;

    // Requisito (abates p/ evoluções; nível p/ primordiais) / status claro.
    Rectangle foot = { tx, ty, tw, 30 };
    bool isEvolution = WeaponIsEvolution(g_arsenalSel + 1);
    int evoSlot = WeaponSlotForId(g_arsenalSel + 1);
    if (unlocked)
    {
        const char *okTxt = isEvolution ? TextFormat("DESBLOQUEADA (%d abates no slot %d)", WEAPON_EVOLVE_KILLS, evoSlot)
                          : (wi.unlockLevel <= 1) ? "DISPONIVEL DESDE O INICIO"
                          : TextFormat("DESBLOQUEADA (Nivel %d)", wi.unlockLevel);
        DrawTextEx(font, okTxt, (Vector2){ foot.x, foot.y }, 16.0f, 1.0f, (Color){ 90, 220, 130, 255 });
    }
    else
    {
        DrawRectangleRounded(foot, 0.4f, 6, Fade(RED, 0.14f));
        const char *lockTxt = isEvolution
            ? TextFormat("BLOQUEADA - %d/%d abates no slot %d", WeaponKillCountForSlot(game, evoSlot), WEAPON_EVOLVE_KILLS, evoSlot)
            : TextFormat("BLOQUEADA - desbloqueia no Nivel %d", wi.unlockLevel);
        DrawTextFitCentered(font, lockTxt, foot, 16.0f, (Color){ 235, 130, 130, 255 }, true);
    }

    DrawButton(arsenalBack, font, true);
    DrawTextEx(font, "Use 1-4 no jogo; evolucoes dividem o mesmo slot  |  ESC para voltar",
               (Vector2){ 380, SCREEN_HEIGHT - 28 }, 14.0f, 1.0f, DARKGRAY);
}

void UpdateTelaArsenal(GameState *game, Vector2 mouse)
{
    Rectangle arsView = { ARS_VIEW_X, ARS_VIEW_Y, ARS_VIEW_W, ARS_VIEW_H };

    // Rolagem por roda do mouse quando o cursor está sobre a lista.
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && CheckCollisionPointRec(mouse, arsView))
    {
        g_arsenalScroll -= wheel * 42.0f;
        ArsenalClampScroll();
    }

    // Seleção por clique — apenas em cards efetivamente visíveis no viewport.
    for (int s = 0; s < WEAPON_COUNT; s++)
    {
        if (CheckCollisionPointRec(mouse, ArsenalCardRect(s)) &&
            CheckCollisionPointRec(mouse, arsView))
        {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && g_arsenalSel != s)
            {
                g_arsenalSel = s;
                g_unlockPulse[s] = 1.0f;
            }
        }
    }

    // Seleção por teclado e setas — rola para manter a seleção visível.
    int prevSel = g_arsenalSel;
    if (IsKeyPressed(KEY_ONE))   g_arsenalSel = 0;
    if (IsKeyPressed(KEY_TWO))   g_arsenalSel = 1;
    if (IsKeyPressed(KEY_THREE)) g_arsenalSel = 2;
    if (IsKeyPressed(KEY_FOUR))  g_arsenalSel = 3;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_RIGHT)) g_arsenalSel = (g_arsenalSel + 1) % WEAPON_COUNT;
    if (IsKeyPressed(KEY_UP)   || IsKeyPressed(KEY_LEFT))  g_arsenalSel = (g_arsenalSel + WEAPON_COUNT - 1) % WEAPON_COUNT;
    if (g_arsenalSel != prevSel)
    {
        g_unlockPulse[g_arsenalSel] = 1.0f;
        ArsenalScrollToSelection();
    }

    arsenalBack.hover = CheckCollisionPointRec(mouse, arsenalBack.bounds);
    if ((arsenalBack.hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) || IsKeyPressed(KEY_ESCAPE))
    {
        game->currentScreen = SCREEN_MENU;
    }
}

// Somente para ferramentas de preview offline (tools/ui_preview.c): força o
// estado de rolagem/seleção antes de um DrawTelaArsenal. NÃO é chamado no jogo.
void ArsenalPreviewSet(float scroll, int sel)
{
    g_arsenalScroll = scroll;
    g_arsenalSel = (sel < 0) ? 0 : (sel >= WEAPON_COUNT ? WEAPON_COUNT - 1 : sel);
    ArsenalClampScroll();
}
