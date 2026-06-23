#include "weapons_model.h"
#include "rlgl.h"
#include "raymath.h"

// ============================================================================
// MODELO: ESPADA SERINGA
// ============================================================================
void DrawSyringeSword(Vector2 handPos, float size, float rotationDeg, Color liquidColor)
{
    rlPushMatrix();
    rlTranslatef(handPos.x, handPos.y, 0.0f);
    rlRotatef(rotationDeg, 0.0f, 0.0f, 1.0f);

    float s = size; // scale factor

    Color metal = (Color){ 200, 205, 215, 255 };
    Color metalDark = (Color){ 140, 150, 160, 255 };
    Color glass = Fade((Color){ 150, 220, 255, 255 }, 0.4f);
    Color liquid = liquidColor; // Skin da arma define o líquido da lâmina
    Color handle = (Color){ 180, 80, 20, 255 };
    Color gold = (Color){ 230, 180, 40, 255 };
    
    // Y axis points DOWN in screen coordinates.
    // We want the sword to point UP when rotationDeg = 0.
    // So the Tip (Needle) is at negative Y. The Pommel (Plunger) is at positive Y.
    
    // --- 1. Pommel (Anel do êmbolo no fundo) ---
    DrawCircleLines(0, s*0.6f, s*0.25f, metal);
    DrawCircleLines(0, s*0.6f, s*0.2f, metalDark);
    
    // --- 2. Grip (Cabo marrom onde a mão segura, centrado em 0,0) ---
    DrawRectangle(-s*0.15f, -s*0.35f, s*0.3f, s*0.7f, handle);
    DrawLine(-s*0.15f, -s*0.15f, s*0.15f, -s*0.15f, metalDark);
    DrawLine(-s*0.15f, s*0.15f, s*0.15f, s*0.15f, metalDark);

    // --- 3. Crossguard (Guarda / Flanges da seringa) ---
    DrawRectangle(-s*0.6f, -s*0.5f, s*1.2f, s*0.15f, metal);
    DrawRectangle(-s*0.6f, -s*0.45f, s*1.2f, s*0.05f, metalDark);
    // Pequenos detalhes nas pontas da guarda
    DrawRectangle(-s*0.8f, -s*0.5f, s*0.2f, s*0.05f, metal);
    DrawRectangle(s*0.6f, -s*0.5f, s*0.2f, s*0.05f, metal);
    
    // --- 4. Base Dourada da Lâmina ---
    DrawRectangle(-s*0.35f, -s*0.65f, s*0.7f, s*0.15f, gold);
    
    // --- 5. Blade (Cilindro de Vidro com Líquido) ---
    Rectangle glassRect = { -s*0.3f, -s*1.65f, s*0.6f, s*1.0f };
    Rectangle liquidRect = { -s*0.25f, -s*1.6f, s*0.5f, s*0.75f };
    DrawRectangleRec(liquidRect, liquid);
    // Linhas de medição
    for(int i=0; i<6; i++) {
        float ly = -s*1.55f + (s*0.12f * i);
        DrawLine(-s*0.25f, ly, -s*0.05f, ly, BLACK);
    }
    // Vidro por cima
    DrawRectangleLinesEx(glassRect, s*0.05f, metalDark);
    DrawRectangleRec(glassRect, glass);
    
    // --- 6. Base da Agulha (Topo do cilindro) ---
    DrawRectangle(-s*0.25f, -s*1.85f, s*0.5f, s*0.2f, metalDark);
    DrawRectangle(-s*0.15f, -s*1.95f, s*0.3f, s*0.1f, gold);
    
    // --- 7. Tip (Agulha) ---
    DrawTriangle(
        (Vector2){ -s*0.1f, -s*1.95f },
        (Vector2){ 0, -s*2.95f },
        (Vector2){ s*0.1f, -s*1.95f },
        metal
    );
    // Linha de brilho
    DrawLine(0, -s*1.95f, 0, -s*2.85f, WHITE);

    rlPopMatrix();
}

// ============================================================================
// MODELO: LÂMINA BIOELÉTRICA (variante melee anti-capsídeo desbloqueável)
// Bisturi/lâmina larga eletrificada que descarrega corrente no capsídeo viral.
// ============================================================================
void DrawScalpel(Vector2 handPos, float size, float rotationDeg, Color primary, Color secondary)
{
    rlPushMatrix();
    rlTranslatef(handPos.x, handPos.y, 0.0f);
    rlRotatef(rotationDeg, 0.0f, 0.0f, 1.0f);
    float s = size;

    Color metal = (Color){ 210, 220, 232, 255 };
    Color metalDk = (Color){ 120, 130, 145, 255 };
    Color handle = (Color){ 40, 44, 54, 255 };

    // Cabo
    DrawRectangleRounded((Rectangle){ -s*0.13f, -s*0.2f, s*0.26f, s*0.9f }, 0.5f, 6, handle);
    DrawRectangleRoundedLines((Rectangle){ -s*0.13f, -s*0.2f, s*0.26f, s*0.9f }, 0.5f, 6, metalDk);
    // Guarda com emissores estáticos (cores da skin)
    DrawRectangle((int)(-s*0.4f), (int)(-s*0.3f), (int)(s*0.8f), (int)(s*0.12f), metalDk);
    DrawCircleV((Vector2){ -s*0.4f, -s*0.24f }, s*0.1f, primary);
    DrawCircleV((Vector2){  s*0.4f, -s*0.24f }, s*0.1f, primary);
    // Lâmina larga (bisturi) apontando para cima (-Y)
    DrawTriangle(
        (Vector2){ -s*0.22f, -s*0.30f },
        (Vector2){ 0.0f,    -s*1.85f },
        (Vector2){  s*0.22f, -s*0.30f },
        metal);
    DrawTriangleLines(
        (Vector2){ -s*0.22f, -s*0.30f },
        (Vector2){ 0.0f,    -s*1.85f },
        (Vector2){  s*0.22f, -s*0.30f },
        metalDk);
    // Fio de corte (gume) brilhante
    DrawLineEx((Vector2){ 0.0f, -s*0.3f }, (Vector2){ 0.0f, -s*1.85f }, s*0.04f, secondary);
    // Arcos elétricos estáticos ao longo da lâmina (cores da skin)
    for (int i = 0; i < 3; i++)
    {
        float y = -s*0.6f - i * s*0.4f;
        float w = s*0.16f - i * s*0.03f;
        DrawLineEx((Vector2){ -w, y }, (Vector2){ w, y - s*0.1f }, s*0.03f, primary);
        DrawLineEx((Vector2){ w, y - s*0.1f }, (Vector2){ -w*0.6f, y - s*0.22f }, s*0.03f, secondary);
    }
    rlPopMatrix();
}

// Mundo atual para a arma melee segurada (slot 1).
static int s_modelWorld = 0; // 0 = WORLD_BACTERIA
void SetWeaponModelWorld(int world) { s_modelWorld = (world == 1) ? 1 : 0; }

// ============================================================================
// MODELO: ARMA SEGURADA POR TIPO (1=melee, 2=projétil, 3=granada, 4=BFG)
// As cores da skin (primary/secondary) sao aplicadas em TODOS os modelos.
// Desenhado no frame local: origem na mao, "para cima" = -Y.
// ============================================================================
void DrawHeldWeapon(int weapon, Vector2 handPos, float size, float rotationDeg, Color primary, Color secondary)
{
    // Armas melee independentes do Mundo: Lâmina Bioelétrica (ID 5, arcos
    // elétricos/estática) e Espada-Seringa (slot 1). A Lâmina é desbloqueável por
    // abates e usável em AMBOS os Mundos, então o modelo não depende mais de s_modelWorld.
    if (weapon == 5)
    {
        DrawScalpel(handPos, size, rotationDeg, primary, secondary);
        return;
    }
    if (weapon <= 1)
    {
        DrawSyringeSword(handPos, size, rotationDeg, primary);
        return;
    }

    rlPushMatrix();
    rlTranslatef(handPos.x, handPos.y, 0.0f);
    rlRotatef(rotationDeg, 0.0f, 0.0f, 1.0f);
    float s = size;

    if (weapon == 2)
    {
        // ---- RIFLE DE VACINA: capsula medica, trilhos de dose e ampola visivel ----
        Color body   = (Color){ 54, 66, 78, 255 };
        Color bodyDk = (Color){ 25, 31, 42, 255 };
        Color glass  = Fade((Color){ 180, 235, 255, 255 }, 0.42f);
        DrawRectangleRounded((Rectangle){ -s*0.20f, -s*0.05f, s*0.40f, s*0.72f }, 0.45f, 6, bodyDk);
        DrawRectangleRounded((Rectangle){ -s*0.32f, -s*0.72f, s*0.64f, s*0.74f }, 0.28f, 8, body);
        DrawRectangleRoundedLines((Rectangle){ -s*0.32f, -s*0.72f, s*0.64f, s*0.74f }, 0.28f, 8, bodyDk);
        DrawRectangleRounded((Rectangle){ -s*0.20f, -s*0.63f, s*0.40f, s*0.45f }, 0.45f, 8, primary);
        DrawRectangleRounded((Rectangle){ -s*0.14f, -s*0.58f, s*0.28f, s*0.34f }, 0.45f, 8, glass);
        for (int i = 0; i < 4; i++)
            DrawLineEx((Vector2){ -s*0.23f, -s*(0.55f - i*0.09f) }, (Vector2){ -s*0.08f, -s*(0.55f - i*0.09f) }, s*0.025f, bodyDk);
        DrawRectangle((int)(-s*0.08f), (int)(-s*1.65f), (int)(s*0.16f), (int)(s*0.95f), bodyDk);
        DrawRectangle((int)(-s*0.18f), (int)(-s*1.35f), (int)(s*0.36f), (int)(s*0.10f), secondary);
        DrawTriangle((Vector2){ -s*0.13f, -s*1.65f }, (Vector2){ 0, -s*2.02f }, (Vector2){ s*0.13f, -s*1.65f }, (Color){ 220, 230, 238, 255 });
        DrawCircleGradient(0, (int)(-s*1.72f), s*0.28f, Fade(primary, 0.75f), BLANK);
        DrawCircleV((Vector2){ 0, -s*1.72f }, s*0.10f, secondary);
        DrawRectangleRounded((Rectangle){ s*0.20f, -s*0.28f, s*0.18f, s*0.50f }, 0.35f, 6, bodyDk);
    }
    else if (weapon == 3)
    {
        // ---- DESESTABILIZADOR DE RNA: plantador de minas com bobina e cartucho ----
        Color body   = (Color){ 50, 42, 68, 255 };
        Color bodyDk = (Color){ 24, 20, 34, 255 };
        DrawRectangleRounded((Rectangle){ -s*0.17f, 0.0f, s*0.34f, s*0.62f }, 0.45f, 6, bodyDk);
        DrawRectangleRounded((Rectangle){ -s*0.34f, -s*1.08f, s*0.68f, s*1.08f }, 0.34f, 8, body);
        DrawRectangleRoundedLines((Rectangle){ -s*0.34f, -s*1.08f, s*0.68f, s*1.08f }, 0.34f, 8, secondary);
        for (int i = 0; i < 4; i++)
        {
            float yy = -s*(0.98f - i*0.18f);
            DrawLineEx((Vector2){ -s*0.25f, yy }, (Vector2){ s*0.25f, yy - s*0.08f }, s*0.035f, (i % 2) ? secondary : primary);
        }
        DrawCircleGradient(0, (int)(-s*0.45f), s*0.42f, Fade(primary, 0.55f), BLANK);
        DrawCircleV((Vector2){ 0, -s*0.45f }, s*0.20f, bodyDk);
        DrawCircleLines(0, (int)(-s*0.45f), s*0.23f, primary);
        DrawRectangleRounded((Rectangle){ -s*0.24f, -s*1.42f, s*0.48f, s*0.36f }, 0.45f, 8, bodyDk);
        DrawCircleV((Vector2){ 0, -s*1.48f }, s*0.26f, primary);
        DrawCircleV((Vector2){ 0, -s*1.48f }, s*0.11f, secondary);
        DrawLineEx((Vector2){ -s*0.32f, -s*1.48f }, (Vector2){ s*0.32f, -s*1.48f }, s*0.035f, bodyDk);
        DrawLineEx((Vector2){ 0, -s*1.80f }, (Vector2){ 0, -s*1.18f }, s*0.035f, bodyDk);
    }
    else
    {
        // ---- BFG IMUNOLOGICO: canhao pesado com nucleo e garfos emissores ----
        Color body   = (Color){ 48, 58, 70, 255 };
        Color bodyDk = (Color){ 20, 27, 36, 255 };
        Color metal  = (Color){ 170, 184, 196, 255 };
        DrawRectangleRounded((Rectangle){ -s*0.22f, 0.0f, s*0.44f, s*0.62f }, 0.45f, 6, bodyDk);
        DrawRectangleRounded((Rectangle){ -s*0.50f, -s*1.05f, s*1.00f, s*1.12f }, 0.28f, 8, body);
        DrawRectangleRoundedLines((Rectangle){ -s*0.50f, -s*1.05f, s*1.00f, s*1.12f }, 0.28f, 8, metal);
        DrawRectangleRounded((Rectangle){ -s*0.34f, -s*0.92f, s*0.68f, s*0.30f }, 0.35f, 6, bodyDk);
        DrawCircleGradient(0, (int)(-s*1.18f), s*0.72f, Fade(primary, 0.85f), BLANK);
        DrawCircleV((Vector2){ 0, -s*1.18f }, s*0.36f, primary);
        DrawCircleV((Vector2){ 0, -s*1.18f }, s*0.18f, secondary);
        DrawCircleLines(0, (int)(-s*1.18f), s*0.48f, secondary);
        DrawRectangle((int)(-s*0.48f), (int)(-s*1.86f), (int)(s*0.18f), (int)(s*0.86f), bodyDk);
        DrawRectangle((int)( s*0.30f), (int)(-s*1.86f), (int)(s*0.18f), (int)(s*0.86f), bodyDk);
        DrawCircleV((Vector2){ -s*0.39f, -s*1.90f }, s*0.13f, secondary);
        DrawCircleV((Vector2){  s*0.39f, -s*1.90f }, s*0.13f, secondary);
        DrawLineEx((Vector2){ -s*0.39f, -s*1.90f }, (Vector2){ 0, -s*1.52f }, s*0.045f, Fade(primary, 0.85f));
        DrawLineEx((Vector2){  s*0.39f, -s*1.90f }, (Vector2){ 0, -s*1.52f }, s*0.045f, Fade(primary, 0.85f));
        DrawRectangleRounded((Rectangle){ s*0.34f, -s*0.25f, s*0.20f, s*0.58f }, 0.3f, 6, bodyDk);
    }

    rlPopMatrix();
}

// Extensão vertical do modelo (em unidades de `size`) ACIMA (up, para -Y) e
// ABAIXO (down, para +Y) da âncora. Os modelos apontam a ponta para cima, então
// up >> down. Inclui folga para glows/auras.
static void WeaponVExtent(int weapon, float *up, float *down)
{
    if (weapon == 5)           { *up = 1.95f; *down = 0.85f; } // lâmina bioelétrica
    else if (weapon <= 1)      { *up = 2.95f; *down = 0.70f; } // espada-seringa (agulha longa)
    else if (weapon == 2)      { *up = 1.98f; *down = 0.76f; } // fuzil (muzzle)
    else if (weapon == 3)      { *up = 1.66f; *down = 0.66f; } // granada
    else                       { *up = 2.10f; *down = 0.66f; } // BFG (orbe/glow)
}

void DrawHeldWeaponFramed(int weapon, Rectangle frame, float maxSize, float rotationDeg,
                          Color primary, Color secondary)
{
    float up, down; WeaponVExtent(weapon, &up, &down);
    const float halfW = 0.9f;   // meia-largura aproximada do modelo (unid. de size)
    const float pad   = 14.0f;  // padding mínimo consistente em todos os lados

    float availH = frame.height - 2.0f * pad; if (availH < 8.0f) availH = 8.0f;
    float availW = frame.width  - 2.0f * pad; if (availW < 8.0f) availW = 8.0f;

    float size = maxSize;
    float fitH = availH / (up + down);     // cabe na altura
    float fitW = availW / (2.0f * halfW);  // cabe na largura
    if (fitH < size) size = fitH;
    if (fitW < size) size = fitW;

    // Âncora deslocada para BAIXO: centraliza o modelo top-heavy no retângulo.
    Vector2 anchor = { frame.x + frame.width * 0.5f,
                       frame.y + frame.height * 0.5f + (up - down) * 0.5f * size };
    DrawHeldWeapon(weapon, anchor, size, rotationDeg, primary, secondary);
}
