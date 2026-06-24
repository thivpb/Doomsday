#include "player_model.h"
#include "weapons_model.h"
#include "../../include/gameplay.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>

// ============================================================================
// PALETA DE CORES POR SKIN DO JOGADOR
// 0 = Padrão (cavaleiro branco)  |  1 = Médica  |  2 = Infectada/Biológica
// ============================================================================
typedef struct PlayerSkinPalette
{
    Color armor;
    Color armorDark;
    Color joint;
    Color detail;   // dourado / cruz / espinhos
    Color earTip;
} PlayerSkinPalette;

static PlayerSkinPalette GetPlayerSkinPalette(int skinId)
{
    PlayerSkinPalette p;
    switch (skinId)
    {
        case 1: // MÉDICA: jaleco branco, detalhes verde-saúde e cruz vermelha
            p.armor     = (Color){ 250, 252, 255, 255 };
            p.armorDark = (Color){ 185, 205, 210, 255 };
            p.joint     = (Color){ 60, 80, 85, 255 };
            p.detail    = (Color){ 225, 55, 60, 255 };   // cruz vermelha
            p.earTip    = (Color){ 0, 190, 130, 255 };   // verde-saúde
            break;
        case 2: // INFECTADA: armadura roxa corrompida com brilho tóxico
            p.armor     = (Color){ 110, 70, 150, 255 };
            p.armorDark = (Color){ 60, 35, 90, 255 };
            p.joint     = (Color){ 30, 15, 45, 255 };
            p.detail    = (Color){ 130, 220, 40, 255 };  // verde-ácido
            p.earTip    = (Color){ 130, 220, 40, 255 };
            break;
        default: // PADRÃO
            p.armor     = (Color){ 235, 240, 250, 255 };
            p.armorDark = (Color){ 160, 170, 190, 255 };
            p.joint     = (Color){ 40, 45, 50, 255 };
            p.detail    = (Color){ 230, 180, 50, 255 };  // dourado
            p.earTip    = (Color){ 30, 100, 200, 255 };  // azul
            break;
    }
    return p;
}

// ============================================================================
// GUARDA-ROUPA MODULAR — overlays cosméticos desenhados sobre o modelo base.
// Ancorados em pontos do corpo (cabeça/torso/pés/ombros) calculados pelo modelo,
// para alinhar nas duas poses (idle e movimento). Despacho centralizado a partir
// de player->cosmetics[], sem condicionais espalhadas pelo resto do jogo.
// ============================================================================
typedef struct PlayerAnchors {
    Vector2 head;  float headR;
    Vector2 torsoC; float torsoW, torsoH;
    Vector2 hipL, hipR;       // topo das pernas (acompanha os frames)
    Vector2 kneeL, kneeR;      // dobra real da perna
    Vector2 footL, footR;     // pés
    Vector2 shoulderL, shoulderR;
    Vector2 handL, handR;     // mãos
    Vector2 center; int dir; bool moving;
} PlayerAnchors;

#define COS_C_WHITE (Color){ 236, 242, 250, 255 }
#define COS_C_CYAN  (Color){ 0, 200, 255, 255 }
#define COS_C_GREEN (Color){ 40, 170, 90, 255 }
#define COS_C_GRAY  (Color){ 150, 165, 180, 255 }
#define COS_C_GOLD  (Color){ 230, 180, 50, 255 }
#define COS_C_MAG   (Color){ 230, 80, 200, 255 }
#define COS_C_PURP  (Color){ 150, 90, 210, 255 }

static Vector2 SafeNorm(Vector2 v, Vector2 fallback)
{
    float len = sqrtf(v.x * v.x + v.y * v.y);
    if (len <= 0.001f) return fallback;
    return (Vector2){ v.x / len, v.y / len };
}

static void DrawCapsuleSegment(Vector2 a, Vector2 b, float thick, Color col)
{
    DrawLineEx(a, b, thick, col);
    DrawCircleV(a, thick * 0.5f, col);
    DrawCircleV(b, thick * 0.5f, col);
}

static void DrawOrientedBox(Vector2 center, Vector2 dir, float halfLen, float halfWidth, Color col)
{
    dir = SafeNorm(dir, (Vector2){ 1.0f, 0.0f });
    Vector2 n = { -dir.y, dir.x };
    Vector2 p1 = Vector2Add(Vector2Add(center, Vector2Scale(dir, -halfLen)), Vector2Scale(n, -halfWidth));
    Vector2 p2 = Vector2Add(Vector2Add(center, Vector2Scale(dir,  halfLen)), Vector2Scale(n, -halfWidth));
    Vector2 p3 = Vector2Add(Vector2Add(center, Vector2Scale(dir,  halfLen)), Vector2Scale(n,  halfWidth));
    Vector2 p4 = Vector2Add(Vector2Add(center, Vector2Scale(dir, -halfLen)), Vector2Scale(n,  halfWidth));
    DrawTriangle(p1, p2, p3, col);
    DrawTriangle(p1, p3, p4, col);
}

// Capacete: VISOR (id3) preservado como referência; CONTENÇÃO (id1) e QUITINA
// (id2) agora são DOMOS orgânicos que envolvem a cabeça (sem retângulo solto),
// com placas laterais, segmentos e brilho — deixando rosto/visor legíveis.
static void CosHelmet(int id, PlayerAnchors a, float size, float t)
{
    if (id <= 0) return;
    Vector2 h = a.head; float r = a.headR; (void)size;
    if (id == 3) { // Visor Microscópico — PRESERVADO (referência de qualidade)
        DrawRectangleRounded((Rectangle){ h.x - r * 1.0f, h.y - r * 1.25f, r * 2.0f, r * 0.9f }, 0.5f, 6, (Color){ 60, 70, 86, 255 });
        DrawRectangleRounded((Rectangle){ h.x - r * 1.05f, h.y - r * 0.35f, r * 2.1f, r * 0.5f }, 0.4f, 4, Fade(COS_C_CYAN, 0.85f));
        DrawRectangle((int)(h.x - r * 1.05f), (int)(h.y - r * 0.18f), (int)(r * 2.1f), 2, Fade(WHITE, 0.6f + 0.4f * sinf(t * 4.0f)));
        return;
    }
    bool chitin = (id == 2);
    Color shell   = chitin ? (Color){ 46, 124, 66, 255 }  : (Color){ 232, 238, 246, 255 };
    Color shellHi = chitin ? (Color){ 92, 182, 112, 255 } : WHITE;
    Color shellDk = chitin ? (Color){ 24, 80, 44, 255 }   : (Color){ 168, 184, 200, 255 };

    Vector2 dome = { h.x, h.y - r * 0.10f };       // centro do domo (levemente acima)
    float R = r * 1.32f;
    // Domo (meia-esfera superior) cobrindo o topo até pouco acima dos olhos.
    DrawCircleSector(dome, R, 180.0f, 360.0f, 26, shell);
    DrawLineEx((Vector2){ dome.x - R, dome.y }, (Vector2){ dome.x + R, dome.y }, 2.5f, shellDk);
    // Placas laterais (protegem as bochechas; rosto/visor abertos no centro).
    DrawRectangleRounded((Rectangle){ h.x - r * 1.05f, dome.y, r * 0.42f, r * 0.95f }, 0.7f, 5, shell);
    DrawRectangleRounded((Rectangle){ h.x + r * 0.63f, dome.y, r * 0.42f, r * 0.95f }, 0.7f, 5, shell);
    // Segmentos/placas (anéis concêntricos) — leitura de carapaça.
    DrawRing(dome, R * 0.60f, R * 0.65f, 182.0f, 358.0f, 24, Fade(shellDk, 0.85f));
    DrawRing(dome, R * 0.90f, R * 0.95f, 182.0f, 358.0f, 24, Fade(shellDk, 0.55f));
    // Brilho (luz superior-esquerda) para volume.
    DrawCircleSector((Vector2){ dome.x - R * 0.32f, dome.y - R * 0.34f }, R * 0.40f, 200.0f, 320.0f, 12, Fade(shellHi, 0.45f));
    if (chitin) {
        // Crista de espinhos orgânicos no topo.
        for (int i = -1; i <= 1; i++) {
            float sx = dome.x + i * R * 0.42f;
            DrawTriangle((Vector2){ sx - r * 0.14f, dome.y - R * 0.80f },
                         (Vector2){ sx,             dome.y - R * 1.14f },
                         (Vector2){ sx + r * 0.14f, dome.y - R * 0.80f }, shellHi);
        }
    } else {
        // Lanterna frontal do capacete de contenção.
        DrawCircleV((Vector2){ dome.x + a.dir * r * 0.30f, dome.y - R * 0.45f }, r * 0.15f, COS_C_CYAN);
    }
}

static void CosFace(int id, PlayerAnchors a, float size, float t)
{
    if (id <= 0) return;
    (void)size;
    Vector2 h = a.head; float r = a.headR; (void)t;
    DrawRectangleRounded((Rectangle){ h.x - r * 0.85f, h.y + r * 0.05f, r * 1.7f, r * 0.7f }, 0.6f, 6,
                         (id == 2) ? (Color){ 70, 30, 70, 255 } : (id == 3) ? (Color){ 40, 120, 70, 255 } : COS_C_WHITE);
    // tiras laterais
    DrawLineEx((Vector2){ h.x - r * 0.85f, h.y + r * 0.2f }, (Vector2){ h.x - r * 1.15f, h.y - r * 0.1f }, 2.0f, COS_C_GRAY);
    DrawLineEx((Vector2){ h.x + r * 0.85f, h.y + r * 0.2f }, (Vector2){ h.x + r * 1.15f, h.y - r * 0.1f }, 2.0f, COS_C_GRAY);
    if (id == 2) DrawCircleV((Vector2){ h.x, h.y + r * 0.4f }, r * 0.22f, COS_C_MAG);              // filtro de plasma
    else if (id == 3) { DrawLineEx((Vector2){ h.x, h.y + r * 0.2f }, (Vector2){ h.x, h.y + r * 0.6f }, 2.0f, COS_C_GREEN);
                        DrawLineEx((Vector2){ h.x - r * 0.18f, h.y + r * 0.15f }, (Vector2){ h.x, h.y + r * 0.35f }, 2.0f, COS_C_GREEN);
                        DrawLineEx((Vector2){ h.x + r * 0.18f, h.y + r * 0.15f }, (Vector2){ h.x, h.y + r * 0.35f }, 2.0f, COS_C_GREEN); }
    else { DrawRectangle((int)(h.x - r * 0.5f), (int)(h.y + r * 0.32f), (int)(r), 2, COS_C_GRAY); } // vinco N95
}

static void CosChest(int id, PlayerAnchors a, float size, float t)
{
    if (id <= 0) return;
    (void)size;
    Vector2 c = a.torsoC; float w = a.torsoW, hh = a.torsoH;
    if (id == 1) { // Placa Celular
        DrawRectangleRounded((Rectangle){ c.x - w * 0.32f, c.y - hh * 0.18f, w * 0.64f, hh * 0.5f }, 0.3f, 6, Fade(COS_C_CYAN, 0.55f));
        DrawCircleLines((int)c.x, (int)c.y, w * 0.18f, Fade(WHITE, 0.6f));
    } else if (id == 2) { // Núcleo de DNA (hélice no peito)
        for (int i = 0; i < 5; i++) {
            float yy = c.y - hh * 0.16f + i * (hh * 0.1f);
            float ph = sinf(t * 2.0f + i * 0.9f);
            DrawCircleV((Vector2){ c.x - ph * w * 0.16f, yy }, 2.6f, COS_C_GREEN);
            DrawCircleV((Vector2){ c.x + ph * w * 0.16f, yy }, 2.6f, Fade(COS_C_GREEN, 0.7f));
        }
    } else { // Coldre Médico (bolsos)
        DrawRectangleRounded((Rectangle){ c.x - w * 0.34f, c.y + hh * 0.08f, w * 0.22f, hh * 0.26f }, 0.3f, 4, COS_C_GOLD);
        DrawRectangleRounded((Rectangle){ c.x + w * 0.12f, c.y + hh * 0.08f, w * 0.22f, hh * 0.26f }, 0.3f, 4, COS_C_GOLD);
    }
}

// Braços: luvas FITAM nas mãos (id1) ou braçadeiras no antebraço (id2). Pequenas,
// ancoradas em mão/ombro — acompanham a pose e não viram blocos soltos.
static void CosArms(int id, PlayerAnchors a, float size, float t)
{
    if (id <= 0) return;
    (void)t;
    for (int s = 0; s < 2; s++) {
        Vector2 hand = s ? a.handR : a.handL;
        Vector2 sh   = s ? a.shoulderR : a.shoulderL;
        if (id == 1) { // Luvas Cirúrgicas (na mão) + punho
            DrawCircleV(hand, size * 0.095f, COS_C_WHITE);
            DrawCircleLines((int)hand.x, (int)hand.y, size * 0.095f, Fade((Color){ 120, 200, 255, 255 }, 0.8f));
            Vector2 cuff = { hand.x + (sh.x - hand.x) * 0.22f, hand.y + (sh.y - hand.y) * 0.22f };
            DrawCircleV(cuff, size * 0.055f, COS_C_WHITE);
        } else { // Braçadeiras de Quitina (no antebraço)
            Vector2 fore = { sh.x + (hand.x - sh.x) * 0.55f, sh.y + (hand.y - sh.y) * 0.55f };
            DrawCircleV(fore, size * 0.085f, COS_C_GREEN);
            DrawCircleLines((int)fore.x, (int)fore.y, size * 0.085f, (Color){ 24, 80, 44, 255 });
            DrawCircleV((Vector2){ fore.x, fore.y - size * 0.05f }, size * 0.05f, (Color){ 70, 165, 92, 255 });
        }
    }
}

// Botas: sapato largo e baixo, com solado plano como nas referências. As cores
// são próprias da peça e não acompanham as skins base do personagem.
static void CosBoots(int id, PlayerAnchors a, float size, float t)
{
    if (id <= 0) return;
    (void)t;
    Color col = (id == 2) ? (Color){ 42, 132, 132, 255 } : (Color){ 222, 42, 35, 255 };
    Color hi  = (id == 2) ? (Color){ 80, 178, 174, 255 } : (Color){ 255, 94, 78, 255 };
    Color dk  = (id == 2) ? (Color){ 16, 66, 72, 255 }   : (Color){ 114, 22, 22, 255 };

    if (!a.moving)
    {
        Vector2 feet[2] = { a.footL, a.footR };
        float w = size * 0.27f;
        float h = size * 0.105f;
        for (int s = 0; s < 2; s++)
        {
            Rectangle boot = { feet[s].x - w * 0.5f, feet[s].y - h * 0.30f, w, h };
            DrawRectangleRounded(boot, 0.20f, 5, col);
            DrawRectangle((int)boot.x, (int)(boot.y + boot.height - size * 0.025f),
                          (int)boot.width, (int)(size * 0.026f), dk);
            DrawLineEx((Vector2){ boot.x + w * 0.10f, boot.y + h * 0.22f },
                       (Vector2){ boot.x + w * 0.34f, boot.y + h * 0.10f },
                       size * 0.012f, Fade(hi, 0.70f));
            DrawLineEx((Vector2){ boot.x, boot.y + boot.height },
                       (Vector2){ boot.x + boot.width, boot.y + boot.height },
                       size * 0.014f, Fade(BLACK, 0.50f));
        }
        return;
    }

    for (int s = 0; s < 2; s++) {
        Vector2 kneeP = s ? a.kneeR : a.kneeL;
        Vector2 foot = s ? a.footR : a.footL;
        if (kneeP.x == 0.0f && kneeP.y == 0.0f)
            kneeP = Vector2Add(foot, (Vector2){ 0.0f, -size * 0.25f });

        Vector2 ankle = Vector2Lerp(kneeP, foot, 0.76f);
        Vector2 toeDir = a.moving ? (Vector2){ (float)a.dir, 0.18f } : (Vector2){ (s ? 1.0f : -1.0f) * 0.34f, 0.16f };
        toeDir = SafeNorm(toeDir, (Vector2){ (float)a.dir, 0.0f });
        Vector2 soleCenter = Vector2Add(foot, Vector2Scale(toeDir, size * 0.035f));

        DrawCapsuleSegment(ankle, foot, size * 0.105f, col);
        DrawOrientedBox(soleCenter, toeDir, size * 0.155f, size * 0.050f, col);
        DrawOrientedBox(Vector2Add(soleCenter, Vector2Scale((Vector2){ -toeDir.y, toeDir.x }, size * 0.035f)),
                        toeDir, size * 0.150f, size * 0.017f, dk);
        DrawLineEx(Vector2Add(soleCenter, Vector2Scale(toeDir, -size * 0.12f)),
                   Vector2Add(soleCenter, Vector2Scale(toeDir,  size * 0.12f)),
                   size * 0.018f, Fade(BLACK, 0.30f));
    }
}

// Efeitos visuais. Envolvem o CORPO INTEIRO (cabeça->pés) acompanhando o herói.
static void CosFX(int id, PlayerAnchors a, float size, float t)
{
    if (id <= 0) return;
    // Bounding vertical do corpo para envolver tudo (sem afetar hitbox/colisão).
    float top = a.head.y - a.headR * 1.9f;
    float bot = (a.footL.y > a.footR.y ? a.footL.y : a.footR.y) + size * 0.06f;
    Vector2 bc = { a.center.x, (top + bot) * 0.5f };
    float bodyR = (bot - top) * 0.5f;

    if (id == 1) { // Aura de Anticorpos — ultrapassa a silhueta, pulsa, leve
        float pulse = 0.5f + 0.5f * sinf(t * 2.6f);
        float R = bodyR + size * 0.22f + pulse * size * 0.08f;
        DrawCircleGradient((int)bc.x, (int)bc.y, R, Fade(COS_C_CYAN, 0.10f + 0.05f * pulse), BLANK);
        DrawCircleLines((int)bc.x, (int)bc.y, R, Fade(COS_C_CYAN, 0.26f + 0.12f * pulse));
        DrawCircleLines((int)bc.x, (int)bc.y, R * 0.93f, Fade(COS_C_CYAN, 0.13f));
    } else if (id == 2) { // Hélice de DNA — PRESERVADA (só escala/posição responsiva)
        for (int i = 0; i < 8; i++) {
            float ang = t * 2.0f + i * (PI / 4.0f);
            float rr = size * 0.42f;
            DrawCircleV((Vector2){ a.center.x + cosf(ang) * rr, a.center.y + sinf(ang * 2.0f) * size * 0.16f },
                        size * 0.04f, Fade(COS_C_GREEN, 0.8f));
        }
    } else { // Partículas de Plasma — distribuídas por todo o corpo (cap. de perf.)
        const int N = 16;
        for (int i = 0; i < N; i++) {
            float ang = t * 1.2f + i * (2.0f * PI / N);
            float wob = 0.82f + 0.18f * sinf(t * 2.4f + i * 1.3f);
            float rx = size * 0.62f * wob;
            float ry = bodyR * 0.96f * wob;
            Vector2 p = { bc.x + cosf(ang) * rx, bc.y + sinf(ang) * ry };
            float sz = 2.0f + 1.6f * (0.5f + 0.5f * sinf(t * 3.0f + i));
            DrawCircleV(p, sz, Fade(COS_C_MAG, 0.45f + 0.3f * sinf(t * 2.0f + i)));
        }
    }
}

// Despacho central (ordem de camadas). A categoria TRASEIRO foi removida do jogo;
// player->cosmetics[COS_BACK] é mantido só para compatibilidade de saves e NÃO é
// desenhado nem exposto na UI.
static void DrawPlayerCosmetics(Player *player, PlayerAnchors a, float size, float time)
{
    // COS_LEGS foi removido do guarda-roupa: saves antigos podem ter valor salvo,
    // mas a peça não é mais desenhada para evitar encaixes ruins no modelo.
    CosBoots(player->cosmetics[COS_BOOTS], a, size, time);
    CosChest(player->cosmetics[COS_CHEST], a, size, time);
    CosArms(player->cosmetics[COS_ARMS], a, size, time);
    CosHelmet(player->cosmetics[COS_HELMET], a, size, time);
    CosFace(player->cosmetics[COS_FACE], a, size, time);
    CosFX(player->cosmetics[COS_FX], a, size, time);
}

// ============================================================================
// MODELO: JOGADOR (Estilo Cavaleiro Coelho)
// ============================================================================
void DrawPlayerModel(Player *player, float size, Color tint, float time, float attackAnimTimer)
{
    Vector2 pPos = player->position;
    
    // Animação
    float walkCycle = player->isMoving ? time * 12.0f : 0.0f;
    float bob = player->isMoving ? fabs(sinf(walkCycle)) * (size * 0.1f) : sinf(time * 3.0f) * (size * 0.03f); // Idle bobbing

    // Direção (-1 esq, 1 dir)
    int dir = (player->facingDir != 0) ? player->facingDir : 1;
    
    // Cores baseadas na imagem anexada
    rlPushMatrix();
    rlTranslatef(pPos.x, pPos.y, 0.0f);
    rlScalef(player->squashX, player->squashY, 1.0f);
    rlTranslatef(-pPos.x, -pPos.y, 0.0f);
    
    // Cores definidas pela skin selecionada do jogador
    PlayerSkinPalette pal = GetPlayerSkinPalette(player->skinId);
    Color colorArmor = pal.armor;          // Base da armadura
    Color colorArmorDark = pal.armorDark;  // Sombra da armadura
    Color colorJoint = pal.joint;          // Juntas escuras
    Color colorVisor = (player->skinId == 2) ? (Color){ 130, 220, 40, 255 } : BLACK; // Visor brilha na skin infectada
    Color colorDetail = pal.detail;        // Detalhes (fivela, cruz, espinhos)
    Color colorBlueTip = pal.earTip;       // Ponta da orelha

    // Cores da skin da arma (aplicadas a TODOS os modelos de arma segurada)
    Color swordLiquid = WeaponSkinPrimary(player->weaponSkinId);
    Color swordGlow   = WeaponSkinSecondary(player->weaponSkinId);
    // Aceita TODAS as armas (1..WEAPON_COUNT), incluindo as evoluções de "fase 2"
    // (6 Rifle Vetorial Replicante, 7 Lança-Minas de RNA, 8 BFG Ômega) — cada uma
    // tem seu próprio modelo em DrawHeldWeapon. Antes o clamp em <=5 fazia a arma
    // segurada cair de volta para a Espada-Seringa durante o gameplay.
    int   heldWeapon  = (player->equippedWeapon >= 1 && player->equippedWeapon <= WEAPON_COUNT) ? player->equippedWeapon : 1;

    // Pontos de ancoragem para os cosméticos (preenchidos por pose abaixo).
    PlayerAnchors anch = { 0 };
    anch.center = pPos; anch.dir = dir;

    if (!player->isMoving) {
        // ========================================================
        // MODO IDLE (Front-facing)
        // ========================================================
        
        // 1. Pernas (Lado a lado)
        // Perna Esq
        Rectangle legL = { pPos.x - size*0.25f, pPos.y + size*0.3f + bob, size*0.2f, size*0.35f };
        DrawRectangleRounded(legL, 0.5f, 4, colorArmorDark);
        DrawRectangleRounded((Rectangle){legL.x, legL.y, size*0.15f, size*0.3f}, 0.5f, 4, colorArmor); // Highlight
        // Pé Esq
        DrawRectangleRounded((Rectangle){ legL.x - size*0.05f, legL.y + size*0.25f, size*0.25f, size*0.1f }, 0.5f, 4, colorArmor);
        
        // Perna Dir
        Rectangle legR = { pPos.x + size*0.05f, pPos.y + size*0.3f + bob, size*0.2f, size*0.35f };
        DrawRectangleRounded(legR, 0.5f, 4, colorArmorDark);
        DrawRectangleRounded((Rectangle){legR.x + size*0.05f, legR.y, size*0.15f, size*0.3f}, 0.5f, 4, colorArmor); // Highlight
        // Pé Dir
        DrawRectangleRounded((Rectangle){ legR.x, legR.y + size*0.25f, size*0.25f, size*0.1f }, 0.5f, 4, colorArmor);

        // 2. Braços
        // Braço Esq (mão direita do personagem, lado esquerdo da tela)
        Vector2 armLStart = { pPos.x - size*0.35f, pPos.y - size*0.05f + bob };
        Vector2 armLHand = { pPos.x - size*0.35f, pPos.y + size*0.2f + bob };
        float swordAngle = 0.0f; // apontando para cima

        if (attackAnimTimer > 0.0f) {
            float animT = attackAnimTimer / 0.22f; // de 1.0 a 0.0
            // Mão levanta para fazer o slash em arco por cima da cabeça
            armLHand.x = pPos.x - size*0.4f + (1.0f - animT) * size*0.8f;
            armLHand.y = pPos.y - size*0.2f - sinf((1.0f - animT) * PI) * size*0.4f;
            swordAngle = dir == 1 ? (-90.0f + (1.0f - animT) * 180.0f) : (90.0f - (1.0f - animT) * 180.0f);
        }

        DrawLineEx(armLStart, armLHand, size*0.12f, colorArmorDark);
        DrawCircleV(armLStart, size*0.15f, colorArmorDark); // Ombro
        DrawHeldWeapon(heldWeapon, armLHand, size*0.5f, swordAngle, swordLiquid, swordGlow); // Arma segurada (varia por tipo)
        DrawCircleV(armLHand, size*0.08f, colorArmor); // Mão segura a espada
        
        // Braço Dir (mão esquerda do personagem, lado direito da tela)
        Vector2 armRStart = { pPos.x + size*0.35f, pPos.y - size*0.05f + bob };
        Vector2 armRHand = { pPos.x + size*0.35f, pPos.y + size*0.2f + bob };
        DrawLineEx(armRStart, armRHand, size*0.12f, colorArmorDark);
        DrawCircleV(armRHand, size*0.08f, colorArmor); // Mão
        DrawCircleV(armRStart, size*0.15f, colorArmorDark); // Ombro

        // 3. Torso e Cinto
        Rectangle torso = { pPos.x - size*0.3f, pPos.y - size*0.1f + bob, size*0.6f, size*0.45f };
        DrawRectangleRounded(torso, 0.4f, 4, colorArmor);
        // Peitoral detalhes (removidos os círculos que pareciam um sutiã)
        // Linha divisória da armadura
        DrawLineEx((Vector2){pPos.x - size*0.25f, pPos.y + size*0.05f + bob}, 
                   (Vector2){pPos.x + size*0.25f, pPos.y + size*0.05f + bob}, size*0.02f, colorArmorDark);
        // Cinto
        DrawRectangle(torso.x, torso.y + torso.height - size*0.1f, torso.width, size*0.08f, colorJoint);
        DrawCircle(pPos.x, torso.y + torso.height - size*0.06f, size*0.05f, colorDetail); // Fivela

        // Detalhe exclusivo por skin (peito)
        if (player->skinId == 1) {
            // Cruz médica vermelha no peitoral
            DrawRectangle(pPos.x - size*0.03f, torso.y + size*0.06f, size*0.06f, size*0.2f, colorDetail);
            DrawRectangle(pPos.x - size*0.1f, torso.y + size*0.13f, size*0.2f, size*0.06f, colorDetail);
        } else if (player->skinId == 2) {
            // Pústulas tóxicas pulsantes na armadura corrompida
            float pulse = 0.8f + sinf(time * 6.0f) * 0.2f;
            DrawCircle(pPos.x - size*0.15f, torso.y + size*0.12f, size*0.05f * pulse, colorDetail);
            DrawCircle(pPos.x + size*0.12f, torso.y + size*0.2f, size*0.04f * pulse, colorDetail);
            DrawCircle(pPos.x + size*0.02f, torso.y + size*0.3f, size*0.03f * pulse, colorDetail);
        }

        // 4. Cabeça e Elmo
        Vector2 headPos = { pPos.x, pPos.y - size*0.35f + bob };
        DrawRectangleRounded((Rectangle){ headPos.x - size*0.25f, headPos.y - size*0.25f, size*0.5f, size*0.5f }, 0.5f, 4, colorArmor);
        // Visor T
        DrawRectangle(headPos.x - size*0.1f, headPos.y - size*0.05f, size*0.2f, size*0.08f, colorVisor); // Linha horizontal
        DrawRectangle(headPos.x - size*0.04f, headPos.y - size*0.05f, size*0.08f, size*0.2f, colorVisor); // Linha vertical

        // Orelhas de coelho
        // Esq
        Vector2 earLStart = { headPos.x - size*0.12f, headPos.y - size*0.2f };
        Vector2 earLEnd = { earLStart.x - size*0.15f, earLStart.y - size*0.35f };
        DrawLineEx(earLStart, earLEnd, size*0.12f, colorArmor);
        DrawLineEx((Vector2){earLStart.x+size*0.02f, earLStart.y}, (Vector2){earLEnd.x+size*0.02f, earLEnd.y}, size*0.04f, WHITE); // brilho
        // Ponta Y azul
        Vector2 yL1 = { earLEnd.x - size*0.06f, earLEnd.y - size*0.08f };
        Vector2 yL2 = { earLEnd.x + size*0.06f, earLEnd.y - size*0.08f };
        DrawLineEx(earLEnd, yL1, size*0.06f, colorBlueTip);
        DrawLineEx(earLEnd, yL2, size*0.06f, colorBlueTip);
        
        // Dir
        Vector2 earRStart = { headPos.x + size*0.12f, headPos.y - size*0.2f };
        Vector2 earREnd = { earRStart.x + size*0.15f, earRStart.y - size*0.35f };
        DrawLineEx(earRStart, earREnd, size*0.12f, colorArmor);
        DrawLineEx((Vector2){earRStart.x-size*0.02f, earRStart.y}, (Vector2){earREnd.x-size*0.02f, earREnd.y}, size*0.04f, WHITE); // brilho
        // Ponta Y azul
        Vector2 yR1 = { earREnd.x - size*0.06f, earREnd.y - size*0.08f };
        Vector2 yR2 = { earREnd.x + size*0.06f, earREnd.y - size*0.08f };
        DrawLineEx(earREnd, yR1, size*0.06f, colorBlueTip);
        DrawLineEx(earREnd, yR2, size*0.06f, colorBlueTip);

        // Âncoras (pose idle / frontal)
        anch.head = headPos; anch.headR = size * 0.25f;
        anch.torsoC = (Vector2){ pPos.x, torso.y + torso.height * 0.5f };
        anch.torsoW = torso.width; anch.torsoH = torso.height;
        anch.hipL = (Vector2){ legL.x + legL.width * 0.5f, legL.y + size * 0.02f };
        anch.hipR = (Vector2){ legR.x + legR.width * 0.5f, legR.y + size * 0.02f };
        anch.kneeL = (Vector2){ legL.x + legL.width * 0.5f, legL.y + size * 0.18f };
        anch.kneeR = (Vector2){ legR.x + legR.width * 0.5f, legR.y + size * 0.18f };
        anch.footL = (Vector2){ legL.x + legL.width * 0.5f - size * 0.02f, legL.y + size * 0.315f };
        anch.footR = (Vector2){ legR.x + legR.width * 0.5f + size * 0.02f, legR.y + size * 0.315f };
        anch.shoulderL = armLStart; anch.shoulderR = armRStart;
        anch.handL = armLHand; anch.handR = armRHand;
        anch.moving = false;

    } else {
        // ========================================================
        // MODO MOVIMENTO (Side-facing)
        // ========================================================

    // 1. Braço de Trás
    float armBackAngle = sinf(walkCycle) * 0.5f;
    Vector2 armBackStart = { pPos.x - dir*size*0.1f, pPos.y - size*0.1f + bob };
    Vector2 armBackElbow = { armBackStart.x + sinf(armBackAngle)*size*0.2f, armBackStart.y + cosf(armBackAngle)*size*0.2f };
    Vector2 armBackHand = { armBackElbow.x + sinf(armBackAngle - 0.2f*dir)*size*0.2f, armBackElbow.y + cosf(armBackAngle - 0.2f*dir)*size*0.2f };
    DrawLineEx(armBackStart, armBackElbow, size*0.1f, colorArmorDark);
    DrawLineEx(armBackElbow, armBackHand, size*0.08f, colorArmorDark);
    DrawCircleV(armBackHand, size*0.06f, colorArmorDark); // Mão de trás

        // Perna de Trás
        float legBackAngle = sinf(walkCycle + PI) * 0.5f;
        Vector2 legBackStart = { pPos.x - dir*size*0.1f, pPos.y + size*0.2f + bob };
        Vector2 legBackKnee = { legBackStart.x + sinf(legBackAngle)*size*0.25f, legBackStart.y + cosf(legBackAngle)*size*0.25f };
        Vector2 legBackFoot = { legBackKnee.x + sinf(legBackAngle + 0.2f*dir)*size*0.25f, legBackKnee.y + cosf(legBackAngle + 0.2f*dir)*size*0.25f };
        DrawLineEx(legBackStart, legBackKnee, size*0.15f, colorArmorDark);
        DrawCircleV(legBackKnee, size*0.07f, colorJoint);
        DrawLineEx(legBackKnee, legBackFoot, size*0.12f, colorArmorDark);

        // 2. Torso
        Rectangle torso = { pPos.x - size*0.2f, pPos.y - size*0.15f + bob, size*0.4f, size*0.45f };
        DrawRectangleRounded(torso, 0.4f, 4, colorArmor);
        // Cinto (agora visível de lado)
        DrawRectangle(torso.x, torso.y + torso.height - size*0.1f, torso.width, size*0.08f, colorJoint);
        DrawCircle(pPos.x + dir*size*0.15f, torso.y + torso.height - size*0.06f, size*0.04f, colorDetail); // Fivela lateral

        // Detalhe exclusivo por skin (lateral)
        if (player->skinId == 1) {
            // Cruz médica vermelha lateral
            DrawRectangle(pPos.x - size*0.025f, torso.y + size*0.08f, size*0.05f, size*0.16f, colorDetail);
            DrawRectangle(pPos.x - size*0.08f, torso.y + size*0.135f, size*0.16f, size*0.05f, colorDetail);
        } else if (player->skinId == 2) {
            float pulse = 0.8f + sinf(time * 6.0f) * 0.2f;
            DrawCircle(pPos.x - dir*size*0.08f, torso.y + size*0.14f, size*0.045f * pulse, colorDetail);
            DrawCircle(pPos.x + dir*size*0.05f, torso.y + size*0.26f, size*0.035f * pulse, colorDetail);
        }

        // Perna da Frente
        float legFrontAngle = sinf(walkCycle) * 0.5f;
        Vector2 legFrontStart = { pPos.x + dir*size*0.1f, pPos.y + size*0.2f + bob };
        Vector2 legFrontKnee = { legFrontStart.x + sinf(legFrontAngle)*size*0.25f, legFrontStart.y + cosf(legFrontAngle)*size*0.25f };
        Vector2 legFrontFoot = { legFrontKnee.x + sinf(legFrontAngle + 0.2f*dir)*size*0.25f, legFrontKnee.y + cosf(legFrontAngle + 0.2f*dir)*size*0.25f };
        DrawLineEx(legFrontStart, legFrontKnee, size*0.16f, colorArmor);
        DrawCircleV(legFrontKnee, size*0.08f, colorJoint);
        DrawLineEx(legFrontKnee, legFrontFoot, size*0.14f, colorArmor);

        // 3. Cabeça / Elmo
        Vector2 headPos = { pPos.x + dir * size*0.05f, pPos.y - size*0.35f + bob };
        DrawRectangleRounded((Rectangle){ headPos.x - size*0.2f, headPos.y - size*0.25f, size*0.4f, size*0.5f }, 0.5f, 4, colorArmor);
        
        // Visor
        Vector2 visorPos = { headPos.x + dir * size*0.1f, headPos.y - size*0.05f };
        DrawRectangle(visorPos.x - (dir==1 ? 0 : size*0.15f), visorPos.y, size*0.15f, size*0.08f, colorVisor);
        DrawRectangle(visorPos.x + dir*size*0.05f - (dir==1 ? 0 : size*0.05f), visorPos.y - size*0.05f, size*0.05f, size*0.15f, colorVisor);

        // Orelhas de coelho (Side-facing)
        // Orelha trás
        Vector2 earBStart = { headPos.x - dir*size*0.05f, headPos.y - size*0.2f };
        Vector2 earBEnd = { earBStart.x - dir*size*0.1f, earBStart.y - size*0.3f };
        DrawLineEx(earBStart, earBEnd, size*0.1f, colorArmorDark);
        // Ponta Y
        Vector2 yB1 = { earBEnd.x - size*0.05f, earBEnd.y - size*0.07f };
        Vector2 yB2 = { earBEnd.x + size*0.05f, earBEnd.y - size*0.07f };
        DrawLineEx(earBEnd, yB1, size*0.05f, colorBlueTip);
        DrawLineEx(earBEnd, yB2, size*0.05f, colorBlueTip);

        // Orelha frente
        Vector2 earFStart = { headPos.x + dir*size*0.05f, headPos.y - size*0.2f };
        Vector2 earFEnd = { earFStart.x - dir*size*0.05f, earFStart.y - size*0.35f };
        DrawLineEx(earFStart, earFEnd, size*0.12f, colorArmor);
        // Ponta Y
        Vector2 yF1 = { earFEnd.x - size*0.06f, earFEnd.y - size*0.08f };
        Vector2 yF2 = { earFEnd.x + size*0.06f, earFEnd.y - size*0.08f };
        DrawLineEx(earFEnd, yF1, size*0.06f, colorBlueTip);
        DrawLineEx(earFEnd, yF2, size*0.06f, colorBlueTip);

        // 4. Braço da Frente
        float armFrontAngle = sinf(walkCycle + PI) * 0.5f;
        Vector2 armFrontStart = { pPos.x + dir * size*0.15f, pPos.y - size*0.1f + bob };
        Vector2 armFrontElbow = { armFrontStart.x + sinf(armFrontAngle)*size*0.2f, armFrontStart.y + cosf(armFrontAngle)*size*0.2f };
        Vector2 armFrontHand = { armFrontElbow.x + sinf(armFrontAngle - 0.2f*dir)*size*0.2f, armFrontElbow.y + cosf(armFrontAngle - 0.2f*dir)*size*0.2f };
        
        float swordAngle = dir == 1 ? 45.0f : -45.0f; // Apontada pra cima na diagonal

        if (attackAnimTimer > 0.0f) {
            float animT = attackAnimTimer / 0.22f; // de 1.0 a 0.0
            armFrontHand.x = pPos.x + dir * size*0.4f;
            armFrontHand.y = pPos.y - size*0.4f + (1.0f - animT) * size*0.8f; // braço desce
            armFrontElbow = armFrontStart; // Simplifica o braço durante ataque
            swordAngle = dir == 1 ? (-45.0f + (1.0f - animT) * 180.0f) : (45.0f - (1.0f - animT) * 180.0f);
        }
        
        DrawLineEx(armFrontStart, armFrontElbow, size*0.12f, colorArmorDark);
        DrawLineEx(armFrontElbow, armFrontHand, size*0.1f, colorArmorDark);
        DrawCircleV(armFrontStart, size*0.15f, colorArmorDark); // Ombro
        DrawHeldWeapon(heldWeapon, armFrontHand, size*0.5f, swordAngle, swordLiquid, swordGlow);
        DrawCircleV(armFrontHand, size*0.08f, colorArmor); // Mão da frente
        
        // Ombro frente
        DrawCircleV(armFrontStart, size*0.15f, colorArmorDark);

        // Âncoras (pose em movimento / lateral)
        anch.head = headPos; anch.headR = size * 0.22f;
        anch.torsoC = (Vector2){ pPos.x, torso.y + torso.height * 0.5f };
        anch.torsoW = torso.width * 1.3f; anch.torsoH = torso.height;
        anch.hipL = legBackStart; anch.hipR = legFrontStart;
        anch.kneeL = legBackKnee; anch.kneeR = legFrontKnee;
        anch.footL = legBackFoot; anch.footR = legFrontFoot;
        anch.shoulderL = armBackStart; anch.shoulderR = armFrontStart;
        anch.handL = armBackHand; anch.handR = armFrontHand;
        anch.moving = true;
    }

    // Cosméticos equipados sobre o modelo base (data-driven via player->cosmetics)
    DrawPlayerCosmetics(player, anch, size, time);

    rlPopMatrix();
}
