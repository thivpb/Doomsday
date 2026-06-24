// game.h
// Definições de constantes, enums e estruturas do RPG.
// Disease's Doomsday — Projeto de Saúde Pública / DF
#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include <stdbool.h>

// ============================================================================
// CONSTANTES DO JOGO
// ============================================================================
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
// Mundo ~2x a area anterior (4000 -> 5657 ≈ 4000*sqrt(2)). A geometria do corpo
// em map_body.c e reescalada automaticamente (macros MBX/MBY/MBR) e a mascara de
// colisao deve ser re-gerada (make collision-mask) sempre que isto mudar.
#define MAP_WIDTH 5657
#define MAP_HEIGHT 5657

// ============================================================================
// CORES GLOBAIS DO TEMA
// ============================================================================
#define THEME_COLOR_MAIN (Color){ 0, 229, 255, 255 }     // Cyan brilhante para bordas e destaques
#define THEME_COLOR_TEXT (Color){ 80, 220, 120, 255 }    // Verde biológico para textos de título
#define THEME_COLOR_BG_DARK (Color){ 6, 18, 10, 255 }    // Verde-petóleo escuro (fundo)
#define THEME_COLOR_BG_LIGHT (Color){ 10, 28, 18, 255 }  // Verde-petóleo um pouco mais claro
#define THEME_COLOR_PANEL (Color){ 14, 10, 26, 255 }     // Cor base para painéis/fundos transparentes
#define THEME_COLOR_BORDER (Color){ 84, 52, 148, 255 }   // Cor base para bordas padrão


// Dimensões do mapa do Tutorial: interior de uma Seringa de Vacina deitada
#define SYRINGE_WIDTH  1600
#define SYRINGE_HEIGHT 400

#define MAX_ENEMIES 60
#define MAX_POWERUPS 20
#define MAX_PARTICLES 250
#define MAX_PROJECTILES 100
#define MAX_DAMAGE_TEXTS 32
#ifndef WEAPON_SLOT_COUNT
#define WEAPON_SLOT_COUNT 4
#endif
#define MAX_CORES 4          // Núcleos de Infecção do escudo do chefe (fase 3)
#define MAX_BOSS_MINIONS 8   // Limite de lacaios invocados ativos do chefe
#define MAX_BIOMINES 12      // Minas do Desestabilizador de RNA ativas no chão
#define MANUAL_SAVE_SLOTS 3
#define AUTO_SAVE_SLOT 4
#define SAVE_SLOT_COUNT 4

// ----------------------------------------------------------------------------
// MUNDOS (expansão: campanha em 2 Mundos temáticos)
// O jogo é dividido em dois Mundos: Bactérias e Vírus. Cada Mundo tem o mesmo
// número de ondas e termina com um chefe na última onda. Concluir o Mundo das
// Bactérias leva (via cutscene de transição) ao Mundo dos Vírus; concluir o
// Mundo dos Vírus leva à Vitória final.
// ----------------------------------------------------------------------------
#define WAVES_PER_WORLD 5    // Ondas em cada Mundo (a 5ª é sempre o chefe)
#define WORLD_COUNT 2        // Total de Mundos da campanha

// Skins disponíveis (player e armas)
#define SKIN_COUNT 3
#define WEAPON_SKIN_COUNT 3

// ============================================================================
// ENUMS
// ============================================================================
typedef enum GameScreen
{
    SCREEN_MENU,
    SCREEN_CONTROLS,
    SCREEN_GAMEPLAY,
    SCREEN_PAUSE,
    SCREEN_GAMEOVER,
    SCREEN_VICTORY,
    SCREEN_SAVE_SELECT,
    SCREEN_LOAD_SELECT,
    SCREEN_SETTINGS,
    SCREEN_TUTORIAL,   // Tutorial inicial: interior da Seringa de Vacina
    SCREEN_LOADING,    // Tela de carregamento entre transições de cena
    SCREEN_QUIZ,       // Tela de quiz educacional (DF)
    SCREEN_UPGRADE,    // Tela de upgrade do SUS
    SCREEN_ARSENAL,    // Tela de arsenal: detalhes de todas as armas
    SCREEN_SKINS,      // Tela de seleção de skins com preview
    SCREEN_ADMIN,      // Modo Administrador / Dev (protegido por senha)
    SCREEN_WORLD_TRANSITION, // Cutscene/tela educativa entre Mundo 1 (Bactérias) e Mundo 2 (Vírus)
    SCREEN_DIFFICULTY_SELECT, // Seleção de dificuldade (cards) ao iniciar/reiniciar um jogo
    SCREEN_STAGE_COMPLETE,    // Pausa curta entre fim da onda e quiz
    SCREEN_STAGE_PROLOGUE     // Prólogo breve antes da próxima onda
} GameScreen;

// ============================================================================
// MUNDOS (expansão da campanha)
// ============================================================================
typedef enum WorldType
{
    WORLD_BACTERIA = 0, // Mundo 1: bactérias (pneumonia, superbactéria KPC)
    WORLD_VIRUS    = 1  // Mundo 2: vírus de RNA com escudo de capsídeo (dengue, influenza)
} WorldType;

// ============================================================================
// DIFICULDADE
// ============================================================================
typedef enum Difficulty
{
    DIFFICULTY_EASY,
    DIFFICULTY_MEDIUM,
    DIFFICULTY_HARD
} Difficulty;

// Configuração central de dificuldade. A IA consulta estes valores em vez de
// usar números mágicos espalhados pelo código.
typedef struct DifficultyConfig
{
    float enemyHealthMul;   // multiplicador de vida dos inimigos
    float enemyDamageMul;   // multiplicador de dano dos inimigos
    float enemySpeedMul;    // multiplicador de velocidade de movimento
    float detectionRange;   // distância para perceber o jogador (entrar em AGGRO)
    float loseSightRange;   // distância para começar a perder o jogador de vista
    float reactionMul;      // multiplica tempos de carga/cooldown (menor = reage mais rápido)
    float dodgeChance;      // 0..1 chance de esquivar de um projétil próximo
    float flankAmount;      // intensidade do flanqueamento (cerco) do melee
    float retreatThreshold; // fração de HP em que inimigos frágeis recuam
    float summonMul;        // frequência de invocação de lacaios (chefe/mini chefe)
    float bossAggroMul;     // agressividade/cadência do chefe
    float aggroMemoryTime;  // segundos perseguindo a última posição conhecida
} DifficultyConfig;

// Destino após a tela de carregamento completar
typedef enum LoadTarget
{
    LOAD_TO_TUTORIAL,
    LOAD_TO_GAMEPLAY,
    LOAD_TO_MENU,
    LOAD_TO_GAMEOVER,
    LOAD_TO_VICTORY
} LoadTarget;

// Sistema de Diálogo do Tutorial (Typewriter + Q para avançar)
typedef struct DialogState
{
    bool  active;       // true = diálogo visível bloqueando gameplay
    int   page;         // página atual dentro do passo
    int   charShown;    // número de caracteres já revelados (efeito typewriter)
    float charTimer;    // acumulador de tempo para revelar próximo caractere
} DialogState;

// Entities
#include "../src/entities/player.h"
#include "../src/entities/enemy.h"
#include "../src/entities/projectiles.h"


typedef enum PowerUpType
{
    HP_RECOVERY,
    SPEED_BOOST,
    SHIELD,
    ATTACK_BOOST,
    // ---- Itens da expansão (drops) ----
    POWERUP_MASK,        // Máscara Hospitalar (Mundo 1): reduz dano recebido
    POWERUP_DISTANCING,  // Distanciamento Social (Mundo 1): aura que repele inimigos
    POWERUP_RNA_GRENADE, // Desestabilizador de Ácidos Ribonucleicos (ambos): dano em área
    POWERUP_CYTOKINE,    // Citocina de Estabilização (ambos): regenera vida
    POWERUP_SUPREME_ORB, // Orbe Supremo: todos os atributos fortes por pouco tempo
    POWERUP_BARRIER,     // Barreira de Plasma: escudo reforçado visível
    POWERUP_TYPE_COUNT
} PowerUpType;

// Quantidade de tipos de power-up "genéricos" sorteados nos drops do mapa
// (os 4 primeiros). Os itens da expansão são dropados por lógica própria.
#define BASE_POWERUP_TYPES 4

// ============================================================================
// ESTRUTURAS DE ENTIDADES
// ============================================================================


typedef struct PowerUp
{
    Vector2 position;
    PowerUpType type;
    bool active;
    float pulseTimer; // Para efeito visual pulsante no HUD e mapa
} PowerUp;

typedef struct Particle
{
    Vector2 position;
    Vector2 velocity;
    Color color;
    float size;
    float lifeTime;
    float maxLifeTime;
    bool active;
} Particle;

// Número de dano flutuante (feedback de combate)
typedef struct DamageText
{
    Vector2 position;
    int value;
    float timer;    // tempo restante de vida
    float maxTime;  // duração total
    Color color;
    bool active;
} DamageText;



// Núcleo de Infecção: estrutura destrutível que alimenta o escudo do chefe (fase 3)
typedef struct InfectionCore
{
    Vector2 position;
    int   hp;
    int   maxHp;
    bool  active;
    float hitFlash;   // brilho ao receber dano
    float pulse;      // animação
} InfectionCore;

// Mina biológica plantada pelo Desestabilizador de RNA. Ela só detona quando
// atingida por tiros do jogador ou pelo melee, criando uma decisão tática.
typedef struct BioMine
{
    Vector2 position;
    bool active;
    float armTimer;
    float autoDetonateTimer; // >0 explode sozinha ao zerar; 0 = apenas gatilhos manuais
    float pulse;
    int damage;
} BioMine;

// ============================================================================
// ESTRUTURAS DE UI
// ============================================================================
typedef struct UIButton
{
    Rectangle bounds;
    const char *text;
    bool hover;
    bool clicked;
} UIButton;

// ============================================================================
// METADADOS DO SLOT DE SAVE
// ============================================================================
typedef struct SaveSlotMeta
{
    bool exists;
    char name[16];
    int level;
    int score;
    int wave;
    char date[32];
} SaveSlotMeta;

// ============================================================================
// ESTADO GLOBAL DO JOGO
// ============================================================================
typedef struct GameState
{
    GameScreen currentScreen;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    PowerUp powerUps[MAX_POWERUPS];
    Particle particles[MAX_PARTICLES];
    int particlePool[MAX_PARTICLES];
    int particlePoolCount;
    Projectile projectiles[MAX_PROJECTILES];
    BioMine bioMines[MAX_BIOMINES];
    
    int totalEnemiesKilled;
    int weaponKills[WEAPON_SLOT_COUNT]; // abates por slot de arma para evoluções
    int enemiesRemaining;
    int wave;
    Camera2D camera;
    
    // Sistema
    bool saveLoaded;
    char notificationMsg[32];
    float timeElapsed;
    float screenShake;
    float uiAnimTimer;
    // ---- Estado de UI (transitório; NÃO é salvo) ----
    // Tempo (s) desde que a tela atual foi aberta — alimenta animações de entrada
    // (fade/slide), morph de fundo e transições.
    float screenAnim;
    GameScreen highlightScreen; // tela "destacada" p/ morph de fundo (item do menu)
    int   menuHighlight;        // índice do item de menu sob o cursor (-1 = nenhum)
    // Tela de seleção de dificuldade: opção tentativa (só vira game->difficulty ao
    // confirmar) e tela para onde "VOLTAR" retorna.
    int   pendingDifficulty;
    int   diffReturnScreen;     // GameScreen de retorno ao cancelar a seleção
    
    // Controle de Input do Nome
    bool nameInputActive;
    
    // Animação de ataque (Slash)
    float slashAnimTimer;
    Vector2 slashAnimPos;
    float slashAnimRadius;
    int slashAnimKind;       // 0=onda circular, 1=estocada, 2=corte lateral, 3=descarga
    Vector2 slashAnimDir;    // direção do ataque para previews e hitbox visual
    int meleeComboStep;      // alterna os ataques melee

    // Números de dano flutuantes
    DamageText damageTexts[MAX_DAMAGE_TEXTS];

    // Acumulador fracionário do dano de veneno no jogador (HP é int)
    float poisonTickAccum;

    // Flash vermelho na tela ao receber dano (feedback visual). Transitório,
    // não é salvo/carregado.
    float hurtFlashTimer;

    // Metadados dos slots carregados na tela de seleção
    SaveSlotMeta slotsMeta[SAVE_SLOT_COUNT];
    
    // Configuracoes de audio independentes (persistidas em Saves/config.txt).
    float musicVolume;
    float sfxVolume;

    // ---- Estado do Tutorial (Seringa de Vacina) ----
    bool inTutorial;            // true = jogador está dentro da seringa
    int  tutorialStep;          // 0=Movimento, 1=Combate, 2=Saída pela agulha
    float tutorialTimer;        // Cronômetro acumulador para o passo de movimento
    bool tutorialEnemySpawned;  // Garante que apenas 1 bactéria tutorial seja criada
    DialogState tutorialDialog; // Estado do sistema de diálogo do tutorial
    DialogState sceneDialog;    // Diálogo das cutscenes do cientista (transição/vitória)
    // Debounce de ataque do tutorial: enquanto um diálogo está ativo este latch
    // fica TRUE; o ataque (SPACE/clique) só dispara depois que o jogador SOLTAR a
    // tecla/botão que fechou a última página — evita atacar a bactéria que acabou
    // de spawnar usando o mesmo input que avançou o texto. (Transitório; não salvo.)
    bool attackInputLatched;
    bool injectionCutscene;     // Cutscene de injeção na seringa
    float injectionTimer;       // Timer da cutscene

    // ---- Tela de Carregamento ----
    LoadTarget  loadTarget;      // Para onde ir após o loading terminar
    float       loadingTimer;    // Tempo decorrido na tela de loading
    float       loadingDuration; // Duração total da tela de loading (segundos)
    int         loadingTip;      // Índice da dica educativa atual
    int         loadSlot;        // Slot de save para carregar após o loading (0 = não carrega)
    bool        shouldClose;     // Flag para fechar o jogo
    bool        syringeTransitionFX; // Efeito de compressão/tremor (tutorial -> gameplay)

    // ---- Banner/Toast de feedback (onda, chefe, troca/desbloqueio de arma) ----
    // Transitório, não é salvo/carregado.
    char  bannerMsg[48];
    char  bannerSub[64];
    float bannerTimer;   // tempo restante visível
    float bannerMax;     // duração total
    Color bannerColor;

    // Maior arma já desbloqueada nesta partida (progressão de RPG).
    // 1=Espada-Seringa, 2=Fuzil, 3=Granada, 4=BFG.
    int   maxWeaponUnlocked;
    // One-shot: avisamos uma única vez quando a Lâmina Bioelétrica abre
    // por abates (>= BIOBLADE_UNLOCK_KILLS). Recomputado no load a partir de kills.
    bool  bioBladeAnnounced;
    bool  weaponEvolutionAnnounced[WEAPON_SLOT_COUNT];

    // ---- Escudo do Chefe (Fase 3): Núcleos de Infecção ----
    InfectionCore cores[MAX_CORES];
    bool  bossShieldActive;   // true = chefe protegido até destruir os núcleos
    bool  bossCoresSpawned;   // garante que os núcleos surjam uma única vez por luta

    // ---- Modo Administrador / Dev (NÃO é salvo no progresso) ----
    bool  adminMode;          // true = modo admin ativo
    int   adminMaxHp;         // valores configurados para aplicar ao iniciar
    int   adminDamage;
    float adminSpeed;
    int   adminLevel;
    int   adminSus;
    bool  adminApply;         // true = aplicar os valores configurados ao começar
    bool  adminUnlockWeapons; // true = libera todas as armas, incluindo a Lamina
    bool  adminUnlockSkins;   // true = libera todos os cosmeticos/skins
    bool  debugCollision;     // overlay de depuração da colisão (admin/debug; transitório, NÃO salvo)

    // ---- Dificuldade ----
    int   difficulty;         // Difficulty (EASY/MEDIUM/HARD)
    DifficultyConfig diff;    // configuração derivada da dificuldade selecionada

    // ---- Percepção do jogador pela IA (transitório; NÃO é salvo) ----
    // Velocidade suavizada do herói, usada para antecipação LIMITADA de mira dos
    // inimigos ranged (eles "lideram" um pouco o tiro conforme a dificuldade).
    Vector2 playerVelSmooth;
    Vector2 playerPrevPos;

    // ---- Mundo atual (expansão: campanha em 2 Mundos) ----
    // WORLD_BACTERIA (0) por padrão — saves antigos, que não gravam este campo,
    // assumem o Mundo das Bactérias graças ao memset(0) no carregamento.
    int   currentWorld;       // WorldType: WORLD_BACTERIA ou WORLD_VIRUS
    bool  worldCompleted;     // true = chefe do Mundo atual derrotado (gatilho de transição)
} GameState;

#endif // GAME_H
