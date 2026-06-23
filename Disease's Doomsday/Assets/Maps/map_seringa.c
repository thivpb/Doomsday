// map_seringa.c
// Implementação do Mapa: Interior da Seringa de Vacina (Tutorial)
// Disease's Doomsday — Projeto de Saúde Pública / DF
#include "map_seringa.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>

// ============================================================================
// ESTILO CENTRALIZADO (cores/parâmetros ajustáveis num só lugar)
// ============================================================================
// Ambiente de fundo (void). Defina 0 para REMOVER facilmente após feedback.
#ifndef SYRINGE_VOID_FX
#define SYRINGE_VOID_FX 1
#endif
#define SYR_METAL      (Color){ 178, 198, 214, 255 }  // metal base
#define SYR_METAL_DARK (Color){ 104, 124, 144, 255 }  // sombra do metal
#define SYR_METAL_HI   (Color){ 232, 244, 252, 255 }  // brilho do metal
#define SYR_GLASS_EDGE (Color){ 95, 125, 155, 255 }   // borda do vidro
#define SYR_HUB_PLAST  (Color){ 120, 200, 235, 255 }  // plástico do hub (azul)

// ----------------------------------------------------------------------------
// AMBIENTE DE FUNDO (VOID) — camadas orgânicas discretas atrás da seringa.
// Coordenadas de mundo; a câmera segue o herói, então elementos fixos dão um
// leve parallax natural. Tudo em baixa opacidade para não competir com o jogo.
// Estruturado para ser facilmente ajustado/removido (SYRINGE_VOID_FX).
// ----------------------------------------------------------------------------
#if SYRINGE_VOID_FX
static void DrawSyringeVoid(float time)
{
    // Região ampla cobrindo o que a câmera enxerga ao redor da seringa.
    const float RX = -900.0f, RY = -520.0f, RW = 3400.0f, RH = 1440.0f;

    // 1) Profundidade: base escura + gradiente azul-arroxeado muito sutil.
    DrawRectangle((int)RX, (int)RY, (int)RW, (int)RH, (Color){ 4, 6, 14, 255 });
    DrawRectangleGradientV((int)RX, (int)RY, (int)RW, (int)RH,
                           (Color){ 12, 14, 34, 255 }, (Color){ 3, 4, 10, 255 });

    // 2) Luz distante pulsante (glows suaves) — “iluminação ao fundo”.
    float p = 0.5f + 0.5f * sinf(time * 0.7f);
    DrawCircleGradient(320, -120, 520.0f + p * 40.0f, Fade((Color){ 40, 70, 150, 255 }, 0.10f + 0.04f * p), BLANK);
    DrawCircleGradient(1180, 700, 600.0f, Fade((Color){ 90, 40, 130, 255 }, 0.09f), BLANK);

    // 3) Membranas/células desfocadas (blobs grandes, baixa opacidade), em
    //    posições determinísticas com leve respiração.
    for (int i = 0; i < 12; i++)
    {
        float bx = RX + 220.0f + fmodf(i * 421.0f, RW - 440.0f);
        float by = RY + 160.0f + fmodf(i * 277.0f, RH - 320.0f);
        float br = 70.0f + (i % 5) * 34.0f + sinf(time * 0.6f + i) * 8.0f;
        Color c = (i % 2) ? (Color){ 70, 110, 200, 255 } : (Color){ 130, 80, 180, 255 };
        DrawCircleV((Vector2){ bx, by }, br, Fade(c, 0.05f));
        DrawCircleLines((int)bx, (int)by, br, Fade(c, 0.06f));
        DrawCircleV((Vector2){ bx, by }, br * 0.5f, Fade(c, 0.04f));
    }

    // 4) Correntes sanguíneas: linhas onduladas atravessando ao fundo.
    for (int s = 0; s < 3; s++)
    {
        Color vc = Fade((Color){ 150, 50, 70, 255 }, 0.10f);
        float baseY = RY + 300.0f + s * 380.0f;
        Vector2 prev = { RX, baseY };
        for (int k = 1; k <= 34; k++)
        {
            float xx = RX + k * (RW / 34.0f);
            float yy = baseY + sinf(xx * 0.004f + time * 0.5f + s) * 60.0f;
            Vector2 cur = { xx, yy };
            DrawLineEx(prev, cur, 7.0f, vc);
            prev = cur;
        }
    }

    // 5) Partículas celulares à deriva (parallax via mundo fixo + drift lento).
    for (int i = 0; i < 60; i++)
    {
        float depth = 0.3f + ((i * 37) % 100) / 100.0f * 0.7f; // 0.3..1.0
        float driftX = sinf(time * (0.15f + depth * 0.2f) + i) * 30.0f;
        float driftY = cosf(time * (0.12f + depth * 0.18f) + i * 1.3f) * 20.0f;
        float bx = RX + fmodf(i * 211.0f + time * 6.0f * depth, RW) + driftX;
        float by = RY + fmodf(i * 313.0f, RH) + driftY;
        float r = 1.5f + depth * 3.5f;
        Color c = (i % 3 == 0) ? (Color){ 120, 200, 255, 255 }
                : (i % 3 == 1) ? (Color){ 160, 140, 230, 255 }
                               : (Color){ 90, 150, 210, 255 };
        DrawCircleV((Vector2){ bx, by }, r, Fade(c, 0.06f + depth * 0.10f));
    }
}
#endif // SYRINGE_VOID_FX

// Desenha a agulha/bocal externos da seringa com leitura metálica clara:
// hub plástico (Luer) + cânula metálica com brilho + bisel afiado + gota.
static void DrawSyringeNeedle(float time, int tutorialStep)
{
    float cy = (float)SYRINGE_HEIGHT / 2.0f;     // eixo da agulha (~200)
    float hubR = SYR_LEFT;                        // borda direita do hub (encosta no cilindro)
    float hubL = SYR_NEEDLE_X - 6.0f;             // borda esquerda do hub (~-26)

    // --- Hub Luer (plástico translúcido), tronco que conecta vidro -> metal ---
    DrawTriangle((Vector2){ hubR, cy - 34.0f }, (Vector2){ hubL, cy - 20.0f }, (Vector2){ hubL, cy + 20.0f }, Fade(SYR_HUB_PLAST, 0.85f));
    DrawTriangle((Vector2){ hubR, cy - 34.0f }, (Vector2){ hubL, cy + 20.0f }, (Vector2){ hubR, cy + 34.0f }, Fade(SYR_HUB_PLAST, 0.85f));
    // brilho superior do hub
    DrawTriangle((Vector2){ hubR, cy - 34.0f }, (Vector2){ hubL, cy - 20.0f }, (Vector2){ hubR, cy - 18.0f }, Fade(SYR_METAL_HI, 0.5f));
    // roscas do Luer-lock (3 anéis)
    for (int k = 0; k < 3; k++)
    {
        float rx = hubL + 6.0f + k * 9.0f;
        DrawLineEx((Vector2){ rx, cy - 16.0f }, (Vector2){ rx, cy + 16.0f }, 2.0f, Fade(SYR_METAL_DARK, 0.7f));
    }
    DrawLineEx((Vector2){ hubL, cy - 20.0f }, (Vector2){ hubR, cy - 34.0f }, 2.0f, SYR_GLASS_EDGE);
    DrawLineEx((Vector2){ hubL, cy + 20.0f }, (Vector2){ hubR, cy + 34.0f }, 2.0f, SYR_GLASS_EDGE);

    // --- Cânula metálica (haste longa e fina indo para a esquerda) ---
    float tipX = hubL - 175.0f;       // ponta afiada
    float shaftTop = cy - 5.0f, shaftBot = cy + 5.0f;
    // corpo da cânula com gradiente (escuro embaixo, claro em cima => cilíndrico)
    DrawRectangleGradientV((int)tipX, (int)shaftTop, (int)(hubL - tipX), (int)(shaftBot - shaftTop),
                           SYR_METAL_HI, SYR_METAL_DARK);
    // brilho especular fino
    DrawRectangle((int)tipX, (int)(cy - 4.0f), (int)(hubL - tipX), 2, Fade(WHITE, 0.7f));
    DrawRectangleLines((int)tipX, (int)shaftTop, (int)(hubL - tipX), (int)(shaftBot - shaftTop), Fade(SYR_METAL_DARK, 0.6f));

    // --- Bisel afiado (ponta chanfrada) ---
    DrawTriangle((Vector2){ tipX - 26.0f, cy + 4.0f }, (Vector2){ tipX, shaftBot }, (Vector2){ tipX, shaftTop }, SYR_METAL);
    DrawTriangle((Vector2){ tipX - 26.0f, cy + 4.0f }, (Vector2){ tipX, shaftTop }, (Vector2){ tipX - 10.0f, cy - 6.0f }, SYR_METAL_HI);

    // --- Gota de vacina na ponta (vida/tema), pulsa lentamente ---
    float dpulse = 0.5f + 0.5f * sinf(time * 2.0f);
    DrawCircleV((Vector2){ tipX - 30.0f - dpulse * 3.0f, cy + 4.0f }, 3.0f + dpulse * 1.5f, Fade((Color){ 120, 210, 255, 255 }, 0.8f));
    (void)tutorialStep;
}

// ============================================================================
// DrawMapSeringa — Renderiza o interior da seringa (chamar dentro BeginMode2D)
// ============================================================================
void DrawMapSeringa(Font font, int tutorialStep, float time, float injectionTimer)
{
#if SYRINGE_VOID_FX
    DrawSyringeVoid(time);
#endif
    // -------------------------------------------------------------------------
    // LÍQUIDO DA VACINA (fundo translúcido azul médico)
    // -------------------------------------------------------------------------
    // Fundo azul médico mais intenso
    DrawRectangle(
        (int)SYR_LEFT, (int)SYR_WALL_TOP,
        (int)(SYR_RIGHT - SYR_LEFT),
        (int)(SYR_WALL_BOTTOM - SYR_WALL_TOP),
        (Color){ 10, 80, 140, 255 }
    );
    // Gradiente / Sobreposição para dar profundidade de líquido
    DrawRectangleGradientV(
        (int)SYR_LEFT, (int)SYR_WALL_TOP,
        (int)(SYR_RIGHT - SYR_LEFT),
        (int)(SYR_WALL_BOTTOM - SYR_WALL_TOP),
        Fade((Color){ 0, 180, 220, 255 }, 0.4f),
        Fade((Color){ 0, 50, 100, 255 }, 0.8f)
    );

    // Grade removida a pedido do usuário

    // Partículas flutuantes (bolhas do líquido da vacina)
    for (int i = 0; i < 40; i++)
    {
        // Movimento da direita para a esquerda e leve oscilação vertical
        float bx = SYR_LEFT + fmodf(i * 137.5f - time * (20.0f + (i%15)), SYR_RIGHT - SYR_LEFT);
        if (bx < SYR_LEFT) bx += (SYR_RIGHT - SYR_LEFT); // wrap
        
        float by = SYR_WALL_TOP + fmodf(i * 93.1f + sinf(time * 1.5f + i) * 15.0f, SYR_WALL_BOTTOM - SYR_WALL_TOP);
        float bradius = 2.0f + (i % 5);
        
        // Efeito de pulsação na bolha
        float alpha = 0.2f + 0.2f * sinf(time * 3.0f + i);
        
        DrawCircleV((Vector2){ bx, by }, bradius, Fade((Color){ 180, 240, 255, 255 }, alpha));
        // Brilho na bolha
        DrawCircleV((Vector2){ bx - bradius*0.3f, by - bradius*0.3f }, bradius*0.4f, Fade(WHITE, alpha * 1.5f));
    }

    // -------------------------------------------------------------------------
    // PAREDES LATERAIS DO CILINDRO (bordas plásticas cinza-metal)
    // -------------------------------------------------------------------------
    // Cima
    DrawRectangle((int)SYR_LEFT, 0,
                  (int)(SYR_RIGHT - SYR_LEFT), (int)SYR_WALL_TOP,
                  (Color){ 155, 175, 192, 255 });
    // Baixo
    DrawRectangle((int)SYR_LEFT, (int)SYR_WALL_BOTTOM,
                  (int)(SYR_RIGHT - SYR_LEFT), SYRINGE_HEIGHT - (int)SYR_WALL_BOTTOM,
                  (Color){ 155, 175, 192, 255 });

    // -------------------------------------------------------------------------
    // ÊMBOLO NA DIREITA (bloco cinza-escuro + anel de vedação)
    // -------------------------------------------------------------------------
    float plungerX = SYR_RIGHT;
    if (injectionTimer > 0.0f)
    {
        // Empurra o êmbolo rapidamente para a esquerda durante a cutscene (1.5s duração)
        float progress = injectionTimer / 1.5f;
        if (progress > 1.0f) progress = 1.0f;
        plungerX = SYR_RIGHT - (progress * (SYR_RIGHT - SYR_LEFT - 40.0f));
    }

    DrawRectangle((int)plungerX, 0, SYRINGE_WIDTH - (int)plungerX, SYRINGE_HEIGHT, (Color){ 128, 148, 162, 255 });
    // Anel de borracha do êmbolo (preto azulado)
    DrawRectangle(
        (int)(plungerX - 16), (int)(SYR_WALL_TOP - 8),
        24, (int)(SYR_WALL_BOTTOM - SYR_WALL_TOP + 16),
        (Color){ 40, 50, 60, 255 }
    );
    DrawRectangleLinesEx((Rectangle){ (int)plungerX, 0, SYRINGE_WIDTH - (int)plungerX, SYRINGE_HEIGHT }, 3.0f,
                          (Color){ 80, 100, 118, 255 });
    // Haste do êmbolo (centro)
    DrawRectangle((int)plungerX, SYRINGE_HEIGHT / 2 - 14, SYRINGE_WIDTH - (int)plungerX, 28,
                  (Color){ 100, 120, 138, 255 });

    // -------------------------------------------------------------------------
    // AFUNILAMENTO INFERIOR (paredes convergentes em direção ao bocal, agora na ESQUERDA)
    // -------------------------------------------------------------------------
    // Triângulo superior
    DrawTriangle(
        (Vector2){ SYR_TAPER_X, SYR_WALL_TOP },
        (Vector2){ SYR_LEFT, (float)SYRINGE_HEIGHT / 2.0f - 50.0f },
        (Vector2){ SYR_TAPER_X, 0.0f },
        (Color){ 155, 175, 192, 255 }
    );
    // Triângulo inferior
    DrawTriangle(
        (Vector2){ SYR_TAPER_X, SYRINGE_HEIGHT },
        (Vector2){ SYR_LEFT, (float)SYRINGE_HEIGHT / 2.0f + 50.0f },
        (Vector2){ SYR_TAPER_X, SYR_WALL_BOTTOM },
        (Color){ 155, 175, 192, 255 }
    );

    // -------------------------------------------------------------------------
    // BOCAL / AGULHA — desenhada por DrawSyringeNeedle (hub + cânula + bisel)
    // -------------------------------------------------------------------------
    DrawSyringeNeedle(time, tutorialStep);

    // -------------------------------------------------------------------------
    // GATILHO DE SAÍDA — pisca em verde no passo 2
    // -------------------------------------------------------------------------
    if (tutorialStep == 2)
    {
        float pulse = sinf(time * 5.0f) * 0.35f + 0.65f;
        DrawRectangle(
            (int)SYR_EXIT_X, (int)SYR_EXIT_Y,
            (int)SYR_EXIT_W, (int)SYR_EXIT_H,
            Fade((Color){ 0, 220, 100, 255 }, pulse * 0.42f)
        );
        DrawRectangleLinesEx(
            (Rectangle){ SYR_EXIT_X, SYR_EXIT_Y, SYR_EXIT_W, SYR_EXIT_H },
            2.5f, (Color){ 0, 220, 100, 255 }
        );
        // Seta indicativa para a ESQUERDA
        float arrowX = SYR_EXIT_X + SYR_EXIT_W / 2.0f - 10.0f;
        float arrowY = SYR_EXIT_Y + SYR_EXIT_H / 2.0f;
        DrawTriangle(
            (Vector2){ arrowX - 14, arrowY },
            (Vector2){ arrowX + 10, arrowY + 18 },
            (Vector2){ arrowX + 10, arrowY - 18 },
            Fade(WHITE, pulse * 0.85f)
        );
        Vector2 saidaSz = MeasureTextEx(font, "SAIDA", 14.0f, 1.0f);
        DrawTextEx(font, "SAIDA",
            (Vector2){ SYR_EXIT_X + SYR_EXIT_W / 2.0f - saidaSz.x / 2.0f, arrowY + 25 },
            14.0f, 1.0f, Fade(WHITE, 0.9f));
    }

    // -------------------------------------------------------------------------
    // MARCAÇÕES DE ESCALA (graduação em mL) ao longo do topo do cilindro.
    // -------------------------------------------------------------------------
    {
        int ticks = 10;
        float span = SYR_RIGHT - SYR_TAPER_X;          // graduação só no corpo reto
        for (int t = 0; t <= ticks; t++)
        {
            float gx = SYR_TAPER_X + (span * t) / ticks;
            bool major = (t % 2 == 0);
            float len = major ? 16.0f : 9.0f;
            DrawLineEx((Vector2){ gx, SYR_WALL_TOP + 6.0f }, (Vector2){ gx, SYR_WALL_TOP + 6.0f + len },
                       major ? 2.0f : 1.0f, Fade(SYR_METAL_HI, 0.55f));
            if (major)
            {
                char lbl[8];
                snprintf(lbl, sizeof(lbl), "%d", ticks - t);
                DrawTextEx(font, lbl, (Vector2){ gx + 3.0f, SYR_WALL_TOP + 8.0f }, 12.0f, 1.0f, Fade(SYR_METAL_HI, 0.5f));
            }
        }
    }

    // -------------------------------------------------------------------------
    // BORDA GERAL DO CILINDRO (highlight metálico) + flanges das extremidades
    // -------------------------------------------------------------------------
    DrawRectangleLinesEx(
        (Rectangle){ SYR_LEFT, SYR_WALL_TOP,
                     SYR_RIGHT - SYR_LEFT, SYR_WALL_BOTTOM - SYR_WALL_TOP },
        4.0f, SYR_GLASS_EDGE
    );
    // Flange (aba de apoio dos dedos) na extremidade direita do cilindro.
    DrawRectangle((int)SYR_RIGHT - 4, (int)SYR_WALL_TOP - 14, 12, 14, SYR_METAL_DARK);
    DrawRectangle((int)SYR_RIGHT - 4, (int)SYR_WALL_BOTTOM, 12, 14, SYR_METAL_DARK);

    // Reflexos de luz no vidro: uma faixa superior intensa e outra inferior sutil.
    DrawRectangle((int)SYR_LEFT + 8, (int)SYR_WALL_TOP + 4,
                  (int)(SYR_RIGHT - SYR_LEFT - 16), 6, Fade(WHITE, 0.22f));
    DrawRectangle((int)SYR_LEFT + 40, (int)SYR_WALL_BOTTOM - 12,
                  (int)(SYR_RIGHT - SYR_LEFT - 120), 4, Fade(WHITE, 0.08f));
}

// ============================================================================
// MapSeringa_CheckExit — Verifica se o jogador está na saída (bocal/agulha)
// ============================================================================
bool MapSeringa_CheckExit(Rectangle playerRect)
{
    Rectangle exitRect = { SYR_EXIT_X, SYR_EXIT_Y, SYR_EXIT_W, SYR_EXIT_H };
    return CheckCollisionRecs(playerRect, exitRect);
}

// ============================================================================
// MapSeringa_ApplyWallCollision — Empurra o jogador para dentro dos limites
// ============================================================================
void MapSeringa_ApplyWallCollision(Vector2 *pos, float r)
{
    float wallTop    = SYR_WALL_TOP;
    float wallBottom = SYR_WALL_BOTTOM;
    float wallLeft   = SYR_LEFT;
    float wallRight  = SYR_RIGHT;

    // Afunilamento: à esquerda de SYR_TAPER_X as paredes convergem no eixo Y
    if (pos->x < SYR_TAPER_X)
    {
        float t = (SYR_TAPER_X - pos->x) / (SYRINGE_WIDTH * 0.35f);
        if (t > 1.0f) t = 1.0f;
        wallTop    = SYR_WALL_TOP    + t * (SYRINGE_HEIGHT / 2.0f - 50.0f - SYR_WALL_TOP);
        wallBottom = SYR_WALL_BOTTOM - t * (SYR_WALL_BOTTOM - (SYRINGE_HEIGHT / 2.0f + 50.0f));
    }

    if (pos->x < wallLeft  + r) pos->x = wallLeft  + r;
    if (pos->x > wallRight - r) pos->x = wallRight - r;
    if (pos->y < wallTop   + r) pos->y = wallTop   + r;
    if (pos->y > wallBottom - r) pos->y = wallBottom - r;
}
