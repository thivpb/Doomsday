// telas.h
// Declarações das funções de desenho de menus, HUD, minimapa e botões interativos.
#ifndef TELAS_H
#define TELAS_H

#include "game.h"

// Desenha um botão UIButton com o estilo Premium da interface
void DrawButton(UIButton botao, Font font, bool enabled);

// Desenha texto centralizado que reduz a fonte para caber na largura da área.
void DrawTextFitCentered(Font font, const char *text, Rectangle area, float maxFont, Color color, bool shadow);

// ============================================================================
// COMPONENTES REUTILIZÁVEIS DE UI (tema médico/imunológico/sci-fi)
// Todos medem o texto com MeasureTextEx e respeitam o retângulo do componente —
// o texto nunca ultrapassa seu painel (encolhe a fonte ou faz wrap).
// ============================================================================
// Ease de entrada (cubic-out) usado por animações de slide/fade. 0..1 -> 0..1.
float UIEase(float t);
// Painel temático com brilho de borda e cantos sci-fi (alpha do fundo ajustável).
void DrawPanel(Rectangle r, Color accent, float bgAlpha);
// Título com halo/sombra e contorno (centralizado em centerX).
void DrawTitleText(Font font, const char *text, float centerX, float y, float fontSize, Color color);
// Texto com quebra de linha (word-wrap) dentro de `area`; encolhe a fonte se
// preciso para caber na altura. Retorna a altura usada.
float DrawTextWrapped(Font font, const char *text, Rectangle area, float fontSize, float spacing, Color color);

// ---- Cutscene do CIENTISTA (reutilizada na transição de Mundo e na vitória) ----
// Avança o typewriter e processa input (SPACE/ENTER/Q/clique) de uma página.
// Retorna 0 = digitando/aguardando; 1 = avançou de página; 2 = concluiu (última
// página confirmada). Compartilhado pela transição pós-Mundo 1 e pela tela final.
int ScientistDialogAdvance(DialogState *dlg, const char *pageText, int pageCount,
                           int voiceScope, float sfxVolume);
// Desenha a cena: cientista em destaque (círculo grande, mesma arte do tutorial)
// + caixa de diálogo grande com efeito typewriter (o texto nunca vaza). `entry`
// (0..1) anima a entrada; `header` rotula a transmissão.
void DrawScientistDialog(Font font, DialogState *dlg, const char *header,
                         const char *pageText, int pageCount, Color accent, float entry);
// Páginas do diálogo final do cientista (a 2a inclui score/nível/abates). Preenche
// `out` (capacidade >= 3) e retorna a contagem. Buffer estável durante a tela.
int VictoryDialogPages(GameState *game, const char **out);
// Tooltip (painel pequeno) ancorado perto de um ponto, mantido dentro da tela.
void DrawTooltip(Font font, const char *text, Vector2 anchor);
// Fundo temático por tela (gradiente + células biológicas flutuantes). `entry`
// (0..1) controla o fade de entrada; faz morph suave entre temas.
void DrawThemedBackground(int screen, float time, float entry);
// Recorte em coordenadas virtuais 1280x720. Converte para pixels reais da
// janela, respeitando o letterbox/fullscreen aplicado em main.c.
void BeginVirtualScissorMode(Rectangle r);
void EndVirtualScissorMode(void);

// ---- Helpers procedurais reutilizáveis do MENU (tema biológico/sci-fi) ----
// Vírus (círculo com espículas + núcleo de RNA), bactéria (bastonete com cílios)
// e símbolo de risco biológico (pulsante). Desenhados em coordenadas de tela.
void DrawMenuVirus(Vector2 center, float radius, float rotationDeg, Color col);
void DrawMenuBacteria(Vector2 center, float size, float angleDeg, Color col);
void DrawMenuBiohazard(Vector2 center, float radius, float pulse, Color col);
// Fundo animado do menu: gradiente azul-marinho + ECG + micróbios nas bordas
// (posições determinísticas, animação por tempo, baixo custo). `accent` faz o
// morph conforme o item destacado.
void DrawMenuBackground(Color accent, float time, float entry);
// Título neon em duas linhas (DISEASE'S / DOOMSDAY) com sombra, glow em camadas
// e entrada animada. centerX define o eixo horizontal; `entry` 0..1.
void DrawNeonTitle(Font font, float centerX, float topY, float scale, float entry, float time);
// Banner/hero do menu a partir de menu_background.png, em proporção correta
// (nunca distorce) dentro de uma área. Retorna true se o PNG foi usado.
bool DrawMenuBanner(Rectangle area, float entry);

// Layout ÚNICO do menu principal: posiciona os 8 botões (menuButtons[].bounds) e
// o campo de nome. Desenho e input usam EXATAMENTE os mesmos retângulos.
void MenuApplyLayout(void);
Rectangle MenuNameRect(void);

// ============================================================================
// SISTEMA VISUAL COMPARTILHADO (padrão Arsenal) — componentes reutilizáveis
// ============================================================================
// Card com estados (normal/hover/selecionado), borda/glow de acento e entrada.
void DrawUICard(Rectangle r, Color accent, bool hover, bool selected, float entry);
// Aba/chip selecionável (tabs no estilo card).
void DrawUITab(Font font, Rectangle r, const char *text, Color accent, bool active, bool hover);
// Título de tela (centralizado, com glow) + sublinha de acento.
void DrawUIScreenTitle(Font font, const char *text, Color accent, float entry);
// Botão "voltar" consistente (canto inferior esquerdo); retorna true se clicado.
bool DrawUIBackButton(Font font, Vector2 mouse, const char *text);
// Barra de atributo (rótulo + valor 0..1 + texto), contida em `w`.
void DrawUIStatBar(Font font, float x, float y, float w, const char *label, float v, const char *valueTxt, Color col);
// Campo de texto/entrada (com placeholder, foco e cursor opcional).
void DrawUIInput(Font font, Rectangle r, const char *text, const char *placeholder, bool active, bool hover, bool showCursor);
// Painel de seção com cabeçalho.
void DrawUISectionPanel(Font font, Rectangle r, const char *title, Color accent, float entry);
// Toast/feedback flutuante (texto curto), alpha 0..1.
void DrawUIToast(Font font, const char *text, Color accent, float alpha);
// Geometria unica dos sliders, compartilhada por desenho e input.
Rectangle SettingsMusicVolumeTrack(void);
Rectangle SettingsSfxVolumeTrack(void);

// Tela de SELEÇÃO DE DIFICULDADE (3 cards: Fácil/Médio/Difícil) — abre ao
// iniciar/reiniciar um jogo; confirma em "INICIAR MISSAO".
void DrawTelaDifficulty(GameState *game, Font font);
void UpdateTelaDifficulty(GameState *game, Vector2 mouse);

// Verifica (com cache de ~1s) se existe algum arquivo de save.
// Evita abrir arquivos do disco a cada frame no menu.
bool AnySaveExistsCached(void);

void DrawSciFiBox(Rectangle r, Color col);

// Funções de desenho das respectivas telas
void DrawHUD(GameState *game, Font font);
void DrawTelaMenu(GameState *game, Font font, float time);
void DrawTelaControles(GameState *game, Font font);
void UpdateTelaTutorial(GameState *game, Vector2 mouse);
void DrawTelaGameplay(GameState *game, Font font, bool drawHUD);
void DrawTelaPausa(GameState *game, Font font);
void DrawTelaGameOver(GameState *game, Font font);
void DrawTelaVitoria(GameState *game, Font font);

// Funções de atualização dos estados de hover e clique dos botões em cada tela
bool UpdateButtonsMenu(GameState *game, Vector2 mouse);
void UpdateButtonsControles(GameState *game, Vector2 mouse);
void UpdateButtonsPause(GameState *game, Vector2 mouse);
void UpdateButtonsGameOver(GameState *game, Vector2 mouse);
void UpdateButtonsVitoria(GameState *game, Vector2 mouse);
void UpdateButtonsSkins(GameState *game, Vector2 mouse);

// Tela de Tutorial (Seringa de Vacina)
// O mundo (cena da seringa) vai para a textura virtual; o HUD é desenhado por
// cima em resolução nativa. DrawTelaTutorial mantém a chamada combinada.
void DrawTutorialWorld(GameState *game, Font font);
void DrawTutorialHUD(GameState *game, Font font);
void DrawTelaTutorial(GameState *game, Font font);

// Libera recursos de GPU criados sob demanda pela gameplay/HUD (biossensor).
void UnloadGameplayResources(void);

// Fundo animado compartilhado (menu + loading): patógenos destruídos por
// seringas. entry (0..1) controla o fade de entrada.
void DrawMenuFXBackground(float time, float entry);

// Tela de Carregamento (Loading)
void DrawTelaLoading(GameState *game, Font font);
int GetLoadingTipCount(void);
const char *GetLoadingTipText(int index);

// Tela de Configurações
void DrawTelaSettings(GameState *game, Font font);
void UpdateButtonsSettings(GameState *game, Vector2 mouse, GameScreen backScreen);
// Funções de desenho e atualização das telas de seleção de save/load
void DrawTelaSaveSelect(GameState *game, Font font, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT]);
void DrawTelaLoadSelect(GameState *game, Font font, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT]);
int UpdateButtonsSaveSelect(GameState *game, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT]);
int UpdateButtonsLoadSelect(GameState *game, Vector2 mouse, Texture2D slotTextures[SAVE_SLOT_COUNT], bool slotTexturesLoaded[SAVE_SLOT_COUNT]);

// Tela de Quiz Educacional
void DrawTelaQuiz(GameState *game, Font font);
void UpdateTelaQuiz(GameState *game, Vector2 mouse);

// Tela de Upgrade do SUS
void DrawTelaUpgrade(GameState *game, Font font);
void UpdateTelaUpgrade(GameState *game, Vector2 mouse);
void DrawTelaStageComplete(GameState *game, Font font);
void UpdateTelaStageComplete(GameState *game, Vector2 mouse);
void DrawTelaStagePrologue(GameState *game, Font font);
void UpdateTelaStagePrologue(GameState *game, Vector2 mouse);

// Tela de Arsenal (detalhes de todas as armas)
void DrawTelaArsenal(GameState *game, Font font);
void UpdateTelaArsenal(GameState *game, Vector2 mouse);

// Tela dedicada de seleção de Skins (com preview)
void DrawTelaSkins(GameState *game, Font font);
void UpdateTelaSkins(GameState *game, Vector2 mouse);

// Tela do Modo Administrador / Dev (senha + configuração)
void DrawTelaAdmin(GameState *game, Font font);
void UpdateTelaAdmin(GameState *game, Vector2 mouse);

#endif // TELAS_H
