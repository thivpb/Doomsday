#ifndef ENEMY_H
#define ENEMY_H

#include "raylib.h"
#include <stdbool.h>

typedef enum EnemyState
{
    IDLE,
    AGGRO,
    ATTACK,
    HURT,
    DEATH
} EnemyState;

// ----------------------------------------------------------------------------
// TIPOS DE PATÓGENO (campo Enemy.type)
// 0..4: tipos legados (compatibilidade de save). 5+: tipos dos Mundos da
// expansão. O comportamento de IA segue principalmente isRanged/tier; o `type`
// define o visual (enemy_model.c / sprites) e algumas particularidades.
// ----------------------------------------------------------------------------
#define ETYPE_SARS        0   // legado: SARS-CoV-2 (vírus, melee equilibrado)
#define ETYPE_DENGUE_OLD  1   // legado: dengue (vírus, ranged rápido)
#define ETYPE_KPC         2   // bactéria: Superbactéria KPC (tanque/chefe Mundo 1)
#define ETYPE_CHAGAS      3   // legado: Trypanosoma cruzi (protozoário, melee)
#define ETYPE_TB          4   // bactéria: Mycobacterium tuberculosis (ranged pesado)
// --- Mundo 1 (Bactérias) ---
#define ETYPE_BACT_MELEE  5   // bactéria corpo a corpo (cocos/pneumonia)
#define ETYPE_BACT_RANGED 6   // bactéria à distância (bacilo atirador)
// --- Mundo 2 (Vírus, com escudo de capsídeo) ---
#define ETYPE_VIRUS_MELEE  7  // vírus envelopado corpo a corpo (tema dengue)
#define ETYPE_VIRUS_RANGED 8  // vírus à distância (tema influenza)
#define ETYPE_VIRUS_BOSS   9  // chefe viral (tema coronavírus, Mundo 2, onda 5)
// IDs ACRESCENTADOS NO FINAL (compatibilidade de saves antigos preservada):
#define ETYPE_VIRUS_SWARM  10 // vírus de enxame (tema rinovírus): pequeno, rápido, frágil
#define ETYPE_VIRUS_ELITE  11 // vírus elite/mutante (tema sarampo): grande, capsídeo reforçado

typedef enum EnemyTier
{
    TIER_1,
    TIER_2,
    TIER_3,
    TIER_3_BOSS,
    TIER_MINIBOSS   // Mini chefe de cada onda (entre o elite e o chefe final)
} EnemyTier;

// Comportamento de IA do patógeno (consultado pela IA/render via o arquétipo).
typedef enum EnemyBehavior
{
    BEHAV_MELEE = 0,  // persegue e causa dano de contato
    BEHAV_RANGED,     // mantém distância e atira (kiting + strafe)
    BEHAV_SWARM,      // pequeno e veloz, ataca em grupo (contato)
    BEHAV_ELITE,      // tanque que alterna entre kiting e investida
    BEHAV_BOSS        // chefe multi-fase (lógica dedicada)
} EnemyBehavior;

typedef struct Enemy
{
    Vector2 position;
    float speed;
    int hp;
    int maxHp;
    EnemyState state;
    Vector2 patrolTarget;
    float patrolTimer;
    // Tipos de Patógenos:
    // 0 = Vírus: SARS-CoV-2 (Equilibrado)
    // 1 = Vírus da Dengue (Mosquito Aedes aegypti - Rápido, Atirador)
    // 2 = Bactéria: KPC (Superbactéria - Lento, Muito HP, Dano alto)
    // 3 = Protozoário: Trypanosoma cruzi (Rápido, corpo-a-corpo)
    // 4 = Bactéria: Mycobacterium tuberculosis (Tuberculose - Atirador pesado)
    int type;
    EnemyTier tier;
    bool active;     // Ativo/vivo no jogo
    bool isTutorialEnemy; // Inimigo de treino do tutorial (não ataca)
    
    // Ranged
    float cooldownTimer;
    float chargeTimer;
    bool isRanged;
    
    // Status effects
    float poisonTimer;
    float poisonAccum; // Acumulador fracionário do dano de veneno (hp é int)
    float slowTimer;
    int   lastHitWeaponSlot; // último slot do jogador que causou dano (para evolução)

    // ---- IA avançada ----
    float flankSign;   // -1 ou +1: lado preferido para cercar o jogador (melee)
    float fleeTimer;   // > 0 = recuando (inimigo frágil com pouca vida)
    bool  isEscort;    // true = lacaio escolta do chefe (protege/orbita o boss)
    int   aiPhase;     // fase de comportamento do CHEFE (0,1,2) p/ detectar transições
    float summonTimer; // cronômetro do chefe para invocar lacaios
    float hitCooldown; // i-frames de acerto p/ chefes/minichefes (evita melt por arma perfurante)

    // ---- Percepção e esquiva (sistema de dificuldade) ----
    Vector2 lastKnownPlayerPos; // última posição vista do jogador
    float aggroMemory;          // tempo restante perseguindo a última posição conhecida
    float dodgeCooldown;        // cooldown entre esquivas de projétil

    // ---- Escudo de capsídeo (Mundo 2 — Vírus) ----
    // Enquanto shieldActive, o inimigo NÃO recebe dano de vida: o dano vai
    // primeiro para shieldHp. Só após quebrar o capsídeo (shieldHp<=0) ele fica
    // vulnerável. Inimigos sem escudo deixam shieldActive=false.
    bool  shieldActive;   // true = capsídeo intacto, protege a vida
    int   shieldHp;       // pontos de vida do escudo
    int   shieldMaxHp;    // capacidade máxima do escudo
    float shieldHitFlash; // brilho ao receber dano no escudo

    // ---- Animação procedural (transitório; NÃO é salvo) ----
    // Tudo baseado em tempo/estado (não em aleatoriedade por frame): o update
    // alimenta estes campos e o renderer os usa para squash/stretch, bobbing,
    // inclinação na direção do movimento, antecipação de ataque e recuo.
    Vector2 velSmooth;  // velocidade suavizada (deslocamento real/seg) p/ lean e stretch
    float   animTime;   // relógio de animação por inimigo (acumula delta)
    float   attackAnim; // envelope 0..1 de antecipação(+)/recuo(-) do ataque ranged
    float   spawnAnim;  // 0..1 "pop-in" ao surgir (cresce de 0 a 1)
} Enemy;

// ============================================================================
// ARQUÉTIPOS DE PATÓGENO (configuração centralizada)
// ----------------------------------------------------------------------------
// Uma única tabela define stats/visual/comportamento de cada tipo, evitando
// blocos duplicados em wave_manager.c e números mágicos espalhados pela IA e
// pelo renderer. A IA lê `behavior`; o renderer lê `sizeScale`/`palette`; o
// wave manager usa EnemyInitFromArchetype() para inicializar TODOS os campos.
// ============================================================================
typedef struct EnemyArchetype
{
    int           type;          // ETYPE_*
    const char   *name;          // nome educativo (ex.: "Rinovirus")
    EnemyBehavior behavior;      // comportamento de IA
    EnemyTier     tier;
    bool          ranged;        // usa ataque à distância
    int           baseHp;        // HP base (onda 1)
    int           hpPerWave;     // incremento de HP por onda
    float         speed;         // velocidade base (px/s)
    int           shieldBase;    // capsídeo base (0 = sem escudo)
    int           shieldPerWave; // incremento do capsídeo por onda
    float         sizeScale;     // multiplicador de tamanho de render (1.0 = comum)
    Color         palette;       // cor procedural principal (fallback sem PNG)
    int           contactDmgBonus; // dano de contato extra (combat_system)
} EnemyArchetype;

// Retorna o arquétipo do tipo informado (NULL se desconhecido).
const EnemyArchetype *EnemyArchetypeFor(int type);

// Inicializa TODOS os campos derivados de stats/visual de `e` a partir do
// arquétipo do `type`, escalando HP por `healthMul` (use 1.0 para deixar a
// escala de dificuldade global por conta do chamador). NÃO define position,
// active nem patrolTarget — isso é responsabilidade do código de spawn.
void EnemyInitFromArchetype(Enemy *e, int type, int wave, float healthMul);

// Conjunto de tipos COMUNS DISTINTOS esperados numa onda do Mundo dos Vírus
// (1..4). Preenche typesOut[] (capacidade >= 4); retorna a contagem. Usado como
// contrato de "presença esperada por onda".
int VirusWaveTypes(int wave, int *typesOut);

// Sacola ORDENADA de tipos para o spawn da onda viral: o spawn usa out[idx % n].
// Contém cada tipo de VirusWaveTypes ao menos uma vez (presença garantida) e
// pondera as proporções (mais enxame/melee, pouco elite). Retorna a contagem.
int VirusWaveBag(int wave, int *out, int cap);

// Tipo do MINI CHEFE viral por onda (introduz o elite na onda 3).
int VirusMiniBossType(int wave);

// Nome do chefe para o HUD, conforme o Mundo atual.
const char *BossDisplayName(int currentWorld);

#endif // ENEMY_H
