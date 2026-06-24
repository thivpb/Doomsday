// gameplay.h
// Declarações das funções de física, combate, IA e persistência do RPG.
#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "game.h"

// Inicializa ou reseta o estado do jogo
void InitGame(GameState *game);

// Inicia a próxima horda/onda de inimigos
void StartNextWave(GameState *game);

// Executa um ciclo de lógica do jogo (movimentação, IA, física, colisão)
void UpdateGameplay(GameState *game, float delta);

// Executa o ataque em área do jogador (Space / Clique)
void PlayerAttack(GameState *game, Vector2 worldMousePos);

// Cria uma partícula no mundo com parâmetros definidos
void InitParticlePool(GameState *game);
void FreeParticle(GameState *game, int idx);
void SpawnParticle(GameState *game, Vector2 position, Vector2 velocity, Color color, float size, float lifeTime);
void SpawnParticleExplosion(GameState *game, Vector2 pos, Color col, int count, float minSpeed, float maxSpeed, float size, float life);

// Spawna um power-up em uma posição específica (por exemplo, após derrotar um inimigo)
void SpawnPowerUpAt(GameState *game, Vector2 position, int type);

// Contabiliza a morte de um inimigo de forma centralizada: XP, score, contadores
// de onda e chance de drop de power-up. Usado por TODAS as armas (lâmina, fuzil,
// granada, BFG, veneno) para manter recompensas consistentes.
// O chamador é responsável por colocar o inimigo em estado DEATH.
void RegisterEnemyKill(GameState *game, Enemy *enemy);

// Salva o progresso do jogo atual em um slot específico (1..3 manuais, 4 autosave)
void SalvarJogoSlot(GameState *game, int slot);
void SalvarJogoSlotSilencioso(GameState *game, int slot);

// Carrega o progresso do jogo a partir de um slot específico (1, 2 ou 3)
void CarregarJogoSlot(GameState *game, int slot);

// Carrega apenas os metadados de um slot específico
SaveSlotMeta CarregarMetadadosSlot(int slot);

// Cria um projétil no mundo disparado por um inimigo
void SpawnProjectile(GameState *game, Vector2 pos, Vector2 target, ProjectileType type, int dmg);

// Inicia uma tela de carregamento e define o destino após completar
// duration: tempo em segundos que a tela de loading fica visível
void RequestLoadingScreen(GameState *game, LoadTarget target, float duration);

// Inicializa o modo Tutorial (posiciona jogador na Seringa de Vacina)
void InitTutorial(GameState *game);

// Executa a lógica de atualização do Tutorial (movimento, combate, saída)
void UpdateTutorial(GameState *game, float delta);

// Executa a lógica da tela de carregamento (timer + transição de cena)
void UpdateTelaLoading(GameState *game, float delta);

// Obtém as linhas de texto para um determinado passo e página do diálogo do tutorial
void GetTutorialDialogText(int step, int page, const char **line1, const char **line2, const char **line3);

// ---- Feedback de combate ----
// Cria um número de dano flutuante na posição informada
void SpawnDamageText(GameState *game, Vector2 pos, int value, Color color);
// Atualiza (sobe/some) os números de dano ativos
void UpdateDamageTexts(GameState *game, float delta);

// ---- Banner/Toast de feedback (onda, chefe, troca/desbloqueio de arma) ----
void ShowBanner(GameState *game, const char *msg, const char *sub, Color color, float duration);
void UpdateBanner(GameState *game, float delta);

// ---- Armas / progressão ----
// Total de entradas no arsenal. As evoluções dividem os mesmos 4 slots de jogo.
#define WEAPON_COUNT           8
#ifndef WEAPON_SLOT_COUNT
#define WEAPON_SLOT_COUNT      4
#endif
#define WEAPON_BIOBLADE        5    // Lâmina Bioelétrica (melee anti-capsídeo desbloqueável)
#define WEAPON_RIFLE_EVOLVED   6    // Rifle Vetorial Replicante
#define WEAPON_RNA_LAUNCHER    7    // Lança-Minas de RNA
#define WEAPON_BFG_EVOLVED     8    // BFG Imunológico Ômega
#define WEAPON_EVOLVE_KILLS    30   // abates por slot necessários para evoluir
#define BIOBLADE_UNLOCK_KILLS  WEAPON_EVOLVE_KILLS
#define RNA_LAUNCHER_FUSE      6.0f // pavio (s) da mina ARREMESSADA pelo Lança-Minas
                                    // de RNA (slot 3 evoluído). Usado no disparo e no
                                    // anel de contagem regressiva do render.

// Informações de uma arma para HUD, Arsenal e Tutorial (fonte única da verdade)
typedef struct WeaponInfo
{
    const char *name;     // Nome da arma
    const char *desc;     // Descrição curta
    const char *special;  // Efeito especial
    const char *playstyle;// Como muda a jogabilidade
    int         baseDamage; // Dano base (somado aos atributos do jogador)
    const char *speedTxt; // Velocidade/cadência (qualitativo)
    float       cooldown; // Cooldown em segundos
    int         unlockLevel; // Nível mínimo para usar
    int         key;      // Tecla (1..4)
    Color       color;    // Cor representativa
    float       maxRange; // Alcance efetivo em px (0 = corpo a corpo / sem alcance)
} WeaponInfo;

// Retorna as informações da arma (weapon = 1..5). Fora do intervalo => Espada-Seringa.
WeaponInfo GetWeaponInfo(int weapon);
// Nível mínimo do jogador para usar a arma (1=Espada-Seringa ... 4=BFG; 5=por abates)
int  WeaponUnlockLevel(int weapon);
// Nome curto da arma (1..5)
const char *WeaponName(int weapon);
int  WeaponSlotForId(int weapon);
int  WeaponEvolutionForSlot(int slot);
int  WeaponBaseForSlot(int slot);
int  WeaponKillCountForSlot(GameState *game, int slot);
int  WeaponKillsToEvolve(GameState *game, int slot);
bool WeaponIsEvolution(int weapon);
// true se a arma está desbloqueada para este estado de jogo: armas 1-4 por
// progressão de nível (maxWeaponUnlocked); Lâmina Bioelétrica por abates
// (totalEnemiesKilled >= BIOBLADE_UNLOCK_KILLS). Fonte única usada por troca/arsenal.
bool WeaponUnlocked(GameState *game, int weapon);
// Define o Mundo atual usado por GetWeaponInfo para nomear/descrever a arma de
// projétil temática (Rifle de Bacteriófagos no Mundo 1; Rifle de Vacina no Mundo
// 2). As armas melee (Espada-Seringa e Lâmina Bioelétrica) não dependem do Mundo.
void SetWeaponWorld(int world);

// ---- Skins ----
// Cores da skin de arma atual (primária = projétil/lâmina, secundária = brilho/trail)
Color WeaponSkinPrimary(int weaponSkinId);
Color WeaponSkinSecondary(int weaponSkinId);
// Nomes legíveis para a UI
const char *PlayerSkinName(int skinId);
const char *WeaponSkinName(int weaponSkinId);

// ---- Configuração persistente (volume + skins + dificuldade) ----
void LoadPlayerConfig(GameState *game);
void SavePlayerConfig(GameState *game);

// ---- Dificuldade ----
// Retorna a configuração para uma dificuldade (EASY/MEDIUM/HARD)
DifficultyConfig MakeDifficultyConfig(int difficulty);
// Aplica game->difficulty em game->diff (recalcula os multiplicadores)
void ApplyDifficulty(GameState *game);
// Nome curto da dificuldade
const char *DifficultyName(int difficulty);

// ---- Núcleos de Infecção / escudo do chefe (fase 3) ----
int  CoresAlive(GameState *game);
void SpawnInfectionCores(GameState *game, Vector2 center);
// Aplica dano numa área aos núcleos; retorna true se algum núcleo foi atingido.
bool HitInfectionCores(GameState *game, Vector2 pos, float radius, int dmg);

// ---- Dano do jogador a um inimigo (i-frames de chefe, limite por golpe,
//      resistência à BFG e escudo do chefe). Retorna o dano efetivo (0=bloqueado).
int  ApplyPlayerDamageToEnemy(GameState *game, Enemy *enemy, int dmg, bool isBFG);
// Classificação biológica do patógeno (afinidade das armas).
bool EnemyIsBacterial(int type);
bool EnemyIsViral(int type);

// ---- Minas do Desestabilizador de RNA ----
void PlantBioMine(GameState *game, Vector2 pos, int dmg);
void PlantBioMineTimed(GameState *game, Vector2 pos, int dmg, float autoDetonateTimer);
bool TriggerBioMinesInRadius(GameState *game, Vector2 pos, float radius);
bool DetonateAllBioMines(GameState *game);

// ---- Transição entre Mundos (Fase 6) ----
// Cutscene/tela educativa exibida ao concluir o Mundo das Bactérias, levando
// ao Mundo dos Vírus. Update trata o avanço; Draw desenha o texto educativo.
void UpdateTelaTransicao(GameState *game);
void DrawTelaTransicao(GameState *game, Font font);

#endif // GAMEPLAY_H
