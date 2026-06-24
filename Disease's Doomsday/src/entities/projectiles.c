#include "projectiles.h"
#include "../../include/game.h"
#include "../../Assets/Maps/map_body.h"
#include "raymath.h"

// ============================================================================
// BALANCEAMENTO DAS ARMAS DE PROJÉTIL DO JOGADOR (Fase 5)
// Velocidade visual, CADÊNCIA (no PlayerAttack) e ALCANCE são parâmetros
// INDEPENDENTES. É o alcance — não um lifetime genérico de 8 s — que limita o
// quão longe o tiro viaja, impedindo a "limpeza" indiscriminada fora da tela.
//   - velocidade ainda responsiva (~620 px/s);
//   - alcance ~1050 px (cabe na faixa 900-1200 do escopo);
//   - lifetime apenas como proteção secundária (> alcance/velocidade ≈ 1,7 s).
// ============================================================================
#define PHAGE_SPEED      620.0f
#define PHAGE_MAX_RANGE 1050.0f
#define PHAGE_LIFE_CAP     2.2f

static inline bool IsStraightRifle(ProjectileType t)
{
    return t == PROJ_PLAYER_PHAGE || t == PROJ_PLAYER_VACCINE ||
           t == PROJ_PLAYER_RIFLE || t == PROJ_PLAYER_RIFLE_EVOLVED;
}

static int ProjectileSourceSlot(ProjectileType t)
{
    if (t == PROJ_PLAYER_PHAGE || t == PROJ_PLAYER_VACCINE ||
        t == PROJ_PLAYER_RIFLE || t == PROJ_PLAYER_RIFLE_EVOLVED) return 2;
    if (t == PROJ_PLAYER_GRENADE) return 3;
    if (t == PROJ_PLAYER_BFG || t == PROJ_PLAYER_BFG_EVOLVED) return 4;
    return 0;
}

static void SpawnProjectileInternal(GameState *game, Vector2 pos, Vector2 dir, ProjectileType type, int dmg, int splitLevel)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (game->projectiles[i].active) continue;
        Projectile *p = &game->projectiles[i];

        dir = Vector2Normalize(dir);
        if (dir.x == 0.0f && dir.y == 0.0f) dir = (Vector2){ 1.0f, 0.0f };

        float speed = 300.0f, maxRange = 0.0f, life = 8.0f;
        switch (type)
        {
            case PROJ_ACID_ARC:       speed = 220.0f; break;
            case PROJ_BULLET_SPREAD:  speed = 350.0f; break;
            case PROJ_VIRAL_SPORE:    speed = 330.0f; break; // material viral (atirador/elite/chefe)
            case PROJ_VOID_BOLT:      speed = 400.0f; break;
            case PROJ_BOSS_BULLET:    speed = 300.0f; break;
            case PROJ_PLAYER_RIFLE:   // rifles retos do jogador: alcance limitado
            case PROJ_PLAYER_PHAGE:
            case PROJ_PLAYER_VACCINE:
            case PROJ_PLAYER_RIFLE_EVOLVED: speed = PHAGE_SPEED; maxRange = PHAGE_MAX_RANGE; life = PHAGE_LIFE_CAP; break;
            case PROJ_PLAYER_GRENADE: speed = 350.0f; life = 1.2f; break; // explode no fim do lifetime
            case PROJ_PLAYER_BFG:     speed = 280.0f; life = 3.0f; break; // perfurante, vida própria
            case PROJ_PLAYER_BFG_EVOLVED: speed = 300.0f; life = 3.2f; maxRange = 960.0f; break;
            default: break;
        }

        p->position = pos;
        p->origin   = pos;
        p->maxRange = maxRange;
        p->velocity = Vector2Scale(dir, speed);
        p->active = true;
        p->type = type;
        p->damage = dmg;
        p->isPlayerProjectile = (type == PROJ_PLAYER_RIFLE || type == PROJ_PLAYER_GRENADE ||
                                 type == PROJ_PLAYER_BFG || type == PROJ_PLAYER_PHAGE ||
                                 type == PROJ_PLAYER_VACCINE || type == PROJ_PLAYER_RIFLE_EVOLVED ||
                                 type == PROJ_PLAYER_BFG_EVOLVED);
        p->lifeTime = life;
        p->splitLevel = splitLevel;
        p->sourceWeaponSlot = ProjectileSourceSlot(type);
        p->hitbox = (Rectangle){ pos.x - 10, pos.y - 10, 20, 20 };
        break;
    }
}

void SpawnProjectile(GameState *game, Vector2 pos, Vector2 target, ProjectileType type, int dmg)
{
    SpawnProjectileInternal(game, pos, Vector2Subtract(target, pos), type, dmg, 0);
}

void SpawnProjectileWithVelocity(GameState *game, Vector2 pos, Vector2 velocity, ProjectileType type, int dmg, int splitLevel)
{
    SpawnProjectileInternal(game, pos, velocity, type, dmg, splitLevel);
}

bool Projectile_Advance(Projectile *p, float dt)
{
    p->position = Vector2Add(p->position, Vector2Scale(p->velocity, dt));
    p->hitbox.x = p->position.x - 10;
    p->hitbox.y = p->position.y - 10;

    // 1) Alcance percorrido: desativa ao exceder maxRange (limite primário).
    if (p->maxRange > 0.0f)
    {
        float dx = p->position.x - p->origin.x, dy = p->position.y - p->origin.y;
        if (dx * dx + dy * dy >= p->maxRange * p->maxRange) { p->active = false; return false; }
    }
    // 2) Parede/void: rifles retos do jogador se dissipam ao sair do corpo,
    //    impedindo limpeza de alvos fora da área jogável.
    if (IsStraightRifle(p->type) && !MapBody_Contains(p->position)) { p->active = false; return false; }

    return true;
}
