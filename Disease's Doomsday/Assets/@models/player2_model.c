// player2_model.c
// 2º personagem jogável: "ANTICORPO-V" (baseado em SPRITES, não procedural).
// Renderiza folhas de sprite animadas (idle/walk/hurt) e encaixa a MESMA arma
// segurada do herói (DrawHeldWeapon), com cores de skin de arma idênticas. As
// 3 skins do jogo (Padrão/Médica/Infectada) viram uma TINTA sobre o sprite
// (adaptação segura); os cosméticos modulares NÃO se aplicam a uma textura fixa
// e por isso ficam desativados aqui — o personagem ORIGINAL mantém tudo intacto.
//
// O fatiamento por frame, o flip e o clamp de índice ficam em sprite_manager
// (DrawSpriteFrame). Aqui só escolhemos folha/frame/posição da arma.
#include "player2_model.h"
#include "weapons_model.h"
#include "../../include/gameplay.h"
#include "../../include/sprite_manager.h"
#include <math.h>

// ============================================================================
// CONSTANTES DE CALIBRAÇÃO (ajustadas via preview offline — tools/player2_preview)
// ----------------------------------------------------------------------------
// Tamanho em TELA (px) da CÉLULA inteira do frame, por estado. A folha de dano
// tem célula maior (512) que idle/walk (400) e o personagem ocupa fração distinta,
// então o tamanho é por estado para manter a aparência consistente.
#define P2_DRAW_IDLE   200.0f
#define P2_DRAW_WALK   200.0f
#define P2_DRAW_HURT   235.0f
// Empurra o sprite p/ baixo: há espaço vazio (transparente) na parte de baixo da
// célula, então o "centro visual" do personagem fica acima do centro da célula.
#define P2_Y_OFFSET     12.0f
// FPS de cada animação.
#define P2_FPS_IDLE     6.0f
#define P2_FPS_WALK    12.0f
#define P2_FPS_HURT    14.0f
// Frames por folha (idle/walk = 4; hurt = 3).
#define P2_FRAMES_IDLE  4
#define P2_FRAMES_WALK  4
#define P2_FRAMES_HURT  3
// Âncora da MÃO (fração de `size`, relativa ao centro do personagem). A arma é
// "presa" aqui; dir-aware no X. Calibrável para a arma não flutuar nem afundar.
#define P2_HAND_DX      0.60f   // afastamento lateral (no sentido da direção)
#define P2_HAND_DY      0.45f   // altura da mão (abaixo do centro)
#define P2_WEAPON_SIZE  0.52f   // multiplicador de tamanho da arma

// Tinta de skin (fallback): WHITE = sem alteração; as demais multiplicam a
// textura para recolorir de forma coesa (Médica clara; Infectada arroxeada).
static Color Hero2SkinTint(int skinId)
{
    switch (skinId)
    {
        case 1:  return (Color){ 210, 235, 245, 255 }; // Médica: branco-azulado
        case 2:  return (Color){ 200, 150, 230, 255 }; // Infectada: roxo
        default: return WHITE;                          // Padrão: textura original
    }
}

void DrawPlayer2Model(Player *player, float size, Color tint, float time,
                      float attackAnimTimer, int animState)
{
    (void)tint; // a cor vem da skin/boost (igual ao herói procedural)
    if (!Player2SpritesReady()) return; // sem PNGs: chamador usa o procedural

    Vector2 pos = player->position;
    int dir = (player->facingDir != 0) ? player->facingDir : 1;
    bool flipX = (dir < 0);

    // Folha / frames / fps / tamanho por estado.
    SpriteID sheet; int frames; float fps; float drawSize;
    switch (animState)
    {
        case 2:  sheet = SPR_HERO2_HURT; frames = P2_FRAMES_HURT; fps = P2_FPS_HURT; drawSize = P2_DRAW_HURT; break;
        case 1:  sheet = SPR_HERO2_WALK; frames = P2_FRAMES_WALK; fps = P2_FPS_WALK; drawSize = P2_DRAW_WALK; break;
        default: sheet = SPR_HERO2_IDLE; frames = P2_FRAMES_IDLE; fps = P2_FPS_IDLE; drawSize = P2_DRAW_IDLE; break;
    }
    // Frame por tempo (stateless). O clamp final fica em DrawSpriteFrame.
    int frame = (int)(time * fps) % frames;
    if (frame < 0) frame += frames;

    // Tinta do corpo: realce dourado durante o boost; senão tinta da skin.
    Color bodyTint = (player->attackBoostTimer > 0.0f)
                     ? (Color){ 255, 215, 120, 255 }
                     : Hero2SkinTint(player->skinId);

    Vector2 center = { pos.x, pos.y + P2_Y_OFFSET };

    // 1) Corpo (sprite animado).
    DrawSpriteFrame(sheet, frame, frames, center, (Vector2){ drawSize, drawSize }, 0.0f, bodyTint, flipX);

    // 2) Arma segurada — mesmo modelo/cores do herói; ângulo segue a mesma lógica.
    int heldWeapon = (player->equippedWeapon >= 1 && player->equippedWeapon <= WEAPON_COUNT)
                     ? player->equippedWeapon : 1;
    Color wpnPrimary = WeaponSkinPrimary(player->weaponSkinId);
    Color wpnGlow    = WeaponSkinSecondary(player->weaponSkinId);

    // swordAngle: idle aponta p/ cima (0); andando na diagonal (±45); durante o
    // ataque faz o arco por cima (igual a DrawPlayerModel).
    float swordAngle = (animState == 1) ? (dir == 1 ? 45.0f : -45.0f) : 0.0f;

    // Posição da mão (encaixe da arma) relativa ao centro do personagem.
    Vector2 hand = { pos.x + dir * size * P2_HAND_DX,
                     pos.y + size * P2_HAND_DY + P2_Y_OFFSET };

    if (attackAnimTimer > 0.0f)
    {
        float animT = attackAnimTimer / 0.22f; // 1.0 -> 0.0
        if (animState == 1)
            swordAngle = dir == 1 ? (-45.0f + (1.0f - animT) * 180.0f) : (45.0f - (1.0f - animT) * 180.0f);
        else
            swordAngle = dir == 1 ? (-90.0f + (1.0f - animT) * 180.0f) : (90.0f - (1.0f - animT) * 180.0f);
        // A mão sobe em arco por cima da cabeça durante o golpe.
        hand.x = pos.x + dir * (size * 0.10f + (1.0f - animT) * size * 0.30f);
        hand.y = pos.y - size * 0.20f - sinf((1.0f - animT) * PI) * size * 0.40f + P2_Y_OFFSET;
    }

    DrawHeldWeapon(heldWeapon, hand, size * P2_WEAPON_SIZE, swordAngle, wpnPrimary, wpnGlow);
}
