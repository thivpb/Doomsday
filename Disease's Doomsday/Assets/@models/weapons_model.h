#ifndef WEAPONS_MODEL_H
#define WEAPONS_MODEL_H

#include "raylib.h"

// Desenha a Espada Seringa (liquidColor varia conforme a skin da arma)
void DrawSyringeSword(Vector2 handPos, float size, float rotationDeg, Color liquidColor);

// Desenha a Lâmina Bioelétrica (variante melee anti-capsídeo desbloqueável;
// arcos elétricos). DrawScalpel é o nome interno do modelo (mantido por compat).
void DrawScalpel(Vector2 handPos, float size, float rotationDeg, Color primary, Color secondary);

// (Legado) Antes alternava o modelo melee do slot 1 por Mundo. Hoje o slot 1 é
// sempre a Espada-Seringa; a Lâmina Bioelétrica mantém ID 5 por compatibilidade.
void SetWeaponModelWorld(int world);

// Desenha a arma SEGURADA conforme o tipo equipado (1=melee, 2=projétil,
// 3=granada, 4=BFG). As cores da skin (primary/secondary) valem para todas.
void DrawHeldWeapon(int weapon, Vector2 handPos, float size, float rotationDeg, Color primary, Color secondary);

// Desenha a arma ENQUADRADA dentro de `frame`: como os modelos são "top-heavy"
// (agulha/cano/orbe apontam para cima), esta função escolhe a escala (até
// `maxSize`) e desloca a âncora para BAIXO de modo que a composição inteira
// fique centralizada no retângulo, com padding — a ponta nunca vaza do painel.
// Usada por previews (Arsenal, cards). Respeita SetWeaponModelWorld para o slot 1.
void DrawHeldWeaponFramed(int weapon, Rectangle frame, float maxSize, float rotationDeg,
                          Color primary, Color secondary);

#endif // WEAPONS_MODEL_H
