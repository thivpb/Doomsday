#ifndef PLAYER2_MODEL_H
#define PLAYER2_MODEL_H

#include "raylib.h"
#include "../../include/game.h"

// Desenha o 2º personagem jogável "ANTICORPO-V" a partir de folhas de sprite
// ANIMADAS (idle/walk/hurt), com a arma segurada encaixada na mão. Assinatura
// paralela a DrawPlayerModel, com um parâmetro extra de estado de animação:
//   animState: 0 = parado (idle), 1 = andando (walk), 2 = tomando dano (hurt)
// `size` é o mesmo "size" do herói procedural (60 no gameplay) e serve de escala
// para a posição/tamanho da arma. `tint` é ignorado (a cor vem da skin/boost,
// igual ao herói procedural). Se as folhas não estiverem carregadas, NÃO desenha
// nada — o chamador deve cair no Anticorpo procedural (ver Player2SpritesReady).
void DrawPlayer2Model(Player *player, float size, Color tint, float time,
                      float attackAnimTimer, int animState);

#endif // PLAYER2_MODEL_H
