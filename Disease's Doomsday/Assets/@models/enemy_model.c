#include "enemy_model.h"
#include "../../include/sprite_manager.h"
#include <math.h>

// ============================================================================
// MODELO: INIMIGOS
// ----------------------------------------------------------------------------
// Pipeline de sprites (Fase 1): se houver um PNG do tipo do inimigo em
// Assets/Sprites/Enemies/..., ele é desenhado; caso contrário, recai no desenho
// procedural abaixo. Os tipos seguem os #define ETYPE_* em enemy.h e a cor base
// vem do ARQUÉTIPO centralizado (enemy.c), evitando paletas duplicadas aqui.
// ============================================================================

// Mapeia o tipo do inimigo para o SpriteID correspondente (-1 = sem mapeamento).
static SpriteID EnemySpriteFor(Enemy *enemy)
{
    switch (enemy->type)
    {
        case ETYPE_BACT_MELEE:  return SPR_BACT_MELEE;
        case ETYPE_BACT_RANGED: return SPR_BACT_RANGED;
        case ETYPE_KPC:         return (enemy->tier == TIER_3_BOSS) ? SPR_BACT_BOSS : SPR_BACT_RANGED;
        case ETYPE_VIRUS_MELEE:  return SPR_VIRUS_MELEE;
        case ETYPE_VIRUS_RANGED: return SPR_VIRUS_RANGED;
        case ETYPE_VIRUS_BOSS:   return SPR_VIRUS_BOSS;
        case ETYPE_VIRUS_SWARM:  return SPR_VIRUS_SWARM;
        case ETYPE_VIRUS_ELITE:  return SPR_VIRUS_ELITE;
        default:                 return (SpriteID)-1;
    }
}

// ============================================================================
// SILHUETAS PROCEDURAIS DOS VÍRUS — cada arquétipo tem forma/tamanho/animação
// próprios (fallback quando não há PNG). `spin` = relógio global (GetTime), usado
// para a rotação; `anim` = relógio por inimigo (enemy->animTime) para pulsações.
// ============================================================================

// 1) Enxame (rinovírus): capsídeo pequeno e compacto, poucas espículas curtas.
static void DrawVirusSwarm(Vector2 c, float r, float rot, float spin, Color body, Color edge, float alpha)
{
    float rr = rot + spin * 50.0f;
    DrawPoly(c, 6, r * 0.95f, rr, body);
    DrawPolyLinesEx(c, 6, r * 0.95f, rr, 2.0f, edge);
    for (int i = 0; i < 6; i++)
    {
        float a = (rr + i * 60.0f) * DEG2RAD;
        Vector2 e = { c.x + cosf(a) * r * 1.2f, c.y + sinf(a) * r * 1.2f };
        DrawLineEx(c, e, 2.0f, body);
        DrawCircleV(e, r * 0.14f, edge);
    }
    DrawCircleV(c, r * 0.4f, Fade((Color){ 70, 80, 20, 255 }, alpha)); // núcleo
}

// 2) Envelopado corpo a corpo (dengue): envelope lipídico + espículas em clube +
//    capsídeo icosaédrico interno + núcleo de RNA pulsante.
static void DrawVirusEnveloped(Vector2 c, float r, float rot, float spin, float anim, Color body, Color edge, float alpha)
{
    float rr = rot + spin * 6.0f;
    DrawCircleV(c, r * 1.18f, Fade(body, alpha * 0.30f));
    DrawCircleLines((int)c.x, (int)c.y, r * 1.18f, Fade(edge, alpha * 0.7f));
    int spk = 16;
    for (int i = 0; i < spk; i++)
    {
        float a = (rr + i * (360.0f / spk)) * DEG2RAD;
        Vector2 base = { c.x + cosf(a) * r * 1.02f, c.y + sinf(a) * r * 1.02f };
        Vector2 tip  = { c.x + cosf(a) * r * 1.30f, c.y + sinf(a) * r * 1.30f };
        DrawLineEx(base, tip, 3.0f, Fade(body, alpha));
        DrawCircleV(tip, r * 0.09f, Fade(edge, alpha));
    }
    DrawPoly(c, 12, r * 0.82f, -rr, body);
    DrawPolyLinesEx(c, 12, r * 0.82f, -rr, 2.0f, Fade(edge, alpha * 0.8f));
    float pls = 0.5f + 0.5f * sinf(anim * 3.0f);
    DrawCircleV(c, r * (0.34f + 0.05f * pls), Fade((Color){ 90, 20, 30, 255 }, alpha));
}

// 3) Atirador (influenza): dois tipos de espícula (HA pontuda + NA cogumelo) e
//    8 segmentos de RNA internos girando.
static void DrawVirusShooter(Vector2 c, float r, float rot, float spin, Color body, Color edge, float alpha)
{
    float rr = rot + spin * 7.0f;
    DrawCircleV(c, r * 0.95f, body);
    DrawCircleLines((int)c.x, (int)c.y, r * 0.95f, Fade(edge, alpha * 0.7f));
    int n = 16;
    for (int i = 0; i < n; i++)
    {
        float a = (rr + i * (360.0f / n)) * DEG2RAD;
        Vector2 base = { c.x + cosf(a) * r * 0.92f, c.y + sinf(a) * r * 0.92f };
        if (i % 2 == 0)
        {
            Vector2 tip = { c.x + cosf(a) * r * 1.40f, c.y + sinf(a) * r * 1.40f }; // HA pontuda
            DrawLineEx(base, tip, 2.5f, Fade(body, alpha));
            DrawCircleV(tip, r * 0.07f, Fade(edge, alpha));
        }
        else
        {
            Vector2 tip = { c.x + cosf(a) * r * 1.22f, c.y + sinf(a) * r * 1.22f }; // NA cogumelo
            DrawLineEx(base, tip, 2.0f, Fade(body, alpha));
            DrawCircleV(tip, r * 0.12f, Fade(body, alpha));
            DrawCircleLines((int)tip.x, (int)tip.y, r * 0.12f, Fade(edge, alpha));
        }
    }
    for (int s = 0; s < 8; s++)
    {
        float a = (rr * 1.5f + s * 45.0f) * DEG2RAD;
        Vector2 p = { c.x + cosf(a) * r * 0.35f, c.y + sinf(a) * r * 0.35f };
        DrawCircleV(p, r * 0.09f, Fade((Color){ 80, 40, 10, 255 }, alpha));
    }
}

// 4) Elite/mutante (sarampo): corpo disforme (polígonos sobrepostos), muitas
//    espículas de comprimentos variados e núcleo pulsante brilhante (mutação).
static void DrawVirusElite(Vector2 c, float r, float rot, float spin, float anim, Color body, Color edge, float alpha)
{
    float pls = 0.5f + 0.5f * sinf(anim * 4.0f);
    float rr = rot + spin * 5.0f;
    DrawCircleV(c, r * 1.55f, Fade(body, alpha * 0.14f)); // halo de elite
    DrawPoly(c, 11, r * 1.02f, rr, body);
    DrawPoly(c, 7, r * 0.92f, -rr * 0.6f, Fade(body, alpha * 0.9f));
    DrawPolyLinesEx(c, 11, r * 1.02f, rr, 3.5f, edge);
    int n = 20;
    for (int i = 0; i < n; i++)
    {
        float a = (rr + i * (360.0f / n)) * DEG2RAD;
        float len = 1.20f + (float)(i % 3) * 0.18f + pls * 0.10f;
        Vector2 base = { c.x + cosf(a) * r * 0.98f, c.y + sinf(a) * r * 0.98f };
        Vector2 tip  = { c.x + cosf(a) * r * len, c.y + sinf(a) * r * len };
        DrawLineEx(base, tip, 3.0f, Fade(body, alpha));
        DrawCircleV(tip, r * 0.10f, Fade(edge, alpha));
    }
    DrawCircleV(c, r * (0.42f + 0.10f * pls), Fade((Color){ 255, 180, 255, 255 }, alpha * (0.6f + 0.3f * pls)));
    DrawCircleV(c, r * 0.26f, Fade((Color){ 90, 20, 60, 255 }, alpha));
}

// 5) Chefe (coronavírus): coroa de espículas em clube; QUANDO O CAPSÍDEO ROMPE
//    (vida < 34%, fase final) muda visualmente — espículas quebradas/faltando e
//    RNA exposto brilhante com rachaduras.
static void DrawVirusBoss(Vector2 c, float r, float rot, float spin, float anim, Color body, Color edge, float alpha, int hp, int maxHp)
{
    float pct = (maxHp > 0) ? (float)hp / (float)maxHp : 1.0f;
    bool broken = (pct < 0.34f);
    float pls = 0.5f + 0.5f * sinf(anim * 4.0f);
    float rr = rot + spin * 4.0f;
    Color shell = broken ? (Color){ 120, 40, 90, 255 } : body;
    DrawCircleV(c, r * 1.35f, Fade(broken ? (Color){ 255, 120, 60, 255 } : body, alpha * (0.16f + (broken ? pls * 0.2f : 0.0f))));
    DrawCircleV(c, r, Fade(shell, alpha));
    DrawCircleLines((int)c.x, (int)c.y, r, Fade(edge, alpha * 0.8f));
    int spikes = 18;
    for (int i = 0; i < spikes; i++)
    {
        if (broken && (i % 3 == 0)) continue; // espículas perdidas ao romper
        float a = (rr + i * (360.0f / spikes)) * DEG2RAD;
        float len = broken ? 1.16f : 1.34f;
        Vector2 base = { c.x + cosf(a) * r * 0.98f, c.y + sinf(a) * r * 0.98f };
        Vector2 tip  = { c.x + cosf(a) * r * len, c.y + sinf(a) * r * len };
        DrawLineEx(base, tip, broken ? 3.0f : 4.0f, Fade(shell, alpha));
        DrawCircleV(tip, r * (broken ? 0.10f : 0.14f), Fade(edge, alpha)); // bulbo (clube)
    }
    for (int i = 0; i < 6; i++)
    {
        float a = (rr * 0.7f + i * 60.0f) * DEG2RAD;
        Vector2 p = { c.x + cosf(a) * r * 0.5f, c.y + sinf(a) * r * 0.5f };
        DrawCircleV(p, r * 0.10f, Fade(edge, alpha * 0.25f)); // depressões da superfície
    }
    if (broken)
    {
        DrawCircleV(c, r * (0.50f + 0.12f * pls), Fade((Color){ 255, 200, 90, 255 }, alpha * (0.5f + 0.4f * pls)));
        DrawCircleV(c, r * 0.34f, Fade((Color){ 120, 30, 20, 255 }, alpha));
        for (int i = 0; i < 5; i++)
        {
            float a = (i * 72.0f + rr) * DEG2RAD;
            Vector2 e = { c.x + cosf(a) * r * 0.95f, c.y + sinf(a) * r * 0.95f };
            DrawLineEx(c, e, 2.5f, Fade((Color){ 255, 220, 120, 255 }, alpha * 0.8f)); // rachadura
        }
    }
    else
    {
        DrawCircleV(c, r * 0.42f, Fade((Color){ 60, 20, 40, 255 }, alpha));
    }
}

void DrawEnemyModel(Enemy *enemy, Vector2 renderPos, float destSize, float rotation, float squashFactor, float alpha)
{
    float currentDestSize = destSize * squashFactor;

    // ---- Pipeline de sprites: usa o PNG se disponível ----
    SpriteID spr = EnemySpriteFor(enemy);
    if (spr != (SpriteID)-1 && SpriteAvailable(spr))
    {
        Color tint = Fade(WHITE, alpha);
        DrawSpriteCentered(spr, renderPos, (Vector2){ currentDestSize * 2.4f, currentDestSize * 2.4f }, rotation, tint);
        return;
    }

    // Cor base a partir do arquétipo centralizado (HURT pisca branco).
    const EnemyArchetype *arch = EnemyArchetypeFor(enemy->type);
    Color baseCol = arch ? arch->palette : RED;
    Color enemyCol = Fade((enemy->state == HURT) ? WHITE : baseCol, alpha);
    Color edgeCol = Fade(WHITE, alpha);
    float spin = (float)GetTime();

    // Tipo 0: Vírus (Círculo espinhoso)
    if (enemy->type == ETYPE_SARS) {
        DrawPoly(renderPos, 8, currentDestSize, rotation + GetTime()*10.0f, enemyCol);
        DrawPolyLinesEx(renderPos, 8, currentDestSize, rotation + GetTime()*10.0f, 3.0f, edgeCol);
        for (int i=0; i<8; i++) {
            float angle = (rotation + GetTime()*10.0f + i * 45.0f) * DEG2RAD;
            Vector2 end = { renderPos.x + cosf(angle) * currentDestSize * 1.3f, renderPos.y + sinf(angle) * currentDestSize * 1.3f };
            DrawLineEx(renderPos, end, 2.0f, enemyCol);
            DrawCircleV(end, 4.0f, edgeCol);
        }
    }
    // Tipo 1: Dengue / Mosquito (legado)
    else if (enemy->type == ETYPE_DENGUE_OLD) {
        DrawPoly(renderPos, 3, currentDestSize, rotation + GetTime()*20.0f, enemyCol);
        DrawPolyLines(renderPos, 3, currentDestSize, rotation + GetTime()*20.0f, edgeCol);
        DrawCircleV(renderPos, currentDestSize * 0.4f, RED);
    }
    // Tipo 2: KPC / Superbactéria (Tanque Hexagonal)
    else if (enemy->type == ETYPE_KPC) {
        int sides = 6;
        float rotSpeed = 2.0f;
        DrawPoly(renderPos, sides, currentDestSize, rotation + GetTime()*rotSpeed, enemyCol);
        DrawPolyLinesEx(renderPos, sides, currentDestSize, rotation + GetTime()*rotSpeed, 4.0f, edgeCol);
        DrawCircleV(renderPos, currentDestSize * 0.5f, BLACK);
    }
    // Tipo 3: Trypanosoma cruzi / Chagas (legado)
    else if (enemy->type == ETYPE_CHAGAS) {
        DrawPoly(renderPos, 4, currentDestSize, rotation + GetTime()*25.0f, enemyCol);
        DrawPoly(renderPos, 4, currentDestSize*0.7f, -rotation - GetTime()*25.0f, SKYBLUE);
    }
    // Tipo 4: Tuberculose (Pentágono)
    else if (enemy->type == ETYPE_TB) {
        DrawPoly(renderPos, 5, currentDestSize, rotation + GetTime()*5.0f, enemyCol);
        DrawPolyLines(renderPos, 5, currentDestSize, rotation + GetTime()*5.0f, edgeCol);
    }
    // Tipo 5: Bactéria corpo a corpo (cocos — aglomerado de esferas)
    else if (enemy->type == ETYPE_BACT_MELEE) {
        DrawCircleV(renderPos, currentDestSize, enemyCol);
        DrawCircleLines((int)renderPos.x, (int)renderPos.y, currentDestSize, edgeCol);
        // pequenos cocos satélites girando
        for (int i = 0; i < 4; i++) {
            float a = (rotation + GetTime() * 60.0f + i * 90.0f) * DEG2RAD;
            Vector2 c = { renderPos.x + cosf(a) * currentDestSize * 0.55f, renderPos.y + sinf(a) * currentDestSize * 0.55f };
            DrawCircleV(c, currentDestSize * 0.35f, Fade(WHITE, alpha * 0.5f));
        }
    }
    // Tipo 6: Bactéria à distância (bacilo — bastonete)
    else if (enemy->type == ETYPE_BACT_RANGED) {
        float ang = rotation * DEG2RAD;
        Vector2 dir = { cosf(ang), sinf(ang) };
        Vector2 a = { renderPos.x - dir.x * currentDestSize, renderPos.y - dir.y * currentDestSize };
        Vector2 b = { renderPos.x + dir.x * currentDestSize, renderPos.y + dir.y * currentDestSize };
        DrawLineEx(a, b, currentDestSize * 0.9f, enemyCol);
        DrawCircleV(a, currentDestSize * 0.45f, enemyCol);
        DrawCircleV(b, currentDestSize * 0.45f, enemyCol);
        DrawCircleV(renderPos, currentDestSize * 0.3f, Fade(BLACK, alpha * 0.6f));
    }
    // --- Mundo 2: cinco identidades virais distintas (silhuetas próprias) ---
    else if (enemy->type == ETYPE_VIRUS_SWARM) {
        DrawVirusSwarm(renderPos, currentDestSize, rotation, spin, enemyCol, edgeCol, alpha);
    }
    else if (enemy->type == ETYPE_VIRUS_MELEE) {
        DrawVirusEnveloped(renderPos, currentDestSize, rotation, spin, enemy->animTime, enemyCol, edgeCol, alpha);
    }
    else if (enemy->type == ETYPE_VIRUS_RANGED) {
        DrawVirusShooter(renderPos, currentDestSize, rotation, spin, enemyCol, edgeCol, alpha);
    }
    else if (enemy->type == ETYPE_VIRUS_ELITE) {
        DrawVirusElite(renderPos, currentDestSize, rotation, spin, enemy->animTime, enemyCol, edgeCol, alpha);
    }
    else if (enemy->type == ETYPE_VIRUS_BOSS) {
        DrawVirusBoss(renderPos, currentDestSize, rotation, spin, enemy->animTime, enemyCol, edgeCol, alpha, enemy->hp, enemy->maxHp);
    }
}
