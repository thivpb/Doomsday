#ifndef INPUT_CONTROLLER_H
#define INPUT_CONTROLLER_H

#include "game.h"

void UpdateBtnState(UIButton *btn, Vector2 mouse);
bool UpdateButtonsMenu(GameState *game, Vector2 mouse);
void UpdateButtonsControles(GameState *game, Vector2 mouse);
void UpdateButtonsPause(GameState *game, Vector2 mouse);
void UpdateButtonsGameOver(GameState *game, Vector2 mouse);
void UpdateButtonsVitoria(GameState *game, Vector2 mouse);
void UpdateButtonsSettings(GameState *game, Vector2 mouse, GameScreen backScreen);
int UpdateButtonsSaveSelect(GameState *game, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT]);
int UpdateButtonsLoadSelect(GameState *game, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT]);

#endif
