#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include "../../include/asset_manager.h"
#include "../../include/sprite_manager.h"
#include "raymath.h"
#include "../../Assets/Maps/map_seringa.h"
#include "../../Assets/Maps/map_body.h"
#include "../../Assets/@models/player_model.h"
#include "../../Assets/@models/enemy_model.h"
#include "../../Assets/@models/doctor_model.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "rlgl.h"

// DrawSciFiBox definition will be moved to telas.c but made non-static, so we declare it here or in telas.h
extern void DrawSciFiBox(Rectangle r, Color col);

void DrawStatusIcons(Vector2 pos, bool poisoned, bool slowed, float time)
{
    float offsetY = 40.0f + sinf(time * 5.0f) * 5.0f;
    Vector2 iconPos = { pos.x, pos.y - offsetY };
    
    int count = 0;
    if (poisoned) count++;
    if (slowed) count++;
    
    if (count == 0) return;
    
    float startX = iconPos.x - ((count - 1) * 12.0f);
    
    int current = 0;
    if (poisoned) {
        Vector2 p = { startX + current * 24.0f, iconPos.y };
        DrawCircleV(p, 8.0f, Fade(PURPLE, 0.8f));
        DrawCircleV(p, 6.0f, Fade(MAGENTA, 0.9f));
        DrawRectangle(p.x - 2, p.y - 4, 4, 8, WHITE);
        DrawRectangle(p.x - 2, p.y + 6, 4, 2, WHITE); // gota shape approximation
        current++;
    }
    if (slowed) {
        Vector2 p = { startX + current * 24.0f, iconPos.y };
        DrawCircleV(p, 8.0f, Fade(BLUE, 0.8f));
        DrawCircleV(p, 6.0f, Fade(SKYBLUE, 0.9f));
        DrawLineEx((Vector2){p.x - 4, p.y}, (Vector2){p.x + 4, p.y}, 2.0f, WHITE);
        current++;
    }
}

// ============================================================================
// AUXILIAR: DESENHAR O JOGADOR DE ACORDO COM A SKIN ATUAL
// ============================================================================
void DrawPlayerHero(GameState *game, Vector2 pPos, float playerSize)
{
    // Rastro (Trail Render)
    rlBegin(RL_QUADS);
    for (int i = 0; i < 9; i++) {
        int idx1 = (game->player.trailIndex + i) % 10;
        int idx2 = (game->player.trailIndex + i + 1) % 10;
        Vector2 p1 = game->player.trail[idx1];
        Vector2 p2 = game->player.trail[idx2];
        
        float t1 = (float)i / 9.0f;
        float t2 = (float)(i + 1) / 9.0f;
        
        float thick1 = 2.0f + 18.0f * t1;
        float thick2 = 2.0f + 18.0f * t2;
        
        Color c1 = Fade((Color){ 0, 229, 255, 255 }, t1 * 0.5f);
        Color c2 = Fade((Color){ 0, 229, 255, 255 }, t2 * 0.5f);
        
        Vector2 dir = Vector2Subtract(p2, p1);
        if (Vector2LengthSqr(dir) > 0.01f) {
            dir = Vector2Normalize(dir);
            Vector2 normal = { -dir.y, dir.x };
            
            rlColor4ub(c1.r, c1.g, c1.b, c1.a);
            rlVertex2f(p1.x + normal.x * thick1, p1.y + normal.y * thick1);
            rlVertex2f(p1.x - normal.x * thick1, p1.y - normal.y * thick1);
            
            rlColor4ub(c2.r, c2.g, c2.b, c2.a);
            rlVertex2f(p2.x - normal.x * thick2, p2.y - normal.y * thick2);
            rlVertex2f(p2.x + normal.x * thick2, p2.y + normal.y * thick2);
        }
    }
    rlEnd();

    // DISTANCIAMENTO SOCIAL: aura visível ao redor do herói que repele patógenos.
    if (game->player.distancingTimer > 0.0f)
    {
        float t = (float)GetTime();
        float r = 175.0f + sinf(t * 4.0f) * 6.0f;
        Color aura = (Color){ 120, 255, 200, 255 };
        DrawCircleV(game->player.position, r, Fade(aura, 0.06f));
        DrawCircleLines((int)game->player.position.x, (int)game->player.position.y, r, Fade(aura, 0.45f));
        DrawCircleLines((int)game->player.position.x, (int)game->player.position.y, r * 0.96f, Fade(aura, 0.2f));
    }
    // MÁSCARA HOSPITALAR: anel protetor sutil indicando dano reduzido.
    if (game->player.maskTimer > 0.0f)
    {
        DrawCircleLines((int)game->player.position.x, (int)game->player.position.y, 46.0f,
                        Fade((Color){ 120, 220, 255, 255 }, 0.5f));
    }
    if (game->player.shieldTimer > 0.0f)
    {
        float t = (float)GetTime();
        float r = 54.0f + sinf(t * 5.0f) * 4.0f;
        Color c = (game->player.supremeTimer > 0.0f) ? (Color){ 255, 220, 80, 255 } : (Color){ 90, 200, 255, 255 };
        DrawCircleV(game->player.position, r, Fade(c, 0.08f));
        DrawCircleLines((int)game->player.position.x, (int)game->player.position.y, r, Fade(c, 0.65f));
        DrawCircleLines((int)game->player.position.x, (int)game->player.position.y, r * 0.84f, Fade(c, 0.30f));
    }

    bool isBoosted = (game->player.attackBoostTimer > 0.0f);
    Color pCol = isBoosted ? GOLD : THEME_COLOR_MAIN;

    // Pipeline de sprites (Fase 1): se houver um PNG do Anticorpo para a skin
    // atual em Assets/Sprites/Player/, desenha-o; senão, mantém o desenho
    // procedural existente (player_model.c). Assim o jogo roda igual sem PNGs.
    SpriteID playerSpr = SPR_PLAYER_DEFAULT;
    if (game->player.skinId == 1) playerSpr = SPR_PLAYER_MEDIC;
    else if (game->player.skinId == 2) playerSpr = SPR_PLAYER_INFECTED;

    if (SpriteAvailable(playerSpr))
    {
        DrawSpriteCentered(playerSpr, game->player.position, (Vector2){ 64.0f, 64.0f }, 0.0f, pCol);
    }
    else
    {
        // Renderiza a forma base do Herói (procedural)
        // Passa o tamanho maior (60.0f em vez de playerSize) para mais detalhes
        DrawPlayerModel(&game->player, 60.0f, pCol, GetTime(), game->slashAnimTimer);
    }
}

// Converte uma posição do mundo para um ponto dentro do minimapa CORPORAL,
// enquadrando a bounding box do corpo (MapBody_WorldBounds) no retângulo `r`.
// Assim os marcadores ficam sempre alinhados à silhueta desenhada no HUD.
static Vector2 WorldToMini(Vector2 world, Rectangle bounds, Rectangle r)
{
    float u = Clamp((world.x - bounds.x) / bounds.width, 0.0f, 1.0f);
    float v = Clamp((world.y - bounds.y) / bounds.height, 0.0f, 1.0f);
    return (Vector2){ r.x + u * r.width, r.y + v * r.height };
}

// Minimapa corporal (biossensor): silhueta do corpo gerada UMA vez a partir da
// occupancy grid de colisão (não relê a imagem 1254x1254 por frame). Textura de
// vida útil única do app; reconstruída se invalidada.
static Texture2D gMiniBodyTex = { 0 };
static bool gMiniBodyReady = false;
static void BuildBodyMinimap(void)
{
    Rectangle b = MapBody_WorldBounds();
    int TW = 64;
    int TH = (int)(TW * (b.height / b.width) + 0.5f);
    if (TH < 8) TH = 8;
    if (TH > 256) TH = 256;
    Image img = GenImageColor(TW, TH, BLANK);
    for (int y = 0; y < TH; y++)
        for (int x = 0; x < TW; x++)
        {
            float wx = b.x + (x + 0.5f) / TW * b.width;
            float wy = b.y + (y + 0.5f) / TH * b.height;
            if (!MapBody_Contains((Vector2){ wx, wy })) continue;
            // realça a membrana (borda) testando os 4 vizinhos no espaço do mundo
            float dx = b.width / TW, dy = b.height / TH;
            bool edge = !MapBody_Contains((Vector2){ wx - dx, wy }) || !MapBody_Contains((Vector2){ wx + dx, wy }) ||
                        !MapBody_Contains((Vector2){ wx, wy - dy }) || !MapBody_Contains((Vector2){ wx, wy + dy });
            ImageDrawPixel(&img, x, y, edge ? (Color){ 196, 96, 110, 255 } : (Color){ 116, 44, 56, 224 });
        }
    if (gMiniBodyReady && gMiniBodyTex.id != 0) UnloadTexture(gMiniBodyTex);
    gMiniBodyTex = LoadTextureFromImage(img);
    UnloadImage(img);
    gMiniBodyReady = true;
}

// Libera a textura do biossensor no encerramento (evita vazamento de GPU). O
// loop principal chama isto antes de fechar a janela.
void UnloadGameplayResources(void)
{
    if (gMiniBodyReady && gMiniBodyTex.id != 0)
    {
        UnloadTexture(gMiniBodyTex);
        gMiniBodyTex = (Texture2D){ 0 };
        gMiniBodyReady = false;
    }
}

void DrawHUD(GameState *game, Font font)
{
    // A. BARRA DE STATUS DO JOGADOR (HP & XP)
    DrawSciFiBox((Rectangle){ 20, 20, 360, 95 }, THEME_COLOR_MAIN);

    // Informações básicas (Nome à esquerda, Nível à direita)
    DrawTextEx(font, game->player.name, (Vector2){ 35, 28 }, 18.0f, 1.0f, THEME_COLOR_MAIN);
    
    char lvlStr[16];
    sprintf(lvlStr, "LV. %d", game->player.level);
    Vector2 lvlSize = MeasureTextEx(font, lvlStr, 14.0f, 1.0f);
    DrawTextEx(font, lvlStr, (Vector2){ 365.0f - lvlSize.x, 30.0f }, 14.0f, 1.0f, GOLD);

    // Barra de HP
    DrawRectangleRounded((Rectangle){ 35, 54, 220, 10 }, 0.5f, 4, (Color){ 45, 10, 15, 255 });
    float hpPercent = (float)game->player.hp / game->player.maxHp;
    if (hpPercent > 0.0f)
    {
        Color hpColor = (hpPercent > 0.45f) ? THEME_COLOR_MAIN : (hpPercent > 0.2f) ? ORANGE : RED;
        DrawRectangleRounded((Rectangle){ 35, 54, 220.0f * hpPercent, 10 }, 0.5f, 4, hpColor);
    }
    
    char hpStr[32];
    sprintf(hpStr, "%d/%d HP", game->player.hp, game->player.maxHp);
    Vector2 hpSize = MeasureTextEx(font, hpStr, 13.0f, 1.0f);
    DrawTextEx(font, hpStr, (Vector2){ 365.0f - hpSize.x, 52.0f }, 13.0f, 1.0f, WHITE);

    // Barra de XP
    DrawTextEx(font, "XP", (Vector2){ 35, 73 }, 11.0f, 1.0f, (Color){ 224, 64, 251, 255 });
    DrawRectangleRounded((Rectangle){ 60, 75, 305, 6 }, 0.5f, 4, BLACK);
    float xpPercent = (float)game->player.xp / game->player.xpNeeded;
    if (xpPercent > 0.0f)
    {
        DrawRectangleRounded((Rectangle){ 60, 75, 305.0f * xpPercent, 6 }, 0.5f, 4, (Color){ 224, 64, 251, 255 });
    }

    // B. PAINEL DE ONDA / HORDA
    Rectangle waveBox = { 490, 20, 300, 60 };
    DrawSciFiBox(waveBox, THEME_COLOR_MAIN);

    const char *waveTxt = TextFormat("ONDA DE INFECÇÃO: %d / 5", game->wave);
    Vector2 waveTxtSize = MeasureTextEx(font, waveTxt, 16.0f, 1.0f);
    DrawTextEx(font, waveTxt, (Vector2){ 640.0f - waveTxtSize.x / 2.0f, 28.0f }, 16.0f, 1.0f, GOLD);

    const char *remTxt = TextFormat("Patógenos Ativos: %d", game->enemiesRemaining);
    Vector2 remTxtSize = MeasureTextEx(font, remTxt, 13.0f, 1.0f);
    DrawTextEx(font, remTxt, (Vector2){ 640.0f - remTxtSize.x / 2.0f, 48.0f }, 13.0f, 1.0f, WHITE);

    // B2. ÓRGÃO/REGIÃO E DOENÇA EM FOCO (reforço educativo do mapa do corpo)
    {
        BodyRegion region = MapBody_GetFocusRegion(game->currentWorld, game->wave);
        const char *worldTxt = (game->currentWorld == WORLD_VIRUS) ? "MUNDO: VÍRUS" : "MUNDO: BACTÉRIAS";
        const char *focusTxt = TextFormat("%s  •  %s",
                                          MapBody_GetRegionLabel(region),
                                          MapBody_GetDiseaseLabel(game->currentWorld, game->wave));
        Rectangle orgBox = { 490, 84, 300, 38 };
        DrawSciFiBox(orgBox, THEME_COLOR_MAIN);
        Vector2 wSz = MeasureTextEx(font, worldTxt, 11.0f, 1.0f);
        DrawTextEx(font, worldTxt, (Vector2){ 640.0f - wSz.x / 2.0f, 88.0f }, 11.0f, 1.0f, GOLD);
        Vector2 fSz = MeasureTextEx(font, focusTxt, 11.0f, 1.0f);
        // Reduz a fonte se o texto exceder a largura do painel
        float fSize = (fSz.x > 286.0f) ? 10.0f : 11.0f;
        fSz = MeasureTextEx(font, focusTxt, fSize, 1.0f);
        DrawTextEx(font, focusTxt, (Vector2){ 640.0f - fSz.x / 2.0f, 104.0f }, fSize, 1.0f,
                   (Color){ 120, 220, 255, 255 });
    }

    // C. PONTUAÇÃO (SUPERIOR DIREITO)
    DrawSciFiBox((Rectangle){ 900, 20, 150, 55 }, THEME_COLOR_MAIN);
    DrawTextEx(font, "SCORE", (Vector2){ 915, 27 }, 12.0f, 1.0f, GRAY);
    DrawTextEx(font, TextFormat("%06d", game->player.score), (Vector2){ 915, 42 }, 20.0f, 1.0f, YELLOW);

    // C.2. BARRA DE VIDA DO CHEFE (fixa no topo, visível mesmo com o chefe fora da tela)
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (game->enemies[i].active && game->enemies[i].tier == TIER_3_BOSS && game->enemies[i].state != DEATH)
        {
            float bossPct = (float)game->enemies[i].hp / game->enemies[i].maxHp;
            if (bossPct < 0.0f) bossPct = 0.0f;

            // Posicionada abaixo da fileira superior do HUD (painel de status do
            // jogador termina em y=115) para não sobrepor nome/nível/onda.
            Rectangle barBg = { 340, 128, 600, 26 };
            DrawRectangleRounded(barBg, 0.4f, 6, Fade((Color){ 20, 0, 6, 255 }, 0.85f));
            Color bossCol = (bossPct > 0.66f) ? (Color){ 200, 40, 60, 255 }
                          : (bossPct > 0.33f) ? (Color){ 230, 60, 200, 255 }
                          : (Color){ 255, 90, 40, 255 };
            if (bossPct > 0.0f)
            {
                Rectangle barFill = { barBg.x, barBg.y, barBg.width * bossPct, barBg.height };
                DrawRectangleRounded(barFill, 0.4f, 6, bossCol);
            }
            DrawRectangleRoundedLines(barBg, 0.4f, 6, (Color){ 255, 80, 90, 255 });

            const char *bossName = BossDisplayName(game->currentWorld);
            Vector2 nSz = MeasureTextEx(font, bossName, 16.0f, 1.0f);
            DrawTextEx(font, bossName, (Vector2){ 640.0f - nSz.x / 2.0f, 108.0f }, 16.0f, 1.0f, (Color){ 255, 120, 130, 255 });
            const char *hpTxt = TextFormat("%d / %d", game->enemies[i].hp, game->enemies[i].maxHp);
            Vector2 hSz = MeasureTextEx(font, hpTxt, 13.0f, 1.0f);
            DrawTextEx(font, hpTxt, (Vector2){ 640.0f - hSz.x / 2.0f, 133.0f }, 13.0f, 1.0f, WHITE);

            // FASE 3: indicador do escudo e quantos Núcleos restam destruir
            if (game->bossShieldActive && CoresAlive(game) > 0)
            {
                const char *st = TextFormat("ESCUDO ATIVO — destrua %d NUCLEOS", CoresAlive(game));
                Vector2 stz = MeasureTextEx(font, st, 15.0f, 1.0f);
                DrawRectangleRounded((Rectangle){ 640.0f - stz.x / 2.0f - 12.0f, 160.0f, stz.x + 24.0f, 24.0f }, 0.4f, 6, Fade((Color){ 0, 20, 30, 255 }, 0.85f));
                DrawRectangleRoundedLines((Rectangle){ 640.0f - stz.x / 2.0f - 12.0f, 160.0f, stz.x + 24.0f, 24.0f }, 0.4f, 6, (Color){ 120, 230, 255, 255 });
                DrawTextEx(font, st, (Vector2){ 640.0f - stz.x / 2.0f, 164.0f }, 15.0f, 1.0f, (Color){ 150, 235, 255, 255 });
            }
            break;
        }
    }

    // INDICADOR DE DIFICULDADE + MODO ADMIN (discreto, canto inferior esquerdo)
    {
        Color dcol = (game->difficulty == DIFFICULTY_HARD) ? (Color){ 255, 90, 90, 255 }
                   : (game->difficulty == DIFFICULTY_EASY) ? (Color){ 120, 220, 140, 255 }
                   : (Color){ 0, 229, 255, 255 };
        DrawTextEx(font, TextFormat("DIFICULDADE: %s", DifficultyName(game->difficulty)),
                   (Vector2){ 20, SCREEN_HEIGHT - 26.0f }, 14.0f, 1.0f, Fade(dcol, 0.9f));
        if (game->adminMode)
        {
            DrawTextEx(font, "ADMIN MODE  [.] limpar fase  [H]cura [L]nivel [P]+SUS [K]so comuns [ ]/[ ]wave",
                       (Vector2){ 220, SCREEN_HEIGHT - 26.0f }, 13.0f, 1.0f, (Color){ 255, 210, 60, 255 });
        }
    }

    // D. INDICADORES VISUAIS DE BUFFS ATIVOS
    int buffCount = 0;
    
    if (game->player.speedTimer > 0.0f)
    {
        Rectangle rBuff = { 20, 130.0f + (float)buffCount * 32.0f, 190, 26 };
        DrawRectangleRec(rBuff, Fade((Color){ 10, 8, 22, 255 }, 0.75f));
        DrawRectangle((int)rBuff.x, (int)rBuff.y, 4, (int)rBuff.height, YELLOW);
        DrawRectangleLinesEx(rBuff, 1.0f, Fade(YELLOW, 0.25f));
        
        DrawTextEx(font, TextFormat("SPEED: %.1fs", game->player.speedTimer), 
                   (Vector2){ rBuff.x + 12, rBuff.y + 7 }, 12.0f, 1.0f, YELLOW);
        buffCount++;
    }

    if (game->player.shieldTimer > 0.0f)
    {
        Rectangle rBuff = { 20, 130.0f + (float)buffCount * 32.0f, 190, 26 };
        DrawRectangleRec(rBuff, Fade((Color){ 10, 8, 22, 255 }, 0.75f));
        DrawRectangle((int)rBuff.x, (int)rBuff.y, 4, (int)rBuff.height, SKYBLUE);
        DrawRectangleLinesEx(rBuff, 1.0f, Fade(SKYBLUE, 0.25f));
        
        DrawTextEx(font, TextFormat("SHIELD: %.1fs", game->player.shieldTimer), 
                   (Vector2){ rBuff.x + 12, rBuff.y + 7 }, 12.0f, 1.0f, SKYBLUE);
        buffCount++;
    }

    if (game->player.attackBoostTimer > 0.0f)
    {
        Rectangle rBuff = { 20, 130.0f + (float)buffCount * 32.0f, 190, 26 };
        DrawRectangleRec(rBuff, Fade((Color){ 10, 8, 22, 255 }, 0.75f));
        DrawRectangle((int)rBuff.x, (int)rBuff.y, 4, (int)rBuff.height, ORANGE);
        DrawRectangleLinesEx(rBuff, 1.0f, Fade(ORANGE, 0.25f));
        
        DrawTextEx(font, TextFormat("DAMAGE x2: %.1fs", game->player.attackBoostTimer), 
                   (Vector2){ rBuff.x + 12, rBuff.y + 7 }, 12.0f, 1.0f, ORANGE);
        buffCount++;
    }

    if (game->player.supremeTimer > 0.0f)
    {
        Rectangle rBuff = { 20, 130.0f + (float)buffCount * 32.0f, 190, 26 };
        DrawRectangleRec(rBuff, Fade((Color){ 18, 14, 6, 255 }, 0.82f));
        DrawRectangle((int)rBuff.x, (int)rBuff.y, 4, (int)rBuff.height, GOLD);
        DrawRectangleLinesEx(rBuff, 1.0f, Fade(GOLD, 0.35f));
        DrawTextEx(font, TextFormat("SUPREMO: %.1fs", game->player.supremeTimer),
                   (Vector2){ rBuff.x + 12, rBuff.y + 7 }, 12.0f, 1.0f, GOLD);
        buffCount++;
    }

    // ------------------------------------------------------------------------
    // HOTBAR DE ARMAS (centro-inferior): quatro slots. A Lâmina Bioelétrica é
    // variante do slot melee e alterna com a Espada-Seringa pela tecla 1.
    // ------------------------------------------------------------------------
    {
        const char *shortNames[WEAPON_SLOT_COUNT] = { "MELEE", "FUZIL", "MINAS RNA", "BFG" };
        float slotW = 156.0f, slotH = 54.0f, gap = 10.0f;
        float totalW = WEAPON_SLOT_COUNT * slotW + (WEAPON_SLOT_COUNT - 1) * gap;
        float startX = (SCREEN_WIDTH - totalW) / 2.0f;
        float y = SCREEN_HEIGHT - slotH - 16.0f;

        const char *weaponHint = "ARMAS (teclas 1-4): evolucoes por abates individuais";
        DrawTextEx(font, weaponHint, (Vector2){ startX, y - 20.0f }, 13.0f, 1.0f, Fade(WHITE, 0.7f));

        for (int s = 0; s < WEAPON_SLOT_COUNT; s++)
        {
            int weaponId = s + 1;
            int slot = s + 1;
            int evoId = WeaponEvolutionForSlot(slot);
            if (WeaponSlotForId(game->player.equippedWeapon) == slot &&
                game->player.equippedWeapon == evoId) weaponId = evoId;
            WeaponInfo wi = GetWeaponInfo(weaponId);
            bool unlocked = WeaponUnlocked(game, s + 1);
            bool current = (WeaponSlotForId(game->player.equippedWeapon) == slot);
            Rectangle r = { startX + s * (slotW + gap), y, slotW, slotH };

            Color bg = current ? Fade(wi.color, 0.22f) : Fade((Color){ 10, 8, 22, 255 }, 0.8f);
            DrawRectangleRounded(r, 0.18f, 6, bg);
            Color border = !unlocked ? Fade(GRAY, 0.4f) : current ? wi.color : Fade(wi.color, 0.5f);
            DrawRectangleRoundedLines(r, 0.18f, 6, border);
            if (current) DrawRectangleRoundedLines(r, 0.18f, 6, Fade(WHITE, 0.5f + 0.3f * sinf((float)GetTime() * 6.0f)));

            // Tecla
            DrawTextEx(font, TextFormat("%d", s + 1), (Vector2){ r.x + 8, r.y + 5 }, 20.0f, 1.0f,
                       unlocked ? wi.color : Fade(GRAY, 0.6f));
            // Nome curto
            DrawTextEx(font, shortNames[s], (Vector2){ r.x + 30, r.y + 8 }, 15.0f, 1.0f,
                       unlocked ? WHITE : Fade(GRAY, 0.7f));

            if (!unlocked)
            {
                const char *lockReq = TextFormat("Nivel %d", wi.unlockLevel);
                DrawTextEx(font, lockReq, (Vector2){ r.x + 30, r.y + 30 }, 13.0f, 1.0f, Fade(RED, 0.9f));
                // cadeado simples
                DrawRectangleRounded((Rectangle){ r.x + slotW - 26, r.y + 8, 16, 14 }, 0.4f, 4, Fade(GRAY, 0.8f));
                DrawRectangleLines((int)(r.x + slotW - 24), (int)(r.y + 4), 12, 8, Fade(GRAY, 0.8f));
            }
            else
            {
                // efeito especial resumido
                const char *tag = (s == 0)
                    ? ((game->player.equippedWeapon == WEAPON_BIOBLADE) ? "descarga anti-escudo" :
                       (WeaponUnlocked(game, evoId) ? "1 alterna lamina" : TextFormat("evolui em %d abates", WeaponKillsToEvolve(game, slot))))
                    : WeaponUnlocked(game, evoId) ? TextFormat("%d alterna evolucao", slot)
                    : TextFormat("evolui em %d abates", WeaponKillsToEvolve(game, slot));
                DrawTextEx(font, tag, (Vector2){ r.x + 30, r.y + 31 }, 12.0f, 1.0f, Fade(wi.color, 0.9f));

                // Barra de cooldown apenas na arma equipada
                if (current)
                {
                    float cdPct = 1.0f - (game->player.attackCooldown / (wi.cooldown <= 0.0f ? 1.0f : wi.cooldown));
                    if (cdPct < 0.0f) cdPct = 0.0f;
                    if (cdPct > 1.0f) cdPct = 1.0f;
                    DrawRectangle((int)r.x + 8, (int)(r.y + slotH - 7), (int)(slotW - 16), 4, Fade(BLACK, 0.6f));
                    DrawRectangle((int)r.x + 8, (int)(r.y + slotH - 7), (int)((slotW - 16) * cdPct), 4,
                                  (cdPct >= 1.0f) ? wi.color : Fade(wi.color, 0.5f));
                }
            }
        }

        // Poções de vida ao lado direito da hotbar
        Rectangle rPot = { startX + totalW + 16.0f, y, 120.0f, slotH };
        if (rPot.x + rPot.width <= SCREEN_WIDTH - 8.0f)
        {
            DrawRectangleRounded(rPot, 0.18f, 6, Fade((Color){ 10, 8, 22, 255 }, 0.8f));
            DrawRectangleRoundedLines(rPot, 0.18f, 6, Fade(GREEN, 0.6f));
            DrawTextEx(font, "[E] POCAO", (Vector2){ rPot.x + 12, rPot.y + 8 }, 14.0f, 1.0f, GREEN);
            DrawTextEx(font, TextFormat("x %d", game->player.healthPotions), (Vector2){ rPot.x + 12, rPot.y + 28 }, 16.0f, 1.0f, WHITE);
        }
    }

    // E. BIOSSENSOR CORPORAL — minimapa no formato do corpo humano
    if (!gMiniBodyReady) BuildBodyMinimap();
    Rectangle miBounds = MapBody_WorldBounds();
    float miH = 128.0f;
    float miW = miH * (miBounds.width / miBounds.height);
    Vector2 miCenter = { 1212.0f, 92.0f };
    Rectangle miRect  = { miCenter.x - miW * 0.5f, miCenter.y - miH * 0.5f, miW, miH };
    Rectangle miPanel = { miRect.x - 12.0f, miRect.y - 12.0f, miRect.width + 24.0f, miRect.height + 24.0f };

    DrawRectangleRounded(miPanel, 0.14f, 6, Fade((Color){ 10, 8, 22, 255 }, 0.72f));
    DrawRectangleRoundedLines(miPanel, 0.14f, 6, Fade(THEME_COLOR_MAIN, 0.32f));
    float cl = 7.0f;
    DrawLineEx((Vector2){ miPanel.x, miPanel.y }, (Vector2){ miPanel.x + cl, miPanel.y }, 1.5f, THEME_COLOR_MAIN);
    DrawLineEx((Vector2){ miPanel.x, miPanel.y }, (Vector2){ miPanel.x, miPanel.y + cl }, 1.5f, THEME_COLOR_MAIN);
    DrawLineEx((Vector2){ miPanel.x + miPanel.width, miPanel.y }, (Vector2){ miPanel.x + miPanel.width - cl, miPanel.y }, 1.5f, THEME_COLOR_MAIN);
    DrawLineEx((Vector2){ miPanel.x + miPanel.width, miPanel.y }, (Vector2){ miPanel.x + miPanel.width, miPanel.y + cl }, 1.5f, THEME_COLOR_MAIN);
    DrawLineEx((Vector2){ miPanel.x, miPanel.y + miPanel.height }, (Vector2){ miPanel.x + cl, miPanel.y + miPanel.height }, 1.5f, THEME_COLOR_MAIN);
    DrawLineEx((Vector2){ miPanel.x, miPanel.y + miPanel.height }, (Vector2){ miPanel.x, miPanel.y + miPanel.height - cl }, 1.5f, THEME_COLOR_MAIN);
    DrawLineEx((Vector2){ miPanel.x + miPanel.width, miPanel.y + miPanel.height }, (Vector2){ miPanel.x + miPanel.width - cl, miPanel.y + miPanel.height }, 1.5f, THEME_COLOR_MAIN);
    DrawLineEx((Vector2){ miPanel.x + miPanel.width, miPanel.y + miPanel.height }, (Vector2){ miPanel.x + miPanel.width, miPanel.y + miPanel.height - cl }, 1.5f, THEME_COLOR_MAIN);

    // Silhueta do corpo (textura única) + leve linha de varredura
    if (gMiniBodyReady && gMiniBodyTex.id != 0)
        DrawTexturePro(gMiniBodyTex, (Rectangle){ 0, 0, (float)gMiniBodyTex.width, (float)gMiniBodyTex.height },
                       miRect, (Vector2){ 0, 0 }, 0.0f, Fade(WHITE, 0.92f));
    float scan = miRect.y + (0.5f + 0.5f * sinf((float)GetTime() * 1.4f)) * miRect.height;
    DrawLineEx((Vector2){ miRect.x, scan }, (Vector2){ miRect.x + miRect.width, scan }, 1.0f, Fade(THEME_COLOR_MAIN, 0.18f));

    // Marcadores normalizados sobre a silhueta corporal
    for (int i = 0; i < MAX_POWERUPS; i++)
        if (game->powerUps[i].active)
            DrawCircleV(WorldToMini(game->powerUps[i].position, miBounds, miRect), 2.0f, YELLOW);

    for (int i = 0; i < MAX_CORES; i++)
        if (game->cores[i].active)
        {
            Vector2 d = WorldToMini(game->cores[i].position, miBounds, miRect);
            DrawCircleV(d, 3.0f, (Color){ 120, 230, 255, 255 });
            DrawCircleLines((int)d.x, (int)d.y, 5.0f, Fade((Color){ 120, 230, 255, 255 }, 0.6f));
        }

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!game->enemies[i].active) continue;
        Vector2 d = WorldToMini(game->enemies[i].position, miBounds, miRect);
        EnemyTier tier = game->enemies[i].tier;
        if (tier == TIER_3_BOSS || tier == TIER_MINIBOSS)
        {
            float pr = (tier == TIER_3_BOSS) ? 5.0f : 4.0f;
            Color bc = (tier == TIER_3_BOSS) ? (Color){ 255, 40, 60, 255 } : (Color){ 255, 140, 40, 255 };
            DrawCircleV(d, pr, bc);
            DrawCircleLines((int)d.x, (int)d.y, pr + 2.0f + sinf((float)GetTime() * 6.0f) * 1.2f, Fade(bc, 0.6f));
        }
        else
        {
            Color dotCol = (game->enemies[i].type == 2) ? MAROON : (game->enemies[i].state == AGGRO) ? RED : ORANGE;
            DrawCircleV(d, 2.0f, dotCol);
        }
    }

    Vector2 pdot = WorldToMini(game->player.position, miBounds, miRect);
    float pPulse = 3.0f + sinf((float)GetTime() * 6.0f) * 0.8f;
    DrawCircleV(pdot, pPulse, SKYBLUE);
    DrawCircleLines((int)pdot.x, (int)pdot.y, pPulse + 2.0f, Fade(SKYBLUE, 0.6f));

    Vector2 lblSz = MeasureTextEx(font, "BIOSSENSOR", 11.0f, 1.0f);
    DrawTextEx(font, "BIOSSENSOR",
               (Vector2){ miPanel.x + (miPanel.width - lblSz.x) * 0.5f, miPanel.y + miPanel.height + 4.0f },
               11.0f, 1.0f, Fade(THEME_COLOR_MAIN, 0.8f));
    
    if (game->saveLoaded)
    {
        DrawRectangleRounded((Rectangle){ 490, 670, 300, 30 }, 0.4f, 4, Fade(GREEN, 0.2f));
        DrawRectangleRoundedLines((Rectangle){ 490, 670, 300, 30 }, 0.4f, 4, GREEN);
        
        Vector2 textSz = MeasureTextEx(font, game->notificationMsg, 14.0f, 1.0f);
        DrawTextEx(font, game->notificationMsg, (Vector2){ 490.0f + 150.0f - textSz.x/2.0f, 678.0f }, 14.0f, 1.0f, GREEN);
        
        if (game->timeElapsed > 3.0f) game->saveLoaded = false;
    }

    // ------------------------------------------------------------------------
    // BANNER / TOAST central (onda, chefe, troca/desbloqueio de arma, level up)
    // ------------------------------------------------------------------------
    if (game->bannerTimer > 0.0f)
    {
        float t = game->bannerTimer / (game->bannerMax <= 0.0f ? 1.0f : game->bannerMax);
        // Aparece e some suavemente (alpha nas pontas)
        float alpha = 1.0f;
        if (t > 0.85f) alpha = (1.0f - t) / 0.15f;          // fade-in
        else if (t < 0.25f) alpha = t / 0.25f;               // fade-out
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 1.0f) alpha = 1.0f;

        bool hasSub = game->bannerSub[0] != '\0';
        float boxW = 640.0f, boxH = hasSub ? 78.0f : 52.0f;
        Rectangle box = { (SCREEN_WIDTH - boxW) / 2.0f, 132.0f, boxW, boxH };
        DrawRectangleRounded(box, 0.25f, 8, Fade((Color){ 8, 10, 16, 255 }, 0.86f * alpha));
        DrawRectangleRoundedLines(box, 0.25f, 8, Fade(game->bannerColor, alpha));
        DrawRectangle((int)box.x, (int)box.y, 5, (int)box.height, Fade(game->bannerColor, alpha));

        Vector2 mSz = MeasureTextEx(font, game->bannerMsg, 26.0f, 1.0f);
        DrawTextEx(font, game->bannerMsg, (Vector2){ SCREEN_WIDTH / 2.0f - mSz.x / 2.0f, box.y + 10.0f },
                   26.0f, 1.0f, Fade(game->bannerColor, alpha));
        if (hasSub)
        {
            Vector2 sSz = MeasureTextEx(font, game->bannerSub, 16.0f, 1.0f);
            DrawTextEx(font, game->bannerSub, (Vector2){ SCREEN_WIDTH / 2.0f - sSz.x / 2.0f, box.y + 46.0f },
                       16.0f, 1.0f, Fade(WHITE, alpha * 0.9f));
        }
    }
}

// ============================================================================
// DEBUG DE COLISÃO (somente modo admin, tecla F1) — overlay translúcido que
// mostra a área caminhável (verde), corpo sem margem (laranja), bloqueada
// (vermelho), a fronteira (amarelo) e o raio de colisão do jogador (ciano);
// no HUD, as coordenadas de mundo sob o cursor. NÃO aparece normalmente e NÃO
// afeta a jogabilidade (apenas desenho). Usa a MESMA fonte de colisão do jogo.
// ============================================================================
static void DrawCollisionDebugWorld(GameState *game)
{
    if (!(game->adminMode && game->debugCollision)) return;
    Rectangle b = MapBody_WorldBounds();
    const int STEP = 40;
    int x0 = (int)b.x - STEP * 2, y0 = (int)b.y - STEP * 2;
    int x1 = (int)(b.x + b.width) + STEP * 2, y1 = (int)(b.y + b.height) + STEP * 2;
    for (int y = y0; y <= y1; y += STEP)
        for (int x = x0; x <= x1; x += STEP)
        {
            Vector2 p = { (float)x, (float)y };
            bool walk = MapBody_ContainsWithMargin(p, BODY_PLAYER_RADIUS);
            bool inside = MapBody_Contains(p);
            Color col = walk ? (Color){ 40, 220, 90, 60 }
                      : inside ? (Color){ 255, 170, 40, 60 }   // corpo, margem insuficiente
                               : (Color){ 230, 40, 40, 36 };   // bloqueado (fora da silhueta)
            DrawRectangle(x - STEP / 2, y - STEP / 2, STEP, STEP, col);
            if (walk) // fronteira: célula caminhável tocando não-caminhável
            {
                bool edge = !MapBody_ContainsWithMargin((Vector2){ p.x + STEP, p.y }, BODY_PLAYER_RADIUS)
                         || !MapBody_ContainsWithMargin((Vector2){ p.x - STEP, p.y }, BODY_PLAYER_RADIUS)
                         || !MapBody_ContainsWithMargin((Vector2){ p.x, p.y + STEP }, BODY_PLAYER_RADIUS)
                         || !MapBody_ContainsWithMargin((Vector2){ p.x, p.y - STEP }, BODY_PLAYER_RADIUS);
                if (edge) DrawCircleV(p, 4.0f, (Color){ 255, 230, 60, 220 });
            }
        }
    DrawCircleLines((int)game->player.position.x, (int)game->player.position.y, BODY_PLAYER_RADIUS, (Color){ 0, 229, 255, 255 });
    DrawCircleV(game->player.position, 3.0f, (Color){ 0, 229, 255, 255 });
}

static void DrawCollisionDebugHUD(GameState *game, Font font)
{
    if (!(game->adminMode && game->debugCollision)) return;
    extern Vector2 g_virtualMouse;
    Vector2 mw = GetScreenToWorld2D(g_virtualMouse, game->camera);
    bool walk = MapBody_ContainsWithMargin(mw, BODY_PLAYER_RADIUS);
    DrawRectangle(8, 150, 384, 70, Fade(BLACK, 0.62f));
    DrawRectangleLines(8, 150, 384, 70, Fade((Color){ 0, 229, 255, 255 }, 0.5f));
    DrawTextEx(font, TextFormat("DEBUG COLISAO [F1]  cursor (%.0f, %.0f)  %s", mw.x, mw.y, walk ? "CAMINHAVEL" : "BLOQUEADO"),
               (Vector2){ 16, 156 }, 14.0f, 1.0f, walk ? (Color){ 120, 255, 160, 255 } : (Color){ 255, 120, 120, 255 });
    DrawTextEx(font, "verde=caminhavel  laranja=corpo sem margem  vermelho=bloqueado",
               (Vector2){ 16, 178 }, 12.0f, 1.0f, RAYWHITE);
    DrawTextEx(font, "amarelo=fronteira   ciano=raio de colisao do jogador",
               (Vector2){ 16, 196 }, 12.0f, 1.0f, RAYWHITE);
}

void DrawTelaGameplay(GameState *game, Font font, bool drawHUD)
{
    // IDENTIDADE VISUAL POR ONDA: cada fase tem cor de fundo, grade, células e
    // borda próprias (1 sangue, 2 inflamação, 3 bacteriana, 4 toxina, 5 colapso).
    Color bgColor, gridBase, cellBase, borderColor;
    int w = game->wave;
    if (w <= 1)       { bgColor=(Color){24,8,8,255};   gridBase=(Color){150,40,40,255};  cellBase=(Color){150,40,40,255};  borderColor=(Color){170,50,50,255}; }
    else if (w == 2)  { bgColor=(Color){18,8,26,255};  gridBase=(Color){150,60,200,255}; cellBase=(Color){130,50,170,255}; borderColor=(Color){150,60,200,255}; }
    else if (w == 3)  { bgColor=(Color){6,18,10,255};  gridBase=(Color){0,150,60,255};   cellBase=(Color){0,90,40,255};    borderColor=(Color){0,140,60,255}; }
    else if (w == 4)  { bgColor=(Color){22,18,6,255};  gridBase=(Color){210,170,30,255}; cellBase=(Color){170,140,20,255}; borderColor=(Color){210,170,30,255}; }
    else              { bgColor=(Color){6,10,28,255};  gridBase=(Color){50,90,210,255};  cellBase=(Color){40,70,170,255};  borderColor=(Color){50,90,210,255}; }

    Color gridColor = Fade(gridBase, 0.15f);

    // Override dramático: quando o CHEFE final está com pouca vida, a arena esquenta.
    bool isBossFight = false;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (game->enemies[i].active && game->enemies[i].tier == TIER_3_BOSS) {
            isBossFight = true;
            float hpPercent = (float)game->enemies[i].hp / game->enemies[i].maxHp;
            if (hpPercent < 0.33f) {
                bgColor = (Color){ 30, 5, 5, 255 };
                gridColor = Fade(RED, 0.25f);
                cellBase = (Color){ 160, 0, 40, 255 };
                borderColor = (Color){ 200, 0, 40, 255 };
            } else if (hpPercent < 0.66f) {
                bgColor = (Color){ 20, 5, 25, 255 };
                gridColor = Fade(MAGENTA, 0.2f);
                cellBase = (Color){ 150, 0, 120, 255 };
                borderColor = (Color){ 200, 0, 150, 255 };
            }
            break;
        }
    }

    BeginMode2D(game->camera);
    // O "mundo" agora é o CORPO HUMANO em si: removemos a arena genérica
    // (gradiente + grade + borda). Fora do corpo é escuro; o jogo acontece
    // dentro da silhueta, que preenche o mapa e tem colisão própria.
    DrawRectangle(-2000, -2000, MAP_WIDTH + 4000, MAP_HEIGHT + 4000, (Color){ 7, 4, 8, 255 });

    // Corpo humano (silhueta grande + órgãos-alvo da doença em foco).
    DrawMapBody(font, game->currentWorld, game->wave, (float)GetTime());
    (void)bgColor; (void)gridColor; (void)gridBase; (void)cellBase;
    (void)borderColor; (void)isBossFight;


    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (game->powerUps[i].active)
        {
            float pulse = 18.0f + sinf(game->powerUps[i].pulseTimer * 6.0f) * 3.0f;
            Vector2 pos = game->powerUps[i].position;

            Color itemCol = YELLOW;
            SpriteID itemSpr = (SpriteID)-1;
            if (game->powerUps[i].type == HP_RECOVERY) { itemCol = (Color){ 50, 220, 100, 255 }; }
            else if (game->powerUps[i].type == SPEED_BOOST) { itemCol = THEME_COLOR_MAIN; }
            else if (game->powerUps[i].type == SHIELD) { itemCol = (Color){ 30, 100, 200, 255 }; }
            else if (game->powerUps[i].type == ATTACK_BOOST) { itemCol = (Color){ 255, 60, 60, 255 }; }
            else if (game->powerUps[i].type == POWERUP_MASK) { itemCol = (Color){ 120, 220, 255, 255 }; itemSpr = SPR_ITEM_MASK; }
            else if (game->powerUps[i].type == POWERUP_DISTANCING) { itemCol = (Color){ 120, 255, 200, 255 }; itemSpr = SPR_ITEM_DISTANCING; }
            else if (game->powerUps[i].type == POWERUP_RNA_GRENADE) { itemCol = (Color){ 120, 255, 160, 255 }; itemSpr = SPR_ITEM_RNA_GRENADE; }
            else if (game->powerUps[i].type == POWERUP_CYTOKINE) { itemCol = (Color){ 80, 230, 140, 255 }; itemSpr = SPR_ITEM_CYTOKINE; }
            else if (game->powerUps[i].type == POWERUP_SUPREME_ORB) { itemCol = (Color){ 255, 220, 80, 255 }; }
            else if (game->powerUps[i].type == POWERUP_BARRIER) { itemCol = (Color){ 90, 200, 255, 255 }; }

            DrawCircleV(pos, pulse + 4.0f, Fade(itemCol, 0.3f));

            if (itemSpr != (SpriteID)-1 && SpriteAvailable(itemSpr))
            {
                DrawSpriteCentered(itemSpr, pos, (Vector2){ pulse * 2.2f, pulse * 2.2f }, 0.0f, WHITE);
            }
            else
            {
                DrawPoly(pos, 4, pulse, 0.0f, itemCol);
                DrawPolyLinesEx(pos, 4, pulse, 0.0f, 2.0f, WHITE);

                if (game->powerUps[i].type == HP_RECOVERY) {
                    DrawRectangle(pos.x - 2, pos.y - 6, 4, 12, WHITE);
                    DrawRectangle(pos.x - 6, pos.y - 2, 12, 4, WHITE);
                } else if (game->powerUps[i].type == SPEED_BOOST) {
                    DrawLineEx((Vector2){pos.x - 4, pos.y - 4}, (Vector2){pos.x + 2, pos.y}, 2.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 4, pos.y + 4}, (Vector2){pos.x + 2, pos.y}, 2.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 8, pos.y - 4}, (Vector2){pos.x - 2, pos.y}, 2.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 8, pos.y + 4}, (Vector2){pos.x - 2, pos.y}, 2.0f, WHITE);
                } else if (game->powerUps[i].type == SHIELD) {
                    DrawRectangle(pos.x - 5, pos.y - 4, 10, 6, WHITE);
                    DrawTriangle((Vector2){pos.x - 5, pos.y + 2}, (Vector2){pos.x, pos.y + 8}, (Vector2){pos.x + 5, pos.y + 2}, WHITE);
                } else if (game->powerUps[i].type == ATTACK_BOOST) {
                    DrawRectangle(pos.x - 1, pos.y - 6, 2, 10, WHITE);
                    DrawRectangle(pos.x - 4, pos.y + 1, 8, 2, WHITE);
                } else if (game->powerUps[i].type == POWERUP_MASK) {
                    DrawRectangleRounded((Rectangle){ pos.x - 8, pos.y - 5, 16, 10 }, 0.6f, 4, WHITE);
                    DrawLineEx((Vector2){pos.x - 8, pos.y - 3}, (Vector2){pos.x - 12, pos.y - 6}, 1.5f, WHITE);
                    DrawLineEx((Vector2){pos.x + 8, pos.y - 3}, (Vector2){pos.x + 12, pos.y - 6}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_DISTANCING) {
                    DrawCircleLines((int)pos.x - 6, (int)pos.y, 3.0f, WHITE);
                    DrawCircleLines((int)pos.x + 6, (int)pos.y, 3.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 2, pos.y}, (Vector2){pos.x + 2, pos.y}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_RNA_GRENADE) {
                    DrawCircleV(pos, 5.0f, WHITE);
                    DrawLineEx((Vector2){pos.x + 3, pos.y - 4}, (Vector2){pos.x + 7, pos.y - 8}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_CYTOKINE) {
                    DrawRectangle(pos.x - 2, pos.y - 6, 4, 12, WHITE);
                    DrawRectangle(pos.x - 6, pos.y - 2, 12, 4, WHITE);
                } else if (game->powerUps[i].type == POWERUP_SUPREME_ORB) {
                    DrawCircleV(pos, 7.0f, WHITE);
                    DrawCircleLines((int)pos.x, (int)pos.y, 12.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 10, pos.y}, (Vector2){pos.x + 10, pos.y}, 1.5f, WHITE);
                    DrawLineEx((Vector2){pos.x, pos.y - 10}, (Vector2){pos.x, pos.y + 10}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_BARRIER) {
                    DrawCircleLines((int)pos.x, (int)pos.y, 10.0f, WHITE);
                    DrawTriangle((Vector2){pos.x, pos.y - 9}, (Vector2){pos.x - 8, pos.y + 5}, (Vector2){pos.x + 8, pos.y + 5}, WHITE);
                }
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (game->enemies[i].active)
        {
            Enemy *enemy = &game->enemies[i];

            // Tamanho de render: base por tier x escala do arquétipo (centralizada).
            // Assim o enxame fica pequeno e o elite/chefe ficam grandes sem números
            // mágicos espalhados (bactérias usam scale 1.0 -> tamanhos inalterados).
            float tierBase = (enemy->tier == TIER_3_BOSS) ? 140.0f
                           : (enemy->tier == TIER_MINIBOSS) ? 90.0f : 45.0f;
            const EnemyArchetype *earch = EnemyArchetypeFor(enemy->type);
            float destSize = tierBase * (earch ? earch->sizeScale : 1.0f);
            Vector2 renderPos = enemy->position;

            // ANIMAÇÃO PROCEDURAL (Etapa 5) — squash/stretch, bobbing, inclinação,
            // antecipação/recuo, flash de dano e morte. Tudo baseado em TEMPO/ESTADO
            // (velSmooth/animTime/attackAnim/spawnAnim), nunca em random por frame.
            float t = (float)GetTime();
            float spd = Vector2Length(enemy->velSmooth);
            float spdN = spd / (enemy->speed + 1.0f);
            if (spdN > 1.0f) spdN = 1.0f;

            float squashFactor = 1.0f;
            float scale = 1.0f;
            float rotation = 0.0f;
            float alpha = 1.0f;

            if (enemy->state == DEATH)
            {
                float deathPct = enemy->cooldownTimer / 0.5f;
                if (deathPct < 0.0f) deathPct = 0.0f;
                if (deathPct > 1.0f) deathPct = 1.0f;
                scale = deathPct;                       // encolhe
                rotation = (1.0f - deathPct) * 360.0f;  // gira ao dissolver
                alpha = deathPct;                       // some
                squashFactor = 0.7f + deathPct * 0.3f;
            }
            else
            {
                // "Pop-in" ao surgir.
                scale = 0.45f + 0.55f * enemy->spawnAnim;
                // Respiração + stretch ao mover + antecipação(+)/recuo(-) do ataque.
                squashFactor = 1.0f + sinf(enemy->animTime * 4.0f) * 0.04f
                             + spdN * 0.10f + enemy->attackAnim * 0.16f;
                // Bobbing vertical (mais intenso em movimento).
                renderPos.y += sinf(enemy->animTime * (6.0f + spdN * 8.0f)) * (2.0f + spdN * 6.0f);
                // Inclinação na direção do movimento horizontal.
                rotation += Clamp(enemy->velSmooth.x * 0.02f, -14.0f, 14.0f);
                // Flash/tremor de dano DETERMINÍSTICO (senoidal, não aleatório).
                if (enemy->state == HURT)
                {
                    float k = enemy->cooldownTimer / 0.25f;
                    k = Clamp(k, 0.0f, 1.0f);
                    renderPos.x += sinf(t * 60.0f) * 6.0f * k;
                    renderPos.y += cosf(t * 70.0f) * 4.0f * k;
                    squashFactor += sinf(k * PI * 4.0f) * 0.18f; // pop de impacto
                }
                // Pulso de fase do chefe (mais "raivoso" nas fases finais).
                if (enemy->tier == TIER_3_BOSS)
                    scale *= 1.0f + 0.05f * (float)enemy->aiPhase * (0.5f + 0.5f * sinf(t * (3.0f + enemy->aiPhase)));
            }

            float currentDestSize = destSize * scale;

            DrawEnemyModel(enemy, renderPos, currentDestSize, rotation, squashFactor, alpha);

            // ESCUDO DE CAPSÍDEO (Mundo 2): anel protetor ao redor do vírus
            // enquanto ativo. Brilha ao receber dano (shieldHitFlash).
            if (enemy->state != DEATH && enemy->shieldActive && enemy->shieldHp > 0)
            {
                float shR = currentDestSize * 1.6f;
                float flash = (enemy->shieldHitFlash > 0.0f) ? (enemy->shieldHitFlash / 0.18f) : 0.0f;
                float pct = (enemy->shieldMaxHp > 0) ? (float)enemy->shieldHp / enemy->shieldMaxHp : 1.0f;
                // Aparência do capsídeo varia por arquétipo: elite REFORÇADO (roxo,
                // anel duplo), enxame FRACO (ciano tênue), demais azul padrão.
                EnemyBehavior shBeh = earch ? earch->behavior : BEHAV_MELEE;
                Color shCol = (shBeh == BEHAV_ELITE) ? (Color){ 190, 140, 255, 255 }
                            : (shBeh == BEHAV_SWARM) ? (Color){ 150, 220, 235, 255 }
                                                     : (Color){ 120, 200, 255, 255 };
                if (SpriteAvailable(SPR_VIRUS_SHIELD))
                {
                    DrawSpriteCentered(SPR_VIRUS_SHIELD, renderPos, (Vector2){ shR * 2.0f, shR * 2.0f }, rotation,
                                       Fade(WHITE, alpha * (0.55f + 0.45f * pct)));
                }
                else
                {
                    DrawCircleV(renderPos, shR, Fade(shCol, alpha * (0.10f + 0.18f * flash)));
                    DrawCircleLines((int)renderPos.x, (int)renderPos.y, shR, Fade(shCol, alpha * (0.5f + 0.5f * flash)));
                    DrawCircleLines((int)renderPos.x, (int)renderPos.y, shR * 0.92f, Fade(shCol, alpha * 0.3f));
                    // Capsídeo reforçado (elite): anel externo extra (mais "espesso").
                    if (shBeh == BEHAV_ELITE)
                        DrawCircleLines((int)renderPos.x, (int)renderPos.y, shR * 1.08f, Fade(shCol, alpha * (0.30f + 0.4f * flash)));
                    // arco que diminui conforme o escudo cai
                    DrawRing(renderPos, shR * 0.96f, shR, -90.0f, -90.0f + 360.0f * pct, 32, Fade(shCol, alpha * 0.7f));
                }
            }

            if (enemy->state != DEATH)
            {
                bool mini = (enemy->tier == TIER_MINIBOSS);
                float size = (enemy->tier == TIER_3_BOSS) ? 400.0f : mini ? 130.0f : 60.0f;
                float barW = size * 1.1f;
                float barH = mini ? 8.0f : 6.0f;
                float yOffset = (enemy->tier == TIER_3_BOSS) ? 200.0f : mini ? 80.0f : 50.0f;
                Rectangle rHPBg = { enemy->position.x - barW / 2.0f, enemy->position.y - yOffset, barW, barH };
                DrawRectangleRec(rHPBg, Fade(RED, 0.4f));

                float enemyHpPercent = (float)enemy->hp / enemy->maxHp;
                if (enemyHpPercent > 0.0f)
                {
                    Rectangle rHPFill = { rHPBg.x, rHPBg.y, barW * enemyHpPercent, barH };
                    DrawRectangleRec(rHPFill, mini ? (Color){ 255, 150, 40, 255 } : GREEN);
                }
                DrawRectangleLinesEx(rHPBg, 1.0f, BLACK);

                // Rótulo do mini chefe (mostra o nome educativo do arquétipo).
                if (mini)
                {
                    const char *mn = (earch && earch->name) ? TextFormat("MINI CHEFE: %s", earch->name) : "MINI CHEFE";
                    Vector2 ms = MeasureTextEx(font, mn, 16.0f, 1.0f);
                    DrawTextEx(font, mn, (Vector2){ enemy->position.x - ms.x / 2.0f, rHPBg.y - 20.0f }, 16.0f, 1.0f, (Color){ 255, 170, 60, 255 });
                }

                DrawStatusIcons(enemy->position, enemy->poisonTimer > 0.0f, enemy->slowTimer > 0.0f, GetTime());
            }
        }
    }

    // NÚCLEOS DE INFECÇÃO (escudo do chefe na fase 3): cristais pulsantes com
    // feixe ligando ao chefe e barra de vida própria.
    Vector2 bossPosForBeam = { 0, 0 };
    bool haveBoss = false;
    for (int i = 0; i < MAX_ENEMIES; i++)
        if (game->enemies[i].active && game->enemies[i].tier == TIER_3_BOSS && game->enemies[i].state != DEATH)
        { bossPosForBeam = game->enemies[i].position; haveBoss = true; break; }

    for (int i = 0; i < MAX_CORES; i++)
    {
        if (!game->cores[i].active) continue;
        Vector2 cp = game->cores[i].position;
        float pulse = 26.0f + sinf(game->cores[i].pulse * 4.0f) * 5.0f;
        // Feixe de energia ligando o núcleo ao chefe (mostra que o protege)
        if (haveBoss)
            DrawLineEx(cp, bossPosForBeam, 3.0f, Fade((Color){ 120, 230, 255, 255 }, 0.25f + 0.15f * sinf(game->cores[i].pulse * 5.0f)));
        Color cc = (game->cores[i].hitFlash > 0.0f) ? WHITE : (Color){ 120, 230, 255, 255 };
        DrawCircleV(cp, pulse + 6.0f, Fade(cc, 0.18f));
        DrawPoly(cp, 6, pulse, game->cores[i].pulse * 30.0f, Fade(cc, 0.85f));
        DrawPolyLinesEx(cp, 6, pulse, game->cores[i].pulse * 30.0f, 3.0f, WHITE);
        DrawCircleV(cp, 8.0f, WHITE);
        // Barra de vida do núcleo
        float bw = 70.0f;
        float hp = (float)game->cores[i].hp / game->cores[i].maxHp;
        if (hp < 0.0f) hp = 0.0f;
        DrawRectangle((int)(cp.x - bw / 2), (int)(cp.y - pulse - 18), (int)bw, 6, Fade(BLACK, 0.6f));
        DrawRectangle((int)(cp.x - bw / 2), (int)(cp.y - pulse - 18), (int)(bw * hp), 6, (Color){ 120, 230, 255, 255 });
    }

    Color wpnPrim = WeaponSkinPrimary(game->player.weaponSkinId);
    Color wpnSec  = WeaponSkinSecondary(game->player.weaponSkinId);

    for (int i = 0; i < MAX_BIOMINES; i++)
    {
        BioMine *m = &game->bioMines[i];
        if (!m->active) continue;
        float pulse = 0.5f + 0.5f * sinf(m->pulse * 7.0f);
        Color armed = (m->armTimer > 0.0f) ? Fade(GRAY, 0.75f) : (Color){ 90, 255, 180, 255 };

        // LANÇA-MINAS DE RNA (evolução do slot 3): mina ARREMESSADA com pavio
        // automático (autoDetonateTimer > 0). Cápsula com fita de RNA (dupla hélice)
        // girando e um anel de contagem regressiva do pavio — bem distinta da mina
        // plantada (base), que continua com o visual original abaixo.
        if (m->autoDetonateTimer > 0.0f)
        {
            float tt = (float)GetTime();
            Color rna = (m->armTimer > 0.0f) ? Fade((Color){ 175, 135, 215, 255 }, 0.85f)
                                             : (Color){ 210, 120, 255, 255 };
            float frac = m->autoDetonateTimer / RNA_LAUNCHER_FUSE;
            if (frac > 1.0f) frac = 1.0f; else if (frac < 0.0f) frac = 0.0f;
            DrawCircleV(m->position, 19.0f + pulse * 3.0f, Fade(rna, 0.16f));
            // Anel-pavio: arco que encolhe conforme o tempo até a detonação passa.
            DrawRing(m->position, 15.0f, 18.0f, -90.0f, -90.0f + 360.0f * frac, 36, Fade(rna, 0.85f));
            // Dupla hélice de RNA (duas senoides cruzadas que giram no tempo).
            for (int s = 0; s < 12; s++)
            {
                float t0 = s / 12.0f;
                float ph = tt * 3.2f + t0 * PI * 3.2f;
                float yy = (t0 - 0.5f) * 28.0f;
                float xx = cosf(ph) * 8.5f;
                Vector2 h1 = { m->position.x + xx, m->position.y + yy };
                Vector2 h2 = { m->position.x - xx, m->position.y + yy };
                if (s % 2 == 0) DrawLineEx(h1, h2, 1.4f, Fade(rna, 0.55f)); // "degraus" do RNA
                DrawCircleV(h1, 2.6f, rna);
                DrawCircleV(h2, 2.6f, Fade(rna, 0.75f));
            }
            DrawCircleV(m->position, 4.0f + pulse * 1.5f, WHITE);
            continue;
        }

        DrawCircleV(m->position, 18.0f + pulse * 3.0f, Fade(armed, 0.16f));
        DrawCircleLines(m->position.x, m->position.y, 16.0f + pulse * 2.0f, Fade(armed, 0.75f));
        DrawCircleV(m->position, 7.0f, (Color){ 40, 24, 56, 255 });
        DrawCircleV(m->position, 3.5f + pulse * 1.5f, armed);
        for (int s = 0; s < 4; s++)
        {
            float a = (m->pulse * 100.0f + s * 90.0f) * DEG2RAD;
            Vector2 a0 = { m->position.x + cosf(a) * 8.0f, m->position.y + sinf(a) * 8.0f };
            Vector2 a1 = { m->position.x + cosf(a) * 17.0f, m->position.y + sinf(a) * 17.0f };
            DrawLineEx(a0, a1, 2.0f, Fade(armed, 0.8f));
        }
    }

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (game->projectiles[i].active)
        {
            Projectile *p = &game->projectiles[i];
            float srcSize = 12.0f;
            Color pCol = YELLOW;
            if (p->type == PROJ_ACID_ARC) pCol = LIME;
            else if (p->type == PROJ_VOID_BOLT) pCol = MAGENTA;
            else if (p->type == PROJ_BOSS_BULLET) pCol = RED;
            else if (p->type == PROJ_PLAYER_RIFLE) pCol = wpnPrim;   // skin da arma
            else if (p->type == PROJ_PLAYER_GRENADE) pCol = ORANGE;
            else if (p->type == PROJ_PLAYER_BFG) pCol = wpnPrim;
            else if (p->type == PROJ_PLAYER_RIFLE_EVOLVED) pCol = (Color){ 255, 170, 90, 255 };
            else if (p->type == PROJ_PLAYER_BFG_EVOLVED) pCol = (Color){ 255, 230, 90, 255 };
            else if (p->type == PROJ_PLAYER_PHAGE) pCol = (Color){ 120, 255, 160, 255 };   // bacteriófago (verde)
            else if (p->type == PROJ_PLAYER_VACCINE) pCol = (Color){ 120, 200, 255, 255 }; // vacina (azul)
            else if (p->type == PROJ_VIRAL_SPORE) pCol = (Color){ 190, 235, 90, 255 };     // material viral

            if (p->type == PROJ_PLAYER_BFG_EVOLVED) {
                // BFG IMUNOLOGICO OMEGA: singularidade em camadas — halo duplo, dois
                // anéis contrarrotativos, coroa de nós orbitais (Ômega) e núcleo
                // ciano incandescente. Bem distinto do orbe simples do BFG base.
                srcSize = 34.0f;
                float tt = (float)GetTime();
                Color omega = (Color){ 255, 230, 90, 255 };
                Color core  = (Color){ 150, 255, 230, 255 };
                DrawCircleGradient((int)p->position.x, (int)p->position.y, srcSize * 1.5f, Fade(omega, 0.30f), BLANK);
                DrawCircleGradient((int)p->position.x, (int)p->position.y, srcSize, omega, BLANK);
                DrawRing(p->position, srcSize * 0.92f, srcSize * 1.05f, tt * 120.0f, tt * 120.0f + 260.0f, 32, Fade(core, 0.80f));
                DrawRing(p->position, srcSize * 0.64f, srcSize * 0.76f, -tt * 165.0f, -tt * 165.0f + 220.0f, 28, Fade(omega, 0.85f));
                for (int n = 0; n < 6; n++) {
                    float a = tt * 1.6f + n * (PI / 3.0f);
                    Vector2 q = { p->position.x + cosf(a) * srcSize * 1.12f, p->position.y + sinf(a) * srcSize * 1.12f };
                    DrawCircleV(q, 3.5f, Fade(core, 0.9f));
                }
                DrawCircleV(p->position, srcSize * 0.42f, omega);
                DrawCircleV(p->position, srcSize * 0.20f, core);
                DrawCircleV(p->position, srcSize * 0.09f, WHITE);
            } else if (p->type == PROJ_PLAYER_BFG) {
                srcSize = 30.0f;
                DrawCircleGradient((int)p->position.x, (int)p->position.y, srcSize, pCol, BLANK);
                DrawCircleLines(p->position.x, p->position.y, srcSize, wpnSec);
            } else if (p->type == PROJ_PLAYER_GRENADE) {
                srcSize = 15.0f;
                DrawCircle(p->position.x, p->position.y, srcSize, pCol);
                DrawCircleLines(p->position.x, p->position.y, srcSize, wpnPrim);
            } else if (p->type == PROJ_PLAYER_RIFLE_EVOLVED) {
                // RIFLE VETORIAL REPLICANTE: dardo âmbar instável que telegrafa a
                // duplicação — rastro principal + dois rastros bifurcados (fork) e
                // duas fagulhas-clone orbitando o núcleo branco-quente.
                float tt = (float)GetTime();
                Color amber = (Color){ 255, 170, 90, 255 };
                Color hot   = (Color){ 255, 245, 190, 255 };
                Vector2 dir = Vector2Normalize(p->velocity);
                if (dir.x == 0.0f && dir.y == 0.0f) dir = (Vector2){ 1.0f, 0.0f };
                Vector2 perp = { -dir.y, dir.x };
                Vector2 back = Vector2Subtract(p->position, Vector2Scale(dir, 26.0f));
                Vector2 forkA = Vector2Add(back, Vector2Scale(perp, 7.0f));
                Vector2 forkB = Vector2Subtract(back, Vector2Scale(perp, 7.0f));
                DrawLineEx(forkA, p->position, 2.5f, Fade(amber, 0.35f));
                DrawLineEx(forkB, p->position, 2.5f, Fade(amber, 0.35f));
                DrawLineEx(back, p->position, 5.0f, Fade(amber, 0.55f));
                DrawCircleGradient((int)p->position.x, (int)p->position.y, 15.0f, Fade(amber, 0.30f), BLANK);
                float ang = atan2f(dir.y, dir.x) * RAD2DEG;
                DrawPoly(p->position, 4, 11.0f, ang, amber);
                DrawPolyLinesEx(p->position, 4, 11.0f, ang, 2.0f, hot);
                for (int c = 0; c < 2; c++) {
                    float a = tt * 9.0f + c * PI;
                    Vector2 q = { p->position.x + cosf(a) * 11.0f, p->position.y + sinf(a) * 11.0f };
                    DrawCircleV(q, 3.0f, Fade(hot, 0.95f));
                }
                DrawCircleV(p->position, 3.8f, hot);
            } else if (p->type == PROJ_PLAYER_RIFLE || p->type == PROJ_PLAYER_PHAGE ||
                       p->type == PROJ_PLAYER_VACCINE) {
                // Projétil do jogador mais legível: rastro curto + núcleo brilhante
                Color coreCol = (p->type == PROJ_PLAYER_RIFLE) ? wpnSec : WHITE;
                Vector2 tail = Vector2Subtract(p->position, Vector2Scale(Vector2Normalize(p->velocity), 22.0f));
                DrawLineEx(tail, p->position, 5.0f, Fade(pCol, 0.45f));
                DrawCircle(p->position.x, p->position.y, 9.0f, pCol);
                DrawCircle(p->position.x, p->position.y, 4.0f, coreCol);
            } else if (p->type == PROJ_VIRAL_SPORE) {
                // Material viral: pequeno virion espiculado (projétil próprio dos vírus).
                float tt = (float)GetTime();
                DrawCircleV(p->position, 10.0f, Fade(pCol, 0.30f));
                for (int s = 0; s < 6; s++) {
                    float a = (tt * 220.0f + s * 60.0f) * DEG2RAD;
                    Vector2 e = { p->position.x + cosf(a) * 11.0f, p->position.y + sinf(a) * 11.0f };
                    DrawLineEx(p->position, e, 2.0f, Fade(pCol, 0.85f));
                }
                DrawCircleV(p->position, 6.0f, pCol);
                DrawCircleV(p->position, 2.5f, (Color){ 60, 40, 10, 255 });
            } else {
                DrawCircle(p->position.x, p->position.y, srcSize, pCol);
                DrawCircleLines(p->position.x, p->position.y, srcSize, WHITE);
            }
        }
    }

    Vector2 pPos = game->player.position;
    float playerSize = 40.0f;

    DrawPlayerHero(game, pPos, playerSize);
    DrawStatusIcons(game->player.position, game->player.poisonTimer > 0.0f, game->player.slowTimer > 0.0f, GetTime());

    if (game->slashAnimTimer > 0.0f)
    {
        float dur = (game->slashAnimKind == 3) ? 0.30f : 0.24f;
        float t = 1.0f - (game->slashAnimTimer / dur);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        Vector2 dir = game->slashAnimDir;
        if (dir.x == 0.0f && dir.y == 0.0f) dir = (Vector2){ 1.0f, 0.0f };
        Vector2 perp = { -dir.y, dir.x };
        float alpha = (1.0f - t);

        if (game->slashAnimKind == 1)
        {
            Vector2 start = Vector2Add(game->slashAnimPos, Vector2Scale(dir, 28.0f));
            Vector2 tip = Vector2Add(game->slashAnimPos, Vector2Scale(dir, game->slashAnimRadius * (0.45f + 0.55f * t)));
            DrawLineEx(start, tip, 10.0f, Fade(wpnPrim, alpha * 0.85f));
            DrawLineEx(Vector2Add(start, Vector2Scale(perp, 8.0f)), tip, 3.0f, Fade(WHITE, alpha));
            DrawCircleV(tip, 14.0f, Fade(wpnSec, alpha * 0.75f));
        }
        else if (game->slashAnimKind == 2)
        {
            Vector2 c = Vector2Add(game->slashAnimPos, Vector2Scale(dir, 54.0f));
            float r = game->slashAnimRadius * (0.58f + 0.20f * t);
            for (int s = 0; s < 7; s++)
            {
                float u = ((float)s / 6.0f - 0.5f) * 2.25f;
                Vector2 p0 = Vector2Add(c, Vector2Add(Vector2Scale(dir, cosf(u) * r * 0.35f), Vector2Scale(perp, sinf(u) * r)));
                DrawCircleV(p0, 5.0f + s * 0.4f, Fade((s % 2) ? wpnSec : wpnPrim, alpha * 0.85f));
            }
            DrawCircleLines(c.x, c.y, r, Fade(wpnPrim, alpha * 0.38f));
        }
        else
        {
            float currentRadius = t * game->slashAnimRadius;
            DrawCircleLines(game->slashAnimPos.x, game->slashAnimPos.y, currentRadius, Fade(wpnSec, alpha * 0.95f));
            DrawCircleLines(game->slashAnimPos.x, game->slashAnimPos.y, currentRadius - 10.0f, Fade(wpnPrim, alpha * 0.75f));
            DrawCircle(game->slashAnimPos.x, game->slashAnimPos.y, currentRadius, Fade(wpnPrim, alpha * 0.14f));
            for (int s = 0; s < 6; s++)
            {
                float a = (s * 60.0f + t * 160.0f) * DEG2RAD;
                Vector2 a0 = { game->slashAnimPos.x + cosf(a) * currentRadius * 0.35f, game->slashAnimPos.y + sinf(a) * currentRadius * 0.35f };
                Vector2 a1 = { game->slashAnimPos.x + cosf(a + 0.4f) * currentRadius, game->slashAnimPos.y + sinf(a + 0.4f) * currentRadius };
                DrawLineEx(a0, a1, 3.0f, Fade(wpnSec, alpha * 0.85f));
            }
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            float lifePercent = game->particles[i].lifeTime / game->particles[i].maxLifeTime;
            float size = game->particles[i].size * (0.3f + 0.7f * lifePercent);
            DrawRectangleV(
                (Vector2){ game->particles[i].position.x - size, game->particles[i].position.y - size },
                (Vector2){ size * 2.0f, size * 2.0f },
                Fade(game->particles[i].color, lifePercent)
            );
        }
    }

    // Números de dano flutuantes (feedback de combate)
    for (int i = 0; i < MAX_DAMAGE_TEXTS; i++)
    {
        if (game->damageTexts[i].active)
        {
            DamageText *dt = &game->damageTexts[i];
            float alpha = dt->timer / dt->maxTime;
            const char *txt = TextFormat("%d", dt->value);
            Vector2 sz = MeasureTextEx(font, txt, 20.0f, 1.0f);
            Vector2 pos = { dt->position.x - sz.x / 2.0f, dt->position.y };
            DrawTextEx(font, txt, (Vector2){ pos.x + 1, pos.y + 1 }, 20.0f, 1.0f, Fade(BLACK, alpha * 0.8f));
            DrawTextEx(font, txt, pos, 20.0f, 1.0f, Fade(dt->color, alpha));
        }
    }

    // Overlay de DEBUG da colisão (admin + F1) — no espaço do mundo, sobre tudo.
    DrawCollisionDebugWorld(game);

    EndMode2D();

    float hpPct = (float)game->player.hp / game->player.maxHp;
    if (hpPct < 0.25f && g_assets.shdLowHP.id != 0) {
        BeginShaderMode(g_assets.shdLowHP);
        float time = (float)GetTime();
        // Resolução do FRAMEBUFFER atual (alvo SSAA pode ser 2x), para o vinheta
        // usar gl_FragCoord/resolution corretamente (0..1).
        float res[2] = { (float)rlGetFramebufferWidth(), (float)rlGetFramebufferHeight() };
        SetShaderValue(g_assets.shdLowHP, g_assets.shdLowHPTimeLoc, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(g_assets.shdLowHP, g_assets.shdLowHPResLoc, res, SHADER_UNIFORM_VEC2);
        
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
        EndShaderMode();
    }

    // Flash vermelho ao receber dano (feedback visual imediato)
    if (game->hurtFlashTimer > 0.0f)
    {
        float a = (game->hurtFlashTimer / 0.35f) * 0.4f;
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(RED, a));
    }

    // Painel/coordenadas do DEBUG de colisão (admin + F1), em espaço de tela.
    DrawCollisionDebugHUD(game, font);

    if (drawHUD)
    {
        DrawHUD(game, font);
    }
}

// Mundo do tutorial (espaço da câmera): cenário da seringa + entidades. É o que
// vai para a textura virtual (com shader/letterbox) e congela atrás do pause.
void DrawTutorialWorld(GameState *game, Font font)
{
    ClearBackground(BLACK);
    BeginMode2D(game->camera);

    DrawMapSeringa(font, game->tutorialStep, (float)GetTime(), game->injectionTimer);

    Vector2 pPos = game->player.position;
    float ps = 38.0f;
    DrawPlayerHero(game, pPos, ps);

    if (game->slashAnimTimer > 0.0f)
    {
        float t = 1.0f - (game->slashAnimTimer / 0.22f);
        float cr = t * game->slashAnimRadius;
        DrawCircleLines(game->slashAnimPos.x, game->slashAnimPos.y, cr, Fade(WHITE, (1.0f - t) * 0.9f));
        DrawCircle(game->slashAnimPos.x, game->slashAnimPos.y, cr, Fade(THEME_COLOR_MAIN, (1.0f - t) * 0.15f));
    }

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (game->enemies[i].active && game->enemies[i].isTutorialEnemy)
        {
            Enemy *e = &game->enemies[i];
            float es = 32.0f;
            
            Vector2 renderPos = e->position;
            float squashX = 1.0f;
            float squashY = 1.0f;
            float scale = 1.0f;
            float rotationAngle = 0.0f;
            float alpha = 1.0f;
            
            if (e->state == HURT)
            {
                // Tremor DETERMINÍSTICO (senoidal), não aleatório por quadro:
                // evita o "chiado" visual e mantém o feedback de impacto legível.
                float t = e->cooldownTimer / 0.25f;
                float k = Clamp(t, 0.0f, 1.0f);
                float gt = (float)GetTime();
                renderPos.x += sinf(gt * 60.0f) * 6.0f * k;
                renderPos.y += cosf(gt * 70.0f) * 4.0f * k;
                squashX = 1.0f + sinf(t * PI * 4.0f) * 0.15f;
                squashY = 1.0f - sinf(t * PI * 4.0f) * 0.15f;
            }
            else if (e->state == DEATH)
            {
                float deathPct = e->cooldownTimer / 0.5f;
                if (deathPct < 0.0f) deathPct = 0.0f;
                if (deathPct > 1.0f) deathPct = 1.0f;
                
                scale = deathPct;
                rotationAngle = (1.0f - deathPct) * PI * 2.0f;
                alpha = deathPct;
            }
            
            float currentEs = es * scale;
            Color bactCol = (e->state == HURT) ? WHITE : (Color){ 50, 200, 80, 255 };
            Color bactLineCol = (Color){ 20, 140, 50, 255 };
            
            bactCol = Fade(bactCol, alpha);
            bactLineCol = Fade(bactLineCol, alpha);
            
            DrawEllipse(renderPos.x, renderPos.y, currentEs * squashX, currentEs * squashY, bactCol);
            DrawEllipseLines(renderPos.x, renderPos.y, currentEs * squashX, currentEs * squashY, bactLineCol);
            
            float c = cosf(rotationAngle);
            float s = sinf(rotationAngle);
            
            Vector2 f1Start = { -currentEs * squashX * c, -currentEs * squashX * s };
            Vector2 f1End   = { (-currentEs * squashX - 15.0f) * c - 10.0f * s, (-currentEs * squashX - 15.0f) * s + 10.0f * c };
            
            Vector2 f2Start = { currentEs * squashX * c, currentEs * squashX * s };
            Vector2 f2End   = { (currentEs * squashX + 15.0f) * c - (-10.0f) * s, (currentEs * squashX + 15.0f) * s + (-10.0f) * c };
            
            DrawLineV(Vector2Add(renderPos, f1Start), Vector2Add(renderPos, f1End), bactLineCol);
            DrawLineV(Vector2Add(renderPos, f2Start), Vector2Add(renderPos, f2End), bactLineCol);
            
            if (e->state != DEATH)
            {
                float barW = 70.0f;
                DrawRectangle(e->position.x - barW/2, e->position.y - es - 14, barW, 8, Fade(RED, 0.5f));
                float hpPct = (float)e->hp / e->maxHp;
                DrawRectangle(e->position.x - barW/2, e->position.y - es - 14, barW * hpPct, 8, (Color){ 50, 200, 80, 255 });
                DrawRectangleLinesEx((Rectangle){ e->position.x - barW/2, e->position.y - es - 14, barW, 8 }, 1.0f, BLACK);
                DrawStatusIcons(e->position, e->poisonTimer > 0.0f, e->slowTimer > 0.0f, GetTime());
            }
        }
    }

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            float lp = game->particles[i].lifeTime / game->particles[i].maxLifeTime;
            float size = game->particles[i].size * (0.3f + 0.7f * lp);
            DrawRectangleV((Vector2){ game->particles[i].position.x - size, game->particles[i].position.y - size }, (Vector2){ size * 2.0f, size * 2.0f }, Fade(game->particles[i].color, lp));
        }
    }

    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (game->projectiles[i].active)
        {
            Vector2 pPos = game->projectiles[i].position;
            DrawCircleV(pPos, 14.0f, Fade((Color){ 50, 200, 80, 255 }, 0.22f));
            DrawCircleV(pPos, 8.0f, (Color){ 50, 200, 80, 255 });
            DrawCircleV(pPos, 4.0f, WHITE);
        }
    }

    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (game->powerUps[i].active)
        {
            float pulse = 18.0f + sinf(game->powerUps[i].pulseTimer * 6.0f) * 3.0f;
            Vector2 pos = game->powerUps[i].position;

            Color itemCol = YELLOW;
            SpriteID itemSpr = (SpriteID)-1;
            if (game->powerUps[i].type == HP_RECOVERY) { itemCol = (Color){ 50, 220, 100, 255 }; }
            else if (game->powerUps[i].type == SPEED_BOOST) { itemCol = THEME_COLOR_MAIN; }
            else if (game->powerUps[i].type == SHIELD) { itemCol = (Color){ 30, 100, 200, 255 }; }
            else if (game->powerUps[i].type == ATTACK_BOOST) { itemCol = (Color){ 255, 60, 60, 255 }; }
            else if (game->powerUps[i].type == POWERUP_MASK) { itemCol = (Color){ 120, 220, 255, 255 }; itemSpr = SPR_ITEM_MASK; }
            else if (game->powerUps[i].type == POWERUP_DISTANCING) { itemCol = (Color){ 120, 255, 200, 255 }; itemSpr = SPR_ITEM_DISTANCING; }
            else if (game->powerUps[i].type == POWERUP_RNA_GRENADE) { itemCol = (Color){ 120, 255, 160, 255 }; itemSpr = SPR_ITEM_RNA_GRENADE; }
            else if (game->powerUps[i].type == POWERUP_CYTOKINE) { itemCol = (Color){ 80, 230, 140, 255 }; itemSpr = SPR_ITEM_CYTOKINE; }
            else if (game->powerUps[i].type == POWERUP_SUPREME_ORB) { itemCol = (Color){ 255, 220, 80, 255 }; }
            else if (game->powerUps[i].type == POWERUP_BARRIER) { itemCol = (Color){ 90, 200, 255, 255 }; }

            DrawCircleV(pos, pulse + 4.0f, Fade(itemCol, 0.3f));

            if (itemSpr != (SpriteID)-1 && SpriteAvailable(itemSpr))
            {
                DrawSpriteCentered(itemSpr, pos, (Vector2){ pulse * 2.2f, pulse * 2.2f }, 0.0f, WHITE);
            }
            else
            {
                DrawPoly(pos, 4, pulse, 0.0f, itemCol);
                DrawPolyLinesEx(pos, 4, pulse, 0.0f, 2.0f, WHITE);

                if (game->powerUps[i].type == HP_RECOVERY) {
                    DrawRectangle(pos.x - 2, pos.y - 6, 4, 12, WHITE);
                    DrawRectangle(pos.x - 6, pos.y - 2, 12, 4, WHITE);
                } else if (game->powerUps[i].type == SPEED_BOOST) {
                    DrawLineEx((Vector2){pos.x - 4, pos.y - 4}, (Vector2){pos.x + 2, pos.y}, 2.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 4, pos.y + 4}, (Vector2){pos.x + 2, pos.y}, 2.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 8, pos.y - 4}, (Vector2){pos.x - 2, pos.y}, 2.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 8, pos.y + 4}, (Vector2){pos.x - 2, pos.y}, 2.0f, WHITE);
                } else if (game->powerUps[i].type == SHIELD) {
                    DrawRectangle(pos.x - 5, pos.y - 4, 10, 6, WHITE);
                    DrawTriangle((Vector2){pos.x - 5, pos.y + 2}, (Vector2){pos.x, pos.y + 8}, (Vector2){pos.x + 5, pos.y + 2}, WHITE);
                } else if (game->powerUps[i].type == ATTACK_BOOST) {
                    DrawRectangle(pos.x - 1, pos.y - 6, 2, 10, WHITE);
                    DrawRectangle(pos.x - 4, pos.y + 1, 8, 2, WHITE);
                } else if (game->powerUps[i].type == POWERUP_MASK) {
                    DrawRectangleRounded((Rectangle){ pos.x - 8, pos.y - 5, 16, 10 }, 0.6f, 4, WHITE);
                    DrawLineEx((Vector2){pos.x - 8, pos.y - 3}, (Vector2){pos.x - 12, pos.y - 6}, 1.5f, WHITE);
                    DrawLineEx((Vector2){pos.x + 8, pos.y - 3}, (Vector2){pos.x + 12, pos.y - 6}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_DISTANCING) {
                    DrawCircleLines((int)pos.x - 6, (int)pos.y, 3.0f, WHITE);
                    DrawCircleLines((int)pos.x + 6, (int)pos.y, 3.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 2, pos.y}, (Vector2){pos.x + 2, pos.y}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_RNA_GRENADE) {
                    DrawCircleV(pos, 5.0f, WHITE);
                    DrawLineEx((Vector2){pos.x + 3, pos.y - 4}, (Vector2){pos.x + 7, pos.y - 8}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_CYTOKINE) {
                    DrawRectangle(pos.x - 2, pos.y - 6, 4, 12, WHITE);
                    DrawRectangle(pos.x - 6, pos.y - 2, 12, 4, WHITE);
                } else if (game->powerUps[i].type == POWERUP_SUPREME_ORB) {
                    DrawCircleV(pos, 7.0f, WHITE);
                    DrawCircleLines((int)pos.x, (int)pos.y, 12.0f, WHITE);
                    DrawLineEx((Vector2){pos.x - 10, pos.y}, (Vector2){pos.x + 10, pos.y}, 1.5f, WHITE);
                    DrawLineEx((Vector2){pos.x, pos.y - 10}, (Vector2){pos.x, pos.y + 10}, 1.5f, WHITE);
                } else if (game->powerUps[i].type == POWERUP_BARRIER) {
                    DrawCircleLines((int)pos.x, (int)pos.y, 10.0f, WHITE);
                    DrawTriangle((Vector2){pos.x, pos.y - 9}, (Vector2){pos.x - 8, pos.y + 5}, (Vector2){pos.x + 8, pos.y + 5}, WHITE);
                }
            }
        }
    }

    // Números de dano flutuantes (também no tutorial)
    for (int i = 0; i < MAX_DAMAGE_TEXTS; i++)
    {
        if (game->damageTexts[i].active)
        {
            DamageText *dt = &game->damageTexts[i];
            float alpha = dt->timer / dt->maxTime;
            const char *txt = TextFormat("%d", dt->value);
            Vector2 sz = MeasureTextEx(font, txt, 20.0f, 1.0f);
            Vector2 dpos = { dt->position.x - sz.x / 2.0f, dt->position.y };
            DrawTextEx(font, txt, (Vector2){ dpos.x + 1, dpos.y + 1 }, 20.0f, 1.0f, Fade(BLACK, alpha * 0.8f));
            DrawTextEx(font, txt, dpos, 20.0f, 1.0f, Fade(dt->color, alpha));
        }
    }

    EndMode2D();

    float hpPct = (float)game->player.hp / game->player.maxHp;
    if (hpPct < 0.25f && g_assets.shdLowHP.id != 0) {
        BeginShaderMode(g_assets.shdLowHP);
        float time = (float)GetTime();
        // Resolução do FRAMEBUFFER atual (alvo SSAA pode ser 2x), para o vinheta
        // usar gl_FragCoord/resolution corretamente (0..1).
        float res[2] = { (float)rlGetFramebufferWidth(), (float)rlGetFramebufferHeight() };
        SetShaderValue(g_assets.shdLowHP, g_assets.shdLowHPTimeLoc, &time, SHADER_UNIFORM_FLOAT);
        SetShaderValue(g_assets.shdLowHP, g_assets.shdLowHPResLoc, res, SHADER_UNIFORM_VEC2);

        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WHITE);
        EndShaderMode();
    }

    // Flash vermelho ao receber dano (feedback visual imediato)
    if (game->hurtFlashTimer > 0.0f)
    {
        float a = (game->hurtFlashTimer / 0.35f) * 0.4f;
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(RED, a));
    }
}

// HUD/overlay do tutorial (espaço de tela). Separado do mundo para que o mundo
// possa ir para a textura virtual (com letterbox) e congelar corretamente atrás
// do menu de pausa, SEM redesenhar a cena duas vezes e SEM um ClearBackground
// que apagava o blit já renderizado (corrige fundo de pausa + desperdício).
void DrawTutorialHUD(GameState *game, Font font)
{
    char stepStr[32];
    sprintf(stepStr, "PASSO %d/3", game->tutorialStep + 1);
    DrawSciFiBox((Rectangle){ 1050, 20, 210, 45 }, (Color){ 0, 200, 100, 255 });
    Vector2 stepSz = MeasureTextEx(font, stepStr, 18.0f, 1.0f);
    DrawTextEx(font, stepStr, (Vector2){ 1155.0f - stepSz.x / 2.0f, 32.0f }, 18.0f, 1.0f, (Color){ 0, 220, 120, 255 });

    if (game->tutorialDialog.active)
    {
        DrawRectangleRounded((Rectangle){ 80, 580, 1120, 120 }, 0.08f, 6, Fade((Color){ 6, 18, 12, 255 }, 0.92f));
        DrawRectangleRoundedLines((Rectangle){ 80, 580, 1120, 120 }, 0.08f, 6, (Color){ 0, 200, 100, 255 });

        const char *fullL1 = "";
        const char *fullL2 = "";
        const char *fullL3 = "";
        GetTutorialDialogText(game->tutorialStep, game->tutorialDialog.page, &fullL1, &fullL2, &fullL3);

        char l1Draw[128] = { 0 };
        char l2Draw[128] = { 0 };
        char l3Draw[128] = { 0 };

        int charLimit = game->tutorialDialog.charShown;
        int len1 = strlen(fullL1);
        int len2 = strlen(fullL2);
        int len3 = strlen(fullL3);

        // Doutor assistente animado (substitui o antigo círculo "Ab"): fala
        // enquanto o texto é digitado e reage no início de cada fala.
        int totalLen = len1 + len2 + len3;
        bool talking = (charLimit < totalLen);
        float reactT = (charLimit < 8) ? 1.0f : 0.0f;
        DrawTutorialDoctor((Vector2){ 122, 640 }, 30.0f, (float)GetTime(), talking, reactT);

        if (charLimit <= len1)
        {
            strncpy(l1Draw, fullL1, charLimit);
        }
        else
        {
            strcpy(l1Draw, fullL1);
            int rem = charLimit - len1;
            if (rem <= len2)
            {
                strncpy(l2Draw, fullL2, rem);
            }
            else
            {
                strcpy(l2Draw, fullL2);
                int rem2 = rem - len2;
                if (rem2 <= len3)
                {
                    strncpy(l3Draw, fullL3, rem2);
                }
                else
                {
                    strcpy(l3Draw, fullL3);
                }
            }
        }

        DrawTextEx(font, l1Draw, (Vector2){ 155, 592 }, 16.0f, 1.0f, WHITE);
        DrawTextEx(font, l2Draw, (Vector2){ 155, 614 }, 16.0f, 1.0f, WHITE);
        DrawTextEx(font, l3Draw, (Vector2){ 155, 636 }, 16.0f, 1.0f, (Color){ 0, 220, 120, 255 });
    }

    DrawSciFiBox((Rectangle){ 20, 20, 240, 55 }, (Color){ 0, 200, 100, 255 });
    DrawTextEx(font, "ANTICORPO", (Vector2){ 30, 27 }, 14.0f, 1.0f, (Color){ 0, 220, 120, 255 });
    DrawRectangleRounded((Rectangle){ 30, 46, 180, 10 }, 0.5f, 4, (Color){ 30, 10, 10, 255 });
    float hpPctPlayer = (float)game->player.hp / game->player.maxHp;
    if (hpPctPlayer > 0.0f)
        DrawRectangleRounded((Rectangle){ 30, 46, 180.0f * hpPctPlayer, 10 }, 0.5f, 4, (Color){ 0, 220, 120, 255 });
    char hpBuf[32];
    sprintf(hpBuf, "%d/%d HP", game->player.hp, game->player.maxHp);
    Vector2 hpSz = MeasureTextEx(font, hpBuf, 12.0f, 1.0f);
    DrawTextEx(font, hpBuf, (Vector2){ 220.0f - hpSz.x, 44.0f }, 12.0f, 1.0f, WHITE);
}

// Compatibilidade: desenha a cena do tutorial completa (mundo + HUD) numa só
// chamada. O loop principal usa DrawTutorialWorld/DrawTutorialHUD separadamente.
void DrawTelaTutorial(GameState *game, Font font)
{
    DrawTutorialWorld(game, font);
    DrawTutorialHUD(game, font);
}
