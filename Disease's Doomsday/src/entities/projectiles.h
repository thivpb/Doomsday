#ifndef PROJECTILES_H
#define PROJECTILES_H

#include "raylib.h"
#include <stdbool.h>

typedef enum ProjectileType
{
    PROJ_ACID_ARC,          // Ácido bacteriano (Tier 1)
    PROJ_BULLET_SPREAD,     // Picada espalhada do Aedes aegypti (Tier 2)
    PROJ_VOID_BOLT,         // Toxina viral concentrada (Tier 3 / Elite)
    PROJ_BOSS_BULLET,       // Projétil da Superbactéria Resistente (Boss)
    PROJ_PLAYER_RIFLE,      // Fuzil de Células T (legado / genérico)
    PROJ_PLAYER_GRENADE,    // Granada Macrófago
    PROJ_PLAYER_BFG,        // Vacina BFG
    PROJ_PLAYER_PHAGE,      // Rifle de Bacteriófagos (Mundo 1): bônus vs. bactérias
    PROJ_PLAYER_VACCINE,    // Rifle de Vacina (Mundo 2): bônus vs. vírus / escudo
    PROJ_PLAYER_RIFLE_EVOLVED, // Rifle evoluído: duplica uma vez no primeiro impacto
    PROJ_PLAYER_BFG_EVOLVED,   // BFG evoluído: perfura e explode ao fim
    PROJ_VIRAL_SPORE        // Material viral disparado pelos vírus atirador/elite/chefe
} ProjectileType;

typedef struct Projectile
{
    Vector2 position;
    Vector2 velocity;
    bool active;
    ProjectileType type;
    int damage;
    Rectangle hitbox;
    bool isPlayerProjectile;
    float lifeTime; // Proteção SECUNDÁRIA (granada/BFG e backstop dos rifles)
    Vector2 origin; // Posição de disparo (mede a distância percorrida)
    float maxRange; // Alcance máximo em px; <= 0 = sem limite por alcance
    int splitLevel; // rifle evoluído: 0 pode duplicar; 1 já é cópia
    int sourceWeaponSlot; // slot que deve receber o abate se matar
} Projectile;

// Forward declaration of GameState
struct GameState;

void SpawnProjectile(struct GameState *game, Vector2 pos, Vector2 target, ProjectileType type, int dmg);
void SpawnProjectileWithVelocity(struct GameState *game, Vector2 pos, Vector2 velocity, ProjectileType type, int dmg, int splitLevel);

// Avança o projétil por dt: move, atualiza a hitbox e aplica os limites de
// Fase 5 — ALCANCE percorrido (maxRange) e saída para o VOID do corpo (rifles
// retos do jogador). Retorna true se o projétil continua ativo; false se
// estourou alcance/void (e marca active=false). Independente de GameState e da
// raylib gráfica (apenas raymath inline + MapBody_Contains), portanto testável.
bool Projectile_Advance(Projectile *p, float dt);

#endif // PROJECTILES_H
