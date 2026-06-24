// rpg.c
// Ponto de entrada principal do RPG modular.
#include "../include/game.h"
#include "../include/gameplay.h"
#include "../include/telas.h"
#include "../include/asset_manager.h"
#include "logic/fsm.h"
#include "rlgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// ----------------------------------------------------------------------------
// SUPERSAMPLING DO MUNDO (SSAA). O mundo (gameplay/tutorial) é renderizado numa
// textura WORLD_SS× maior que a virtual 1280x720 e REDUZIDA no blit. Isso
// elimina o borrão do antigo UPSCALE de um alvo fixo 1280x720 para a janela
// (em telas maiores), mantendo o personagem/cenário nítidos. Um override de
// projeção mapeia o espaço LÓGICO 1280x720 sobre o alvo maior, então NENHUMA
// coordenada de desenho muda (HUD, colisão, escala lógica permanecem iguais).
// ----------------------------------------------------------------------------
#define WORLD_SS 2

static void BeginWorldRender(RenderTexture2D t)
{
    BeginTextureMode(t);
    ClearBackground(BLACK);
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPushMatrix();
    rlLoadIdentity();
    rlOrtho(0.0, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0, -1.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
}
static void EndWorldRender(void)
{
    rlDrawRenderBatchActive();
    rlMatrixMode(RL_PROJECTION);
    rlPopMatrix();
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    EndTextureMode();
}

// Globais para escalonamento da janela física e virtual (Letterbox)
float g_scale = 1.0f;
Vector2 g_mouseOffset = { 0.0f, 0.0f };
Vector2 g_virtualMouse = { 0.0f, 0.0f };
Font g_gameFont;

// FSM state variables exported to fsm.c
Texture2D slotTextures[SAVE_SLOT_COUNT] = { 0 };
bool slotTexturesLoaded[SAVE_SLOT_COUNT] = { false };
GameScreen loadSelectBackScreen = SCREEN_MENU;
GameScreen settingsBackScreen = SCREEN_MENU;
Image screenshotTemp = { 0 };
bool hasScreenshotTemp = false;

static void ScreenshotPathForSlot(int slot, char *path, int pathSize)
{
    if (slot == AUTO_SAVE_SLOT) snprintf(path, pathSize, "Saves/auto_save.png");
    else snprintf(path, pathSize, "Saves/screenshot_slot_%d.png", slot);
}

int main(void)
{
    // Relative asset/save paths must resolve beside the executable even when
    // the game is launched directly from Finder or another working directory.
    const char *applicationDirectory = GetApplicationDirectory();
    if (applicationDirectory[0] == '\0' || !ChangeDirectory(applicationDirectory))
    {
        fprintf(stderr, "Failed to enter application directory: %s\n", applicationDirectory);
        return EXIT_FAILURE;
    }

    // Habilita janela redimensionável e sincronização vertical (VSync)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Disease's Doomsday");
    SetWindowMinSize(SCREEN_WIDTH, SCREEN_HEIGHT);
    SetTargetFPS(60);
    SetExitKey(0); // Desativa ESC fechar direto para podermos usar como pause/voltar

    // Carrega todos os assets globais (Fonte, Música, SFX)
    LoadGameAssets();
    g_gameFont = g_assets.font;

    // Inicializa o estado global do jogo
    GameState game;
    memset(&game, 0, sizeof(GameState));
    game.currentScreen = SCREEN_MENU;
    game.musicVolume = 1.0f;
    game.sfxVolume = 1.0f;
    game.difficulty = DIFFICULTY_MEDIUM; // padrão antes de ler config
    LoadPlayerConfig(&game);  // Restaura audio, skins e dificuldade (Saves/config.txt)
    ApplySfxVolume(game.sfxVolume);
    
    GameScreen previousScreen = SCREEN_MENU;

    // Inicialização do Áudio e Carregamento do Tema (DeepVoid.mp3)
    // AudioDevice é iniciado no asset_manager
    Music musicA = g_assets.musicMain;
    Music musicB = g_assets.musicB;
    bool streamAPlaying = true;
    bool crossfadeActive = false;
    bool musicLoaded = (musicA.frameCount > 0) && (musicB.frameCount > 0);
    float autoSaveTimer = 0.0f;

    if (musicLoaded)
    {
        musicA.looping = false;
        musicB.looping = false;
        PlayMusicStream(musicA);
        SetMusicVolume(musicA, game.musicVolume);
        SetMusicVolume(musicB, 0.0f);
    }

    // Render target do MUNDO em WORLD_SS× (SSAA). É reduzido no blit para a
    // região do letterbox — nitidez sem borrão de upscale. Filtro bilinear para
    // uma redução suave (downsample), não ampliação.
    RenderTexture2D target = LoadRenderTexture(SCREEN_WIDTH * WORLD_SS, SCREEN_HEIGHT * WORLD_SS);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    // Loop de jogo principal
    while (!WindowShouldClose() && !game.shouldClose)
    {
        // Atualiza a música de fundo com Crossfade de 12 segundos
        if (musicLoaded)
        {
            if (streamAPlaying)
            {
                UpdateMusicStream(musicA);
                float time = GetMusicTimePlayed(musicA);
                float length = GetMusicTimeLength(musicA);
                
                if (length > 12.0f && time >= (length - 12.0f))
                {
                    if (!crossfadeActive)
                    {
                        PlayMusicStream(musicB);
                        crossfadeActive = true;
                    }
                    UpdateMusicStream(musicB);
                    float fadeProgress = (time - (length - 12.0f)) / 12.0f;
                    if (fadeProgress > 1.0f) fadeProgress = 1.0f;
                    if (fadeProgress < 0.0f) fadeProgress = 0.0f;
                    
                    SetMusicVolume(musicA, (1.0f - fadeProgress) * game.musicVolume);
                    SetMusicVolume(musicB, fadeProgress * game.musicVolume);
                }
                else
                {
                    SetMusicVolume(musicA, game.musicVolume);
                    SetMusicVolume(musicB, 0.0f);
                    crossfadeActive = false;
                }
                
                if (time >= length - 0.1f || !IsMusicStreamPlaying(musicA))
                {
                    StopMusicStream(musicA);
                    streamAPlaying = false;
                    crossfadeActive = false;
                    SetMusicVolume(musicB, game.musicVolume);
                }
            }
            else
            {
                UpdateMusicStream(musicB);
                float time = GetMusicTimePlayed(musicB);
                float length = GetMusicTimeLength(musicB);
                
                if (length > 12.0f && time >= (length - 12.0f))
                {
                    if (!crossfadeActive)
                    {
                        PlayMusicStream(musicA);
                        crossfadeActive = true;
                    }
                    UpdateMusicStream(musicA);
                    float fadeProgress = (time - (length - 12.0f)) / 12.0f;
                    if (fadeProgress > 1.0f) fadeProgress = 1.0f;
                    if (fadeProgress < 0.0f) fadeProgress = 0.0f;
                    
                    SetMusicVolume(musicB, (1.0f - fadeProgress) * game.musicVolume);
                    SetMusicVolume(musicA, fadeProgress * game.musicVolume);
                }
                else
                {
                    SetMusicVolume(musicB, game.musicVolume);
                    SetMusicVolume(musicA, 0.0f);
                    crossfadeActive = false;
                }
                
                if (time >= length - 0.1f || !IsMusicStreamPlaying(musicB))
                {
                    StopMusicStream(musicB);
                    streamAPlaying = true;
                    crossfadeActive = false;
                    SetMusicVolume(musicA, game.musicVolume);
                }
            }
        }

        // --------------------------------------------------------------------
        // A. CÁLCULO DE ESCALONAMENTO E COORDENADAS VIRTUAIS
        // --------------------------------------------------------------------
        float scaleX = (float)GetScreenWidth() / SCREEN_WIDTH;
        float scaleY = (float)GetScreenHeight() / SCREEN_HEIGHT;
        g_scale = (scaleX < scaleY) ? scaleX : scaleY;
        
        g_mouseOffset.x = (GetScreenWidth() - (SCREEN_WIDTH * g_scale)) * 0.5f;
        g_mouseOffset.y = (GetScreenHeight() - (SCREEN_HEIGHT * g_scale)) * 0.5f;

        Vector2 rawMouse = GetMousePosition();
        g_virtualMouse.x = (rawMouse.x - g_mouseOffset.x) / g_scale;
        g_virtualMouse.y = (rawMouse.y - g_mouseOffset.y) / g_scale;

        // --------------------------------------------------------------------
        // B. ATUALIZAÇÃO DA LÓGICA CONFORME A TELA ATUAL
        // --------------------------------------------------------------------
        UpdateStateMachine(&game);
        
        // CUIDADO: o main ainda precisa lidar com KEY_F5 (quicksave)
        if (game.currentScreen == SCREEN_GAMEPLAY && IsKeyPressed(KEY_F5))
        {
            // Garante renderização da gameplay limpa antes de capturar
            BeginWorldRender(target);
            DrawTelaGameplay(&game, g_gameFont, false);
            EndWorldRender();

            // Quicksave para o Slot 1 (Padrão)
            SalvarJogoSlot(&game, 1);
            
            // Captura e exporta screenshot limpa
            Image quicksaveImg = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&quicksaveImg);
            ImageResize(&quicksaveImg, 280, 158);
            ExportImage(quicksaveImg, "Saves/screenshot_slot_1.png");
            UnloadImage(quicksaveImg);
            
            game.saveLoaded = true;
            strcpy(game.notificationMsg, "GAME SAVED!");
            game.timeElapsed = 0.0f;
        }

        // --------------------------------------------------------------------
        // GESTÃO DE TRANSIÇÕES DE TELA (CAPTURA DE SCREENSHOTS E LOAD DE TEXTURAS)
        // Executado após a atualização para que transições no mesmo frame sejam detectadas
        // --------------------------------------------------------------------
        if (game.currentScreen != previousScreen)
        {
            // Se saiu de uma tela de slots ou de pausa, limpa a screenshot/texturas
            if (previousScreen == SCREEN_SAVE_SELECT || previousScreen == SCREEN_LOAD_SELECT || previousScreen == SCREEN_PAUSE)
            {
                // Limpa texturas dos slots se saiu deles
                if (previousScreen == SCREEN_SAVE_SELECT || previousScreen == SCREEN_LOAD_SELECT)
                {
                    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
                    {
                        if (slotTexturesLoaded[i])
                        {
                            UnloadTexture(slotTextures[i]);
                            slotTextures[i] = (Texture2D){ 0 };
                            slotTexturesLoaded[i] = false;
                        }
                    }
                }
                
                // Limpa o screenshot temporário se não estamos salvando e não estamos na pausa
                if (game.currentScreen != SCREEN_SAVE_SELECT && game.currentScreen != SCREEN_PAUSE)
                {
                    if (hasScreenshotTemp)
                    {
                        UnloadImage(screenshotTemp);
                        hasScreenshotTemp = false;
                    }
                }
            }

            // Entrando na tela de salvamento: garante screenshot e carrega metadados/texturas
            if (game.currentScreen == SCREEN_SAVE_SELECT)
            {
                if (hasScreenshotTemp)
                {
                    UnloadImage(screenshotTemp);
                    hasScreenshotTemp = false;
                }

                // Renderiza a jogabilidade limpa (sem overlays) para a render target
                BeginWorldRender(target);
                DrawTelaGameplay(&game, g_gameFont, false);
                EndWorldRender();

                screenshotTemp = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&screenshotTemp);
                ImageResize(&screenshotTemp, 280, 158);
                hasScreenshotTemp = true;

                for (int i = 0; i < MANUAL_SAVE_SLOTS; i++)
                {
                    char path[64];
                    ScreenshotPathForSlot(i + 1, path, sizeof(path));
                    // Evita double-load: descarrega a textura anterior do slot.
                    if (slotTexturesLoaded[i]) { UnloadTexture(slotTextures[i]); slotTexturesLoaded[i] = false; }
                    if (FileExists(path))
                    {
                        slotTextures[i] = LoadTexture(path);
                        slotTexturesLoaded[i] = true;
                    }
                    else
                    {
                        slotTexturesLoaded[i] = false;
                    }
                }

                // Carrega os metadados mais recentes dos slots
                for (int i = 0; i < SAVE_SLOT_COUNT; i++)
                {
                    game.slotsMeta[i] = CarregarMetadadosSlot(i + 1);
                }
            }

            // Entrando na tela de carregamento: carrega metadados/texturas
            if (game.currentScreen == SCREEN_LOAD_SELECT)
            {
                loadSelectBackScreen = previousScreen;

                for (int i = 0; i < SAVE_SLOT_COUNT; i++)
                {
                    char path[64];
                    ScreenshotPathForSlot(i + 1, path, sizeof(path));
                    // Evita double-load: descarrega a textura anterior do slot.
                    if (slotTexturesLoaded[i]) { UnloadTexture(slotTextures[i]); slotTexturesLoaded[i] = false; }
                    if (FileExists(path))
                    {
                        slotTextures[i] = LoadTexture(path);
                        slotTexturesLoaded[i] = true;
                    }
                    else
                    {
                        slotTexturesLoaded[i] = false;
                    }
                }

                // Carrega os metadados mais recentes dos slots
                for (int i = 0; i < SAVE_SLOT_COUNT; i++)
                {
                    game.slotsMeta[i] = CarregarMetadadosSlot(i + 1);
                }
            }

            // Entrando na tela de configurações
            if (game.currentScreen == SCREEN_SETTINGS)
            {
                settingsBackScreen = previousScreen;
            }
        }

        previousScreen = game.currentScreen;

        // --------------------------------------------------------------------
        // C. RENDERIZAÇÃO NA TEXTURA VIRTUAL (1280x720) - APENAS MUNDO DO JOGO
        // --------------------------------------------------------------------
        BeginWorldRender(target);

        // Fundo do mundo (textura virtual). DIFERENCIA explicitamente o fundo do
        // TUTORIAL (seringa) e da GAMEPLAY (corpo): ao pausar DENTRO da seringa, o
        // fundo congelado deve ser a própria cena do tutorial — nunca a gameplay
        // do corpo. game.inTutorial distingue os dois contextos (FSM usa o mesmo
        // flag para voltar do pause à tela certa). O shader/câmera já são tratados
        // abaixo para SCREEN_PAUSE/TUTORIAL/GAMEPLAY.
        // Qualquer sobreposição aberta DE DENTRO do tutorial (pause; ou load-
        // select aberto a partir da pausa do tutorial) deve congelar a cena da
        // SERINGA — nunca o corpo. game.inTutorial distingue os contextos.
        bool overlayInTutorial = game.inTutorial &&
            (game.currentScreen == SCREEN_PAUSE ||
             (game.currentScreen == SCREEN_LOAD_SELECT && loadSelectBackScreen == SCREEN_PAUSE));
        if (game.currentScreen == SCREEN_TUTORIAL || overlayInTutorial)
        {
            // Apenas o MUNDO do tutorial vai para a textura virtual; o HUD é
            // desenhado depois (resolução nativa). Sem desenho duplicado.
            DrawTutorialWorld(&game, g_gameFont);
        }
        else if (game.currentScreen == SCREEN_GAMEPLAY ||
                 game.currentScreen == SCREEN_PAUSE ||
                 game.currentScreen == SCREEN_SAVE_SELECT ||
                 game.currentScreen == SCREEN_QUIZ ||
                 game.currentScreen == SCREEN_UPGRADE ||
                 game.currentScreen == SCREEN_STAGE_COMPLETE ||
                 game.currentScreen == SCREEN_STAGE_PROLOGUE ||
                 (game.currentScreen == SCREEN_LOAD_SELECT && loadSelectBackScreen == SCREEN_PAUSE))
        {
            // Desenha apenas o mundo 2D (sem o HUD) na textura virtual
            DrawTelaGameplay(&game, g_gameFont, false);
        }

        EndWorldRender();

        if (game.currentScreen == SCREEN_GAMEPLAY && !game.inTutorial)
        {
            autoSaveTimer += GetFrameTime();
            if (autoSaveTimer >= 90.0f)
            {
                autoSaveTimer = 0.0f;
                SalvarJogoSlotSilencioso(&game, AUTO_SAVE_SLOT);
                Image autoImg = LoadImageFromTexture(target.texture);
                ImageFlipVertical(&autoImg);
                ImageResize(&autoImg, 280, 158);
                ExportImage(autoImg, "Saves/auto_save.png");
                UnloadImage(autoImg);
            }
        }
        else if (game.currentScreen == SCREEN_MENU || game.currentScreen == SCREEN_LOADING)
        {
            autoSaveTimer = 0.0f;
        }

        // --------------------------------------------------------------------
        // D. DESENHA A TEXTURA REDIMENSIONADA NA TELA FÍSICA
        // --------------------------------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);

        // O shader biológico (distorção orgânica + vinheta) representa estar
        // DENTRO do corpo: vale na gameplay e na pausa da gameplay. O tutorial
        // ocorre numa seringa (vidro/metal) — aplicá-lo ali distorce a cena e
        // divergia entre jogo e pausa. Por isso o tutorial fica de fora.
        bool useBiologicalShader = g_assets.shaderLoaded &&
            (game.currentScreen == SCREEN_GAMEPLAY ||
             (game.currentScreen == SCREEN_PAUSE && !game.inTutorial));
        
        if (useBiologicalShader)
        {
            float timeVal = (float)GetTime();
            int timeLoc = GetShaderLocation(g_assets.biologicalShader, "time");
            SetShaderValue(g_assets.biologicalShader, timeLoc, &timeVal, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(g_assets.biologicalShader);
        }

        // Renderiza a textura virtual centralizada na tela com letterbox
        DrawTexturePro(
            target.texture,
            (Rectangle){ 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
            (Rectangle){ g_mouseOffset.x, g_mouseOffset.y, SCREEN_WIDTH * g_scale, SCREEN_HEIGHT * g_scale },
            (Vector2){ 0, 0 }, 
            0.0f, 
            WHITE
        );

        if (useBiologicalShader)
        {
            EndShaderMode();
        }

        // --------------------------------------------------------------------
        // E. DESENHA INTERFACE E HUD COM NITIDEZ NATIVA NA TELA FÍSICA (hudCamera)
        // --------------------------------------------------------------------
        Camera2D hudCamera = { 0 };
        hudCamera.zoom = g_scale;
        hudCamera.offset = g_mouseOffset;
        hudCamera.target = (Vector2){ 0.0f, 0.0f };
        hudCamera.rotation = 0.0f;

        BeginMode2D(hudCamera);

        DrawStateMachine(&game);

        EndMode2D();

        EndDrawing();
    }

    // Limpeza e encerramento de áudio e texturas
    // As músicas são descarregadas no asset_manager

    UnloadRenderTexture(target);
    UnloadGameplayResources();   // libera a textura do biossensor (HUD)
    // Limpa quaisquer texturas de slot/screenshot ainda carregadas
    for (int i = 0; i < SAVE_SLOT_COUNT; i++)
        if (slotTexturesLoaded[i]) { UnloadTexture(slotTextures[i]); slotTexturesLoaded[i] = false; }
    if (hasScreenshotTemp) { UnloadImage(screenshotTemp); hasScreenshotTemp = false; }
    UnloadGameAssets();
    CloseWindow();

    return 0;
}
