#include "../../include/input_controller.h"
#include "../../include/telas.h"
#include "../../include/gameplay.h"
#include "../../include/asset_manager.h"
#include "raymath.h"
#include <stdio.h>
#include <string.h>

extern UIButton menuButtons[8];
extern UIButton pauseButtons[5];
extern UIButton controlsButton;
extern UIButton gameOverButtons[2];
extern UIButton victoryButtons[2];

extern UIButton settingsBtnVoltar;

static void PlayMenuHoverSfxThrottled(void)
{
    static double lastHoverSfxTime = -10.0;
    const double now = GetTime();
    const double minInterval = 0.09;

    if (g_assets.sfxMenuHover.frameCount <= 0) return;
    if (now - lastHoverSfxTime < minInterval) return;
    if (IsSoundPlaying(g_assets.sfxMenuHover)) return;

    PlaySound(g_assets.sfxMenuHover);
    lastHoverSfxTime = now;
}

void UpdateBtnState(UIButton *btn, Vector2 mouse)
{
    bool wasHovered = btn->hover;
    btn->hover = CheckCollisionPointRec(mouse, btn->bounds);
    btn->clicked = false;
    if (btn->hover && !wasHovered && g_assets.sfxMenuHover.frameCount > 0)
        PlayMenuHoverSfxThrottled();
    if (btn->hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        btn->clicked = true;
        if (g_assets.sfxMenuClick.frameCount > 0) PlaySound(g_assets.sfxMenuClick);
    }
}

bool UpdateButtonsMenu(GameState *game, Vector2 mouse)
{
    // Layout ÚNICO: garante que hitbox == desenho (mesmos retângulos).
    MenuApplyLayout();

    // Emite partículas decorativas subindo (verde biológico e azul ciano)
    if (GetRandomValue(0, 8) == 0)
    {
        Vector2 pPos = { (float)GetRandomValue(0, SCREEN_WIDTH), SCREEN_HEIGHT + 10.0f };
        Vector2 pVel = { (float)GetRandomValue(-20, 20), (float)GetRandomValue(-60, -25) };
        Color pCol = (GetRandomValue(0, 1) == 0) ? (Color){0,200,100,255} : (Color){0,229,255,255};
        SpawnParticle(game, pPos, pVel, pCol, (float)GetRandomValue(2, 5), (float)GetRandomValue(3, 6));
    }

    // Atualiza lógica das partículas decorativas
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            game->particles[i].position.y += game->particles[i].velocity.y * GetFrameTime();
            game->particles[i].position.x += game->particles[i].velocity.x * GetFrameTime();
            game->particles[i].lifeTime -= GetFrameTime();
            if (game->particles[i].lifeTime <= 0.0f) FreeParticle(game, i);
        }
    }

    // Lógica do Input Box do Nome do Jogador (mesmo retângulo do desenho)
    Rectangle nameBounds = MenuNameRect();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(mouse, nameBounds))
        {
            game->nameInputActive = true;
        }
        else
        {
            game->nameInputActive = false;
        }
    }

    // Digitação se ativo
    if (game->nameInputActive)
    {
        int key = GetCharPressed();
        while (key > 0)
        {
            if ((key >= 32) && (key <= 125) && (strlen(game->player.name) < 15))
            {
                int len = strlen(game->player.name);
                game->player.name[len] = (char)key;
                game->player.name[len + 1] = '\0';
            }
            key = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE))
        {
            int len = strlen(game->player.name);
            if (len > 0)
            {
                game->player.name[len - 1] = '\0';
            }
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        {
            game->nameInputActive = false;
        }
    }

    // Determina se existem arquivos de save (com cache de ~1s)
    bool anySaveExists = AnySaveExistsCached();

    // Evita hover e clique acidentais nos botões do menu durante a digitação
    if (!game->nameInputActive)
    {
        for (int i = 0; i < 8; i++)
        {
            if (i == 1 && !anySaveExists)
            {
                menuButtons[i].hover = false;
                menuButtons[i].clicked = false;
            }
            else
            {
                UpdateBtnState(&menuButtons[i], mouse);
            }
        }

        // Morph do FUNDO conforme o item destacado (hover): cada subtela tem tema.
        game->highlightScreen = SCREEN_MENU;
        game->menuHighlight = -1;
        for (int i = 0; i < 8; i++)
        {
            if (!menuButtons[i].hover) continue;
            game->menuHighlight = i;
            switch (i)
            {
                case 2: game->highlightScreen = SCREEN_ARSENAL;  break;
                case 3: game->highlightScreen = SCREEN_SKINS;    break;
                case 4: game->highlightScreen = SCREEN_CONTROLS; break;
                case 5: game->highlightScreen = SCREEN_SETTINGS; break;
                case 6: game->highlightScreen = SCREEN_ADMIN;    break;
                default: break;
            }
            break;
        }
        // O seletor de dificuldade do rodapé foi REMOVIDO: a escolha agora é feita
        // na tela dedicada SCREEN_DIFFICULTY_SELECT (aberta por "JOGAR").
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            menuButtons[i].hover = false;
            menuButtons[i].clicked = false;
        }
    }

    // Ações (0 Jogar,1 Carregar,2 Arsenal,3 Skins,4 Tutorial,5 Config,6 Admin,7 Sair)
    if (menuButtons[0].clicked) // JOGAR -> abre a seleção de dificuldade
    {
        game->pendingDifficulty = game->difficulty; // pré-seleciona a persistida
        game->diffReturnScreen = SCREEN_MENU;
        game->currentScreen = SCREEN_DIFFICULTY_SELECT;
    }
    else if (menuButtons[1].clicked) // CARREGAR JOGO
    {
        if (anySaveExists)
        {
            for (int i = 0; i < SAVE_SLOT_COUNT; i++)
            {
                game->slotsMeta[i] = CarregarMetadadosSlot(i + 1);
            }
            game->currentScreen = SCREEN_LOAD_SELECT;
        }
    }
    else if (menuButtons[2].clicked) // ARSENAL
    {
        game->currentScreen = SCREEN_ARSENAL;
    }
    else if (menuButtons[3].clicked) // SKINS
    {
        game->currentScreen = SCREEN_SKINS;
    }
    else if (menuButtons[4].clicked) // TUTORIAL
    {
        game->currentScreen = SCREEN_CONTROLS;
    }
    else if (menuButtons[5].clicked) // CONFIG
    {
        game->currentScreen = SCREEN_SETTINGS;
    }
    else if (menuButtons[6].clicked) // MODO ADMIN
    {
        game->currentScreen = SCREEN_ADMIN;
    }

    return menuButtons[7].clicked; // Retorna true se clicou em SAIR
}

void UpdateButtonsControles(GameState *game, Vector2 mouse)
{
    UpdateBtnState(&controlsButton, mouse);
    if (controlsButton.clicked)
    {
        game->currentScreen = SCREEN_MENU;
    }
}

void UpdateButtonsPause(GameState *game, Vector2 mouse)
{
    if (game->uiAnimTimer < 0.5f) return;

    for (int i = 0; i < 5; i++)
    {
        UpdateBtnState(&pauseButtons[i], mouse);
    }

    if (pauseButtons[0].clicked) // VOLTAR
    {
        game->currentScreen = game->inTutorial ? SCREEN_TUTORIAL : SCREEN_GAMEPLAY;
    }
    else if (pauseButtons[1].clicked) // SALVAR
    {
        // Carrega metadados dos slots para a tela de salvamento
        for (int j = 0; j < MANUAL_SAVE_SLOTS; j++)
        {
            game->slotsMeta[j] = CarregarMetadadosSlot(j + 1);
        }
        game->currentScreen = SCREEN_SAVE_SELECT;
    }
    else if (pauseButtons[2].clicked) // CARREGAR
    {
        // Carrega metadados dos slots para a tela de carregamento
        for (int j = 0; j < SAVE_SLOT_COUNT; j++)
        {
            game->slotsMeta[j] = CarregarMetadadosSlot(j + 1);
        }
        game->currentScreen = SCREEN_LOAD_SELECT;
    }
    else if (pauseButtons[3].clicked) // CONFIGURACOES
    {
        game->currentScreen = SCREEN_SETTINGS;
    }
    else if (pauseButtons[4].clicked) // MENU
    {
        RequestLoadingScreen(game, LOAD_TO_MENU, 1.5f);
    }
}

void UpdateButtonsGameOver(GameState *game, Vector2 mouse)
{
    for (int i = 0; i < 2; i++)
    {
        UpdateBtnState(&gameOverButtons[i], mouse);
    }

    if (gameOverButtons[0].clicked) // TENTAR NOVAMENTE -> escolher dificuldade
    {
        game->pendingDifficulty = game->difficulty;
        game->diffReturnScreen = SCREEN_GAMEOVER;
        game->currentScreen = SCREEN_DIFFICULTY_SELECT;
    }
    else if (gameOverButtons[1].clicked) // MENU
    {
        RequestLoadingScreen(game, LOAD_TO_MENU, 1.5f);
    }
}

void UpdateButtonsVitoria(GameState *game, Vector2 mouse)
{
    // Partículas douradas festivas
    if (GetRandomValue(0, 5) == 0)
    {
        Vector2 pPos = { (float)GetRandomValue(0, SCREEN_WIDTH), SCREEN_HEIGHT + 10.0f };
        Vector2 pVel = { (float)GetRandomValue(-20, 20), (float)GetRandomValue(-80, -35) };
        SpawnParticle(game, pPos, pVel, GOLD, (float)GetRandomValue(2, 5), (float)GetRandomValue(3, 5));
    }

    // Atualiza lógica das partículas festivas
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            game->particles[i].position.y += game->particles[i].velocity.y * GetFrameTime();
            game->particles[i].position.x += game->particles[i].velocity.x * GetFrameTime();
            game->particles[i].lifeTime -= GetFrameTime();
            if (game->particles[i].lifeTime <= 0.0f) FreeParticle(game, i);
        }
    }

    // FASE 0 — o cientista narra o encerramento. Avança o diálogo; só quando ele
    // termina (transmissão encerrada) liberamos as opções de Jogar novamente/Menu.
    if (game->sceneDialog.active)
    {
        const char *pages[4];
        int pc = VictoryDialogPages(game, pages);
        if (game->sceneDialog.page >= pc) game->sceneDialog.page = pc - 1;
        if (ScientistDialogAdvance(&game->sceneDialog, pages[game->sceneDialog.page], pc,
                                   SCIENTIST_VOICE_VICTORY, game->sfxVolume) == 2)
            game->sceneDialog.active = false;
        return;
    }

    // FASE 1 — pós-diálogo: apenas as duas opções.
    for (int i = 0; i < 2; i++)
    {
        UpdateBtnState(&victoryButtons[i], mouse);
    }

    if (victoryButtons[0].clicked) // NOVA JORNADA -> escolher dificuldade
    {
        game->pendingDifficulty = game->difficulty;
        game->diffReturnScreen = SCREEN_VICTORY;
        game->currentScreen = SCREEN_DIFFICULTY_SELECT;
    }
    else if (victoryButtons[1].clicked) // MENU
    {
        RequestLoadingScreen(game, LOAD_TO_MENU, 1.5f);
    }
}



static void SaveFilePathForSlot(int slot, char *pathTxt, int txtSize, char *pathPng, int pngSize)
{
    if (slot == AUTO_SAVE_SLOT)
    {
        snprintf(pathTxt, txtSize, "Saves/auto_save.txt");
        snprintf(pathPng, pngSize, "Saves/auto_save.png");
    }
    else
    {
        snprintf(pathTxt, txtSize, "Saves/save_slot_%d.txt", slot);
        snprintf(pathPng, pngSize, "Saves/screenshot_slot_%d.png", slot);
    }
}

int UpdateButtonsSaveSelect(GameState *game, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT])
{
    // Verifica cliques nos Cards
    for (int i = 0; i < MANUAL_SAVE_SLOTS; i++)
    {
        float cardX = 80.0f + (float)i * 400.0f;
        Rectangle cardBounds = { cardX, 140, 320, 430 };

        // Verifica primeiro clique no botão de Apagar se o slot existe
        if (game->slotsMeta[i].exists)
        {
            Rectangle deleteBounds = { cardX + 20, 495, 280, 35 };
            if (CheckCollisionPointRec(mouse, deleteBounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                char pathTxt[64];
                char pathPng[64];
                SaveFilePathForSlot(i + 1, pathTxt, sizeof(pathTxt), pathPng, sizeof(pathPng));
                remove(pathTxt);
                remove(pathPng);

                if (slotTexturesLoaded[i])
                {
                    UnloadTexture(slotTextures[i]);
                    slotTextures[i] = (Texture2D){ 0 };
                    slotTexturesLoaded[i] = false;
                }

                game->slotsMeta[i] = CarregarMetadadosSlot(i + 1);
                return 0; // Atualiza a tela sem sair dela
            }
        }

        if (CheckCollisionPointRec(mouse, cardBounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            return i + 1; // Retorna o slot selecionado (1, 2 ou 3)
        }
    }

    // Verifica clique no botão Voltar
    UIButton btnVoltar = { { 490, 640, 300, 50 }, "VOLTAR", false, false };
    if (CheckCollisionPointRec(mouse, btnVoltar.bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return -1; // Sinaliza que clicou em voltar
    }

    return 0;
}

int UpdateButtonsLoadSelect(GameState *game, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT])
{
    Rectangle autoBounds = { 80, 118, 1120, 112 };
    if (game->slotsMeta[AUTO_SAVE_SLOT - 1].exists &&
        CheckCollisionPointRec(mouse, autoBounds) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return AUTO_SAVE_SLOT;
    }

    // Verifica cliques nos Cards (apenas slots que existem)
    for (int i = 0; i < MANUAL_SAVE_SLOTS; i++)
    {
        float cardX = 80.0f + (float)i * 400.0f;
        Rectangle cardBounds = { cardX, 260, 320, 300 };

        if (game->slotsMeta[i].exists)
        {
            // Verifica primeiro clique no botão de Apagar
            Rectangle deleteBounds = { cardX + 20, 562, 280, 30 };
            if (CheckCollisionPointRec(mouse, deleteBounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                char pathTxt[64];
                char pathPng[64];
                SaveFilePathForSlot(i + 1, pathTxt, sizeof(pathTxt), pathPng, sizeof(pathPng));
                remove(pathTxt);
                remove(pathPng);

                if (slotTexturesLoaded[i])
                {
                    UnloadTexture(slotTextures[i]);
                    slotTextures[i] = (Texture2D){ 0 };
                    slotTexturesLoaded[i] = false;
                }

                game->slotsMeta[i] = CarregarMetadadosSlot(i + 1);
                return 0; // Atualiza a tela sem sair dela
            }

            if (CheckCollisionPointRec(mouse, cardBounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                return i + 1; // Retorna o slot selecionado (1, 2 ou 3)
            }
        }
    }

    // Verifica clique no botão Voltar
    UIButton btnVoltar = { { 490, 650, 300, 50 }, "VOLTAR", false, false };
    if (CheckCollisionPointRec(mouse, btnVoltar.bounds) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        return -1; // Sinaliza que clicou em voltar
    }

    return 0;
}

void UpdateButtonsSettings(GameState *game, Vector2 mouse, GameScreen backScreen)
{
    UpdateBtnState(&settingsBtnVoltar, mouse);
    if (settingsBtnVoltar.clicked)
    {
        SavePlayerConfig(game); // persiste volume + skins ao sair
        game->currentScreen = backScreen;
        return;
    }

    // Sliders independentes; desenho e input compartilham as mesmas geometrias.
    Rectangle tracks[2] = { SettingsMusicVolumeTrack(), SettingsSfxVolumeTrack() };
    for (int i = 0; i < 2; i++)
    {
        Rectangle track = tracks[i];
        Rectangle sliderHit = { track.x - 14, track.y - 16, track.width + 28, track.height + 32 };
        if (!CheckCollisionPointRec(mouse, sliderHit) || !IsMouseButtonDown(MOUSE_LEFT_BUTTON)) continue;

        float pct = Clamp((mouse.x - track.x) / track.width, 0.0f, 1.0f);
        if (i == 0) game->musicVolume = pct;
        else
        {
            game->sfxVolume = pct;
            ApplySfxVolume(pct);
        }
    }

}
