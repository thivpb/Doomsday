// telas.c
// Implementação do HUD, minimapa, efeitos visuais e menus interativos.
#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include "../../include/input_controller.h"
#include "../../include/asset_manager.h"
#include "../../include/sprite_manager.h"
#include "../../Assets/@models/menu_title_glyphs.h"
#include "../../Assets/@models/menu_organisms.h"
#include "raymath.h"
#include "../../Assets/@models/enemy_model.h"
#include "../../Assets/@models/doctor_model.h"
#include "../../Assets/@models/weapons_model.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "rlgl.h"

extern float g_scale;
extern Vector2 g_mouseOffset;
extern Vector2 g_virtualMouse;

static float EaseOutCubic(float t) {
    t -= 1.0f;
    return t * t * t + 1.0f;
}

void BeginVirtualScissorMode(Rectangle r)
{
    int x = (int)floorf(g_mouseOffset.x + r.x * g_scale);
    int y = (int)floorf(g_mouseOffset.y + r.y * g_scale);
    int right = (int)ceilf(g_mouseOffset.x + (r.x + r.width) * g_scale);
    int bottom = (int)ceilf(g_mouseOffset.y + (r.y + r.height) * g_scale);

    BeginScissorMode(x, y, right - x, bottom - y);
}

void EndVirtualScissorMode(void)
{
    EndScissorMode();
}

static Rectangle VisibleVirtualScreenRect(void)
{
    float scale = (g_scale > 0.0f) ? g_scale : 1.0f;
    return (Rectangle){
        -g_mouseOffset.x / scale,
        -g_mouseOffset.y / scale,
        (float)GetScreenWidth() / scale,
        (float)GetScreenHeight() / scale
    };
}

static void DrawVisibleVirtualGradientV(Color top, Color bottom)
{
    Rectangle r = VisibleVirtualScreenRect();
    DrawRectangleGradientV((int)floorf(r.x), (int)floorf(r.y),
                           (int)ceilf(r.width), (int)ceilf(r.height),
                           top, bottom);
}

// ============================================================================
// CACHE: EXISTE ALGUM SAVE? (evita fopen em disco a cada frame no menu)
// ============================================================================
bool AnySaveExistsCached(void)
{
    static double lastCheck = -10.0;
    static bool cached = false;

    double now = GetTime();
    if (now - lastCheck > 1.0)
    {
        lastCheck = now;
        cached = false;
        for (int i = 1; i <= SAVE_SLOT_COUNT; i++)
        {
            char path[64];
            if (i == AUTO_SAVE_SLOT) snprintf(path, sizeof(path), "Saves/auto_save.txt");
            else snprintf(path, sizeof(path), "Saves/save_slot_%d.txt", i);
            if (FileExists(path))
            {
                cached = true;
                break;
            }
        }
    }
    return cached;
}

// ============================================================================
// DEFINIÇÃO DOS BOTÕES DAS TELAS (GLOBAIS DA UI)
// ============================================================================
// Menu principal: coluna única, centralizada, com espaçamento uniforme. As
// bounds são a ÚNICA fonte de verdade (desenho e hitbox usam o mesmo retângulo).
// "JOGAR" abre a seleção de dificuldade; o seletor antigo foi removido.
UIButton menuButtons[] = {
    { { 470, 362, 340, 36 }, "JOGAR", false, false },
    { { 470, 404, 340, 36 }, "CARREGAR JOGO", false, false },
    { { 470, 446, 340, 36 }, "ARSENAL (ARMAS)", false, false },
    { { 470, 488, 340, 36 }, "SKINS", false, false },
    { { 470, 530, 340, 36 }, "TUTORIAL", false, false },
    { { 470, 572, 340, 36 }, "CONFIGURACOES", false, false },
    { { 470, 614, 340, 36 }, "MODO ADMIN", false, false },
    { { 470, 656, 340, 36 }, "SAIR", false, false }
};
#define MENU_BTN_COUNT ((int)(sizeof(menuButtons) / sizeof(menuButtons[0])))

UIButton pauseButtons[] = {
    { { 500, 210, 280, 40 }, "CONTINUAR", false, false },
    { { 500, 260, 280, 40 }, "SALVAR PROGRESSO", false, false },
    { { 500, 310, 280, 40 }, "CARREGAR SAVE", false, false },
    { { 500, 360, 280, 40 }, "CONFIGURACOES", false, false },
    { { 500, 410, 280, 40 }, "MENU PRINCIPAL", false, false }
};

UIButton controlsButton = { { 490, 660, 300, 42 }, "VOLTAR", false, false };

UIButton gameOverButtons[] = {
    { { 490, 390, 300, 50 }, "TENTAR NOVAMENTE", false, false },
    { { 490, 460, 300, 50 }, "MENU PRINCIPAL", false, false }
};

UIButton victoryButtons[] = {
    { { 490, 390, 300, 50 }, "JOGAR NOVAMENTE", false, false },
    { { 490, 460, 300, 50 }, "MENU PRINCIPAL", false, false }
};

// ============================================================================
// AUXILIAR: TEXTO CENTRADO QUE SE AJUSTA À LARGURA (com sombra opcional)
// Reduz a fonte ate caber em (maxW) para nunca estourar/sobrepor componentes.
// ============================================================================
void DrawTextFitCentered(Font font, const char *text, Rectangle area, float maxFont, Color color, bool shadow)
{
    float fs = maxFont;
    float padding = 16.0f;
    Vector2 sz = MeasureTextEx(font, text, fs, 1.0f);
    while (sz.x > area.width - padding && fs > 9.0f)
    {
        fs -= 1.0f;
        sz = MeasureTextEx(font, text, fs, 1.0f);
    }
    Vector2 pos = { area.x + (area.width - sz.x) / 2.0f, area.y + (area.height - sz.y) / 2.0f };
    if (shadow)
        DrawTextEx(font, text, (Vector2){ pos.x + 1.5f, pos.y + 1.5f }, fs, 1.0f, Fade(BLACK, 0.55f));
    DrawTextEx(font, text, pos, fs, 1.0f, color);
}

// ============================================================================
// AUXILIAR: BOTÃO ESTILIZADO COM HOVER/PRESS ANIMADO E TEXTO QUE SE AJUSTA
// - hover: leve escala + brilho/glow + borda pulsante ciano;
// - pressionado: compressão (escala < 1) para feedback de clique;
// - desabilitado: cinza apagado e claramente inativo;
// - texto encolhe automaticamente para nunca sair do botão.
// ============================================================================
void DrawButton(UIButton botao, Font font, bool enabled)
{
    extern Vector2 g_virtualMouse;
    bool pressed = enabled && botao.hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
                   CheckCollisionPointRec(g_virtualMouse, botao.bounds);

    // Escala animada (hover cresce, pressionado comprime)
    float sc = !enabled ? 1.0f : pressed ? 0.96f : (botao.hover ? 1.04f : 1.0f);
    Rectangle b = botao.bounds;
    float cx = b.x + b.width / 2.0f, cy = b.y + b.height / 2.0f;
    Rectangle r = { cx - b.width * sc / 2.0f, cy - b.height * sc / 2.0f, b.width * sc, b.height * sc };

    Color corFundo = Fade((Color){ 26, 21, 44, 255 }, 0.78f);
    Color corBorda = (Color){ 104, 76, 172, 255 };
    Color corTexto = WHITE;

    if (!enabled)
    {
        corFundo = Fade((Color){ 15, 12, 24, 255 }, 0.5f);
        corBorda = Fade(GRAY, 0.25f);
        corTexto = Fade(GRAY, 0.4f);
    }
    else if (pressed)
    {
        corFundo = (Color){ 120, 80, 220, 255 };
        corBorda = THEME_COLOR_MAIN;
        corTexto = WHITE;
    }
    else if (botao.hover)
    {
        // Glow externo pulsante ao passar o mouse
        float pulse = 0.45f + 0.25f * sinf((float)GetTime() * 6.0f);
        DrawRectangleRounded((Rectangle){ r.x - 5, r.y - 5, r.width + 10, r.height + 10 }, 0.3f, 8, Fade(THEME_COLOR_MAIN, 0.12f * pulse + 0.05f));
        corFundo = Fade(THEME_COLOR_BORDER, 0.85f);
        corBorda = THEME_COLOR_MAIN;
        corTexto = THEME_COLOR_MAIN;
    }

    // Fundo e borda
    DrawRectangleRounded(r, 0.25f, 8, corFundo);
    DrawRectangleRoundedLines(r, 0.25f, 8, corBorda);
    if (enabled && botao.hover && !pressed)
    {
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
        DrawRectangleRoundedLines(r, 0.25f, 8, Fade(THEME_COLOR_MAIN, pulse));
        // Acento lateral animado
        DrawRectangle((int)r.x + 3, (int)(r.y + r.height * 0.2f), 3, (int)(r.height * 0.6f), Fade(THEME_COLOR_MAIN, pulse));
    }

    // Texto centralizado e ajustado à largura (com sombra)
    DrawTextFitCentered(font, botao.text, r, 22.0f, corTexto, true);
}

// ============================================================================
// COMPONENTES REUTILIZÁVEIS DE UI
// ============================================================================
float UIEase(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return EaseOutCubic(t);
}

static Color ColLerp(Color a, Color b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return (Color){ (unsigned char)(a.r + (b.r - a.r) * t),
                    (unsigned char)(a.g + (b.g - a.g) * t),
                    (unsigned char)(a.b + (b.b - a.b) * t),
                    (unsigned char)(a.a + (b.a - a.a) * t) };
}

// Paleta temática por tela: top/bottom do gradiente + cor de acento.
static void ScreenPalette(int screen, Color *top, Color *bot, Color *accent)
{
    switch (screen)
    {
        case SCREEN_ARSENAL:  *top=(Color){14,16,22,255};  *bot=(Color){8,12,16,255};  *accent=(Color){255,200,80,255};  break;
        case SCREEN_SKINS:    *top=(Color){20,10,28,255};  *bot=(Color){10,6,18,255};   *accent=(Color){200,110,255,255}; break;
        case SCREEN_TUTORIAL:
        case SCREEN_CONTROLS: *top=(Color){8,18,28,255};   *bot=(Color){5,10,18,255};   *accent=(Color){0,200,255,255};   break;
        case SCREEN_SETTINGS: *top=(Color){10,18,20,255};  *bot=(Color){6,10,12,255};   *accent=(Color){0,229,200,255};   break;
        case SCREEN_ADMIN:    *top=(Color){22,8,8,255};    *bot=(Color){10,4,5,255};    *accent=(Color){255,80,80,255};   break;
        case SCREEN_GAMEOVER: *top=(Color){22,6,12,255};   *bot=(Color){8,3,6,255};     *accent=(Color){230,60,70,255};   break;
        case SCREEN_VICTORY:  *top=(Color){10,16,36,255};  *bot=(Color){6,8,20,255};    *accent=(Color){255,210,90,255};  break;
        default:              *top=THEME_COLOR_BG_DARK;     *bot=THEME_COLOR_BG_LIGHT;   *accent=THEME_COLOR_MAIN;         break;
    }
}

void DrawThemedBackground(int screen, float time, float entry)
{
    // Morph suave entre temas (lerp do estado atual para o alvo da tela).
    static Color cTop = {6,18,10,255}, cBot = {10,28,18,255}, cAcc = {0,229,255,255};
    static int inited = 0;
    Color tTop, tBot, tAcc; ScreenPalette(screen, &tTop, &tBot, &tAcc);
    if (!inited) { cTop=tTop; cBot=tBot; cAcc=tAcc; inited=1; }
    float k = 6.0f * GetFrameTime(); if (k > 1.0f) k = 1.0f;
    cTop = ColLerp(cTop, tTop, k); cBot = ColLerp(cBot, tBot, k); cAcc = ColLerp(cAcc, tAcc, k);

    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, cTop, cBot);

    // Células biológicas flutuando (posições determinísticas por índice + tempo).
    float fade = UIEase(entry);
    for (int i = 0; i < 26; i++)
    {
        float seedx = fmodf(i * 137.0f, (float)SCREEN_WIDTH);
        float baseY = fmodf(i * 89.0f, (float)SCREEN_HEIGHT);
        float y = baseY - fmodf(time * (12.0f + (i % 5) * 6.0f), (float)SCREEN_HEIGHT + 60.0f);
        if (y < -30.0f) y += SCREEN_HEIGHT + 60.0f;
        float x = seedx + sinf(time * 0.6f + i) * 18.0f;
        float r = 6.0f + (i % 4) * 5.0f;
        float a = (0.05f + 0.05f * sinf(time * 1.4f + i)) * fade;
        DrawCircleV((Vector2){ x, y }, r, Fade(cAcc, a));
        DrawCircleLines((int)x, (int)y, r, Fade(cAcc, a * 1.4f));
    }
    // Vinheta suave nas bordas para focar o conteúdo central.
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, 90, Fade(BLACK, 0.35f), Fade(BLACK, 0.0f));
    DrawRectangleGradientV(0, SCREEN_HEIGHT - 90, SCREEN_WIDTH, 90, Fade(BLACK, 0.0f), Fade(BLACK, 0.35f));
}

void DrawPanel(Rectangle r, Color accent, float bgAlpha)
{
    DrawRectangleRounded(r, 0.06f, 10, Fade((Color){ 8, 10, 16, 255 }, bgAlpha));
    DrawRectangleRoundedLines(r, 0.06f, 10, Fade(accent, 0.55f));
    float len = 16.0f, th = 2.0f;
    DrawLineEx((Vector2){ r.x, r.y }, (Vector2){ r.x + len, r.y }, th, accent);
    DrawLineEx((Vector2){ r.x, r.y }, (Vector2){ r.x, r.y + len }, th, accent);
    DrawLineEx((Vector2){ r.x + r.width, r.y }, (Vector2){ r.x + r.width - len, r.y }, th, accent);
    DrawLineEx((Vector2){ r.x + r.width, r.y }, (Vector2){ r.x + r.width, r.y + len }, th, accent);
    DrawLineEx((Vector2){ r.x, r.y + r.height }, (Vector2){ r.x + len, r.y + r.height }, th, accent);
    DrawLineEx((Vector2){ r.x, r.y + r.height }, (Vector2){ r.x, r.y + r.height - len }, th, accent);
    DrawLineEx((Vector2){ r.x + r.width, r.y + r.height }, (Vector2){ r.x + r.width - len, r.y + r.height }, th, accent);
    DrawLineEx((Vector2){ r.x + r.width, r.y + r.height }, (Vector2){ r.x + r.width, r.y + r.height - len }, th, accent);
}

void DrawTitleText(Font font, const char *text, float centerX, float y, float fontSize, Color color)
{
    Vector2 sz = MeasureTextEx(font, text, fontSize, 2.0f);
    Vector2 pos = { centerX - sz.x * 0.5f, y };
    float glow = 0.30f + 0.12f * sinf((float)GetTime() * 3.0f);
    for (int gx = -2; gx <= 2; gx++)
        for (int gy = -2; gy <= 2; gy++)
            if (gx || gy)
                DrawTextEx(font, text, (Vector2){ pos.x + gx * 2.0f, pos.y + gy * 2.0f }, fontSize, 2.0f, Fade(color, 0.05f * glow));
    DrawTextEx(font, text, (Vector2){ pos.x + 3, pos.y + 3 }, fontSize, 2.0f, Fade(BLACK, 0.5f));
    DrawTextEx(font, text, pos, fontSize, 2.0f, color);
}

float DrawTextWrapped(Font font, const char *text, Rectangle area, float fontSize, float spacing, Color color)
{
    int n = 0; char buf[1024];
    for (const char *p = text; *p && n < 1023; p++) buf[n++] = *p;
    buf[n] = '\0';

    for (float fs = fontSize; fs >= 9.0f; fs -= 1.0f)
    {
        float lineH = fs * 1.25f;
        float spaceW = MeasureTextEx(font, " ", fs, spacing).x;
        // 1) Mede quantas linhas o wrap usa (sem desenhar).
        char copy[1024]; for (int i = 0; i <= n; i++) copy[i] = buf[i];
        float x = area.x, y = area.y; char *s = copy;
        for (char *q = copy; ; q++)
        {
            if (*q == ' ' || *q == '\0')
            {
                char saved = *q; *q = '\0';
                Vector2 ws = MeasureTextEx(font, s, fs, spacing);
                if (x > area.x && x + ws.x > area.x + area.width) { x = area.x; y += lineH; }
                x += ws.x + spaceW;
                *q = saved; s = q + 1;
                if (saved == '\0') break;
            }
        }
        if (y + lineH > area.y + area.height && fs > 9.0f) continue; // não coube: encolhe

        // 2) Desenha de fato.
        char draw[1024]; for (int i = 0; i <= n; i++) draw[i] = buf[i];
        float dx = area.x, dy = area.y; char *d = draw;
        for (char *q = draw; ; q++)
        {
            if (*q == ' ' || *q == '\0')
            {
                char saved = *q; *q = '\0';
                Vector2 ws = MeasureTextEx(font, d, fs, spacing);
                if (dx > area.x && dx + ws.x > area.x + area.width) { dx = area.x; dy += lineH; }
                DrawTextEx(font, d, (Vector2){ dx, dy }, fs, spacing, color);
                dx += ws.x + spaceW;
                *q = saved; d = q + 1;
                if (saved == '\0') break;
            }
        }
        return (dy + lineH) - area.y;
    }
    return 0.0f;
}

void DrawTooltip(Font font, const char *text, Vector2 anchor)
{
    float fs = 15.0f, pad = 8.0f;
    Vector2 sz = MeasureTextEx(font, text, fs, 1.0f);
    Rectangle r = { anchor.x + 14.0f, anchor.y + 14.0f, sz.x + pad * 2.0f, sz.y + pad * 2.0f };
    if (r.x + r.width > SCREEN_WIDTH - 6.0f) r.x = anchor.x - r.width - 14.0f;
    if (r.y + r.height > SCREEN_HEIGHT - 6.0f) r.y = SCREEN_HEIGHT - r.height - 6.0f;
    DrawRectangleRounded(r, 0.3f, 6, Fade((Color){ 8, 10, 16, 255 }, 0.92f));
    DrawRectangleRoundedLines(r, 0.3f, 6, Fade(THEME_COLOR_MAIN, 0.8f));
    DrawTextEx(font, text, (Vector2){ r.x + pad, r.y + pad }, fs, 1.0f, WHITE);
}

// ============================================================================
// SISTEMA VISUAL COMPARTILHADO (padrão Arsenal) — implementação
// ============================================================================
void DrawUICard(Rectangle r, Color accent, bool hover, bool selected, float entry)
{
    float a = (selected ? 0.95f : 0.80f) * entry;
    DrawRectangleRounded(r, 0.07f, 10, Fade((Color){ 10, 13, 20, 255 }, a));
    if (selected)
    {
        float gp = 0.5f + 0.5f * sinf((float)GetTime() * 5.0f);
        DrawRectangleRoundedLines((Rectangle){ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, 0.07f, 10, Fade(accent, (0.35f + 0.45f * gp) * entry));
        DrawRectangleRoundedLines(r, 0.07f, 10, Fade(accent, entry));
    }
    else
    {
        DrawRectangleRoundedLines(r, 0.07f, 10, Fade(accent, (hover ? 0.9f : 0.5f) * entry));
    }
    // Faixa de acento no topo.
    DrawRectangleRounded((Rectangle){ r.x + 12, r.y + 10, r.width - 24, 5 }, 0.8f, 4, Fade(accent, entry));
}

void DrawUITab(Font font, Rectangle r, const char *text, Color accent, bool active, bool hover)
{
    Color bg = active ? Fade(accent, 0.22f) : Fade((Color){ 12, 14, 22, 255 }, 0.85f);
    Color br = active ? accent : (hover ? Fade(accent, 0.8f) : THEME_COLOR_BORDER);
    DrawRectangleRounded(r, 0.3f, 6, bg);
    DrawRectangleRoundedLines(r, 0.3f, 6, br);
    DrawTextFitCentered(font, text, r, 18.0f, active ? accent : (hover ? WHITE : Fade(WHITE, 0.8f)), true);
    if (active)
        DrawRectangleRounded((Rectangle){ r.x + r.width * 0.2f, r.y + r.height - 4, r.width * 0.6f, 3 }, 0.8f, 4, accent);
}

void DrawUIScreenTitle(Font font, const char *text, Color accent, float entry)
{
    float y = 36.0f - (1.0f - entry) * 16.0f;
    DrawTitleText(font, text, SCREEN_WIDTH / 2.0f, y, 40.0f, Fade(accent, entry));
    Vector2 sz = MeasureTextEx(font, text, 40.0f, 2.0f);
    float lw = sz.x * 0.6f;
    DrawRectangleRounded((Rectangle){ SCREEN_WIDTH / 2.0f - lw / 2.0f, y + 50.0f, lw, 3.0f }, 0.8f, 4, Fade(accent, 0.7f * entry));
}

bool DrawUIBackButton(Font font, Vector2 mouse, const char *text)
{
    Rectangle r = { 40.0f, SCREEN_HEIGHT - 64.0f, 200.0f, 46.0f };
    UIButton b = { r, text, CheckCollisionPointRec(mouse, r), false };
    DrawButton(b, font, true);
    return b.hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void DrawUIStatBar(Font font, float x, float y, float w, const char *label, float v, const char *valueTxt, Color col)
{
    v = Clamp(v, 0.0f, 1.0f);
    DrawTextEx(font, label, (Vector2){ x, y }, 15.0f, 1.0f, Fade(WHITE, 0.85f));
    float by = y + 20.0f, bh = 12.0f;
    DrawRectangleRounded((Rectangle){ x, by, w, bh }, 0.7f, 6, Fade(BLACK, 0.5f));
    DrawRectangleRounded((Rectangle){ x, by, w * v, bh }, 0.7f, 6, col);
    DrawRectangleRoundedLines((Rectangle){ x, by, w, bh }, 0.7f, 6, Fade(col, 0.7f));
    if (valueTxt)
    {
        Vector2 vs = MeasureTextEx(font, valueTxt, 14.0f, 1.0f);
        DrawTextEx(font, valueTxt, (Vector2){ x + w - vs.x, y }, 14.0f, 1.0f, col);
    }
}

void DrawUIInput(Font font, Rectangle r, const char *text, const char *placeholder, bool active, bool hover, bool showCursor)
{
    Color br = active ? THEME_COLOR_MAIN : (hover ? YELLOW : THEME_COLOR_BORDER);
    DrawRectangleRounded(r, 0.2f, 6, Fade(THEME_COLOR_PANEL, 0.9f));
    DrawRectangleRoundedLines(r, 0.2f, 6, br);
    bool empty = (text == NULL || text[0] == '\0');
    const char *shown = empty ? (placeholder ? placeholder : "") : text;
    float fs = 20.0f;
    Vector2 ts = MeasureTextEx(font, shown, fs, 1.0f);
    // encolhe se passar da caixa
    while (ts.x > r.width - 24.0f && fs > 10.0f) { fs -= 1.0f; ts = MeasureTextEx(font, shown, fs, 1.0f); }
    Vector2 pos = { r.x + r.width / 2.0f - ts.x / 2.0f, r.y + r.height / 2.0f - ts.y / 2.0f };
    DrawTextEx(font, shown, pos, fs, 1.0f, empty ? Fade(GRAY, 0.55f) : WHITE);
    if (active && showCursor)
        DrawRectangle((int)(pos.x + ts.x + 3), (int)pos.y, 2, (int)ts.y, THEME_COLOR_MAIN);
}

void DrawUISectionPanel(Font font, Rectangle r, const char *title, Color accent, float entry)
{
    DrawRectangleRounded(r, 0.05f, 8, Fade((Color){ 8, 11, 18, 255 }, 0.75f * entry));
    DrawRectangleRoundedLines(r, 0.05f, 8, Fade(accent, 0.5f * entry));
    if (title)
    {
        DrawTextEx(font, title, (Vector2){ r.x + 16, r.y + 10 }, 22.0f, 1.0f, Fade(accent, entry));
        DrawLineEx((Vector2){ r.x + 16, r.y + 38 }, (Vector2){ r.x + r.width - 16, r.y + 38 }, 1.0f, Fade(accent, 0.4f * entry));
    }
}

void DrawUIToast(Font font, const char *text, Color accent, float alpha)
{
    if (alpha <= 0.0f) return;
    float fs = 20.0f, pad = 16.0f;
    Vector2 sz = MeasureTextEx(font, text, fs, 1.0f);
    Rectangle r = { SCREEN_WIDTH / 2.0f - sz.x / 2.0f - pad, 120.0f, sz.x + pad * 2.0f, sz.y + pad };
    DrawRectangleRounded(r, 0.4f, 8, Fade((Color){ 8, 12, 18, 255 }, 0.92f * alpha));
    DrawRectangleRoundedLines(r, 0.4f, 8, Fade(accent, alpha));
    DrawTextEx(font, text, (Vector2){ r.x + pad, r.y + pad / 2.0f }, fs, 1.0f, Fade(WHITE, alpha));
}

Rectangle SettingsMusicVolumeTrack(void) { return (Rectangle){ 555.0f, 292.0f, 300.0f, 16.0f }; }
Rectangle SettingsSfxVolumeTrack(void)   { return (Rectangle){ 555.0f, 348.0f, 300.0f, 16.0f }; }

// ============================================================================
// HELPERS PROCEDURAIS DO MENU (vírus, bactéria, biohazard, fundo, título neon)
// ============================================================================
void DrawMenuVirus(Vector2 c, float radius, float rotationDeg, Color col)
{
    DrawCircleV(c, radius * 1.15f, Fade(col, 0.10f));       // halo
    int spikes = 10;
    for (int i = 0; i < spikes; i++)
    {
        float a = (rotationDeg + i * (360.0f / spikes)) * DEG2RAD;
        Vector2 base = { c.x + cosf(a) * radius * 0.72f, c.y + sinf(a) * radius * 0.72f };
        Vector2 tip  = { c.x + cosf(a) * radius * 1.18f, c.y + sinf(a) * radius * 1.18f };
        DrawLineEx(base, tip, 2.5f, Fade(col, 0.85f));
        DrawCircleV(tip, radius * 0.10f, Fade(col, 0.9f));
    }
    DrawCircleV(c, radius * 0.72f, Fade(col, 0.28f));
    DrawCircleLines((int)c.x, (int)c.y, radius * 0.72f, Fade(col, 0.85f));
    DrawCircleV(c, radius * 0.32f, Fade(col, 0.6f));        // núcleo de RNA
}

void DrawMenuBacteria(Vector2 c, float size, float angleDeg, Color col)
{
    float a = angleDeg * DEG2RAD;
    Vector2 dir = { cosf(a), sinf(a) };
    Vector2 pa = { c.x - dir.x * size, c.y - dir.y * size };
    Vector2 pb = { c.x + dir.x * size, c.y + dir.y * size };
    // cílios (flagelos) perpendiculares
    Vector2 perp = { -dir.y, dir.x };
    for (int i = -2; i <= 2; i++)
    {
        Vector2 p = { c.x + dir.x * (size * 0.45f * i), c.y + dir.y * (size * 0.45f * i) };
        DrawLineEx(p, (Vector2){ p.x + perp.x * size * 0.55f, p.y + perp.y * size * 0.55f }, 1.5f, Fade(col, 0.4f));
        DrawLineEx(p, (Vector2){ p.x - perp.x * size * 0.55f, p.y - perp.y * size * 0.55f }, 1.5f, Fade(col, 0.4f));
    }
    DrawLineEx(pa, pb, size * 0.78f, Fade(col, 0.5f));      // corpo (bastonete)
    DrawCircleV(pa, size * 0.39f, Fade(col, 0.5f));
    DrawCircleV(pb, size * 0.39f, Fade(col, 0.5f));
    DrawCircleLines((int)c.x, (int)c.y, size * 0.5f, Fade(col, 0.7f));
}

void DrawMenuBiohazard(Vector2 c, float radius, float pulse, Color col)
{
    float r = radius * (1.0f + pulse * 0.06f);
    for (int k = 0; k < 3; k++)                              // 3 lâminas (crescentes)
    {
        float base = -90.0f + k * 120.0f;
        DrawRing(c, r * 0.46f, r, base + 18.0f, base + 102.0f, 24, Fade(col, 0.85f));
    }
    DrawCircleV(c, r * 0.26f, Fade(col, 0.9f));
    DrawCircleLines((int)c.x, (int)c.y, r * 0.5f, Fade(col, 0.6f));
}

void DrawMenuBackground(Color accent, float time, float entry)
{
    // Fundo azul-marinho quase preto (referência) com leve morph do acento.
    Color top = (Color){ 9, 13, 30, 255 };
    Color bot = (Color){ 4, 6, 16, 255 };
    Rectangle visible = VisibleVirtualScreenRect();
    DrawVisibleVirtualGradientV(top, bot);
    // brilho de acento "respirando" no topo (segue o item destacado)
    float breath = 0.06f + 0.03f * sinf(time * 1.5f);
    DrawRectangleGradientV((int)floorf(visible.x), (int)floorf(visible.y),
                           (int)ceilf(visible.width), 220,
                           Fade(accent, breath * entry), Fade(accent, 0.0f));

    float fade = UIEase(entry);

    // Micróbios decorativos nas BORDAS (posições determinísticas; deixam o centro livre).
    // {x, y, tamanho, tipo(0=virus,1=bact), corIndex}
    static const float decor[][5] = {
        {  70,  90, 30, 0, 0 }, {  150, 280, 22, 1, 1 }, {  60, 470, 26, 0, 2 }, { 120, 640, 20, 1, 1 },
        { 1210, 80, 32, 0, 0 }, { 1130, 250, 24, 1, 1 }, { 1225, 430, 22, 0, 2 }, { 1150, 610, 28, 1, 1 },
        {  300, 60, 18, 1, 1 }, {  980, 60, 20, 0, 0 },  { 360, 670, 18, 0, 2 }, { 940, 668, 20, 1, 1 },
    };
    Color virusCol = (Color){ 170, 90, 230, 255 };   // roxo (vírus)
    Color bactCol  = (Color){ 90, 210, 120, 255 };   // verde (bactéria)
    Color cyanCol  = (Color){ 60, 200, 255, 255 };   // ciano (médico)
    int n = (int)(sizeof(decor) / sizeof(decor[0]));
    for (int i = 0; i < n; i++)
    {
        Vector2 c = { decor[i][0], decor[i][1] + sinf(time * 0.6f + i) * 8.0f }; // flutuação lenta + parallax
        float sz = decor[i][2];
        Color col = (decor[i][4] == 0) ? virusCol : (decor[i][4] == 1) ? bactCol : cyanCol;
        col = Fade(col, fade);
        if ((int)decor[i][3] == 0) DrawMenuVirus(c, sz, time * 25.0f + i * 30.0f, col);     // vírus girando
        else                       DrawMenuBacteria(c, sz, time * 12.0f + i * 40.0f, col);   // bactéria flutuando
    }

    // Símbolo de risco biológico pulsante (canto inferior direito, vermelho coral).
    DrawMenuBiohazard((Vector2){ 1180, 560 }, 46.0f, 0.5f + 0.5f * sinf(time * 2.0f), Fade((Color){ 255, 90, 90, 255 }, 0.5f * fade));
}

void DrawNeonTitle(Font font, float centerX, float topY, float scale, float entry, float time)
{
    float e = UIEase(entry);
    float fs = 78.0f * scale;
    float slide = (1.0f - e) * -28.0f;
    float bob = sinf(time * 2.0f) * 3.0f;
    const char *lines[2] = { "DISEASE'S", "DOOMSDAY" };
    Color neon = THEME_COLOR_TEXT;                 // verde neon (identidade)
    Color deep = (Color){ 0, 70, 45, 255 };

    for (int li = 0; li < 2; li++)
    {
        Vector2 sz = MeasureTextEx(font, lines[li], fs, 4.0f);
        float x = centerX - sz.x * 0.5f;
        float y = topY + slide + bob + li * (fs * 0.96f);

        // Glow em camadas (bloom) — vários offsets com alfa baixo.
        for (int rad = 12; rad >= 4; rad -= 4)
            for (int d = 0; d < 8; d++)
            {
                float a = d * (PI / 4.0f);
                DrawTextEx(font, lines[li], (Vector2){ x + cosf(a) * rad, y + sinf(a) * rad },
                           fs, 4.0f, Fade(neon, 0.05f * e));
            }
        // Sombra escura + contorno.
        DrawTextEx(font, lines[li], (Vector2){ x + 4, y + 5 }, fs, 4.0f, Fade(BLACK, 0.55f * e));
        for (int dx = -2; dx <= 2; dx += 2)
            for (int dy = -2; dy <= 2; dy += 2)
                if (dx || dy)
                    DrawTextEx(font, lines[li], (Vector2){ x + dx, y + dy }, fs, 4.0f, Fade(deep, 0.6f * e));
        // Preenchimento neon + brilho interno.
        DrawTextEx(font, lines[li], (Vector2){ x, y }, fs, 4.0f, Fade(neon, e));
        DrawTextEx(font, lines[li], (Vector2){ x, y - 1 }, fs, 4.0f, Fade((Color){ 200, 255, 210, 255 }, 0.35f * e));
    }
}

bool DrawMenuBanner(Rectangle area, float entry)
{
    if (!SpriteAvailable(SPR_UI_MENU_BANNER)) return false;
    Texture2D t = GetSprite(SPR_UI_MENU_BANNER);
    if (t.id == 0 || t.width <= 0 || t.height <= 0) return false;
    // Escala PROPORCIONAL (nunca distorce): cabe inteiro dentro de `area`.
    float scale = fminf(area.width / (float)t.width, area.height / (float)t.height);
    float w = t.width * scale, h = t.height * scale;
    float e = UIEase(entry);
    float x = area.x + (area.width - w) * 0.5f;
    float y = area.y + (area.height - h) * 0.5f + (1.0f - e) * -18.0f;
    Rectangle src = { 0, 0, (float)t.width, (float)t.height };
    Rectangle dst = { x, y, w, h };
    DrawTexturePro(t, src, dst, (Vector2){ 0, 0 }, 0.0f, Fade(WHITE, e));
    return true;
}

// ============================================================================
// SISTEMA ANIMADO DO MENU (organismos + ECG + título por glifos)
// Estado num único struct, inicializado UMA vez. Tudo por delta time.
// ============================================================================
// População de organismos (vírus/bactérias) animada no fundo do menu.
#define MENU_ORGANISM_TARGET     25
#define MENU_ORGANISM_MIN_ACTIVE 24
#define MENU_ORGANISM_MAX        25     // máximo de ORGANISMOS ativos
#define MENU_BIOHAZARD_COUNT     5
#define MENU_ORG_SLOTS           (MENU_ORGANISM_MAX + MENU_BIOHAZARD_COUNT)
#define MENU_SYRINGE_COUNT       2
#define MENU_SYRINGE_MAX         4
#define MENU_PART_MAX            120
#define MENU_ORGANISM_EXCLUDED   14     // organela roxa com cauda

typedef struct MenuOrganism {
    Vector2 basePosition, position, velocity;
    float rotation, rotationSpeed, phase, depth;
    float fade;          // 0..1 fade-in ao (re)aparecer
    float respawnTimer;  // > 0 = inativo, aguardando respawn
    float radius;        // raio de colisão renderizado (atualizado por frame)
    Color tint;
    int   catIdx;        // índice no catálogo (virus_bacterias.png); -1 = biohazard
    int   isBiohazard;
    bool  active;
} MenuOrganism;

typedef struct MenuSyringe {
    Vector2 position, velocity;
    float rotation, scale, cooldown, depth, wobble;
    bool  active;
} MenuSyringe;

typedef struct MenuDestroyParticle {
    Vector2 position, velocity;
    Color color;
    float life, maxLife, size;
    bool  active;
} MenuDestroyParticle;

typedef struct MenuFX {
    bool  init;
    MenuOrganism org[MENU_ORG_SLOTS];
    int   orgCount;     // nº de slots de ORGANISMO (vírus/bactéria) = TARGET
    int   bioStart;     // índice onde começam os biohazards
    int   bioCount;
    MenuSyringe syr[MENU_SYRINGE_MAX];
    int   syrCount;
    MenuDestroyParticle part[MENU_PART_MAX];
    unsigned rng;
} MenuFX;

static MenuFX MFX;

static unsigned MfxRandU(void) { MFX.rng = MFX.rng * 1664525u + 1013904223u; return MFX.rng; }
static float MfxRandF(void) { return (MfxRandU() >> 8) / 16777216.0f; }
static float MfxRange(float a, float b) { return a + (b - a) * MfxRandF(); }

// Desenha uma textura centralizada (organismos do catálogo / biohazard / seringa).
static void DrawTexCentered(Texture2D t, Vector2 c, Vector2 sz, float rot, Color tint)
{
    if (t.id == 0) return;
    Rectangle src = { 0, 0, (float)t.width, (float)t.height };
    Rectangle dst = { c.x, c.y, sz.x, sz.y };
    DrawTexturePro(t, src, dst, (Vector2){ sz.x * 0.5f, sz.y * 0.5f }, rot, tint);
}

// Cor representativa do organismo (para a mini-explosão), a partir do catálogo.
static Color MenuOrgColor(int catIdx)
{
    if (catIdx < 0 || catIdx >= MenuOrganismCount()) return (Color){ 120, 220, 160, 255 };
    const char *c = MENU_ORGANISMS[catIdx].color;
    if (c[0] == 'g') return (Color){ 90, 230, 120, 255 };   // green
    if (c[0] == 'b') return (Color){ 70, 200, 255, 255 };   // blue
    return (Color){ 190, 110, 255, 255 };                   // purple
}

// Escolhe o modelo de organismo MENOS representado (mantém os 18 sempre visíveis).
static int MenuPickCat(void)
{
    int n = MenuOrganismCount();
    if (n <= 0) return 0;
    if (n > 64) n = 64;
    int counts[64] = { 0 };
    for (int i = 0; i < MFX.orgCount; i++)
        if (MFX.org[i].active && MFX.org[i].catIdx >= 0 && MFX.org[i].catIdx < n)
            counts[MFX.org[i].catIdx]++;
    int off = (int)(MfxRandF() * n), best = 0, bestc = 1 << 30;
    for (int k = 0; k < n; k++)
    {
        int c = (off + k) % n;
        if (c == MENU_ORGANISM_EXCLUDED) continue;
        if (counts[c] < bestc) { bestc = counts[c]; best = c; }
    }
    return best;
}

// Converte uma posição na lista de modelos permitidos para o índice real do
// catálogo, pulando a organela com cauda sem remover o asset da pipeline.
static int MenuAllowedCatAt(int allowedIndex)
{
    return (allowedIndex >= MENU_ORGANISM_EXCLUDED) ? allowedIndex + 1 : allowedIndex;
}

// Posiciona/reseta um organismo por toda a tela, longe das seringas e dos demais
// organismos. O painel e o título são desenhados depois, portanto continuam
// legíveis mesmo quando um elemento atravessa a região central ao fundo.
static void MenuOrgPlace(MenuOrganism *o, int forceCat, bool fadeIn)
{
    float zx = 60.0f, zy = 60.0f;
    for (int tries = 0; tries < 24; tries++)
    {
        zx = MfxRange(28.0f, SCREEN_WIDTH - 28.0f);
        zy = MfxRange(28.0f, SCREEN_HEIGHT - 28.0f);
        bool ok = true;
        for (int s = 0; s < MFX.syrCount; s++)
        {
            if (!MFX.syr[s].active) continue;
            float dx = zx - MFX.syr[s].position.x, dy = zy - MFX.syr[s].position.y;
            if (dx * dx + dy * dy < 120.0f * 120.0f) { ok = false; break; }
        }
        for (int i = 0; ok && i < MFX.orgCount; i++)
        {
            MenuOrganism *other = &MFX.org[i];
            if (!other->active || other == o || other->isBiohazard) continue;
            float dx = zx - other->position.x, dy = zy - other->position.y;
            if (dx * dx + dy * dy < 72.0f * 72.0f) ok = false;
        }
        if (ok) break;
    }
    o->basePosition = o->position = (Vector2){ zx, zy };
    o->depth = MfxRange(0.40f, 1.0f);
    o->rotation = MfxRange(0.0f, 360.0f);
    o->rotationSpeed = MfxRange(-9.0f, 9.0f) * o->depth;
    o->phase = MfxRange(0.0f, 6.2831f);
    float moveAngle = MfxRange(0.0f, 2.0f * PI);
    float moveSpeed = MfxRange(10.0f, 24.0f);
    o->velocity = (Vector2){ cosf(moveAngle) * moveSpeed, sinf(moveAngle) * moveSpeed };
    o->tint = WHITE;
    o->catIdx = (forceCat >= 0) ? forceCat : MenuPickCat();
    o->isBiohazard = 0;
    o->active = true;
    o->respawnTimer = 0.0f;
    o->fade = fadeIn ? 0.0f : 1.0f;
}

static void MenuSyringeSpawn(MenuSyringe *s)
{
    float speed = MfxRange(48.0f, 72.0f);   // travessia calma, ainda mais rápida que os organismos
    int edge = (int)(MfxRandF() * 4.0f);
    Vector2 p, v;
    if (edge == 0)      { p = (Vector2){ -100.0f, MfxRange(80, 640) };  v = (Vector2){ speed, MfxRange(-40, 40) }; }
    else if (edge == 1) { p = (Vector2){ 1380.0f, MfxRange(80, 640) };  v = (Vector2){ -speed, MfxRange(-40, 40) }; }
    else if (edge == 2) { p = (Vector2){ MfxRange(140, 1140), -100.0f }; v = (Vector2){ MfxRange(-50, 50), speed }; }
    else                { p = (Vector2){ MfxRange(140, 1140), 820.0f };  v = (Vector2){ MfxRange(-50, 50), -speed }; }
    s->position = p; s->velocity = v;
    s->rotation = atan2f(v.y, v.x) * RAD2DEG + 90.0f;
    s->depth = MfxRange(0.45f, 0.95f);
    s->scale = MfxRange(0.85f, 1.1f);
    s->cooldown = 0.0f;
    s->wobble = MfxRange(0.0f, 6.2831f);
    s->active = true;
}

static void MenuSpawnExplosion(Vector2 at, Color base)
{
    int count = 8 + (int)(MfxRandF() * 7.0f);   // 8..14
    for (int k = 0; k < count; k++)
    {
        int slot = -1;
        for (int i = 0; i < MENU_PART_MAX; i++) if (!MFX.part[i].active) { slot = i; break; }
        if (slot < 0) break;
        MenuDestroyParticle *p = &MFX.part[slot];
        float ang = MfxRange(0.0f, 6.2831f), spd = MfxRange(60.0f, 220.0f);
        p->position = at;
        p->velocity = (Vector2){ cosf(ang) * spd, sinf(ang) * spd };
        // cor herdada do organismo, com leve variação clara (neon).
        p->color = (Color){
            (unsigned char)fminf(255, base.r + MfxRange(0, 40)),
            (unsigned char)fminf(255, base.g + MfxRange(0, 40)),
            (unsigned char)fminf(255, base.b + MfxRange(0, 40)), 255 };
        p->maxLife = p->life = MfxRange(0.30f, 0.6f);
        p->size = MfxRange(2.0f, 5.0f);
        p->active = true;
    }
}

static void MenuFX_Init(void)
{
    MFX.rng = 0x1357abcdu;
    MFX.syrCount = 0;     // seringas inicializadas após (placement consulta isto)

    int target = MENU_ORGANISM_TARGET;
    MFX.orgCount = target;
    int n = MenuOrganismCount(); if (n <= 0) n = 18;
    int allowedCount = n - ((MENU_ORGANISM_EXCLUDED >= 0 && MENU_ORGANISM_EXCLUDED < n) ? 1 : 0);
    for (int i = 0; i < target; i++)
    {
        MenuOrganism o = (MenuOrganism){0};
        // Os primeiros slots garantem que todos os modelos permitidos apareçam;
        // o restante repete modelos variados (escala/profundidade/rotação distintas).
        MenuOrgPlace(&o, (i < allowedCount) ? MenuAllowedCatAt(i) : -1, false);
        MFX.org[i] = o;
    }
    // Biohazards (símbolo vermelho), estáveis, em camadas diferentes.
    float bhx[5] = { 116, 318, 1054, 1172, 930 };
    float bhy[5] = { 176, 610, 154, 502, 654 };
    float bhd[5] = { 0.58f, 0.46f, 0.72f, 0.86f, 0.52f };
    MFX.bioStart = target; MFX.bioCount = MENU_BIOHAZARD_COUNT;
    for (int b = 0; b < MFX.bioCount; b++)
    {
        MenuOrganism o = (MenuOrganism){0};
        o.basePosition = o.position = (Vector2){ bhx[b], bhy[b] };
        o.depth = bhd[b];
        o.rotation = 0; o.rotationSpeed = MfxRange(-2.0f, 2.0f);
        o.phase = MfxRange(0, 6.2831f);
        o.tint = WHITE; o.catIdx = -1; o.isBiohazard = 1; o.active = true; o.fade = 1.0f;
        MFX.org[MFX.bioStart + b] = o;
    }

    // Seringas (PARTE 3).
    MFX.syrCount = MENU_SYRINGE_COUNT;
    for (int i = 0; i < MFX.syrCount; i++) MenuSyringeSpawn(&MFX.syr[i]);

    for (int i = 0; i < MENU_PART_MAX; i++) MFX.part[i].active = false;
    MFX.init = true;
}

// Atualiza posições, seringas, colisões, explosões e reposição (sem desenhar).
static void MenuFX_Update(float dt, float time)
{
    if (dt > 0.1f) dt = 0.1f;  // estabilidade se houver hitch

    // Organismos: deriva + oscilação + fade; calcula raio de colisão.
    for (int i = 0; i < MFX.bioStart + MFX.bioCount; i++)
    {
        MenuOrganism *o = &MFX.org[i];
        if (!o->isBiohazard && !o->active)
        {
            if (o->respawnTimer > 0.0f) o->respawnTimer -= dt;
            if (o->respawnTimer <= 0.0f) MenuOrgPlace(o, -1, true);  // reaparece com fade
            continue;
        }
        if (o->fade < 1.0f) { o->fade += dt * 2.2f; if (o->fade > 1.0f) o->fade = 1.0f; }
        o->basePosition.x += o->velocity.x * dt * o->depth;
        o->basePosition.y += o->velocity.y * dt * o->depth;
        const float margin = 46.0f;
        if (o->basePosition.y < -margin) o->basePosition.y = SCREEN_HEIGHT + margin;
        if (o->basePosition.y > SCREEN_HEIGHT + margin) o->basePosition.y = -margin;
        if (o->basePosition.x < -margin) o->basePosition.x = SCREEN_WIDTH + margin;
        if (o->basePosition.x > SCREEN_WIDTH + margin) o->basePosition.x = -margin;
        o->position.x = o->basePosition.x + sinf(time * 0.6f + o->phase) * 9.0f * o->depth;
        o->position.y = o->basePosition.y + cosf(time * 0.5f + o->phase * 1.3f) * 7.0f * o->depth;
        o->rotation += o->rotationSpeed * dt;
        float targetH = (o->isBiohazard ? 64.0f : 48.0f) + o->depth * 40.0f;
        o->radius = targetH * 0.42f;
    }

    // Seringas: travessia lenta, com leve variação de rota; reentram nas bordas.
    for (int s = 0; s < MFX.syrCount; s++)
    {
        MenuSyringe *sy = &MFX.syr[s];
        if (!sy->active) { MenuSyringeSpawn(sy); continue; }
        if (sy->cooldown > 0.0f) sy->cooldown -= dt;
        sy->wobble += dt;
        Vector2 dir = Vector2Normalize(sy->velocity);
        Vector2 perp = { -dir.y, dir.x };
        Vector2 vel = Vector2Add(sy->velocity, Vector2Scale(perp, sinf(sy->wobble * 1.2f) * 7.0f));
        sy->position = Vector2Add(sy->position, Vector2Scale(vel, dt));
        sy->rotation = atan2f(vel.y, vel.x) * RAD2DEG + 90.0f;
        if (sy->position.x < -170 || sy->position.x > 1450 || sy->position.y < -170 || sy->position.y > 890)
            MenuSyringeSpawn(sy);
    }

    // Colisão seringa × organismo (1 por seringa por frame; cooldown curto).
    for (int s = 0; s < MFX.syrCount; s++)
    {
        MenuSyringe *sy = &MFX.syr[s];
        if (!sy->active || sy->cooldown > 0.0f) continue;
        Texture2D st = GetSprite(SPR_MENU_SYRINGE);
        float syrH = (70.0f + sy->depth * 60.0f) * sy->scale;
        float syrR = (st.id ? syrH : 60.0f) * 0.26f;
        for (int i = 0; i < MFX.orgCount; i++)   // só organismos (biohazard não é destruído)
        {
            MenuOrganism *o = &MFX.org[i];
            if (!o->active) continue;
            float dx = o->position.x - sy->position.x, dy = o->position.y - sy->position.y;
            float rr = o->radius + syrR + 6.0f;  // margem moderada
            if (dx * dx + dy * dy <= rr * rr)
            {
                MenuSpawnExplosion(o->position, MenuOrgColor(o->catIdx));
                o->active = false;
                o->respawnTimer = MfxRange(0.3f, 0.8f);
                sy->cooldown = MfxRange(0.25f, 0.5f);
                break;  // uma colisão por seringa por frame
            }
        }
    }

    // Reposição imediata se a população cair abaixo do mínimo visível.
    int activeCount = 0;
    for (int i = 0; i < MFX.orgCount; i++) if (MFX.org[i].active) activeCount++;
    while (activeCount < MENU_ORGANISM_MIN_ACTIVE)
    {
        int best = -1; float bestT = 1e9f;
        for (int i = 0; i < MFX.orgCount; i++)
            if (!MFX.org[i].active && MFX.org[i].respawnTimer < bestT) { bestT = MFX.org[i].respawnTimer; best = i; }
        if (best < 0) break;
        MenuOrgPlace(&MFX.org[best], -1, true);
        activeCount++;
    }

    // Partículas das explosões.
    for (int i = 0; i < MENU_PART_MAX; i++)
    {
        MenuDestroyParticle *p = &MFX.part[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.0f) { p->active = false; continue; }
        p->position = Vector2Add(p->position, Vector2Scale(p->velocity, dt));
        p->velocity = Vector2Scale(p->velocity, 1.0f - 3.0f * dt);  // desaceleração
    }
}

// Desenha organismos cujo `depth` está em [dmin, dmax) (camada de profundidade).
static void MenuFX_DrawOrganisms(float dmin, float dmax, float time, float entry)
{
    for (int i = 0; i < MFX.bioStart + MFX.bioCount; i++)
    {
        MenuOrganism *o = &MFX.org[i];
        if (!o->isBiohazard && !o->active) continue;
        if (o->depth < dmin || o->depth >= dmax) continue;

        float glow = 0.85f + 0.15f * sinf(time * 2.0f + o->phase);
        float pulse = 1.0f + 0.03f * sinf(time * 1.8f + o->phase);  // pulsação sutil
        Texture2D t = o->isBiohazard ? GetSprite(SPR_MENU_BIOHAZARD) : GetMenuOrganism(o->catIdx);
        if (t.id != 0 && t.height > 0)
        {
            float targetH = (o->isBiohazard ? 64.0f : 48.0f) + o->depth * 40.0f;
            float sc = targetH / t.height * pulse;       // escala uniforme (preserva proporção)
            Vector2 sz = { t.width * sc, t.height * sc };
            float a = (0.42f + 0.55f * o->depth) * entry * glow * o->fade;
            if (o->isBiohazard) a *= 0.85f;
            DrawTexCentered(t, o->position, sz, o->rotation, Fade(WHITE, a)); // tint WHITE: cor original
        }
        else if (o->isBiohazard)
        {
            DrawMenuBiohazard(o->position, 30.0f * o->depth + 18.0f, glow, Fade((Color){ 255, 70, 70, 255 }, entry));
        }
    }
}

// Desenha seringas cujo `depth` está em [dmin, dmax).
static void MenuFX_DrawSyringes(float dmin, float dmax, float entry)
{
    Texture2D t = GetSprite(SPR_MENU_SYRINGE);
    if (t.id == 0 || t.height <= 0) return;
    for (int s = 0; s < MFX.syrCount; s++)
    {
        MenuSyringe *sy = &MFX.syr[s];
        if (!sy->active || sy->depth < dmin || sy->depth >= dmax) continue;
        float targetH = (70.0f + sy->depth * 60.0f) * sy->scale;
        float sc = targetH / t.height;
        Vector2 sz = { t.width * sc, t.height * sc };
        float a = (0.5f + 0.45f * sy->depth) * entry;
        DrawTexCentered(t, sy->position, sz, sy->rotation, Fade(WHITE, a));
    }
}

// Desenha as mini-explosões (partículas neon).
static void MenuFX_DrawParticles(void)
{
    for (int i = 0; i < MENU_PART_MAX; i++)
    {
        MenuDestroyParticle *p = &MFX.part[i];
        if (!p->active) continue;
        float a = p->life / p->maxLife;
        float r = p->size * (0.6f + 0.8f * a);
        DrawCircleV(p->position, r, Fade(p->color, a));
        DrawCircleV(p->position, r * 0.5f, Fade((Color){ 235, 255, 245, 255 }, a * 0.7f));
    }
}

// Título DISEASE'S / DOOMSDAY montado letra a letra a partir dos glifos da arte.
static void MenuFX_DrawTitle(Font font, float centerX, float topY, float screenAnim, float time)
{
    if (!MenuTitleGlyphsReady())
    {
        DrawNeonTitle(font, centerX, topY, 1.0f, screenAnim / 0.5f, time);  // fallback (fonte)
        return;
    }
    float scale = 0.92f;
    float left = centerX - (MENU_TITLE_W * scale) * 0.5f;
    float collective = 0.02f * (0.5f + 0.5f * sinf(time * 1.2f));  // pulso coletivo sutil (tempo)
    float sweep = fmodf(time * 0.45f, 1.6f);   // brilho atravessando as letras (0..1.6)
    for (int i = 0; i < MENU_TITLE_GLYPH_COUNT; i++)
    {
        const MenuGlyphDef *g = &MENU_TITLE_GLYPHS[i];
        // Entrada sequencial por glifo (baseada no tempo bruto da tela).
        float ge = UIEase((screenAnim - 0.10f - i * 0.03f) / 0.40f);
        if (ge <= 0.01f) continue;
        Texture2D t = GetTitleGlyph(i);
        if (t.id == 0) continue;
        float ph = i * 0.5f;
        float bob = sinf(time * 3.0f + ph) * 3.0f;            // deslocamento vertical
        float squash = 1.0f + sinf(time * 4.0f + ph) * 0.03f + collective; // compressão/pulso
        float gw = g->w * scale, gh = g->h * scale * squash;
        float gx = left + g->x * scale;
        float gy = topY + g->y * scale + bob + (1.0f - ge) * -24.0f;
        Rectangle src = { 0, 0, (float)t.width, (float)t.height };
        Rectangle dst = { gx, gy, gw, gh };
        DrawTexturePro(t, src, dst, (Vector2){ 0, 0 }, 0.0f, Fade(WHITE, ge));
        // Brilho atravessando (light sweep): realce branco quando o sweep passa pelo glifo.
        float gp = (float)i / (float)MENU_TITLE_GLYPH_COUNT;
        float d = fabsf(sweep - gp);
        if (d < 0.10f)
            DrawTexturePro(t, src, dst, (Vector2){ 0, 0 }, 0.0f, Fade((Color){ 220, 255, 230, 255 }, (0.10f - d) / 0.10f * 0.5f * ge));
    }
}

// Fundo animado COMPARTILHADO (menu + loading): vírus/bactérias sendo
// destruídos por seringas, em camadas por profundidade. `entry` (0..1) controla
// o fade de entrada. Centraliza a estética para que a tela de carregamento use
// exatamente a mesma identidade do menu principal, sem duplicar a lógica do FX.
void DrawMenuFXBackground(float time, float entry)
{
    float dt = GetFrameTime();
    if (!MFX.init) MenuFX_Init();
    DrawVisibleVirtualGradientV((Color){ 9, 13, 30, 255 }, (Color){ 4, 6, 16, 255 });
    MenuFX_Update(dt, time);
    MenuFX_DrawOrganisms(0.00f, 0.60f, time, entry);   // organismos distantes
    MenuFX_DrawSyringes(0.00f, 0.70f, entry);          // seringas distantes
    MenuFX_DrawOrganisms(0.60f, 0.85f, time, entry);   // intermediários
    MenuFX_DrawParticles();                            // mini-explosões
    MenuFX_DrawOrganisms(0.85f, 1.01f, time, entry);   // próximos
    MenuFX_DrawSyringes(0.70f, 1.01f, entry);          // seringas próximas
}

// ---- Layout ÚNICO do menu (desenho e input compartilham os retângulos) ----
extern UIButton menuButtons[8];
static Rectangle g_menuName = { 0, 0, 0, 0 };
void MenuApplyLayout(void)
{
    float cx = SCREEN_WIDTH / 2.0f;
    float bw = 340.0f, bh = 36.0f, gap = 6.0f, pitch = bh + gap;
    float bx = cx - bw / 2.0f;
    float firstY = 332.0f;
    for (int i = 0; i < 8; i++)
        menuButtons[i].bounds = (Rectangle){ bx, firstY + i * pitch, bw, bh };
    g_menuName = (Rectangle){ bx, 280.0f, bw, 40.0f };
}
Rectangle MenuNameRect(void) { return g_menuName; }

// ============================================================================
// 1. TELA: MENU PRINCIPAL
// ============================================================================
void DrawTelaMenu(GameState *game, Font font, float time)
{
    float dt = GetFrameTime();
    if (!MFX.init) MenuFX_Init();
    MenuApplyLayout();                       // posiciona botões + campo de nome (única fonte)
    float entry = UIEase(game->screenAnim / 0.5f);

    // Acento que faz MORPH conforme o item destacado no menu.
    Color morphAccent = THEME_COLOR_MAIN;
    switch (game->highlightScreen)
    {
        case SCREEN_ARSENAL:  morphAccent = (Color){ 0, 220, 255, 255 };  break; // ciano
        case SCREEN_SKINS:    morphAccent = (Color){ 190, 110, 255, 255 }; break; // roxo
        case SCREEN_CONTROLS: morphAccent = (Color){ 90, 220, 120, 255 };  break; // verde
        case SCREEN_SETTINGS: morphAccent = (Color){ 80, 150, 255, 255 };  break; // azul
        case SCREEN_ADMIN:    morphAccent = (Color){ 255, 120, 110, 255 }; break; // vermelho suave
        default:              morphAccent = THEME_COLOR_MAIN;              break;
    }
    static Color curAccent = { 0, 229, 255, 255 };
    float k = 5.0f * dt; if (k > 1.0f) k = 1.0f;
    curAccent = (Color){
        (unsigned char)(curAccent.r + (morphAccent.r - curAccent.r) * k),
        (unsigned char)(curAccent.g + (morphAccent.g - curAccent.g) * k),
        (unsigned char)(curAccent.b + (morphAccent.b - curAccent.b) * k), 255 };

    // Fundo azul-marinho quase preto + brilho de acento no topo + vinheta inferior.
    Rectangle visible = VisibleVirtualScreenRect();
    DrawVisibleVirtualGradientV((Color){ 9, 13, 30, 255 }, (Color){ 4, 6, 16, 255 });
    DrawRectangleGradientV((int)floorf(visible.x), (int)floorf(visible.y),
                           (int)ceilf(visible.width), 150,
                           Fade(curAccent, (0.05f + 0.03f * sinf(time * 1.5f)) * entry),
                           Fade(curAccent, 0.0f));
    DrawRectangleGradientV((int)floorf(visible.x), SCREEN_HEIGHT - 90,
                           (int)ceilf(visible.width), (int)ceilf(visible.y + visible.height - (SCREEN_HEIGHT - 90)),
                           Fade(BLACK, 0.0f), Fade(BLACK, 0.4f));

    // FX do fundo (PARTE 7 — ordem de camadas): atualiza tudo e desenha por
    // profundidade, intercalando seringas e explosões entre os organismos.
    MenuFX_Update(dt, time);
    MenuFX_DrawOrganisms(0.00f, 0.60f, time, entry);   // organismos distantes
    MenuFX_DrawSyringes(0.00f, 0.70f, entry);          // seringas distantes
    MenuFX_DrawOrganisms(0.60f, 0.85f, time, entry);   // organismos intermediários
    MenuFX_DrawParticles();                            // mini-explosões
    MenuFX_DrawOrganisms(0.85f, 1.01f, time, entry);   // organismos próximos
    MenuFX_DrawSyringes(0.70f, 1.01f, entry);          // seringas próximas

    // Partículas decorativas existentes (sobem suavemente).
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            float s = game->particles[i].size;
            DrawRectangleV((Vector2){ game->particles[i].position.x - s, game->particles[i].position.y - s }, (Vector2){ s * 2.0f, s * 2.0f }, Fade(game->particles[i].color, game->particles[i].lifeTime / game->particles[i].maxLifeTime));
        }
    }

    // Título DISEASE'S / DOOMSDAY montado letra a letra a partir dos glifos da arte.
    MenuFX_DrawTitle(font, SCREEN_WIDTH / 2.0f, 44.0f, game->screenAnim, time);

    // ---- Painel central de interação (nome + botões), mais alto e centralizado ----
    Rectangle panel = { SCREEN_WIDTH / 2.0f - 200.0f, 248.0f, 400.0f, 414.0f };
    DrawPanel(panel, Fade(curAccent, entry), 0.66f * entry);

    // Desenha Campo de Texto para o Nome do Jogador (mesmo retângulo do input).
    Rectangle nameBounds = MenuNameRect();
    bool nameHover = CheckCollisionPointRec(g_virtualMouse, nameBounds);
    
    Color boxBg = Fade(THEME_COLOR_PANEL, 0.85f);
    Color boxBorder = THEME_COLOR_BORDER;
    
    if (game->nameInputActive)
    {
        boxBorder = THEME_COLOR_MAIN; // Cyan se ativo
    }
    else if (nameHover)
    {
        boxBorder = YELLOW; // Amarelo no hover
    }
    
    DrawRectangleRounded(nameBounds, 0.2f, 6, boxBg);
    DrawRectangleRoundedLines(nameBounds, 0.2f, 6, boxBorder);
    
    // Rótulo acima do campo
    DrawTextEx(font, "Seu Anticorpo:", (Vector2){ nameBounds.x + 5, nameBounds.y - 18 }, 14.0f, 1.0f, GRAY);
    
    // Desenha o texto do nome ou placeholder
    int fontSize = 20;
    if (game->player.name[0] == '\0')
    {
        if (game->nameInputActive)
        {
            const char *cursorStr = ((int)(time * 2.0f) % 2 == 0) ? "|" : "";
            Vector2 textSz = MeasureTextEx(font, cursorStr, (float)fontSize, 1.0f);
            Vector2 textPos = {
                nameBounds.x + (nameBounds.width / 2.0f) - (textSz.x / 2.0f),
                nameBounds.y + (nameBounds.height / 2.0f) - (textSz.y / 2.0f)
            };
            DrawTextEx(font, cursorStr, textPos, (float)fontSize, 1.0f, WHITE);
        }
        else
        {
            Vector2 textSz = MeasureTextEx(font, "NOME DO ANTICORPO...", (float)fontSize, 1.0f);
            Vector2 textPos = {
                nameBounds.x + (nameBounds.width / 2.0f) - (textSz.x / 2.0f),
                nameBounds.y + (nameBounds.height / 2.0f) - (textSz.y / 2.0f)
            };
            DrawTextEx(font, "NOME DO ANTICORPO...", textPos, (float)fontSize, 1.0f, Fade(GRAY, 0.5f));
        }
    }
    else
    {
        char nameWithCursor[32];
        if (game->nameInputActive && ((int)(time * 2.0f) % 2 == 0))
        {
            sprintf(nameWithCursor, "%s|", game->player.name);
        }
        else
        {
            sprintf(nameWithCursor, "%s", game->player.name);
        }
        
        Vector2 textSz = MeasureTextEx(font, nameWithCursor, (float)fontSize, 1.0f);
        Vector2 textPos = {
            nameBounds.x + (nameBounds.width / 2.0f) - (textSz.x / 2.0f),
            nameBounds.y + (nameBounds.height / 2.0f) - (textSz.y / 2.0f)
        };
        DrawTextEx(font, nameWithCursor, textPos, (float)fontSize, 1.0f, WHITE);
    }

    // Se existe arquivo de save em qualquer um dos 3 slots, indica que pode carregar
    bool anySaveExists = AnySaveExistsCached();

    // Botões do menu com ENTRADA EM CASCATA (slide+fade escalonado por índice). O
    // hitbox real (menuButtons[i].bounds) não se move; apenas o desenho desliza.
    for (int i = 0; i < MENU_BTN_COUNT; i++)
    {
        float be = UIEase((game->screenAnim - 0.18f - i * 0.05f) / 0.35f);
        UIButton b = menuButtons[i];
        b.bounds.x -= (1.0f - be) * 70.0f; // desliza da esquerda
        bool enabled = (i == 1) ? anySaveExists : true;
        if (be <= 0.02f) continue;          // ainda não "entrou"
        DrawButton(b, font, enabled);
    }

    // Rodapé discreto (sem seletor de dificuldade — agora há tela dedicada).
    const char *hint = "JOGAR escolhe a dificuldade  -  ESC pausa no jogo";
    Vector2 hs = MeasureTextEx(font, hint, 13.0f, 1.0f);
    DrawTextEx(font, hint, (Vector2){ 640.0f - hs.x / 2.0f, SCREEN_HEIGHT - 24.0f }, 13.0f, 1.0f, Fade(THEME_COLOR_TEXT, 0.55f));
}

// ============================================================================
// 1b. TELA: SELEÇÃO DE DIFICULDADE (cards) — abre ao iniciar/reiniciar um jogo
// ============================================================================
static Rectangle DiffCardRect(int i)
{
    float w = 360.0f, h = 400.0f, gap = 30.0f;
    float startX = (SCREEN_WIDTH - (3.0f * w + 2.0f * gap)) / 2.0f;
    return (Rectangle){ startX + i * (w + gap), 158.0f, w, h };
}
static Rectangle DiffStartRect(void) { return (Rectangle){ SCREEN_WIDTH / 2.0f - 175.0f, 572.0f, 350.0f, 52.0f }; }
static Rectangle DiffBackRect(void)  { return (Rectangle){ 40.0f, 652.0f, 220.0f, 46.0f }; }

static const Color DIFF_COL[3] = {
    { 110, 225, 150, 255 }, // Fácil — verde/ciano
    { 255, 190, 70, 255 },  // Médio — amarelo/laranja
    { 255, 84, 116, 255 }   // Difícil — vermelho/magenta
};

// Barra de intensidade (rótulo + barra preenchida) contida na largura `w`.
static void DiffIntBar(Font font, float x, float y, float w, const char *label, float v, Color col, float a)
{
    v = Clamp(v, 0.0f, 1.0f);
    DrawTextEx(font, label, (Vector2){ x, y }, 13.0f, 1.0f, Fade(WHITE, 0.8f * a));
    float by = y + 17.0f, bh = 9.0f;
    DrawRectangleRounded((Rectangle){ x, by, w, bh }, 0.7f, 4, Fade(BLACK, 0.5f * a));
    DrawRectangleRounded((Rectangle){ x, by, w * v, bh }, 0.7f, 4, Fade(col, a));
}

void DrawTelaDifficulty(GameState *game, Font font)
{
    extern Vector2 g_virtualMouse;
    float time = (float)GetTime();
    float entry = UIEase(game->screenAnim / 0.5f);

    // Fundo: acento conforme a opção tentativa (verde/amarelo/vermelho).
    int pd = game->pendingDifficulty; if (pd < 0 || pd > 2) pd = DIFFICULTY_MEDIUM;
    DrawMenuBackground(DIFF_COL[pd], time, game->screenAnim / 0.5f);

    DrawTitleText(font, "SELECIONE A DIFICULDADE", SCREEN_WIDTH / 2.0f, 70.0f, 40.0f, Fade(THEME_COLOR_TEXT, entry));
    const char *sub = "A dificuldade so e aplicada ao confirmar em INICIAR MISSAO.";
    Vector2 ss = MeasureTextEx(font, sub, 16.0f, 1.0f);
    DrawTextEx(font, sub, (Vector2){ SCREEN_WIDTH / 2.0f - ss.x / 2.0f, 124.0f }, 16.0f, 1.0f, Fade(WHITE, 0.8f * entry));

    const char *names[3] = { "FACIL", "MEDIO", "DIFICIL" };
    const char *subt[3]  = { "Acessivel", "Recomendado", "Extremo" };
    const char *desc[3]  = {
        "Inimigos um pouco mais lentos e janelas de esquiva generosas. Ideal para aprender e curtir a campanha.",
        "Desafio elevado: IA mais agressiva e coordenada, com invocacoes mais frequentes. A experiencia recomendada.",
        "Experiencia extrema para veteranos: reacao quase instantanea, esquiva e flanqueamento implacaveis."
    };
    const char *barLabels[3] = { "IA / coordenacao", "Velocidade", "Pressao / cadencia" };
    float barVals[3][3] = {
        { 0.45f, 0.55f, 0.40f },
        { 0.72f, 0.74f, 0.68f },
        { 1.00f, 0.95f, 1.00f }
    };

    for (int i = 0; i < 3; i++)
    {
        Rectangle r = DiffCardRect(i);
        Color col = DIFF_COL[i];
        bool selected = (game->pendingDifficulty == i);
        bool hover = CheckCollisionPointRec(g_virtualMouse, r);
        bool pressed = hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        // Entrada em cascata + leve elevação no hover/selecionado.
        float ce = UIEase((game->screenAnim - 0.1f - i * 0.08f) / 0.4f);
        float dy = (1.0f - ce) * 50.0f + ((hover || selected) ? -6.0f : 0.0f) + (pressed ? 3.0f : 0.0f);
        Rectangle dr = { r.x, r.y + dy, r.width, r.height };

        // Fundo + borda por estado.
        DrawRectangleRounded(dr, 0.06f, 10, Fade((Color){ 10, 13, 20, 255 }, (selected ? 0.95f : 0.82f) * ce));
        if (selected)
        {
            float gp = 0.5f + 0.5f * sinf(time * 5.0f);
            DrawRectangleRoundedLines((Rectangle){ dr.x - 3, dr.y - 3, dr.width + 6, dr.height + 6 }, 0.06f, 10, Fade(col, (0.4f + 0.5f * gp) * ce));
            DrawRectangleRoundedLines(dr, 0.06f, 10, Fade(col, ce));
        }
        else
        {
            DrawRectangleRoundedLines(dr, 0.06f, 10, Fade(col, (hover ? 0.9f : 0.5f) * ce));
        }
        // Faixa de acento no topo do card.
        DrawRectangleRounded((Rectangle){ dr.x + 14, dr.y + 12, dr.width - 28, 6 }, 0.8f, 4, Fade(col, ce));

        float pad = 22.0f;
        // Nome (ajustado para nunca estourar).
        DrawTextFitCentered(font, names[i], (Rectangle){ dr.x, dr.y + 26, dr.width, 40 }, 32.0f, Fade(col, ce), true);
        // Badge RECOMENDADO no Médio.
        if (i == 1)
        {
            const char *bd = "RECOMENDADO";
            Vector2 bs = MeasureTextEx(font, bd, 12.0f, 1.0f);
            Rectangle badge = { dr.x + dr.width / 2.0f - bs.x / 2.0f - 10, dr.y + 64, bs.x + 20, 20 };
            DrawRectangleRounded(badge, 0.5f, 6, Fade(col, 0.22f * ce));
            DrawRectangleRoundedLines(badge, 0.5f, 6, Fade(col, ce));
            DrawTextEx(font, bd, (Vector2){ badge.x + 10, badge.y + 4 }, 12.0f, 1.0f, Fade(col, ce));
        }
        // Subtítulo.
        Vector2 sts = MeasureTextEx(font, subt[i], 17.0f, 1.0f);
        DrawTextEx(font, subt[i], (Vector2){ dr.x + dr.width / 2.0f - sts.x / 2.0f, dr.y + 92 }, 17.0f, 1.0f, Fade(WHITE, 0.85f * ce));
        // Descrição (wrap; encolhe p/ caber na altura).
        DrawTextWrapped(font, desc[i], (Rectangle){ dr.x + pad, dr.y + 120, dr.width - pad * 2, 92 }, 15.0f, 1.0f, Fade(WHITE, 0.8f * ce));
        // Indicador de intensidade (3 barras).
        DrawTextEx(font, "INTENSIDADE", (Vector2){ dr.x + pad, dr.y + 222 }, 13.0f, 1.0f, Fade(col, ce));
        for (int b = 0; b < 3; b++)
            DiffIntBar(font, dr.x + pad, dr.y + 244 + b * 30, dr.width - pad * 2, barLabels[b], barVals[i][b], col, ce);
        // Selecionado.
        if (selected)
        {
            const char *sel = "> SELECIONADO <";
            Vector2 zs = MeasureTextEx(font, sel, 14.0f, 1.0f);
            DrawTextEx(font, sel, (Vector2){ dr.x + dr.width / 2.0f - zs.x / 2.0f, dr.y + dr.height - 28 }, 14.0f, 1.0f, Fade(col, ce));
        }
    }

    // Botões: INICIAR MISSAO (confirma) + VOLTAR.
    UIButton startBtn = { DiffStartRect(), "INICIAR MISSAO", CheckCollisionPointRec(g_virtualMouse, DiffStartRect()), false };
    DrawButton(startBtn, font, true);
    UIButton backBtn = { DiffBackRect(), "VOLTAR", CheckCollisionPointRec(g_virtualMouse, DiffBackRect()), false };
    DrawButton(backBtn, font, true);

    const char *nav = "Setas / A-D para escolher   -   ENTER confirma   -   ESC volta";
    Vector2 ns = MeasureTextEx(font, nav, 14.0f, 1.0f);
    DrawTextEx(font, nav, (Vector2){ SCREEN_WIDTH / 2.0f - ns.x / 2.0f, SCREEN_HEIGHT - 26.0f }, 14.0f, 1.0f, Fade(THEME_COLOR_TEXT, 0.6f));
}

void UpdateTelaDifficulty(GameState *game, Vector2 mouse)
{
    if (game->pendingDifficulty < 0 || game->pendingDifficulty > 2)
        game->pendingDifficulty = DIFFICULTY_MEDIUM;

    // Seleção por clique no card (NÃO muda só por passar o mouse).
    bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    for (int i = 0; i < 3; i++)
        if (clicked && CheckCollisionPointRec(mouse, DiffCardRect(i)))
            game->pendingDifficulty = i;

    // Navegação por teclado.
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D))
        game->pendingDifficulty = (game->pendingDifficulty + 1) % 3;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A))
        game->pendingDifficulty = (game->pendingDifficulty + 2) % 3;

    // VOLTAR / ESC: retorna à tela de origem sem alterar a dificuldade.
    bool backClick = clicked && CheckCollisionPointRec(mouse, DiffBackRect());
    if (backClick || IsKeyPressed(KEY_ESCAPE))
    {
        GameScreen ret = (GameScreen)game->diffReturnScreen;
        if (ret == SCREEN_DIFFICULTY_SELECT) ret = SCREEN_MENU; // segurança
        game->currentScreen = ret;
        return;
    }

    // INICIAR MISSAO / ENTER: confirma — só AQUI a dificuldade é aplicada.
    bool startClick = clicked && CheckCollisionPointRec(mouse, DiffStartRect());
    if (startClick || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
    {
        int chosen = game->pendingDifficulty;
        if (chosen < 0 || chosen > 2) chosen = DIFFICULTY_MEDIUM;
        GameScreen ret = (GameScreen)game->diffReturnScreen;

        game->difficulty = chosen;   // InitGame preserva e aplica (ApplyDifficulty)
        InitGame(game);
        SavePlayerConfig(game);      // persiste a dificuldade escolhida

        if (ret == SCREEN_MENU)
        {
            // Jogo novo completo: passa pelo tutorial (cutscene de injeção).
            RequestLoadingScreen(game, LOAD_TO_TUTORIAL, 2.0f);
        }
        else
        {
            // Reinício (Game Over / Vitória): vai direto à gameplay.
            game->inTutorial = false;
            RequestLoadingScreen(game, LOAD_TO_GAMEPLAY, 2.0f);
        }
    }
}



// ============================================================================
// 2. TELA: TUTORIAL (GUIA EM ABAS) — acessível pelo Menu
// ============================================================================
#define TUT_TAB_COUNT 7
static int g_tutTab = 0;
static float g_tutWeaponsScroll = 0.0f;
static const char *g_tutTabNames[TUT_TAB_COUNT] = {
    "BASICO", "ARMAS", "INIMIGOS", "CHEFE", "SKINS", "XP/UPGRADES", "HORDAS"
};
static const char *g_tutTabSub[TUT_TAB_COUNT] = {
    "Objetivo e controles", "Equipamentos e combate", "Conheca as ameacas",
    "Batalha da onda final", "Identidade visual", "Evolucao do anticorpo",
    "Progressao das ondas"
};

static Color TutAccent(int tab)
{
    static const Color colors[TUT_TAB_COUNT] = {
        { 80, 220, 160, 255 }, { 255, 196, 72, 255 }, { 80, 205, 255, 255 },
        { 255, 84, 108, 255 }, { 205, 110, 255, 255 }, { 255, 150, 70, 255 },
        { 90, 145, 255, 255 }
    };
    if (tab < 0 || tab >= TUT_TAB_COUNT) tab = 0;
    return colors[tab];
}

static Rectangle TutTabRect(int i)
{
    return (Rectangle){ 54.0f, 116.0f + i * 66.0f, 264.0f, 56.0f };
}

// Ícones vetoriais simples para identificar rapidamente cada seção. São usados
// tanto na navegação quanto no cabeçalho do painel de detalhes.
static void DrawTutorialIcon(int tab, Vector2 c, float size, Color accent, float time)
{
    float pulse = 1.0f + 0.04f * sinf(time * 2.5f + tab);
    float s = size * pulse;
    DrawCircleV(c, s * 0.58f, Fade(accent, 0.10f));
    DrawCircleLines((int)c.x, (int)c.y, s * 0.52f, Fade(accent, 0.75f));

    switch (tab)
    {
        case 0: // controles: direcional
            DrawRectangleRounded((Rectangle){ c.x - s*0.12f, c.y - s*0.38f, s*0.24f, s*0.76f }, 0.3f, 4, accent);
            DrawRectangleRounded((Rectangle){ c.x - s*0.38f, c.y - s*0.12f, s*0.76f, s*0.24f }, 0.3f, 4, accent);
            break;
        case 1: // arma/projétil
            DrawLineEx((Vector2){ c.x-s*0.32f, c.y+s*0.24f }, (Vector2){ c.x+s*0.28f, c.y-s*0.30f }, s*0.16f, accent);
            DrawCircleV((Vector2){ c.x+s*0.34f, c.y-s*0.36f }, s*0.10f, WHITE);
            break;
        case 2: // patógeno
            DrawCircleV(c, s*0.27f, Fade(accent, 0.75f));
            for (int i = 0; i < 8; i++) {
                float a = i * PI/4.0f + time*0.15f;
                Vector2 a0 = { c.x+cosf(a)*s*0.27f, c.y+sinf(a)*s*0.27f };
                Vector2 a1 = { c.x+cosf(a)*s*0.43f, c.y+sinf(a)*s*0.43f };
                DrawLineEx(a0, a1, 2.0f, accent); DrawCircleV(a1, s*0.05f, accent);
            }
            break;
        case 3: // chefe/coroa
            DrawTriangle((Vector2){c.x-s*.34f,c.y+s*.20f}, (Vector2){c.x-s*.24f,c.y-s*.30f}, (Vector2){c.x,c.y+s*.02f}, accent);
            DrawTriangle((Vector2){c.x,c.y+s*.02f}, (Vector2){c.x+s*.24f,c.y-s*.30f}, (Vector2){c.x+s*.34f,c.y+s*.20f}, accent);
            DrawRectangleRounded((Rectangle){c.x-s*.34f,c.y+s*.15f,s*.68f,s*.17f},0.3f,4,accent);
            break;
        case 4: // skin/escudo
            DrawPoly(c, 6, s*0.34f, 30.0f, Fade(accent, 0.75f));
            DrawPolyLinesEx(c, 6, s*0.34f, 30.0f, 3.0f, WHITE);
            break;
        case 5: // evolução
            DrawLineEx((Vector2){c.x-s*.28f,c.y+s*.28f}, (Vector2){c.x+s*.28f,c.y-s*.28f}, 4.0f, accent);
            DrawTriangle((Vector2){c.x+s*.28f,c.y-s*.28f}, (Vector2){c.x+s*.02f,c.y-s*.22f}, (Vector2){c.x+s*.22f,c.y+s*.02f}, accent);
            DrawCircleV((Vector2){c.x-s*.25f,c.y+s*.25f},s*.09f,WHITE);
            break;
        default: // hordas/ondas
            for (int i = 0; i < 3; i++)
                DrawRing(c, s*(0.12f+i*.10f), s*(0.15f+i*.10f), 200, 340, 18, Fade(accent, 1.0f-i*.18f));
            break;
    }
}

// Linha "rótulo: texto" auxiliar do tutorial
static void TutLine(Font font, float x, float y, const char *label, Color lc, const char *text)
{
    DrawTextEx(font, label, (Vector2){ x, y }, 18.0f, 1.0f, lc);
    if (text && text[0])
    {
        float lw = MeasureTextEx(font, label, 18.0f, 1.0f).x;
        DrawTextEx(font, text, (Vector2){ x + lw + 10.0f, y }, 18.0f, 1.0f, Fade(WHITE, 0.85f));
    }
}

// Preview usa exatamente o mesmo modelo procedural da gameplay. Assim, o guia
// permanece sincronizado quando a identidade visual de um arquétipo evoluir.
static void DrawTutorialEnemyPreview(int type, Vector2 center, float size, bool boss)
{
    Enemy preview = {0};
    EnemyInitFromArchetype(&preview, type, 5, 1.0f);
    preview.active = true;
    preview.spawnAnim = 1.0f;
    preview.animTime = (float)GetTime();
    if (boss) preview.tier = TIER_3_BOSS;
    DrawEnemyModel(&preview, center, size, 0.0f, 1.0f, 1.0f);
}

static void DrawEnemyGuideRow(Font font, Rectangle row, int type,
                              const char *wave, const char *role)
{
    const EnemyArchetype *arch = EnemyArchetypeFor(type);
    Color color = arch ? arch->palette : SKYBLUE;
    const char *name = (arch && arch->name) ? arch->name : "Patogeno";

    DrawRectangleRounded(row, 0.18f, 6, Fade((Color){ 5, 13, 20, 255 }, 0.72f));
    DrawRectangleRoundedLines(row, 0.18f, 6, Fade(color, 0.48f));
    // DrawEnemyModel recebe um RAIO, e alguns vírus projetam espículas até
    // aproximadamente 1.8x esse valor. O orçamento abaixo mantém inclusive a
    // silhueta externa dentro da coluna de 46 px reservada ao preview.
    float previewRadius = (row.height >= 70.0f) ? 20.0f : 11.5f;
    // O bacilo e um bastonete horizontal: suas extremidades e espessura
    // ultrapassam o raio nominal, portanto usa escala propria no card.
    if (type == ETYPE_BACT_RANGED) previewRadius = 13.5f;
    DrawTutorialEnemyPreview(type, (Vector2){ row.x + 27.0f, row.y + row.height*0.5f },
                             previewRadius, false);
    DrawTextEx(font, name, (Vector2){ row.x + 54.0f, row.y + 7.0f },
               15.0f, 1.0f, color);
    Vector2 ws = MeasureTextEx(font, wave, 11.0f, 1.0f);
    DrawTextEx(font, wave, (Vector2){ row.x + row.width - ws.x - 9.0f, row.y + 9.0f },
               11.0f, 1.0f, Fade(GOLD, 0.9f));
    if (row.height >= 70.0f)
    {
        DrawTextWrapped(font, role,
                        (Rectangle){ row.x + 54.0f, row.y + 29.0f,
                                     row.width - 64.0f, row.height - 35.0f },
                        12.0f, 1.0f, Fade(WHITE, 0.78f));
    }
    else
    {
        DrawTextEx(font, role, (Vector2){ row.x + 54.0f, row.y + 29.0f },
                   12.0f, 1.0f, Fade(WHITE, 0.78f));
    }
}

static void DrawTutorialWeaponCard(Font font, Rectangle card, int weapon,
                                   const char *slot, const char *role,
                                   Color primary, Color secondary, float time)
{
    WeaponInfo wi = GetWeaponInfo(weapon);
    Color color = wi.color;
    DrawRectangleRounded(card, 0.08f, 8, Fade((Color){ 5, 13, 20, 255 }, 0.76f));
    DrawRectangleRoundedLines(card, 0.08f, 8, Fade(color, 0.58f));

    Rectangle preview = { card.x + 14.0f, card.y + 18.0f, card.width * 0.43f, card.height - 36.0f };
    float bob = sinf(time * 2.0f + weapon) * 6.0f;
    Vector2 pc = { preview.x + preview.width * 0.5f, preview.y + preview.height * 0.5f + bob };
    DrawCircleGradient((int)pc.x, (int)pc.y, preview.height * 0.54f, Fade(color, 0.22f), BLANK);
    DrawCircleLines((int)pc.x, (int)pc.y, preview.height * 0.44f, Fade(color, 0.28f));
    DrawHeldWeaponFramed(weapon, (Rectangle){ preview.x, preview.y + bob, preview.width, preview.height },
                         58.0f, sinf(time * 1.7f + weapon) * 10.0f, primary, secondary);

    float tx = card.x + card.width * 0.49f;
    float tw = card.x + card.width - tx - 16.0f;
    DrawTextEx(font, slot, (Vector2){ tx, card.y + 16.0f }, 14.0f, 1.0f, Fade(GOLD, 0.95f));
    DrawTextWrapped(font, wi.name, (Rectangle){ tx, card.y + 38.0f, tw, 50.0f },
                    22.0f, 1.0f, color);
    DrawTextWrapped(font, wi.special,
                    (Rectangle){ tx, card.y + 92.0f, tw, 45.0f },
                    16.0f, 1.0f, Fade(WHITE, 0.84f));
    DrawTextWrapped(font, role,
                    (Rectangle){ tx, card.y + 139.0f, tw, card.height - 150.0f },
                    15.0f, 1.0f, Fade((Color){ 130, 230, 170, 255 }, 0.92f));
}

static void DrawTutContent(GameState *game, Font font, Rectangle panel)
{
    float x = panel.x + 28.0f;
    float y = panel.y + 22.0f;
    float step = 30.0f;
    Color accent = TutAccent(g_tutTab);

    switch (g_tutTab)
    {
        case 0: // BASICO + CONTROLES
            DrawTextEx(font, "OBJETIVO", (Vector2){ x, y }, 22.0f, 1.0f, accent); y += 30.0f;
            DrawTextEx(font, "Voce e um Anticorpo. Sobreviva a 5 hordas de patogenos, derrote o", (Vector2){ x, y }, 17.0f, 1.0f, Fade(WHITE, 0.85f)); y += 24.0f;
            DrawTextEx(font, "chefe da onda 5 e proteja o organismo do Distrito Federal.", (Vector2){ x, y }, 17.0f, 1.0f, Fade(WHITE, 0.85f)); y += 38.0f;
            DrawTextEx(font, "CONTROLES", (Vector2){ x, y }, 22.0f, 1.0f, accent); y += 32.0f;
            TutLine(font, x, y, "WASD / SETAS", YELLOW, "Mover o Anticorpo"); y += step;
            TutLine(font, x, y, "CLIQUE ESQ. / ESPACO", YELLOW, "Atacar com a arma equipada"); y += step;
            TutLine(font, x, y, "1  2  3  4", YELLOW, "Selecionar / trocar de arma"); y += step;
            TutLine(font, x, y, "E", YELLOW, "Usar pocao de vida (cura 50%)"); y += step;
            TutLine(font, x, y, "ESC", YELLOW, "Pausar / voltar"); y += step;
            TutLine(font, x, y, "F5 / F9", YELLOW, "Salvar / carregar rapido (slot 1)"); y += step;
            TutLine(font, x, y, "POWER-UPS", GOLD, "Cura, Velocidade, Escudo e Dano x2 (brilham no mapa)");
            break;

        case 1: // ARMAS
        {
            Color wp = WeaponSkinPrimary(game->player.weaponSkinId);
            Color ws = WeaponSkinSecondary(game->player.weaponSkinId);
            float t = (float)GetTime();
            const float headerH = 38.0f;
            const float cardW = 382.0f;
            const float cardH = 190.0f;
            const float gapX = 18.0f;
            const float gapY = 18.0f;
            const float viewH = panel.height - 18.0f;
            // 4 linhas de cards + cabeçalho + margem inferior. ATENÇÃO: este mesmo
            // cálculo é repetido em UpdateTelaTutorial (rolagem) — mantenha os dois
            // idênticos, senão a roda do mouse limita a rolagem antes do conteúdo.
            const float contentH = headerH + 4.0f * cardH + 3.0f * gapY + 24.0f;
            const float maxScroll = contentH - viewH;
            if (g_tutWeaponsScroll < 0.0f) g_tutWeaponsScroll = 0.0f;
            if (g_tutWeaponsScroll > maxScroll) g_tutWeaponsScroll = maxScroll;

            float sy = y - g_tutWeaponsScroll;
            DrawTextEx(font, "Quatro slots. Cada arma evolui com 30 abates proprios. Role para ver tudo.",
                       (Vector2){ x, sy }, 18.0f, 1.0f, Fade(WHITE, 0.88f));
            sy += headerH;

            DrawTutorialWeaponCard(font, (Rectangle){ x, sy, cardW, cardH }, 1, "SLOT 1",
                                   "Combo alterna estocada frontal e corte rapido para segurar inimigos perto.", wp, ws, t);
            DrawTutorialWeaponCard(font, (Rectangle){ x + cardW + gapX, sy, cardW, cardH }, WEAPON_BIOBLADE, "SLOT 1 / 30 ABATES",
                                   "Depois de desbloquear, aperte 1 para alternar com a Espada-Seringa.", wp, ws, t);
            sy += cardH + gapY;
            DrawTutorialWeaponCard(font, (Rectangle){ x, sy, cardW, cardH }, 2, "SLOT 2 / NIVEL 2",
                                   "Tiro preciso para finalizar alvos em linha e controlar distancia.", wp, ws, t);
            DrawTutorialWeaponCard(font, (Rectangle){ x + cardW + gapX, sy, cardW, cardH }, WEAPON_RIFLE_EVOLVED, "SLOT 2 / 30 ABATES",
                                   "Projeteis replicantes criam um disparo extra no primeiro impacto.", wp, ws, t);
            sy += cardH + gapY;
            DrawTutorialWeaponCard(font, (Rectangle){ x, sy, cardW, cardH }, 3, "SLOT 3 / NIVEL 3",
                                   "Planta minas no chao; aperte mouse 2 para detonar todas.", wp, ws, t);
            DrawTutorialWeaponCard(font, (Rectangle){ x + cardW + gapX, sy, cardW, cardH }, WEAPON_RNA_LAUNCHER, "SLOT 3 / 30 ABATES",
                                   "Arremessa minas a distancia; elas explodem em 6s ou com mouse 2.", wp, ws, t);
            sy += cardH + gapY;
            DrawTutorialWeaponCard(font, (Rectangle){ x, sy, cardW, cardH }, 4, "SLOT 4 / NIVEL 4",
                                   "Projetil pesado perfurante para limpar corredores e chefes.", wp, ws, t);
            DrawTutorialWeaponCard(font, (Rectangle){ x + cardW + gapX, sy, cardW, cardH }, WEAPON_BFG_EVOLVED, "SLOT 4 / 30 ABATES",
                                   "Atravessa inimigos e explode em area no fim do trajeto.", wp, ws, t);

            if (maxScroll > 0.0f)
            {
                float trackX = panel.x + panel.width - 13.0f;
                float trackY = panel.y + 14.0f;
                float trackH = panel.height - 28.0f;
                float thumbH = trackH * (viewH / contentH);
                float thumbY = trackY + (g_tutWeaponsScroll / maxScroll) * (trackH - thumbH);
                DrawRectangleRounded((Rectangle){ trackX, trackY, 5.0f, trackH }, 0.8f, 4, Fade(BLACK, 0.45f));
                DrawRectangleRounded((Rectangle){ trackX, thumbY, 5.0f, thumbH }, 0.8f, 4, Fade(accent, 0.78f));
            }
            break;
        }

        case 2: // INIMIGOS
        {
            const int virusTypes[5] = {
                ETYPE_VIRUS_SWARM, ETYPE_VIRUS_MELEE, ETYPE_VIRUS_RANGED,
                ETYPE_VIRUS_ELITE, ETYPE_VIRUS_BOSS
            };
            const char *virusWave[5] = { "ONDA 1", "ONDA 2", "ONDA 3", "ONDA 4", "ONDA 5" };
            const char *virusRole[5] = {
                "Enxame pequeno, veloz e fragil; capsideo fraco.",
                "Corpo a corpo; investida e dano de contato.",
                "Atirador; recua e dispara material viral.",
                "Mutante grande; escudo forte e invoca enxames.",
                "Chefe multifase; coroa, rajadas e invocacoes."
            };

            float leftX = x;
            float rightX = x + 300.0f;
            DrawTextEx(font, "MUNDO 1 - BACTERIAS", (Vector2){ leftX, y }, 17.0f, 1.0f,
                       (Color){ 110, 225, 135, 255 });
            DrawTextEx(font, "MUNDO 2 - VIRUS", (Vector2){ rightX, y }, 17.0f, 1.0f, accent);

            const int bacteriaTypes[3] = { ETYPE_BACT_MELEE, ETYPE_BACT_RANGED, ETYPE_KPC };
            const char *bacteriaWave[3] = { "ONDAS 1-5", "ONDAS 2-5", "ELITE / CHEFE" };
            const char *bacteriaRole[3] = {
                "Coco: persegue e ataca por contato.",
                "Bacilo: atirador que controla distancia.",
                "KPC: tanque resistente com tiro pesado."
            };
            for (int i = 0; i < 3; i++)
                DrawEnemyGuideRow(font, (Rectangle){ leftX, y + 27.0f + i*92.0f, 274.0f, 78.0f },
                                  bacteriaTypes[i], bacteriaWave[i], bacteriaRole[i]);

            for (int i = 0; i < 5; i++)
                DrawEnemyGuideRow(font, (Rectangle){ rightX, y + 27.0f + i*61.0f, 478.0f, 52.0f },
                                  virusTypes[i], virusWave[i], virusRole[i]);
            break;
        }

        case 3: // CHEFE
        {
            Rectangle bactCard = { x, y, 378.0f, 287.0f };
            Rectangle virusCard = { x + 400.0f, y, 378.0f, 287.0f };
            DrawRectangleRounded(bactCard, 0.08f, 8, Fade((Color){ 35, 10, 14, 255 }, 0.70f));
            DrawRectangleRoundedLines(bactCard, 0.08f, 8, Fade((Color){ 255, 80, 90, 255 }, 0.55f));
            DrawRectangleRounded(virusCard, 0.08f, 8, Fade((Color){ 24, 8, 35, 255 }, 0.70f));
            DrawRectangleRoundedLines(virusCard, 0.08f, 8, Fade((Color){ 210, 90, 225, 255 }, 0.60f));

            DrawTutorialEnemyPreview(ETYPE_KPC, (Vector2){ bactCard.x + 49, bactCard.y + 54 }, 27.0f, true);
            DrawTextEx(font, "MUNDO 1", (Vector2){ bactCard.x + 92, bactCard.y + 18 }, 13.0f, 1.0f, GOLD);
            DrawTextEx(font, "SUPERBACTERIA KPC", (Vector2){ bactCard.x + 92, bactCard.y + 40 },
                       18.0f, 1.0f, (Color){ 255, 80, 90, 255 });
            DrawTextEx(font, "Tanque resistente a antibioticos.", (Vector2){ bactCard.x + 18, bactCard.y + 91 }, 14.0f, 1.0f, Fade(WHITE, .82f));
            DrawTextEx(font, "Fase 1  Tiros pesados e escolta", (Vector2){ bactCard.x + 18, bactCard.y + 124 }, 14.0f, 1.0f, Fade(WHITE, .78f));
            DrawTextEx(font, "Fase 2  Acelera e invoca lacaios", (Vector2){ bactCard.x + 18, bactCard.y + 151 }, 14.0f, 1.0f, Fade(WHITE, .78f));
            DrawTextEx(font, "Fase 3  Rajada radial enfurecida", (Vector2){ bactCard.x + 18, bactCard.y + 178 }, 14.0f, 1.0f, Fade(WHITE, .78f));
            DrawTextEx(font, "Granada e BFG ajudam contra sua vida alta.", (Vector2){ bactCard.x + 18, bactCard.y + 226 }, 13.0f, 1.0f, (Color){ 120, 220, 140, 255 });

            DrawTutorialEnemyPreview(ETYPE_VIRUS_BOSS, (Vector2){ virusCard.x + 51, virusCard.y + 54 }, 21.0f, true);
            DrawTextEx(font, "MUNDO 2", (Vector2){ virusCard.x + 96, virusCard.y + 18 }, 13.0f, 1.0f, GOLD);
            DrawTextEx(font, "CORONAVIRUS", (Vector2){ virusCard.x + 96, virusCard.y + 40 },
                       18.0f, 1.0f, (Color){ 220, 90, 220, 255 });
            DrawTextEx(font, "Chefe viral com capsideo reforcado.", (Vector2){ virusCard.x + 18, virusCard.y + 91 }, 14.0f, 1.0f, Fade(WHITE, .82f));
            DrawTextEx(font, "Fase 1  Quebre o escudo protetor", (Vector2){ virusCard.x + 18, virusCard.y + 124 }, 14.0f, 1.0f, Fade(WHITE, .78f));
            DrawTextEx(font, "Fase 2  Invoca enxames de rinovirus", (Vector2){ virusCard.x + 18, virusCard.y + 151 }, 14.0f, 1.0f, Fade(WHITE, .78f));
            DrawTextEx(font, "Fase 3  RNA exposto e rajada radial", (Vector2){ virusCard.x + 18, virusCard.y + 178 }, 14.0f, 1.0f, Fade(WHITE, .78f));
            DrawTextEx(font, "Lamina Bioeletrica rompe o capsideo.", (Vector2){ virusCard.x + 18, virusCard.y + 226 }, 13.0f, 1.0f, (Color){ 120, 220, 140, 255 });

            DrawTextEx(font, "Ambos aparecem sempre na onda 5, protegidos por lacaios. Elimine os Nucleos de Infeccao.",
                       (Vector2){ x, y + 307.0f }, 14.0f, 1.0f, Fade(accent, 0.92f));
            break;
        }

        case 4: // SKINS
            DrawTextEx(font, "Abra 'SKINS' no menu para escolher (preview ao vivo).", (Vector2){ x, y }, 17.0f, 1.0f, Fade(WHITE, 0.85f)); y += 34.0f;
            DrawTextEx(font, "SKINS DO ANTICORPO:", (Vector2){ x, y }, 18.0f, 1.0f, accent); y += 28.0f;
            TutLine(font, x + 16, y, "PADRAO", YELLOW, "cavaleiro imune classico."); y += 26.0f;
            TutLine(font, x + 16, y, "MEDICA", YELLOW, "jaleco branco com cruz vermelha."); y += 26.0f;
            TutLine(font, x + 16, y, "INFECTADA", YELLOW, "armadura roxa corrompida."); y += 34.0f;
            DrawTextEx(font, "SKIN DA ARMA muda a cor dos disparos (Padrao, Plasma, Toxica).", (Vector2){ x, y }, 16.0f, 1.0f, Fade(WHITE, 0.8f)); y += 26.0f;
            DrawTextEx(font, "As skins sao visuais e nao alteram o balanceamento.", (Vector2){ x, y }, 15.0f, 1.0f, Fade(GRAY, 0.9f));
            break;

        case 5: // XP / UPGRADES
            DrawTextEx(font, "PROGRESSAO DE RPG", (Vector2){ x, y }, 22.0f, 1.0f, accent); y += 32.0f;
            TutLine(font, x, y, "XP:", GOLD, "derrotar patogenos enche a barra roxa de XP."); y += 28.0f;
            TutLine(font, x, y, "NIVEL:", GOLD, "ao subir, ganha +Vida, +Dano, +Velocidade e cura total."); y += 28.0f;
            TutLine(font, x, y, "ARMAS:", GOLD, "Fuzil (Nv2), Granada (Nv3) e BFG (Nv4) desbloqueiam por nivel."); y += 28.0f;
            DrawTextEx(font, "PONTOS DO SUS", (Vector2){ x, y }, 20.0f, 1.0f, accent); y += 28.0f;
            TutLine(font, x, y, "QUIZ:", GOLD, "acertar o quiz entre ondas da +50 Pontos do SUS."); y += 26.0f;
            TutLine(font, x, y, "LOJA:", GOLD, "gaste os pontos em +Vida Max, +Velocidade ou +Dano."); y += 26.0f;
            DrawTextEx(font, "A tela de Melhorias do SUS aparece automaticamente entre as ondas.", (Vector2){ x, y }, 15.0f, 1.0f, Fade(WHITE, 0.8f));
            break;

        default: // HORDAS
            DrawTextEx(font, "SISTEMA DE HORDAS", (Vector2){ x, y }, 22.0f, 1.0f, accent); y += 32.0f;
            DrawTextEx(font, "Sao 5 ondas, cada uma mais dificil: mais inimigos e mais fortes.", (Vector2){ x, y }, 17.0f, 1.0f, Fade(WHITE, 0.85f)); y += 26.0f;
            DrawTextEx(font, "Elimine todos os patogenos da onda para avancar.", (Vector2){ x, y }, 17.0f, 1.0f, Fade(WHITE, 0.85f)); y += 26.0f;
            DrawTextEx(font, "Entre as ondas: QUIZ educativo + tela de Melhorias do SUS.", (Vector2){ x, y }, 17.0f, 1.0f, Fade(WHITE, 0.85f)); y += 32.0f;
            TutLine(font, x, y, "ONDA 5:", (Color){ 255, 90, 90, 255 }, "batalha de CHEFE garantida (veja a aba CHEFE)."); y += 30.0f;
            DrawTextEx(font, "Um banner no topo avisa cada nova horda e a chegada do chefe.", (Vector2){ x, y }, 15.0f, 1.0f, (Color){ 120, 220, 140, 255 });
            break;
    }
}

void DrawTelaControles(GameState *game, Font font)
{
    float time = (float)GetTime();
    DrawThemedBackground(SCREEN_CONTROLS, time, game->screenAnim / 0.4f);
    float entry = UIEase(game->screenAnim / 0.4f);
    Color acc = TutAccent(g_tutTab);
    DrawUIScreenTitle(font, "TUTORIAL", acc, entry);

    const char *screenSub = "GUIA DE CAMPO DO ANTICORPO";
    Vector2 screenSubSize = MeasureTextEx(font, screenSub, 14.0f, 1.0f);
    DrawTextEx(font, screenSub,
               (Vector2){ SCREEN_WIDTH/2.0f-screenSubSize.x/2.0f, 91.0f },
               14.0f, 1.0f, Fade(WHITE, 0.62f*entry));

    // Coluna de navegação no padrão Arsenal: cards empilhados com ícone,
    // numeração, nome e estado selecionado.
    for (int i = 0; i < TUT_TAB_COUNT; i++)
    {
        Rectangle tr = TutTabRect(i);
        bool active = (g_tutTab == i);
        bool hover = CheckCollisionPointRec(g_virtualMouse, tr);
        Color tabAccent = TutAccent(i);
        float ce = UIEase((game->screenAnim - i*0.04f) / 0.4f);
        Rectangle dr = tr; dr.x -= (1.0f-ce)*34.0f;
        DrawUICard(dr, tabAccent, hover, active, ce);
        // A faixa de destaque do card ocupa o TOPO (y+10..15). Todo o conteúdo
        // (ícone, contador, nome, marcador) fica em ZONAS SEPARADAS abaixo dela,
        // com padding real — nada sobrepõe a faixa nem os textos entre si.
        // Zona do ícone (coluna esquerda, centrada na área de conteúdo).
        DrawTutorialIcon(i, (Vector2){dr.x+30.0f, dr.y+33.0f}, 25.0f,
                         active ? tabAccent : Fade(tabAccent, 0.72f), time);
        // Zona do contador (NN), logo abaixo da faixa, à direita do ícone.
        DrawTextEx(font, TextFormat("%02d", i+1),
                   (Vector2){dr.x+52.0f, dr.y+26.0f}, 13.0f, 1.0f,
                   Fade(tabAccent, 0.8f*ce));
        // Zona do nome (medida e centrada verticalmente, sem invadir a faixa).
        Rectangle tabName = { dr.x+76.0f, dr.y+21.0f, dr.width-88.0f, 26.0f };
        DrawTextFitCentered(font, g_tutTabNames[i], tabName, 16.0f,
                            active ? tabAccent : Fade(WHITE, hover ? 0.95f : 0.76f), true);
        // Marcador de seleção (faixa vertical à esquerda, dentro da zona).
        if (active)
            DrawRectangleRounded((Rectangle){dr.x+4.0f,dr.y+20.0f,4.0f,dr.height-28.0f},0.8f,4,tabAccent);
    }

    // Painel principal de detalhes, equivalente ao preview amplo do Arsenal.
    Rectangle detail = { 338.0f, 116.0f, 882.0f, 500.0f };
    DrawUICard(detail, acc, false, true, entry);

    // Cabeçalho visual da categoria selecionada. Posicionado ABAIXO da faixa de
    // destaque do painel (y+10..15) para que ícone, título e subtítulo não sejam
    // invadidos por ela nem pelo badge de progresso (à direita).
    DrawTutorialIcon(g_tutTab, (Vector2){392.0f, 168.0f}, 48.0f, acc, time);
    DrawTextEx(font, g_tutTabNames[g_tutTab], (Vector2){ 438.0f, 137.0f },
               26.0f, 1.0f, Fade(acc, entry));
    DrawTextEx(font, g_tutTabSub[g_tutTab], (Vector2){ 438.0f, 170.0f },
               15.0f, 1.0f, Fade(WHITE, 0.68f*entry));

    // Indicador integrado ao cabeçalho do painel, longe dos seletores.
    char prog[16];
    snprintf(prog, sizeof(prog), "%d / %d", g_tutTab + 1, TUT_TAB_COUNT);
    Vector2 ps = MeasureTextEx(font, prog, 16.0f, 1.0f);
    Rectangle progressBadge = { detail.x+detail.width-104.0f, 137.0f, 76.0f, 30.0f };
    DrawRectangleRounded(progressBadge, 0.5f, 6, Fade((Color){ 8, 14, 20, 255 }, 0.78f * entry));
    DrawRectangleRoundedLines(progressBadge, 0.5f, 6, Fade(acc, 0.75f * entry));
    DrawTextEx(font, prog,
               (Vector2){ progressBadge.x + (progressBadge.width - ps.x) * 0.5f,
                          progressBadge.y + (progressBadge.height - ps.y) * 0.5f },
               16.0f, 1.0f, Fade(acc, entry));

    DrawLineEx((Vector2){detail.x+24.0f,194.0f},
               (Vector2){detail.x+detail.width-24.0f,194.0f}, 2.0f, Fade(acc,0.35f*entry));

    // Conteúdo com slide/fade ao trocar de categoria.
    static int prevTab = -1; static float tabAnim = 0.0f;
    if (g_tutTab != prevTab) { prevTab = g_tutTab; tabAnim = 0.0f; }
    tabAnim += GetFrameTime();
    float ta = UIEase(tabAnim / 0.3f);
    Rectangle panel = { 358.0f, 207.0f, 842.0f, 389.0f };
    DrawUISectionPanel(font, panel, NULL, acc, entry);
    BeginVirtualScissorMode(panel);
    rlPushMatrix();
    rlTranslatef((1.0f - ta) * 36.0f, 0.0f, 0.0f);
    DrawTutContent(game, font, panel);
    rlPopMatrix();
    EndVirtualScissorMode();

    const char *tutorialHint = "SETAS / W-S: navegar   |   clique para selecionar   |   ESC: voltar";
    DrawTextEx(font, tutorialHint,
               (Vector2){ 54.0f, 637.0f },
               15.0f, 1.0f, Fade(WHITE, 0.6f));

    // Botão voltar
    DrawButton(controlsButton, font, true);
}

// Atualização da tela de tutorial: navegação de abas + voltar
void UpdateTelaTutorial(GameState *game, Vector2 mouse)
{
    int oldTab = g_tutTab;
    for (int i = 0; i < TUT_TAB_COUNT; i++)
    {
        if (CheckCollisionPointRec(mouse, TutTabRect(i)) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            g_tutTab = i;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_S))
        g_tutTab = (g_tutTab + 1) % TUT_TAB_COUNT;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_W))
        g_tutTab = (g_tutTab + TUT_TAB_COUNT - 1) % TUT_TAB_COUNT;
    if (g_tutTab != oldTab && g_tutTab == 1) g_tutWeaponsScroll = 0.0f;

    if (g_tutTab == 1)
    {
        Rectangle weaponsPanel = { 358.0f, 207.0f, 842.0f, 389.0f };
        float wheel = GetMouseWheelMove();
        // DEVE casar com o contentH de DrawTutContent (4 linhas de cards): headerH
        // (38) + 4*cardH (190) + 3*gapY (18) + margem inferior (24). Antes usava 3
        // linhas, e por isso não dava para rolar até a última arma (slot 4).
        const float contentH = 38.0f + 4.0f * 190.0f + 3.0f * 18.0f + 24.0f;
        const float viewH = weaponsPanel.height - 18.0f;
        float maxScroll = contentH - viewH;
        if (maxScroll < 0.0f) maxScroll = 0.0f;
        if (CheckCollisionPointRec(mouse, weaponsPanel) && wheel != 0.0f)
            g_tutWeaponsScroll -= wheel * 44.0f;
        if (IsKeyPressed(KEY_PAGE_DOWN)) g_tutWeaponsScroll += 150.0f;
        if (IsKeyPressed(KEY_PAGE_UP)) g_tutWeaponsScroll -= 150.0f;
        if (g_tutWeaponsScroll < 0.0f) g_tutWeaponsScroll = 0.0f;
        if (g_tutWeaponsScroll > maxScroll) g_tutWeaponsScroll = maxScroll;
    }

    UpdateBtnState(&controlsButton, mouse);
    if (controlsButton.clicked || IsKeyPressed(KEY_ESCAPE))
        game->currentScreen = SCREEN_MENU;
}




// ============================================================================
// 3. TELA: PAUSA (OVERLAY)
// ============================================================================
void DrawTelaPausa(GameState *game, Font font)
{
    // Escurece a tela de fundo da gameplay
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.65f));

    float animDuration = 0.5f;
    float t = game->uiAnimTimer / animDuration;
    if (t > 1.0f) t = 1.0f;
    float ease = EaseOutCubic(t);
    float offsetY = (1.0f - ease) * -800.0f; // Começa de cima

    rlPushMatrix();
    rlTranslatef(0.0f, offsetY, 0.0f);

    // Caixa de diálogo central (Glassmorphism místico do void)
    DrawRectangleRounded((Rectangle){ 420, 130, 440, 350 }, 0.05f, 6, Fade((Color){ 12, 8, 22, 255 }, 0.92f));
    DrawRectangleRoundedLines((Rectangle){ 420, 130, 440, 350 }, 0.05f, 6, THEME_COLOR_BORDER);

    const char *pauseTxt = "GAME PAUSED";
    Vector2 pauseSize = MeasureTextEx(font, pauseTxt, 28.0f, 1.0f);
    DrawTextEx(font, pauseTxt, (Vector2){ (SCREEN_WIDTH / 2.0f) - (pauseSize.x / 2.0f), 155.0f }, 28.0f, 1.0f, THEME_COLOR_MAIN);

    for (int i = 0; i < 5; i++)
    {
        DrawButton(pauseButtons[i], font, true);
    }
    
    rlPopMatrix();
}



// ============================================================================
// 4. TELA: GAME OVER
// ============================================================================
// Linha de estatística alinhada (rótulo à esquerda, valor à direita) dentro de
// um painel — texto sempre contido (mede com MeasureTextEx).
static void StatLine(Font font, Rectangle panel, float y, const char *label, const char *value, Color valCol)
{
    float fs = 21.0f, padX = 22.0f;
    DrawTextEx(font, label, (Vector2){ panel.x + padX, y }, fs, 1.0f, Fade(WHITE, 0.85f));
    Vector2 vs = MeasureTextEx(font, value, fs, 1.0f);
    DrawTextEx(font, value, (Vector2){ panel.x + panel.width - padX - vs.x, y }, fs, 1.0f, valCol);
}

void DrawTelaGameOver(GameState *game, Font font)
{
    float time = (float)GetTime();
    DrawThemedBackground(SCREEN_GAMEOVER, time, game->screenAnim / 0.5f);

    float entry = UIEase(game->screenAnim / 0.5f);
    float slide = (1.0f - entry) * 46.0f;

    rlPushMatrix();
    rlTranslatef(0.0f, slide, 0.0f);

    // "Flatline" dramática atrás do título (sem prejudicar legibilidade).
    float baseY = 132.0f;
    DrawLineEx((Vector2){ 120, baseY }, (Vector2){ 520, baseY }, 2.0f, Fade(RED, 0.35f * entry));
    DrawLineEx((Vector2){ 520, baseY }, (Vector2){ 560, baseY - 26 }, 2.0f, Fade(RED, 0.5f * entry));
    DrawLineEx((Vector2){ 560, baseY - 26 }, (Vector2){ 600, baseY + 30 }, 2.0f, Fade(RED, 0.5f * entry));
    DrawLineEx((Vector2){ 600, baseY + 30 }, (Vector2){ 640, baseY }, 2.0f, Fade(RED, 0.5f * entry));
    DrawLineEx((Vector2){ 640, baseY }, (Vector2){ 1160, baseY }, 2.0f, Fade(RED, 0.35f * entry));

    float pulse = 1.0f + sinf(time * 4.0f) * 0.04f;
    DrawTitleText(font, "GAME OVER", SCREEN_WIDTH / 2.0f, 86.0f, 66.0f * pulse, Fade((Color){ 235, 60, 70, 255 }, entry));

    // Painel de estatísticas alinhado.
    Rectangle panel = { 400, 210, 480, 170 };
    DrawPanel(panel, Fade((Color){ 230, 80, 90, 255 }, entry), 0.72f * entry);
    StatLine(font, panel, panel.y + 26, "Pontuacao final", TextFormat("%d", game->player.score), GOLD);
    StatLine(font, panel, panel.y + 64, "Nivel final", TextFormat("Lvl %d", game->player.level), WHITE);
    StatLine(font, panel, panel.y + 102, "Patogenos eliminados", TextFormat("%d", game->totalEnemiesKilled), (Color){ 120, 220, 140, 255 });

    rlPopMatrix();

    // Botões (sem transform p/ manter hitbox correta; perfeitamente alinhados).
    for (int i = 0; i < 2; i++) DrawButton(gameOverButtons[i], font, true);
}



// ============================================================================
// CUTSCENE DO CIENTISTA — reutilizada pela transição pós-Mundo 1 e pela vitória.
// Cientista em destaque (mesma arte do tutorial, grande) + caixa de diálogo grande
// com typewriter. O texto faz wrap e nunca vaza. Avança com SPACE/ENTER/Q/clique.
// ============================================================================
int ScientistDialogAdvance(DialogState *dlg, const char *pageText, int pageCount,
                           int voiceScope, float sfxVolume)
{
    int totalLen = (int)strlen(pageText);
    float speed = ScientistVoiceCharDelay(totalLen, 0.018f);
    dlg->charTimer += GetFrameTime();
    while (dlg->charTimer >= speed && dlg->charShown < totalLen) { dlg->charTimer -= speed; dlg->charShown++; }
    SyncScientistVoice(voiceScope, dlg->page, dlg->charShown, totalLen, sfxVolume);

    bool confirm = IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) ||
                   IsKeyPressed(KEY_Q) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    if (!confirm) return 0;
    if (dlg->charShown < totalLen)
    {
        dlg->charShown = totalLen;
        StopScientistVoice();
        return 0;
    } // 1o input: revela tudo
    StopScientistVoice();
    if (dlg->page < pageCount - 1) { dlg->page++; dlg->charShown = 0; dlg->charTimer = 0.0f; return 1; }
    return 2; // última página confirmada
}

void DrawScientistDialog(Font font, DialogState *dlg, const char *header,
                         const char *pageText, int pageCount, Color accent, float entry)
{
    float t = (float)GetTime();
    int totalLen = (int)strlen(pageText);

    // --- Cientista em destaque (círculo grande à esquerda) ---
    Vector2 dc = { 290.0f, 348.0f };
    float dr = 168.0f;
    DrawCircleV(dc, (dr + 26.0f) * (0.98f + 0.02f * sinf(t * 2.0f)), Fade(accent, 0.10f * entry)); // halo "transmissão"
    DrawCircleLines((int)dc.x, (int)dc.y, dr + 14.0f, Fade(accent, 0.35f * entry));
    bool talking = (dlg->charShown < totalLen);
    float reactT = (dlg->charShown < 10) ? 1.0f : 0.0f;
    DrawTutorialDoctor(dc, dr, t, talking, reactT);
    const char *who = "CIENTISTA-CHEFE";
    Vector2 ws = MeasureTextEx(font, who, 22.0f, 1.0f);
    DrawTextEx(font, who, (Vector2){ dc.x - ws.x * 0.5f, dc.y + dr + 22.0f }, 22.0f, 1.0f, Fade(accent, entry));

    // --- Caixa de diálogo grande (direita) ---
    Rectangle box = { 520.0f, 152.0f, 706.0f, 392.0f };
    DrawPanel(box, Fade(accent, entry), 0.85f * entry);
    DrawTextEx(font, header, (Vector2){ box.x + 28.0f, box.y + 20.0f }, 22.0f, 1.0f, Fade(accent, entry));
    DrawLineEx((Vector2){ box.x + 28.0f, box.y + 52.0f }, (Vector2){ box.x + box.width - 28.0f, box.y + 52.0f },
               2.0f, Fade(accent, 0.4f * entry));

    // Texto revelado (typewriter) com WRAP — nunca vaza da caixa.
    char shown[1024];
    int n = dlg->charShown; if (n > totalLen) n = totalLen; if (n > 1023) n = 1023;
    memcpy(shown, pageText, (size_t)n); shown[n] = '\0';
    Rectangle textArea = { box.x + 28.0f, box.y + 66.0f, box.width - 56.0f, box.height - 128.0f };
    DrawTextWrapped(font, shown, textArea, 21.0f, 1.0f, Fade(WHITE, entry));

    // Indicadores de página + prompt pulsante.
    for (int i = 0; i < pageCount; i++)
        DrawCircleV((Vector2){ box.x + 28.0f + i * 18.0f, box.y + box.height - 28.0f }, 5.0f,
                    (i == dlg->page) ? accent : Fade(accent, 0.3f));
    const char *prompt = (dlg->charShown < totalLen) ? "[ESPACO] revelar"
                       : (dlg->page < pageCount - 1) ? "[ESPACO] continuar >" : "[ESPACO] avancar >";
    float a = 0.55f + 0.45f * sinf(t * 4.0f);
    Vector2 ps = MeasureTextEx(font, prompt, 16.0f, 1.0f);
    DrawTextEx(font, prompt, (Vector2){ box.x + box.width - ps.x - 26.0f, box.y + box.height - 32.0f },
               16.0f, 1.0f, Fade(accent, a * entry));
}

// ============================================================================
// 5. TELA: VITÓRIA — cena final do cientista + relatório (score/nível/abates)
// ============================================================================
int VictoryDialogPages(GameState *game, const char **out)
{
    static char statsPage[300];
    snprintf(statsPage, sizeof statsPage,
        "Relatorio final da missao: pontuacao de %d, nivel %d alcancado e %d "
        "patogenos eliminados. Esses numeros nao sao so estatisticas: mostram "
        "quantas ameacas voce impediu de avancar pelo organismo.",
        game->player.score, game->player.level, game->totalEnemiesKilled);
    out[0] = "Voce conseguiu, Anticorpo. Contra todas as previsoes, o organismo foi "
             "estabilizado: as bacterias foram contidas, as ameacas virais eliminadas "
             "e a infeccao nao conseguiu se espalhar.";
    out[1] = statsPage;
    out[2] = "Em nome de toda a equipe medica, obrigado. Transmissao encerrada.";
    return 3;
}

void DrawTelaVitoria(GameState *game, Font font)
{
    float time = (float)GetTime();
    DrawThemedBackground(SCREEN_VICTORY, time, game->screenAnim / 0.5f);

    // Partículas festivas (atualizadas no UpdateButtonsVitoria).
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            float s = game->particles[i].size;
            DrawRectangleV((Vector2){ game->particles[i].position.x - s, game->particles[i].position.y - s }, (Vector2){ s * 2.0f, s * 2.0f }, Fade(game->particles[i].color, game->particles[i].lifeTime / game->particles[i].maxLifeTime));
        }
    }

    float entry = UIEase(game->screenAnim / 0.5f);

    // -------- FASE 0: o cientista narra o encerramento (relatório integrado) --------
    if (game->sceneDialog.active)
    {
        DrawTitleText(font, "ORGANISMO CURADO!", SCREEN_WIDTH / 2.0f, 54.0f, 30.0f, Fade(THEME_COLOR_TEXT, entry));
        const char *pages[4];
        int pc = VictoryDialogPages(game, pages);
        if (game->sceneDialog.page >= pc) game->sceneDialog.page = pc - 1;
        DrawScientistDialog(font, &game->sceneDialog, "TRANSMISSAO // RELATORIO FINAL",
                            pages[game->sceneDialog.page], pc, GOLD, entry);
        return;
    }

    // -------- FASE 1: encerrada a transmissão, mostra stats + apenas 2 opções --------
    float slide = (1.0f - entry) * 46.0f;

    // Raios celebratórios irradiando atrás do título (tema imunológico).
    Vector2 burst = { SCREEN_WIDTH / 2.0f, 110.0f };
    for (int r = 0; r < 16; r++)
    {
        float a = (time * 0.6f + r * (360.0f / 16.0f)) * DEG2RAD;
        float len = 120.0f + sinf(time * 2.0f + r) * 24.0f;
        DrawLineEx(burst, (Vector2){ burst.x + cosf(a) * len, burst.y + sinf(a) * len },
                   2.0f, Fade(GOLD, 0.10f * entry));
    }

    rlPushMatrix();
    rlTranslatef(0.0f, slide, 0.0f);

    float pulse = 1.0f + sinf(time * 3.5f) * 0.04f;
    DrawTitleText(font, "ORGANISMO CURADO!", SCREEN_WIDTH / 2.0f, 78.0f, 60.0f * pulse, Fade(THEME_COLOR_TEXT, entry));

    Rectangle congrats = { 240, 150, 800, 34 };
    DrawTextWrapped(font, "Voce erradicou todas as infeccoes do Distrito Federal!",
                    congrats, 19.0f, 1.0f, Fade((Color){ 120, 230, 170, 255 }, entry));

    Rectangle panel = { 400, 200, 480, 170 };
    DrawPanel(panel, Fade(GOLD, entry), 0.72f * entry);
    StatLine(font, panel, panel.y + 26, "Pontuacao final", TextFormat("%d", game->player.score), GOLD);
    StatLine(font, panel, panel.y + 64, "Nivel final", TextFormat("Lvl %d", game->player.level), WHITE);
    StatLine(font, panel, panel.y + 102, "Total de patogenos", TextFormat("%d", game->totalEnemiesKilled), (Color){ 120, 220, 140, 255 });

    rlPopMatrix();

    for (int i = 0; i < 2; i++) DrawButton(victoryButtons[i], font, true);
}



// ============================================================================
// 6. MINI-MAPA & HUD DA GAMEPLAY
// ============================================================================
// Auxiliary Sci-fi corner brackets container helper
void DrawSciFiBox(Rectangle r, Color col)
{
    // Deep dark translucent panel background (cockpit glass style)
    DrawRectangleRec(r, Fade((Color){ 8, 6, 16, 255 }, 0.65f));
    
    // Sleek thin border
    DrawRectangleLinesEx(r, 1.0f, Fade(col, 0.25f));
    
    // High-tech corner bracket markings
    float len = 10.0f;
    float thickness = 2.0f;
    // Top-left corner
    DrawLineEx((Vector2){ r.x, r.y }, (Vector2){ r.x + len, r.y }, thickness, col);
    DrawLineEx((Vector2){ r.x, r.y }, (Vector2){ r.x, r.y + len }, thickness, col);
    // Top-right corner
    DrawLineEx((Vector2){ r.x + r.width, r.y }, (Vector2){ r.x + r.width - len, r.y }, thickness, col);
    DrawLineEx((Vector2){ r.x + r.width, r.y }, (Vector2){ r.x + r.width, r.y + len }, thickness, col);
    // Bottom-left corner
    DrawLineEx((Vector2){ r.x, r.y + r.height }, (Vector2){ r.x + len, r.y + r.height }, thickness, col);
    DrawLineEx((Vector2){ r.x, r.y + r.height }, (Vector2){ r.x, r.y + r.height - len }, thickness, col);
    // Bottom-right corner
    DrawLineEx((Vector2){ r.x + r.width, r.y + r.height }, (Vector2){ r.x + r.width - len, r.y + r.height }, thickness, col);
    DrawLineEx((Vector2){ r.x + r.width, r.y + r.height }, (Vector2){ r.x + r.width, r.y + r.height - len }, thickness, col);
}

// ============================================================================
// 8. TELAS DE SELEÇÃO DE SAVE E LOAD
// ============================================================================

void DrawTelaSaveSelect(GameState *game, Font font, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT])
{
    // Fundo escuro translúcido para revelar a run desfocada/limpa no fundo
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade((Color){ 10, 8, 22, 255 }, 0.75f));

    // Título
    const char *titulo = "SAVE PROGRESS";
    Vector2 titleSize = MeasureTextEx(font, titulo, 42.0f, 1.5f);
    DrawTextEx(font, titulo, (Vector2){ (SCREEN_WIDTH / 2.0f) - (titleSize.x / 2.0f), 40.0f }, 42.0f, 1.5f, SKYBLUE);
    
    const char *sub = "Choose a manual slot. Autosave is separate and cannot be overwritten here.";
    Vector2 subSize = MeasureTextEx(font, sub, 18.0f, 1.0f);
    DrawTextEx(font, sub, (Vector2){ (SCREEN_WIDTH / 2.0f) - (subSize.x / 2.0f), 90.0f }, 18.0f, 1.0f, GRAY);

    // 3 Slots Cards
    for (int i = 0; i < MANUAL_SAVE_SLOTS; i++)
    {
        float cardX = 80.0f + (float)i * 400.0f;
        Rectangle cardBounds = { cardX, 140, 320, 430 };

        bool hover = CheckCollisionPointRec(mouse, cardBounds);

        Color cardBg = Fade((Color){ 20, 18, 32, 255 }, 0.85f);
        Color borderCol = hover ? THEME_COLOR_MAIN : THEME_COLOR_BORDER;

        DrawRectangleRounded(cardBounds, 0.05f, 6, cardBg);
        DrawRectangleRoundedLines(cardBounds, 0.05f, 6, borderCol);

        // Screenshot Box (16:9 ratio, e.g. 280 x 158)
        Rectangle imgBounds = { cardX + 20, 160, 280, 158 };
        DrawRectangleRec(imgBounds, BLACK);

        if (slotTexturesLoaded[i])
        {
            DrawTexturePro(slotTextures[i],
                           (Rectangle){ 0, 0, (float)slotTextures[i].width, (float)slotTextures[i].height },
                           imgBounds, (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
        else
        {
            DrawRectangleLinesEx(imgBounds, 1.0f, GRAY);
            Vector2 textSz = MeasureTextEx(font, "NO PREVIEW", 18.0f, 1.0f);
            DrawTextEx(font, "NO PREVIEW",
                       (Vector2){ imgBounds.x + imgBounds.width / 2.0f - textSz.x / 2.0f, imgBounds.y + imgBounds.height / 2.0f - textSz.y / 2.0f },
                       18.0f, 1.0f, DARKGRAY);
        }

        // Metadados do Slot
        SaveSlotMeta meta = game->slotsMeta[i];

        // Rótulo do Slot
        if (i == 0)
        {
            DrawTextEx(font, "SLOT 1 (DEFAULT)", (Vector2){ cardX + 20, 335 }, 22.0f, 1.0f, GOLD);
        }
        else
        {
            DrawTextEx(font, TextFormat("SLOT %d", i + 1), (Vector2){ cardX + 20, 335 }, 22.0f, 1.0f, GOLD);
        }

        if (meta.exists)
        {
            DrawTextEx(font, TextFormat("Hero: %s", meta.name), (Vector2){ cardX + 20, 365 }, 18.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Level: Lvl %d", meta.level), (Vector2){ cardX + 20, 390 }, 18.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Wave: %d / 5", meta.wave), (Vector2){ cardX + 20, 415 }, 18.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Score: %d", meta.score), (Vector2){ cardX + 20, 440 }, 18.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Date: %s", meta.date), (Vector2){ cardX + 20, 470 }, 14.0f, 1.0f, GRAY);

            // Botão Apagar Save
            Rectangle deleteBounds = { cardX + 20, 495, 280, 35 };
            bool deleteHover = CheckCollisionPointRec(mouse, deleteBounds);
            Color delBg = deleteHover ? RED : Fade(RED, 0.3f);
            DrawRectangleRounded(deleteBounds, 0.25f, 6, delBg);
            DrawRectangleRoundedLines(deleteBounds, 0.25f, 6, RED);
            
            int delFontSize = 16;
            Vector2 delTextSz = MeasureTextEx(font, "DELETE SAVE", (float)delFontSize, 1.0f);
            DrawTextEx(font, "DELETE SAVE", (Vector2){ deleteBounds.x + deleteBounds.width/2.0f - delTextSz.x/2.0f, deleteBounds.y + deleteBounds.height/2.0f - delTextSz.y/2.0f }, (float)delFontSize, 1.0f, WHITE);

            // Hover do card como um todo (evitando desenhar se mouse está sobre o botão apagar)
            if (hover && !deleteHover)
            {
                DrawTextEx(font, "CLICK TO OVERWRITE", (Vector2){ cardX + 20, 545 }, 14.0f, 1.0f, RED);
            }
        }
        else
        {
            DrawTextEx(font, "EMPTY SLOT", (Vector2){ cardX + 20, 365 }, 20.0f, 1.0f, DARKGRAY);

            if (hover)
            {
                DrawTextEx(font, "CLICK TO SAVE GAME", (Vector2){ cardX + 20, 545 }, 14.0f, 1.0f, GREEN);
            }
        }
    }

    // Botão Voltar
    UIButton btnVoltar = { { 490, 640, 300, 50 }, "BACK", false, false };
    btnVoltar.hover = CheckCollisionPointRec(mouse, btnVoltar.bounds);
    DrawButton(btnVoltar, font, true);
}

void DrawTelaLoadSelect(GameState *game, Font font, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT])
{
    // Fundo escuro translúcido para revelar a run desfocada/limpa no fundo
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade((Color){ 10, 8, 22, 255 }, 0.75f));

    // Título
    const char *titulo = "LOAD GAME";
    Vector2 titleSize = MeasureTextEx(font, titulo, 42.0f, 1.5f);
    DrawTextEx(font, titulo, (Vector2){ (SCREEN_WIDTH / 2.0f) - (titleSize.x / 2.0f), 40.0f }, 42.0f, 1.5f, SKYBLUE);
    
    const char *sub = "Choose autosave or one of your manual slots";
    Vector2 subSize = MeasureTextEx(font, sub, 18.0f, 1.0f);
    DrawTextEx(font, sub, (Vector2){ (SCREEN_WIDTH / 2.0f) - (subSize.x / 2.0f), 90.0f }, 18.0f, 1.0f, GRAY);

    int autoIdx = AUTO_SAVE_SLOT - 1;
    SaveSlotMeta autoMeta = game->slotsMeta[autoIdx];
    Rectangle autoBounds = { 80, 118, 1120, 112 };
    bool autoHover = CheckCollisionPointRec(mouse, autoBounds);
    Color autoBorder = (autoHover && autoMeta.exists) ? THEME_COLOR_MAIN : GOLD;
    DrawRectangleRounded(autoBounds, 0.05f, 6, Fade((Color){ 20, 18, 32, 255 }, autoMeta.exists ? 0.88f : 0.45f));
    DrawRectangleRoundedLines(autoBounds, 0.05f, 6, Fade(autoBorder, autoMeta.exists ? 0.95f : 0.45f));
    DrawTextEx(font, "SAVE AUTOMATICO", (Vector2){ autoBounds.x + 24, autoBounds.y + 16 }, 24.0f, 1.0f, GOLD);
    DrawTextEx(font, "Criado automaticamente durante o gameplay. Nao ocupa slots manuais.",
               (Vector2){ autoBounds.x + 24, autoBounds.y + 48 }, 16.0f, 1.0f, Fade(WHITE, 0.72f));
    if (autoMeta.exists)
    {
        DrawTextEx(font, TextFormat("Hero: %s   Lvl %d   Wave %d/5   Score %d",
                   autoMeta.name, autoMeta.level, autoMeta.wave, autoMeta.score),
                   (Vector2){ autoBounds.x + 24, autoBounds.y + 76 }, 16.0f, 1.0f, WHITE);
        DrawTextEx(font, TextFormat("Date: %s", autoMeta.date),
                   (Vector2){ autoBounds.x + 790, autoBounds.y + 76 }, 14.0f, 1.0f, GRAY);
    }
    else
    {
        DrawTextEx(font, "Nenhum autosave encontrado ainda.", (Vector2){ autoBounds.x + 24, autoBounds.y + 76 }, 16.0f, 1.0f, DARKGRAY);
    }

    // 3 Slots Cards manuais
    for (int i = 0; i < MANUAL_SAVE_SLOTS; i++)
    {
        float cardX = 80.0f + (float)i * 400.0f;
        Rectangle cardBounds = { cardX, 260, 320, 300 };

        bool hover = CheckCollisionPointRec(mouse, cardBounds);
        SaveSlotMeta meta = game->slotsMeta[i];

        Color cardBg = Fade((Color){ 20, 18, 32, 255 }, 0.85f);
        Color borderCol = (hover && meta.exists) ? THEME_COLOR_MAIN : THEME_COLOR_BORDER;

        if (!meta.exists)
        {
            borderCol = Fade(borderCol, 0.4f);
            cardBg = Fade(cardBg, 0.5f);
        }

        DrawRectangleRounded(cardBounds, 0.05f, 6, cardBg);
        DrawRectangleRoundedLines(cardBounds, 0.05f, 6, borderCol);

        // Screenshot Box (16:9 ratio, e.g. 280 x 158)
        Rectangle imgBounds = { cardX + 20, 278, 280, 118 };
        DrawRectangleRec(imgBounds, BLACK);

        if (slotTexturesLoaded[i])
        {
            DrawTexturePro(slotTextures[i],
                           (Rectangle){ 0, 0, (float)slotTextures[i].width, (float)slotTextures[i].height },
                           imgBounds, (Vector2){ 0, 0 }, 0.0f, WHITE);
        }
        else
        {
            DrawRectangleLinesEx(imgBounds, 1.0f, DARKGRAY);
            Vector2 textSz = MeasureTextEx(font, "NO PREVIEW", 18.0f, 1.0f);
            DrawTextEx(font, "NO PREVIEW",
                       (Vector2){ imgBounds.x + imgBounds.width / 2.0f - textSz.x / 2.0f, imgBounds.y + imgBounds.height / 2.0f - textSz.y / 2.0f },
                       18.0f, 1.0f, DARKGRAY);
        }

        // Rótulo do Slot
        if (i == 0)
        {
            DrawTextEx(font, "SLOT 1 (DEFAULT)", (Vector2){ cardX + 20, 410 }, 20.0f, 1.0f, GOLD);
        }
        else
        {
            DrawTextEx(font, TextFormat("SLOT %d", i + 1), (Vector2){ cardX + 20, 410 }, 20.0f, 1.0f, GOLD);
        }

        if (meta.exists)
        {
            DrawTextEx(font, TextFormat("Hero: %s", meta.name), (Vector2){ cardX + 20, 438 }, 16.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Level: Lvl %d", meta.level), (Vector2){ cardX + 20, 460 }, 16.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Wave: %d / 5", meta.wave), (Vector2){ cardX + 20, 482 }, 16.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Score: %d", meta.score), (Vector2){ cardX + 20, 504 }, 16.0f, 1.0f, WHITE);
            DrawTextEx(font, TextFormat("Date: %s", meta.date), (Vector2){ cardX + 20, 526 }, 13.0f, 1.0f, GRAY);

            // Botão Apagar Save
            Rectangle deleteBounds = { cardX + 20, 562, 280, 30 };
            bool deleteHover = CheckCollisionPointRec(mouse, deleteBounds);
            Color delBg = deleteHover ? RED : Fade(RED, 0.3f);
            DrawRectangleRounded(deleteBounds, 0.25f, 6, delBg);
            DrawRectangleRoundedLines(deleteBounds, 0.25f, 6, RED);
            
            int delFontSize = 16;
            Vector2 delTextSz = MeasureTextEx(font, "DELETE SAVE", (float)delFontSize, 1.0f);
            DrawTextEx(font, "DELETE SAVE", (Vector2){ deleteBounds.x + deleteBounds.width/2.0f - delTextSz.x/2.0f, deleteBounds.y + deleteBounds.height/2.0f - delTextSz.y/2.0f }, (float)delFontSize, 1.0f, WHITE);

            // Hover do card como um todo
            if (hover && !deleteHover)
            {
                DrawTextEx(font, "CLICK TO LOAD", (Vector2){ cardX + 20, 598 }, 13.0f, 1.0f, SKYBLUE);
            }
        }
        else
        {
            DrawTextEx(font, "EMPTY SLOT", (Vector2){ cardX + 20, 438 }, 18.0f, 1.0f, DARKGRAY);
        }
    }

    // Botão Voltar
    UIButton btnVoltar = { { 490, 650, 300, 50 }, "BACK", false, false };
    btnVoltar.hover = CheckCollisionPointRec(mouse, btnVoltar.bounds);
    DrawButton(btnVoltar, font, true);
}






// 10. TELA: SETTINGS
// ============================================================================
UIButton settingsBtnVoltar = { { 490, 600, 300, 50 }, "VOLTAR", false, false };

static void DrawSettingsVolumeSlider(Font font, const char *label, Rectangle track, float value)
{
    value = Clamp(value, 0.0f, 1.0f);
    Vector2 labelSz = MeasureTextEx(font, label, 20.0f, 1.0f);
    DrawTextEx(font, label, (Vector2){ track.x - labelSz.x - 28.0f, track.y - 6.0f }, 20.0f, 1.0f, WHITE);

    Rectangle hit = { track.x - 14.0f, track.y - 14.0f, track.width + 28.0f, track.height + 28.0f };
    bool hover = CheckCollisionPointRec(g_virtualMouse, hit);
    DrawRectangleRounded((Rectangle){ track.x - 2, track.y - 2, track.width + 4, track.height + 4 },
                         0.8f, 6, Fade((Color){ 2, 8, 12, 255 }, 0.72f));
    DrawRectangleRounded(track, 0.8f, 6, Fade(BLACK, 0.62f));
    DrawRectangleRounded((Rectangle){ track.x, track.y, track.width * value, track.height },
                         0.8f, 6, THEME_COLOR_MAIN);
    DrawRectangleRoundedLines(track, 0.8f, 6,
                              Fade(hover ? THEME_COLOR_MAIN : THEME_COLOR_BORDER, 0.9f));

    float radius = 15.0f;
    float knobX = track.x + track.width * value;
    if (knobX < track.x + radius) knobX = track.x + radius;
    if (knobX > track.x + track.width - radius) knobX = track.x + track.width - radius;
    float knobY = track.y + track.height * 0.5f;
    float t = (float)GetTime();
    Color virusCol = hover ? (Color){ 120, 255, 210, 255 } : (Color){ 80, 220, 190, 255 };
    DrawCircleV((Vector2){ knobX, knobY }, radius + 2.0f,
                Fade(THEME_COLOR_MAIN, 0.24f + (hover ? 0.18f : 0.0f)));
    DrawMenuVirus((Vector2){ knobX, knobY }, radius * (0.82f + 0.08f * sinf(t * 4.0f)),
                  t * 90.0f, virusCol);
    DrawCircleV((Vector2){ knobX, knobY }, 4.0f, Fade(WHITE, 0.88f));

    DrawTextEx(font, TextFormat("%d%%", (int)(value * 100.0f + 0.5f)),
               (Vector2){ track.x + track.width + 16.0f, track.y - 5.0f },
               20.0f, 1.0f, WHITE);
}

void DrawTelaSettings(GameState *game, Font font)
{
    DrawThemedBackground(SCREEN_SETTINGS, (float)GetTime(), game->screenAnim / 0.4f);

    const char *title = "CONFIGURACOES";
    Vector2 titleSz = MeasureTextEx(font, title, 42.0f, 1.5f);
    DrawTextEx(font, title, (Vector2){ SCREEN_WIDTH / 2.0f - titleSz.x / 2.0f, 60 }, 42.0f, 1.5f, SKYBLUE);

    DrawSciFiBox((Rectangle){ 320, 178, 640, 290 }, THEME_COLOR_MAIN);

    DrawTextEx(font, "AUDIO", (Vector2){ 395, 210 }, 28.0f, 1.0f, YELLOW);
    DrawLine(395, 248, 885, 248, Fade(YELLOW, 0.5f));

    DrawSettingsVolumeSlider(font, "MUSICA", SettingsMusicVolumeTrack(), game->musicVolume);
    DrawSettingsVolumeSlider(font, "EFEITOS SONOROS", SettingsSfxVolumeTrack(), game->sfxVolume);

    DrawTextEx(font, "Audio salvo automaticamente ao voltar.", (Vector2){ 395, 415 }, 14.0f, 1.0f, GRAY);

    DrawButton(settingsBtnVoltar, font, true);
}



// ============================================================================
// 11. TELA: LOADING (CARREGAMENTO)
// ============================================================================
static const char *loadingTips[] = {
    "DICA DE SAUDE: A vacina contra a gripe (Influenza) ajuda a prevenir complicacoes graves e deve ser tomada anualmente na UBS.",
    "DICA DE SAUDE: Para combater a Dengue, permita a entrada dos agentes da Vigilancia Ambiental em sua residencia para inspecao.",
    "DICA DE SAUDE: O Distrito Federal possui diversas Unidades Basicas de Saude (UBS) que oferecem atendimento e vacinacao gratuitos.",
    "DICA DE SAUDE: Lavar as maos frequentemente com agua e sabao e uma das maneiras mais eficazes de prevenir a transmissao de patogenos.",
    "DICA DE SAUDE: Informe a Vigilancia Ambiental sobre lotes vagos com acumulo de lixo ou agua parada na sua regiao administrativa.",
    "DICA DE SAUDE: Nao se automedique com antibioticos. O uso incorreto cria superbacterias resistentes, como a KPC, comuns em hospitais.",
    "DICA DE SAUDE: Use antibioticos apenas com prescricao e complete todo o tratamento, mesmo que os sintomas melhorem antes.",
    "DICA DE SAUDE: Mascaras e distanciamento reduzem a transmissao de doencas respiratorias como a gripe (Influenza).",
    "CURIOSIDADE: Bacteriofagos sao virus que infectam bacterias. Vacinas treinam o sistema imune contra virus de RNA, como dengue e influenza.",
    "CURIOSIDADE: O capsideo e a capa proteica que protege o material genetico do virus. Neutraliza-lo e essencial para conter a infeccao."
};

int GetLoadingTipCount(void)
{
    return (int)(sizeof(loadingTips) / sizeof(loadingTips[0]));
}

const char *GetLoadingTipText(int index)
{
    int count = GetLoadingTipCount();
    if (index < 0 || index >= count) index = 0;
    return count > 0 ? loadingTips[index] : "CARREGANDO...";
}

void DrawTelaLoading(GameState *game, Font font)
{
    // Transição tutorial -> gameplay: a animação de injeção acontece NA CENA DA
    // SERINGA (êmbolo empurrando — cutscene do tutorial em map_seringa.c). Aqui a
    // tela de loading fica LIMPA: removidas a antiga "cápsula em Y comprimida
    // entre placas cinzas", o tremor e o flash branco. fxActive apenas ajusta o
    // texto do título.
    bool fxActive = game->syringeTransitionFX;

    // Fundo: MESMA estética do menu principal (seringas destruindo patógenos),
    // com fade de entrada suave para não dar flash preto.
    float entry = game->loadingTimer / 0.35f; if (entry > 1.0f) entry = 1.0f;
    DrawMenuFXBackground((float)GetTime(), entry);
    // Escurecimento da faixa central/inferior para legibilidade de texto.
    DrawRectangleGradientV(0, 180, SCREEN_WIDTH, 220, Fade(BLACK, 0.0f), Fade(BLACK, 0.45f));
    DrawRectangleGradientV(0, 400, SCREEN_WIDTH, SCREEN_HEIGHT - 400, Fade(BLACK, 0.45f), Fade(BLACK, 0.6f));

    // Partículas decorativas de tecido por cima do fundo (muito sutis).
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            float s = game->particles[i].size;
            DrawRectangleV((Vector2){ game->particles[i].position.x - s, game->particles[i].position.y - s }, (Vector2){ s * 2.0f, s * 2.0f }, Fade(game->particles[i].color, game->particles[i].lifeTime / game->particles[i].maxLifeTime * 0.15f));
        }
    }

    // Título: fonte de identidade do jogo, com brilho, contorno (contraste sobre
    // o fundo animado), leve oscilação e reticências animadas.
    float ttime = (float)GetTime();
    const char *base = fxActive ? "INJETANDO NA CORRENTE SANGUINEA"
                                : "CARREGANDO SISTEMA IMUNOLOGICO";
    int dots = 1 + ((int)(ttime * 2.0f)) % 3;            // 1..3 reticências
    char titulo[96];
    snprintf(titulo, sizeof(titulo), "%s%.*s", base, dots, "...");
    float tfs = 30.0f;
    Vector2 titleSz = MeasureTextEx(font, titulo, tfs, 1.5f);
    float tbob = sinf(ttime * 2.2f) * 3.0f;
    Vector2 tpos = { (SCREEN_WIDTH / 2.0f) - (titleSz.x / 2.0f), 232.0f + tbob };
    Color tAccent = fxActive ? (Color){ 255, 120, 120, 255 } : THEME_COLOR_MAIN;
    // brilho
    DrawTextEx(font, titulo, (Vector2){ tpos.x, tpos.y }, tfs, 1.5f, Fade(tAccent, 0.18f));
    // contorno escuro
    DrawTextEx(font, titulo, (Vector2){ tpos.x + 2, tpos.y + 2 }, tfs, 1.5f, Fade(BLACK, 0.6f));
    DrawTextEx(font, titulo, tpos, tfs, 1.5f, tAccent);

    // Barra de progresso (Fundo)
    Rectangle progressBg = { 340, 310, 600, 30 };
    DrawRectangleRounded(progressBg, 0.4f, 6, Fade(BLACK, 0.6f));
    DrawRectangleRoundedLines(progressBg, 0.4f, 6, THEME_COLOR_BORDER);

    // Barra de progresso (Preenchimento)
    float pct = game->loadingDuration > 0.0f ? (game->loadingTimer / game->loadingDuration) : 1.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;
    Rectangle progressFill = { 340, 310, 600 * pct, 30 };
    if (pct > 0.02f)
    {
        DrawRectangleRounded(progressFill, 0.4f, 6, THEME_COLOR_MAIN);
    }

    // Porcentagem
    char pctText[16];
    sprintf(pctText, "%d%%", (int)(pct * 100));
    Vector2 pctSz = MeasureTextEx(font, pctText, 18.0f, 1.0f);
    DrawTextEx(font, pctText, (Vector2){ (SCREEN_WIDTH / 2.0f) - (pctSz.x / 2.0f), 316.0f }, 18.0f, 1.0f, BLACK);

    // Painel da dica educativa (Sci-fi Box)
    Rectangle tipBox = { 140, 480, 1000, 140 };
    DrawSciFiBox(tipBox, (Color){ 0, 200, 100, 255 });

    // Título da dica
    const char *tipTitle = "CONSELHO DE SAUDE PUBLICA DO DF";
    Vector2 tipTitleSz = MeasureTextEx(font, tipTitle, 16.0f, 1.0f);
    DrawTextEx(font, tipTitle, (Vector2){ (SCREEN_WIDTH / 2.0f) - (tipTitleSz.x / 2.0f), 500.0f }, 16.0f, 1.0f, (Color){ 0, 220, 120, 255 });

    // Texto da dica (com wrapping em 2 linhas)
    const char *tipText = GetLoadingTipText(game->loadingTip);
    
    char line1[256] = {0};
    char line2[256] = {0};
    int len = (int)strlen(tipText);
    int splitIdx = len / 2;
    // Encontra o espaço mais próximo do meio
    while (splitIdx > 0 && tipText[splitIdx] != ' ') {
        splitIdx--;
    }
    // Cópia SEMPRE limitada ao tamanho do buffer (robusto a dicas longas).
    if (splitIdx > 0 && splitIdx < len) {
        int n1 = splitIdx;
        if (n1 > (int)sizeof(line1) - 1) n1 = (int)sizeof(line1) - 1;
        memcpy(line1, tipText, (size_t)n1);
        line1[n1] = '\0';
        snprintf(line2, sizeof(line2), "%s", tipText + splitIdx + 1);
    } else {
        snprintf(line1, sizeof(line1), "%s", tipText);
    }

    Vector2 line1Sz = MeasureTextEx(font, line1, 15.0f, 1.0f);
    DrawTextEx(font, line1, (Vector2){ (SCREEN_WIDTH / 2.0f) - (line1Sz.x / 2.0f), 535.0f }, 15.0f, 1.0f, WHITE);

    if (line2[0] != '\0') {
        Vector2 line2Sz = MeasureTextEx(font, line2, 15.0f, 1.0f);
        DrawTextEx(font, line2, (Vector2){ (SCREEN_WIDTH / 2.0f) - (line2Sz.x / 2.0f), 565.0f }, 15.0f, 1.0f, WHITE);
    }

}
