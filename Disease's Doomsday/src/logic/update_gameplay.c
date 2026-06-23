#include "../../include/gameplay.h"
#include "../../include/spatial_grid.h"
#include "../../Assets/Maps/map_seringa.h"
#include "../../Assets/Maps/map_body.h"
#include "../systems/combat_system.h"
#include "../systems/wave_manager.h"
#include "raymath.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../include/asset_manager.h"
#include "../../include/telas.h"
#include <time.h>
#ifdef _WIN32
#include <process.h>
#define THREAD_RETURN void
#define THREAD_END() _endthread()
#define START_THREAD(func, arg) _beginthread(func, 0, arg)
#else
#include <pthread.h>
#define THREAD_RETURN void *
#define THREAD_END() return NULL
static void StartDetachedThread(void *(*func)(void *), void *arg)
{
    pthread_t thread;
    if (pthread_create(&thread, NULL, func, arg) == 0) {
        pthread_detach(thread);
    }
}
#define START_THREAD(func, arg) StartDetachedThread(func, arg)
#endif
#include <math.h>

// ============================================================================
// AUXILIAR: EMISSOR DE PARTÍCULAS
// ============================================================================
void InitParticlePool(GameState *game)
{
    game->particlePoolCount = MAX_PARTICLES;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        game->particles[i].active = false;
        game->particlePool[i] = MAX_PARTICLES - 1 - i;
    }
}

void FreeParticle(GameState *game, int idx)
{
    if (game->particles[idx].active) {
        game->particles[idx].active = false;
        if (game->particlePoolCount < MAX_PARTICLES) {
            game->particlePool[game->particlePoolCount++] = idx;
        }
    }
}

void SpawnParticle(GameState *game, Vector2 position, Vector2 velocity, Color color, float size, float lifeTime)
{
    if (game->particlePoolCount > 0)
    {
        game->particlePoolCount--;
        int idx = game->particlePool[game->particlePoolCount];

        game->particles[idx].position = position;
        game->particles[idx].velocity = velocity;
        game->particles[idx].color = color;
        game->particles[idx].size = size;
        game->particles[idx].lifeTime = lifeTime;
        game->particles[idx].maxLifeTime = lifeTime;
        game->particles[idx].active = true;
    }
}

// Emite uma explosão radial de partículas
void SpawnParticleExplosion(GameState *game, Vector2 pos, Color col, int count, float minSpeed, float maxSpeed, float size, float life)
{
    for (int i = 0; i < count; i++)
    {
        float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
        float speed = (float)GetRandomValue((int)(minSpeed * 10), (int)(maxSpeed * 10)) / 10.0f;
        Vector2 vel = { cosf(angle) * speed, sinf(angle) * speed };
        SpawnParticle(game, pos, vel, col, size, life + (float)GetRandomValue(-2, 2)/10.0f);
    }
}

// ============================================================================
// AUXILIAR: NÚMEROS DE DANO FLUTUANTES (FEEDBACK DE COMBATE)
// ============================================================================
void SpawnDamageText(GameState *game, Vector2 pos, int value, Color color)
{
    for (int i = 0; i < MAX_DAMAGE_TEXTS; i++)
    {
        if (!game->damageTexts[i].active)
        {
            game->damageTexts[i].active = true;
            game->damageTexts[i].position = (Vector2){ pos.x + (float)GetRandomValue(-12, 12), pos.y - 30.0f };
            game->damageTexts[i].value = value;
            game->damageTexts[i].maxTime = 0.7f;
            game->damageTexts[i].timer = 0.7f;
            game->damageTexts[i].color = color;
            return;
        }
    }
}

void UpdateDamageTexts(GameState *game, float delta)
{
    for (int i = 0; i < MAX_DAMAGE_TEXTS; i++)
    {
        if (game->damageTexts[i].active)
        {
            game->damageTexts[i].position.y -= 55.0f * delta; // sobe
            game->damageTexts[i].timer -= delta;
            if (game->damageTexts[i].timer <= 0.0f) game->damageTexts[i].active = false;
        }
    }
}

// ============================================================================
// BANNER / TOAST DE FEEDBACK (onda, chefe, troca/desbloqueio de arma, level up)
// ============================================================================
void ShowBanner(GameState *game, const char *msg, const char *sub, Color color, float duration)
{
    snprintf(game->bannerMsg, sizeof(game->bannerMsg), "%s", msg ? msg : "");
    snprintf(game->bannerSub, sizeof(game->bannerSub), "%s", sub ? sub : "");
    game->bannerColor = color;
    game->bannerTimer = duration;
    game->bannerMax   = duration;
}

void UpdateBanner(GameState *game, float delta)
{
    if (game->bannerTimer > 0.0f) game->bannerTimer -= delta;
}

// ============================================================================
// SKINS: CORES E NOMES
// ============================================================================
Color WeaponSkinPrimary(int weaponSkinId)
{
    switch (weaponSkinId)
    {
        case 1:  return (Color){ 220, 70, 255, 255 };  // Plasma (magenta)
        case 2:  return (Color){ 130, 220, 40, 255 };  // Tóxica (verde-ácido)
        default: return (Color){ 120, 200, 255, 255 }; // Padrão (azul-imune)
    }
}

Color WeaponSkinSecondary(int weaponSkinId)
{
    switch (weaponSkinId)
    {
        case 1:  return (Color){ 0, 229, 255, 255 };   // Plasma (ciano)
        case 2:  return (Color){ 200, 255, 90, 255 };  // Tóxica (lima)
        default: return (Color){ 235, 245, 255, 255 }; // Padrão (branco-gelo)
    }
}

const char *PlayerSkinName(int skinId)
{
    switch (skinId)
    {
        case 1:  return "MEDICA";
        case 2:  return "INFECTADA";
        default: return "PADRAO";
    }
}

const char *WeaponSkinName(int weaponSkinId)
{
    switch (weaponSkinId)
    {
        case 1:  return "PLASMA";
        case 2:  return "TOXICA";
        default: return "PADRAO";
    }
}

// ============================================================================
// CONFIGURAÇÃO PERSISTENTE (VOLUME + SKINS)
// ============================================================================
void LoadPlayerConfig(GameState *game)
{
    FILE *f = fopen("Saves/config.txt", "r");
    if (f != NULL)
    {
        char line[512] = {0};
        float musicVol = 1.0f, sfxVol = 1.0f;
        int skin = 0, wskin = 0, diff = DIFFICULTY_MEDIUM;
        int consumed = 0;
        int got = 0;

        if (fgets(line, sizeof(line), f) != NULL)
        {
            // AUDIO2: musica, efeitos, skins e dificuldade. O formato anterior
            // comecava por um unico float; nesse caso ele alimenta os dois canais.
            got = sscanf(line, "AUDIO2 %f %f %d %d %d%n",
                         &musicVol, &sfxVol, &skin, &wskin, &diff, &consumed);
            if (got != 5)
            {
                float legacyVol = 1.0f;
                got = sscanf(line, "%f %d %d %d%n", &legacyVol, &skin, &wskin, &diff, &consumed);
                musicVol = legacyVol;
                sfxVol = legacyVol;
            }
        }

        if (got >= 3)
        {
            musicVol = Clamp(musicVol, 0.0f, 1.0f);
            sfxVol = Clamp(sfxVol, 0.0f, 1.0f);
            if (skin < 0 || skin >= SKIN_COUNT) skin = 0;
            if (wskin < 0 || wskin >= WEAPON_SKIN_COUNT) wskin = 0;
            game->musicVolume = musicVol;
            game->sfxVolume = sfxVol;
            game->player.skinId = skin;
            game->player.weaponSkinId = wskin;
            if (got >= 4 && diff >= DIFFICULTY_EASY && diff <= DIFFICULTY_HARD)
                game->difficulty = diff;
        }

        // Guarda-roupa modular fica no restante da mesma linha, tanto no formato
        // legado quanto em AUDIO2.
        const char *cursor = line + consumed;
        for (int s = 0; s < COS_SLOT_COUNT; s++)
        {
            char *end = NULL;
            long v = strtol(cursor, &end, 10);
            if (end != cursor) { game->player.cosmetics[s] = (v > 0 ? (int)v : 0); cursor = end; }
            else               { game->player.cosmetics[s] = 0; }
        }
        // Categoria "Traseiro" foi removida do jogo: ignora qualquer item
        // traseiro de saves/config antigos (mantém a largura para compat.).
        game->player.cosmetics[COS_BACK] = 0;
        fclose(f);
    }
    // Sempre garante uma configuração de dificuldade válida (mesmo sem arquivo).
    ApplyDifficulty(game);
}

void SavePlayerConfig(GameState *game)
{
    FILE *f = fopen("Saves/config.txt", "w");
    if (f == NULL) return;
    fprintf(f, "AUDIO2 %.6f %.6f %d %d %d", game->musicVolume, game->sfxVolume,
            game->player.skinId, game->player.weaponSkinId, game->difficulty);
    // Guarda-roupa modular (mesma linha, na ordem do enum CosmeticSlot).
    for (int s = 0; s < COS_SLOT_COUNT; s++)
        fprintf(f, " %d", game->player.cosmetics[s]);
    fprintf(f, "\n");
    fclose(f);
}

// ============================================================================
// AUXILIAR: SPAWN DE POWER-UPS
// ============================================================================
void SpawnPowerUpAt(GameState *game, Vector2 position, int forcedType)
{
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (!game->powerUps[i].active)
        {
            game->powerUps[i].position = position;
            game->powerUps[i].type = (forcedType >= 0) ? (PowerUpType)forcedType : (PowerUpType)GetRandomValue(0, BASE_POWERUP_TYPES - 1);
            game->powerUps[i].active = true;
            game->powerUps[i].pulseTimer = 0.0f;
            break;
        }
    }
}


// ============================================================================
// AUXILIAR: RECOMPENSA DE MORTE DE INIMIGO (CENTRALIZADA)
// Antes cada arma dava recompensas diferentes — o golpe melee dava o DOBRO de
// XP/score das armas de fogo e era a única fonte de drops de power-up, o que
// tornava os fuzis/granada/BFG inúteis. Agora todas as mortes passam por aqui.
// ============================================================================
void RegisterEnemyKill(GameState *game, Enemy *enemy)
{
    game->player.xp += (enemy->tier + 1) * 10;
    game->player.score += (enemy->tier + 1) * 50;
    game->totalEnemiesKilled++;
    if (game->enemiesRemaining > 0) game->enemiesRemaining--;

    // DESBLOQUEIO DA LÂMINA BIOELÉTRICA por abates — feedback único e
    // claro. A arma fica disponível para o resto da campanha (estado deriva de
    // totalEnemiesKilled, que é salvo, então o desbloqueio sobrevive a save/fase).
    if (!game->bioBladeAnnounced && !game->inTutorial &&
        game->totalEnemiesKilled >= BIOBLADE_UNLOCK_KILLS)
    {
        game->bioBladeAnnounced = true;
        WeaponInfo wbi = GetWeaponInfo(WEAPON_BIOBLADE);
        ShowBanner(game, TextFormat("NOVA ARMA: %s", wbi.name),
                   "Slot [1]: aperte 1 para alternar e quebrar capsideos", wbi.color, 3.6f);
        SpawnParticleExplosion(game, game->player.position, wbi.color, 22, 60.0f, 200.0f, 4.0f, 0.6f);
        if (g_assets.sfxPickup.frameCount > 0) PlaySound(g_assets.sfxPickup);
    }

    // Bônus de recompensa para mini chefes e chefe final
    if (enemy->tier == TIER_MINIBOSS) { game->player.xp += 60; game->player.score += 300; }
    else if (enemy->tier == TIER_3_BOSS) { game->player.xp += 200; game->player.score += 1500; }

    // Chance de drop de power-up (25%) válida para qualquer arma; chefes sempre dropam
    if (enemy->tier == TIER_MINIBOSS || enemy->tier == TIER_3_BOSS || GetRandomValue(0, 100) < 25)
    {
        SpawnPowerUpAt(game, enemy->position, -1);
    }

    // ------------------------------------------------------------------
    // DROPS DE ITENS DA EXPANSÃO (chance ~12% ao matar). Itens variam por
    // Mundo: Máscara e Distanciamento são exclusivos do Mundo 1 (Bactérias);
    // o Desestabilizador de RNA e a Citocina são compartilhados.
    // ------------------------------------------------------------------
    if (!game->inTutorial && GetRandomValue(0, 99) < 12)
    {
        int itemType;
        if (game->currentWorld == WORLD_BACTERIA)
        {
            switch (GetRandomValue(0, 3))
            {
                case 0:  itemType = POWERUP_MASK; break;
                case 1:  itemType = POWERUP_DISTANCING; break;
                case 2:  itemType = POWERUP_RNA_GRENADE; break;
                default: itemType = POWERUP_CYTOKINE; break;
            }
        }
        else // WORLD_VIRUS: apenas itens compartilhados
        {
            itemType = (GetRandomValue(0, 1) == 0) ? POWERUP_RNA_GRENADE : POWERUP_CYTOKINE;
        }
        SpawnPowerUpAt(game, enemy->position, itemType);
    }
}

// ============================================================================
// DIFICULDADE
// ============================================================================
DifficultyConfig MakeDifficultyConfig(int difficulty)
{
    // Filosofia de rebalanceamento (Etapa 5): a pressão vem de COMPORTAMENTO
    // (reação, esquiva, flanqueamento, detecção, cadência, invocações, memória de
    // perseguição) e não de "HP esponjoso". Por isso os multiplicadores de vida e
    // dano sobem pouco entre as dificuldades, enquanto a IA fica nitidamente mais
    // esperta/rápida. O dano máximo é limitado e o jogador tem i-frames, então não
    // há dano inevitável — apenas janelas de esquiva mais curtas.
    DifficultyConfig d;
    switch (difficulty)
    {
        case DIFFICULTY_EASY:
            // "FÁCIL" ≈ o desafio do DIFÍCIL antigo, porém justo: IA agressiva,
            // mas vida/dano moderados e janelas de esquiva confortáveis.
            d.enemyHealthMul = 1.00f; d.enemyDamageMul = 0.90f; d.enemySpeedMul = 1.05f;
            d.detectionRange = 560.0f; d.loseSightRange = 1050.0f; d.reactionMul = 0.88f;
            d.dodgeChance = 0.18f; d.flankAmount = 0.60f; d.retreatThreshold = 0.28f;
            d.summonMul = 1.00f; d.bossAggroMul = 1.00f; d.aggroMemoryTime = 3.0f;
            break;
        case DIFFICULTY_HARD:
            // "DIFÍCIL" extremo: reage quase instantaneamente, esquiva e flanqueia
            // muito, persegue por muito tempo — sem inflar HP de forma absurda.
            d.enemyHealthMul = 1.40f; d.enemyDamageMul = 1.35f; d.enemySpeedMul = 1.28f;
            d.detectionRange = 820.0f; d.loseSightRange = 1500.0f; d.reactionMul = 0.55f;
            d.dodgeChance = 0.50f; d.flankAmount = 1.05f; d.retreatThreshold = 0.18f;
            d.summonMul = 1.70f; d.bossAggroMul = 1.50f; d.aggroMemoryTime = 6.0f;
            break;
        case DIFFICULTY_MEDIUM:
        default:
            // "MÉDIO": significativamente mais difícil que o FÁCIL, com IA mais
            // coordenada (mais flanqueamento, esquiva e invocações).
            d.enemyHealthMul = 1.20f; d.enemyDamageMul = 1.10f; d.enemySpeedMul = 1.15f;
            d.detectionRange = 680.0f; d.loseSightRange = 1250.0f; d.reactionMul = 0.68f;
            d.dodgeChance = 0.34f; d.flankAmount = 0.85f; d.retreatThreshold = 0.24f;
            d.summonMul = 1.30f; d.bossAggroMul = 1.25f; d.aggroMemoryTime = 4.5f;
            break;
    }
    return d;
}

void ApplyDifficulty(GameState *game)
{
    if (game->difficulty < DIFFICULTY_EASY || game->difficulty > DIFFICULTY_HARD)
        game->difficulty = DIFFICULTY_MEDIUM;
    game->diff = MakeDifficultyConfig(game->difficulty);
}

const char *DifficultyName(int difficulty)
{
    switch (difficulty)
    {
        case DIFFICULTY_EASY: return "FACIL";
        case DIFFICULTY_HARD: return "DIFICIL";
        default:              return "MEDIO";
    }
}

// ============================================================================
// NÚCLEOS DE INFECÇÃO (escudo do chefe na fase 3)
// ============================================================================
int CoresAlive(GameState *game)
{
    int n = 0;
    for (int i = 0; i < MAX_CORES; i++)
        if (game->cores[i].active) n++;
    return n;
}

// Cria os Núcleos de Infecção ao redor do chefe e ativa o escudo.
// Usa MapBody_PlaceCores (geometria validada): cada núcleo nasce INTEIRAMENTE
// dentro do corpo, com margem de aproximação melee (CORE_SPAWN_MARGIN), longe
// do chefe (CORE_BOSS_CLEARANCE) e dos outros núcleos (CORE_INTER_DISTANCE).
// Determinístico, com fallback no centro do tórax — nenhum núcleo depende de
// arma de longa distância para ser destruído.
void SpawnInfectionCores(GameState *game, Vector2 center)
{
    Vector2 pts[MAX_CORES];
    int n = MapBody_PlaceCores(center, pts, MAX_CORES,
                               CORE_SPAWN_MARGIN, CORE_BOSS_CLEARANCE, CORE_INTER_DISTANCE);

    for (int i = 0; i < MAX_CORES; i++)
    {
        Vector2 p = (i < n) ? pts[i] : MapBody_GetSafeCenter();

        // Revalidação defensiva: se por algum motivo o ponto não couber com a
        // margem do herói, recua para um ponto livre em direção ao centro.
        if (!MapBody_ContainsWithMargin(p, BODY_PLAYER_RADIUS))
            p = MapBody_FindSpawnPoint(p, BODY_PLAYER_RADIUS);

        game->cores[i].position = p;
        game->cores[i].maxHp = 120 + game->wave * 20;
        game->cores[i].hp = game->cores[i].maxHp;
        game->cores[i].active = true;
        game->cores[i].hitFlash = 0.0f;
        game->cores[i].pulse = (float)i;
        SpawnParticleExplosion(game, p, (Color){ 120, 230, 255, 255 }, 18, 60.0f, 200.0f, 4.0f, 0.7f);
    }
    game->bossShieldActive = true;
    game->bossCoresSpawned = true;
}

bool HitInfectionCores(GameState *game, Vector2 pos, float radius, int dmg)
{
    bool hit = false;
    for (int i = 0; i < MAX_CORES; i++)
    {
        if (!game->cores[i].active) continue;
        float r = radius + 30.0f;
        if (Vector2DistanceSqr(pos, game->cores[i].position) <= r * r)
        {
            game->cores[i].hp -= dmg;
            game->cores[i].hitFlash = 0.18f;
            SpawnParticleExplosion(game, game->cores[i].position, (Color){ 120, 230, 255, 255 }, 8, 50.0f, 150.0f, 3.0f, 0.4f);
            hit = true;
            if (game->cores[i].hp <= 0)
            {
                game->cores[i].active = false;
                SpawnParticleExplosion(game, game->cores[i].position, (Color){ 200, 250, 255, 255 }, 26, 80.0f, 260.0f, 5.0f, 0.8f);
                game->screenShake = 0.5f;
            }
        }
    }
    if (hit && CoresAlive(game) == 0 && game->bossShieldActive)
    {
        game->bossShieldActive = false;
        ShowBanner(game, "ESCUDO DESTRUIDO!", "O chefe esta vulneravel — ataque agora!",
                   (Color){ 120, 255, 160, 255 }, 3.0f);
    }
    return hit;
}

// Classificação biológica do patógeno (para afinidade das armas).
bool EnemyIsBacterial(int type)
{
    return (type == ETYPE_KPC || type == ETYPE_TB ||
            type == ETYPE_BACT_MELEE || type == ETYPE_BACT_RANGED);
}
bool EnemyIsViral(int type)
{
    return (type == ETYPE_SARS || type == ETYPE_DENGUE_OLD ||
            type == ETYPE_VIRUS_MELEE || type == ETYPE_VIRUS_RANGED || type == ETYPE_VIRUS_BOSS ||
            type == ETYPE_VIRUS_SWARM || type == ETYPE_VIRUS_ELITE);
}

// Aplica a afinidade da arma ao dano: o Rifle de Bacteriófagos é muito eficaz
// contra BACTÉRIAS (bacteriófagos infectam bactérias) e o Rifle de Vacina é
// muito eficaz contra VÍRUS (imunização). +60% de dano contra o alvo certo.
int ScaleAffinityDamage(int dmg, ProjectileType pt, int enemyType)
{
    if (pt == PROJ_PLAYER_PHAGE && EnemyIsBacterial(enemyType)) return (int)(dmg * 1.6f);
    if (pt == PROJ_PLAYER_VACCINE && EnemyIsViral(enemyType))   return (int)(dmg * 1.6f);
    return dmg;
}

// Aplica dano de uma arma do jogador a um inimigo, com proteções anti-melt:
// - chefe protegido pelo escudo (fase 3) não recebe dano;
// - chefes/mini chefes têm i-frames contra a BFG perfurante (não derretem);
// - resistência específica contra a BFG; limite máximo de dano por golpe.
// Retorna o dano efetivamente aplicado (0 = bloqueado).
int ApplyPlayerDamageToEnemy(GameState *game, Enemy *enemy, int dmg, bool isBFG)
{
    bool bossLike = (enemy->tier == TIER_3_BOSS || enemy->tier == TIER_MINIBOSS);

    // Escudo do chefe final (fase 3): invulnerável enquanto houver núcleos.
    if (enemy->tier == TIER_3_BOSS && game->bossShieldActive && CoresAlive(game) > 0)
    {
        if (GetRandomValue(0, 4) == 0)
            SpawnParticle(game, enemy->position, (Vector2){ 0, -20 }, (Color){ 120, 230, 255, 255 }, 4.0f, 0.3f);
        return 0;
    }

    // Escudo de capsídeo (Mundo 2 — Vírus): enquanto ativo, o dano é absorvido
    // pelo escudo e NÃO atinge a vida. Só após quebrar o capsídeo o vírus fica
    // vulnerável. (Inimigos sem escudo têm shieldActive=false e ignoram isto.)
    if (enemy->shieldActive && enemy->shieldHp > 0)
    {
        enemy->shieldHp -= dmg;
        enemy->shieldHitFlash = 0.18f;
        SpawnParticle(game, enemy->position, (Vector2){ 0, -20 }, (Color){ 150, 220, 255, 255 }, 4.0f, 0.25f);
        if (enemy->shieldHp <= 0)
        {
            enemy->shieldHp = 0;
            enemy->shieldActive = false;
            // "estilhaço" do capsídeo ao quebrar (+ som)
            SpawnParticleExplosion(game, enemy->position, (Color){ 150, 220, 255, 255 }, 18, 80.0f, 220.0f, 4.0f, 0.6f);
            game->screenShake = 0.3f;
            PlayEnemyDamageSfx(enemy->type, enemy->tier);
        }
        return dmg; // dano aplicado ao ESCUDO (não à vida)
    }

    if (bossLike)
    {
        // i-frames de acerto: impede que a BFG perfurante aplique dano todo frame.
        if (isBFG && enemy->hitCooldown > 0.0f) return 0;

        // Resistência à arma perfurante (BFG), que cresce nas ondas finais.
        if (isBFG)
        {
            float resist = (enemy->tier == TIER_3_BOSS) ? 0.35f : 0.5f;
            dmg = (int)(dmg * resist);
            enemy->hitCooldown = 0.12f;
        }

        // Limite máximo de dano por golpe (evita morte instantânea por arma forte).
        int cap = enemy->maxHp / 12;
        if (cap < 25) cap = 25;
        if (dmg > cap) dmg = cap;
    }

    if (dmg < 1) dmg = 1;
    enemy->hp -= dmg;
    return dmg;
}

// ============================================================================
// DETONAÇÃO DO DESESTABILIZADOR DE ÁCIDOS RIBONUCLEICOS (granada — item)
// Dano em área que ATACA O RNA do patógeno: thematicamente atravessa o capsídeo,
// então quebra o escudo viral e ainda fere a vida. Forte vs. bactéria e vírus.
// ============================================================================
static void DetonateRNAGrenade(GameState *game, Vector2 center)
{
    const float radius = 240.0f;
    int dmg = 60 + game->player.level * 4;

    // Feedback visual/sonoro da explosão
    SpawnParticleExplosion(game, center, (Color){ 120, 255, 160, 255 }, 26, 90.0f, 280.0f, 5.0f, 0.7f);
    SpawnParticleExplosion(game, center, ORANGE, 18, 60.0f, 200.0f, 4.0f, 0.5f);
    game->screenShake = 0.5f;
    PlaySound(g_assets.sfxGrenadeExplode);

    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        Enemy *e = &game->enemies[i];
        if (!e->active || e->state == DEATH || e->isTutorialEnemy) continue;
        if (Vector2DistanceSqr(center, e->position) > radius * radius) continue;

        // Desestabiliza o RNA: dissolve o capsídeo (escudo) antes de ferir a vida.
        if (e->shieldActive)
        {
            e->shieldActive = false;
            e->shieldHp = 0;
            SpawnParticleExplosion(game, e->position, (Color){ 150, 220, 255, 255 }, 12, 70.0f, 200.0f, 4.0f, 0.5f);
        }

        int applied = ApplyPlayerDamageToEnemy(game, e, dmg, false);
        if (applied <= 0) continue;
        SpawnDamageText(game, e->position, applied, (Color){ 120, 255, 160, 255 });
        if (e->hp <= 0)
        {
            e->hp = 0;
            e->state = DEATH;
            e->cooldownTimer = 0.5f;
            SpawnParticleExplosion(game, e->position, GOLD, 18, 50.0f, 150.0f, 4.0f, 0.7f);
            if (!game->inTutorial) RegisterEnemyKill(game, e);
        }
        else
        {
            e->state = HURT;
            e->cooldownTimer = 0.25f;
        }
    }
    // Também danifica os Núcleos de Infecção do escudo do chefe.
    HitInfectionCores(game, center, radius, dmg);
}

static void DetonateBioMine(GameState *game, BioMine *mine)
{
    if (!mine || !mine->active) return;

    Vector2 center = mine->position;
    int dmg = mine->damage;
    mine->active = false;

    const float radius = 210.0f;
    SpawnParticleExplosion(game, center, (Color){ 90, 255, 180, 255 }, 30, 90.0f, 320.0f, 5.0f, 0.75f);
    SpawnParticleExplosion(game, center, (Color){ 255, 90, 180, 255 }, 18, 70.0f, 230.0f, 4.0f, 0.55f);
    game->screenShake = 0.55f;
    PlaySound(g_assets.sfxGrenadeExplode);

    int collIndices[MAX_ENEMIES];
    int collCount = GetEnemiesInRadius(game, center, radius, collIndices);
    for (int k = 0; k < collCount; k++)
    {
        int eIdx = collIndices[k];
        Enemy *e = &game->enemies[eIdx];
        if (!e->active || e->state == DEATH || e->isTutorialEnemy) continue;

        if (e->shieldActive)
        {
            e->shieldHp -= dmg;
            if (e->shieldHp <= 0)
            {
                e->shieldHp = 0;
                e->shieldActive = false;
                SpawnParticleExplosion(game, e->position, (Color){ 150, 220, 255, 255 }, 14, 70.0f, 210.0f, 4.0f, 0.5f);
            }
        }

        int applied = ApplyPlayerDamageToEnemy(game, e, dmg, false);
        if (applied <= 0) continue;
        e->poisonTimer = 2.0f;
        SpawnDamageText(game, e->position, applied, (Color){ 90, 255, 180, 255 });
        if (e->hp <= 0)
        {
            e->hp = 0;
            e->state = DEATH;
            e->cooldownTimer = 0.5f;
            SpawnParticleExplosion(game, e->position, GOLD, 18, 50.0f, 150.0f, 4.0f, 0.7f);
            if (!game->inTutorial) RegisterEnemyKill(game, e);
        }
        else
        {
            e->state = HURT;
            e->cooldownTimer = 0.25f;
        }
    }

    HitInfectionCores(game, center, radius, dmg);
}

void PlantBioMine(GameState *game, Vector2 pos, int dmg)
{
    int slot = -1;
    for (int i = 0; i < MAX_BIOMINES; i++)
    {
        if (!game->bioMines[i].active) { slot = i; break; }
    }
    if (slot < 0)
    {
        float oldestPulse = game->bioMines[0].pulse;
        slot = 0;
        for (int i = 1; i < MAX_BIOMINES; i++)
        {
            if (game->bioMines[i].pulse > oldestPulse)
            {
                oldestPulse = game->bioMines[i].pulse;
                slot = i;
            }
        }
    }

    game->bioMines[slot].position = pos;
    game->bioMines[slot].active = true;
    game->bioMines[slot].armTimer = 0.18f;
    game->bioMines[slot].pulse = 0.0f;
    game->bioMines[slot].damage = dmg;
    SpawnParticleExplosion(game, pos, (Color){ 90, 255, 180, 255 }, 12, 35.0f, 110.0f, 3.0f, 0.35f);
}

bool TriggerBioMinesInRadius(GameState *game, Vector2 pos, float radius)
{
    bool triggered = false;
    float r2 = radius * radius;
    for (int i = 0; i < MAX_BIOMINES; i++)
    {
        BioMine *mine = &game->bioMines[i];
        if (!mine->active || mine->armTimer > 0.0f) continue;
        if (Vector2DistanceSqr(pos, mine->position) <= r2)
        {
            DetonateBioMine(game, mine);
            triggered = true;
        }
    }
    return triggered;
}

bool DetonateAllBioMines(GameState *game)
{
    bool triggered = false;
    for (int i = 0; i < MAX_BIOMINES; i++)
    {
        BioMine *mine = &game->bioMines[i];
        if (!mine->active) continue;
        DetonateBioMine(game, mine);
        triggered = true;
    }
    return triggered;
}

// ============================================================================
// APLICA O EFEITO DE UM POWER-UP / ITEM COLETADO (fonte única)
// Centraliza os efeitos para os dois pontos de coleta (gameplay e tutorial),
// evitando duplicação e tratando todos os tipos do enum PowerUpType.
// ============================================================================
void ApplyPowerUpEffect(GameState *game, PowerUpType type)
{
    switch (type)
    {
        case HP_RECOVERY:
            game->player.hp += 35;
            if (game->player.hp > game->player.maxHp) game->player.hp = game->player.maxHp;
            SpawnParticleExplosion(game, game->player.position, GREEN, 15, 40.0f, 100.0f, 4.0f, 0.8f);
            break;
        case SPEED_BOOST:
            game->player.speedTimer = 8.0f;
            break;
        case SHIELD:
            game->player.shieldTimer = 7.0f;
            break;
        case ATTACK_BOOST:
            game->player.attackBoostTimer = 8.0f;
            break;
        case POWERUP_MASK:
            // Máscara Hospitalar: reduz o dano recebido por um tempo (prevenção
            // de transmissão / proteção respiratória).
            game->player.maskTimer = 12.0f;
            ShowBanner(game, "MASCARA HOSPITALAR", "Dano recebido reduzido — protecao respiratoria!",
                       (Color){ 120, 220, 255, 255 }, 2.5f);
            break;
        case POWERUP_DISTANCING:
            // Distanciamento Social: aura que repele os patógenos por um tempo.
            game->player.distancingTimer = 10.0f;
            ShowBanner(game, "DISTANCIAMENTO SOCIAL", "Aura de protecao afasta os patogenos!",
                       (Color){ 120, 255, 200, 255 }, 2.5f);
            break;
        case POWERUP_RNA_GRENADE:
            // Desestabilizador de RNA: explosão imediata em área ao redor do herói.
            DetonateRNAGrenade(game, game->player.position);
            ShowBanner(game, "DESESTABILIZADOR DE RNA", "Explosao desfaz o RNA dos patogenos!",
                       (Color){ 120, 255, 160, 255 }, 2.2f);
            break;
        case POWERUP_CYTOKINE:
            // Citocina de Estabilização: cura imediata + regeneração ao longo do
            // tempo. (Citocinas reais sinalizam o sistema imune; em excesso causam
            // "tempestade de citocinas" — aqui é simplificação lúdica de cura.)
            game->player.hp += 20;
            if (game->player.hp > game->player.maxHp) game->player.hp = game->player.maxHp;
            game->player.regenTimer = 6.0f;
            game->player.regenAccum = 0.0f;
            SpawnParticleExplosion(game, game->player.position, (Color){ 80, 230, 140, 255 }, 16, 40.0f, 120.0f, 4.0f, 0.8f);
            ShowBanner(game, "CITOCINA DE ESTABILIZACAO", "Vida regenerando...",
                       (Color){ 80, 230, 140, 255 }, 2.2f);
            break;
        case POWERUP_TYPE_COUNT:
        default:
            break;
    }
}

// ============================================================================
// INICIALIZAÇÃO DO JOGO
// ============================================================================
void InitGame(GameState *game)
{
    // Preserva nome, audio, skins, dificuldade e configuracao do modo admin.
    char tempName[16] = "";
    float tempMusicVol = 1.0f, tempSfxVol = 1.0f;
    int tempSkin = 0, tempWSkin = 0;
    int tempCos[COS_SLOT_COUNT] = { 0 };   // cosméticos do guarda-roupa (preferência)
    int tempDiff = DIFFICULTY_MEDIUM;
    bool tAdmin = false, tAdminApply = false, tAdminUnlockWeapons = false, tAdminUnlockSkins = false;
    int tAdmMaxHp = 0, tAdmDmg = 0, tAdmLevel = 0, tAdmSus = 0;
    float tAdmSpeed = 0.0f;
    if (game != NULL)
    {
        snprintf(tempName, sizeof(tempName), "%s", game->player.name);
        tempMusicVol = game->musicVolume;
        tempSfxVol = game->sfxVolume;
        tempSkin = game->player.skinId;
        tempWSkin = game->player.weaponSkinId;
        for (int s = 0; s < COS_SLOT_COUNT; s++) tempCos[s] = game->player.cosmetics[s];
        tempDiff = game->difficulty;
        tAdmin = game->adminMode; tAdminApply = game->adminApply;
        tAdminUnlockWeapons = game->adminUnlockWeapons; tAdminUnlockSkins = game->adminUnlockSkins;
        tAdmMaxHp = game->adminMaxHp; tAdmDmg = game->adminDamage;
        tAdmLevel = game->adminLevel; tAdmSus = game->adminSus; tAdmSpeed = game->adminSpeed;
    }

    memset(game, 0, sizeof(GameState));

    // Restaura dificuldade e modo admin
    game->difficulty = tempDiff;
    ApplyDifficulty(game);
    game->adminMode = tAdmin; game->adminApply = tAdminApply;
    game->adminUnlockWeapons = tAdminUnlockWeapons; game->adminUnlockSkins = tAdminUnlockSkins;
    game->adminMaxHp = tAdmMaxHp; game->adminDamage = tAdmDmg;
    game->adminLevel = tAdmLevel; game->adminSus = tAdmSus; game->adminSpeed = tAdmSpeed;

    if (tempName[0] != '\0')
    {
        snprintf(game->player.name, sizeof(game->player.name), "%s", tempName);
    }
    else
    {
        strcpy(game->player.name, "HERO");
    }
    game->musicVolume = tempMusicVol;
    game->sfxVolume = tempSfxVol;
    game->player.skinId = tempSkin;
    game->player.weaponSkinId = tempWSkin;
    for (int s = 0; s < COS_SLOT_COUNT; s++) game->player.cosmetics[s] = tempCos[s];

    // Jogador inicial
    game->player.position = (Vector2){ MAP_WIDTH / 2.0f, MAP_HEIGHT / 2.0f };
    // Status base padrao
    game->player.speed = 280.0f;
    game->player.maxHp = 100;
    game->player.attackPower = 15;


    game->player.hp = game->player.maxHp;
    game->player.score = 0;
    game->player.level = 1;
    game->player.xp = 0;
    game->player.xpNeeded = 100;
    game->player.attackCooldown = 0.0f;
    game->player.squashX = 1.0f;
    game->player.squashY = 1.0f;
    game->player.trailIndex = 0;
    game->player.equippedWeapon = 1; // 1 = Lâmina Imunológica
    game->maxWeaponUnlocked = 1;     // só a Lâmina liberada no início (progressão)
    game->player.healthPotions = 3;
    game->player.poisonTimer = 0.0f;
    game->player.slowTimer = 0.0f;
    game->player.speedTimer = 0.0f;
    game->player.shieldTimer = 0.0f;
    game->player.attackBoostTimer = 0.0f;
    game->player.facingDir = 1;
    game->player.isMoving = false;
    for (int i = 0; i < 10; i++) game->player.trail[i] = game->player.position;

    // Sistema
    game->wave = 1;
    game->totalEnemiesKilled = 0;
    game->bioBladeAnnounced = false; // Lâmina Bioelétrica ainda bloqueada (0 abates)
    game->timeElapsed = 0.0f;
    game->screenShake = 0.0f;
    game->slashAnimTimer = 0.0f;
    game->slashAnimDir = (Vector2){ 1.0f, 0.0f };

    // Campanha começa sempre no Mundo das Bactérias
    game->currentWorld = WORLD_BACTERIA;
    game->worldCompleted = false;

    // Câmera
    game->camera.target = game->player.position;
    game->camera.offset = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
    game->camera.rotation = 0.0f;
    game->camera.zoom = 1.0f;

    // Limpa vetores
    for (int i = 0; i < MAX_ENEMIES; i++) game->enemies[i].active = false;
    for (int i = 0; i < MAX_POWERUPS; i++) game->powerUps[i].active = false;
    for (int i = 0; i < MAX_CORES; i++) game->cores[i].active = false;
    game->bossShieldActive = false;
    game->bossCoresSpawned = false;
    InitParticlePool(game);

    // MODO ADMIN: aplica os atributos configurados pelo jogador (se ativo).
    if (game->adminMode && game->adminApply)
    {
        if (game->adminMaxHp > 0)   { game->player.maxHp = game->adminMaxHp; game->player.hp = game->adminMaxHp; }
        if (game->adminDamage > 0)  game->player.attackPower = game->adminDamage;
        if (game->adminSpeed > 0)   game->player.speed = game->adminSpeed;
        if (game->adminLevel > 0)   game->player.level = game->adminLevel;
        if (game->adminSus > 0)     game->player.susPoints = game->adminSus;
        // Com nível alto via admin, libera as armas correspondentes.
        game->maxWeaponUnlocked = 1;
        for (int w = 2; w <= 4; w++)
            if (game->player.level >= WeaponUnlockLevel(w)) game->maxWeaponUnlocked = w;
        if (game->adminUnlockWeapons)
        {
            game->maxWeaponUnlocked = 4;
            if (game->totalEnemiesKilled < BIOBLADE_UNLOCK_KILLS)
                game->totalEnemiesKilled = BIOBLADE_UNLOCK_KILLS;
            game->bioBladeAnnounced = true;
        }
    }

    // Inicia o Tutorial (Seringa de Vacina) em vez de ir diretamente à onda 1
    InitTutorial(game);
}



// ============================================================================
// TUTORIAL: INICIALIZAÇÃO DA SERINGA DE VACINA
// ============================================================================
void InitTutorial(GameState *game)
{
    // Reseta flags do tutorial
    game->inTutorial           = true;
    game->tutorialStep         = 0;
    game->tutorialTimer        = 0.0f;
    game->tutorialEnemySpawned = false;

    // Garante estado de combate limpo ao entrar na seringa
    game->player.hp = game->player.maxHp;
    game->player.poisonTimer = 0.0f;
    game->player.slowTimer = 0.0f;
    game->player.damageCooldown = 0.0f;
    game->player.attackCooldown = 0.0f;
    game->poisonTickAccum = 0.0f;
    if (game->player.equippedWeapon < 1 || game->player.equippedWeapon > 4)
        game->player.equippedWeapon = 1;

    // Inicia o diálogo do passo 0 (página 0)
    game->tutorialDialog.active    = true;
    game->tutorialDialog.page      = 0;
    game->tutorialDialog.charShown = 0;
    game->tutorialDialog.charTimer = 0.0f;

    // Reposiciona o jogador no canto direito da Seringa (para andar para a esquerda)
    game->player.position = (Vector2){ SYRINGE_WIDTH - 200.0f, SYRINGE_HEIGHT / 2.0f };

    // Ajusta a câmera para o mapa menor da seringa
    game->camera.target = game->player.position;
    game->camera.offset = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
    game->camera.zoom   = 1.0f;

    // Limpa power-ups anteriores
    for (int i = 0; i < MAX_POWERUPS; i++) game->powerUps[i].active = false;

    // Define a tela como Tutorial
    game->currentScreen = SCREEN_TUTORIAL;
}

// ============================================================================
// NÚmero de páginas de diálogo por passo do tutorial
// ============================================================================
static const int DIALOG_PAGES_PER_STEP[3] = { 2, 2, 2 };

// ============================================================================
// TUTORIAL: LÓGICA DE ATUALIZAÇÃO (3 PASSOS + SISTEMA DE DIÁLOGO)
// ============================================================================
void UpdateTutorial(GameState *game, float delta)
{
    UpdateGrid(game);
    DialogState *dlg = &game->tutorialDialog;

    // ========================================================================
    // SISTEMA DE DIÁLOGO: Typewriter + Q/ESPAÇO para avançar página
    // ========================================================================
    if (dlg->active)
    {
        // Debounce de ataque: enquanto o diálogo está aberto, "trava" o ataque.
        // Só será liberado quando o jogador SOLTAR a tecla/botão (ver passo 1).
        // Assim o mesmo SPACE/clique que avança/fecha o texto não ataca a bactéria.
        game->attackInputLatched = true;

        // BUGFIX: durante um diálogo bloqueante o jogador deve PARAR imediatamente.
        // Antes, se ele estava andando ao abrir o diálogo, a flag isMoving ficava
        // true e o modelo continuava com a animação de caminhada parado no lugar.
        game->player.isMoving = false;
        game->player.squashX = Lerp(game->player.squashX, 1.0f, 12.0f * delta);
        game->player.squashY = Lerp(game->player.squashY, 1.0f, 12.0f * delta);

        // Obtém o texto do diálogo para o passo e página atuais para calcular o comprimento total
        const char *l1, *l2, *l3;
        GetTutorialDialogText(game->tutorialStep, dlg->page, &l1, &l2, &l3);
        int totalLen = strlen(l1) + strlen(l2) + strlen(l3);

        // --- Typewriter: revela um caractere a cada ~0.03s ---
        float typewriterSpeed = ScientistVoiceCharDelay(totalLen, 0.030f); // segundos por caractere
        dlg->charTimer += delta;
        if (dlg->charTimer >= typewriterSpeed)
        {
            dlg->charTimer -= typewriterSpeed;
            if (dlg->charShown < totalLen)
            {
                dlg->charShown++;
            }
        }
        SyncScientistVoice(SCIENTIST_VOICE_TUTORIAL,
                           game->tutorialStep * 10 + dlg->page,
                           dlg->charShown, totalLen, game->sfxVolume);

        // --- Q ou ESPAÇO avançam o diálogo ---
        if (IsKeyPressed(KEY_Q) || IsKeyPressed(KEY_SPACE))
        {
            if (dlg->charShown < totalLen)
            {
                // Se o texto ainda não terminou de aparecer: pula pro fim
                dlg->charShown = totalLen;
                StopScientistVoice();
            }
            else
            {
                StopScientistVoice();
                int maxPages = DIALOG_PAGES_PER_STEP[game->tutorialStep];

                // Verifica se ainda faltam páginas no passo atual
                if (dlg->page < maxPages - 1)
                {
                    // Próxima página
                    dlg->page++;
                    dlg->charShown = 0;
                    dlg->charTimer = 0.0f;
                }
                else
                {
                    // Última página do passo: fecha o diálogo e libera a gameplay.
                    dlg->active    = false;
                    dlg->page      = 0;
                    dlg->charShown = 0;
                    dlg->charTimer = 0.0f;
                    // Limpa estado de ataque ao trocar de etapa: nada de ataque
                    // pendente, slash residual ou cooldown herdado. O ataque segue
                    // travado (attackInputLatched) até o jogador soltar o input.
                    game->player.attackCooldown = 0.0f;
                    game->slashAnimTimer        = 0.0f;
                }
            }
        }
        // Diálogo ativo = só anima câmera, não move jogador nem processa passo
        game->camera.target.x += (game->player.position.x - game->camera.target.x) * 0.10f;
        game->camera.target.y += (game->player.position.y - game->camera.target.y) * 0.10f;
        return; // bloqueia resto do update durante o diálogo
    }

    // ========================================================================
    // MOVIMENTAÇÃO DO JOGADOR (só quando diálogo está fechado)
    // ========================================================================
    float currentSpeed = game->player.speed;
    if (game->player.speedTimer > 0.0f) currentSpeed *= 1.6f;

    Vector2 moveDir = { 0.0f, 0.0f };
    // Movimento completo dentro da seringa (as paredes limitam o eixo Y)
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { moveDir.x += 1.0f; game->player.facingDir = 1; }
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) { moveDir.x -= 1.0f; game->player.facingDir = -1; }
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) moveDir.y += 1.0f;
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) moveDir.y -= 1.0f;

    bool moving = (moveDir.x != 0.0f || moveDir.y != 0.0f);
    game->player.isMoving = moving;

    if (moving)
    {
        moveDir = Vector2Normalize(moveDir);
        game->player.position = Vector2Add(
            game->player.position,
            Vector2Scale(moveDir, currentSpeed * delta)
        );
        // Partículas de rastro suave
        if (GetRandomValue(0, 8) == 0)
        {
            Vector2 smokeVel = { -moveDir.x * 25.0f, -moveDir.y * 25.0f };
            SpawnParticle(game, game->player.position, smokeVel, Fade((Color){100,220,150,255}, 0.4f), 4.0f, 0.5f);
        }
    }


    // Colisão com paredes do mapa (via função do mapa da seringa)
    MapSeringa_ApplyWallCollision(&game->player.position, 20.0f);

    // --- Atualiza partículas (devolvendo ao pool corretamente) ---
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            game->particles[i].velocity.y += 150.0f * delta;
            game->particles[i].velocity.x *= 0.96f; // Drag

            game->particles[i].position = Vector2Add(
                game->particles[i].position,
                Vector2Scale(game->particles[i].velocity, delta)
            );
            game->particles[i].lifeTime -= delta;
            if (game->particles[i].lifeTime <= 0.0f)
                FreeParticle(game, i); // BUGFIX: antes vazava o slot do pool
        }
    }

    // Decaimento de cooldowns e buffs do jogador no tutorial
    if (game->player.attackCooldown > 0.0f) game->player.attackCooldown -= delta;
    if (game->player.damageCooldown > 0.0f) game->player.damageCooldown -= delta;
    if (game->player.speedTimer > 0.0f) game->player.speedTimer -= delta;
    if (game->player.shieldTimer > 0.0f) game->player.shieldTimer -= delta;
    if (game->player.attackBoostTimer > 0.0f) game->player.attackBoostTimer -= delta;
    // BUGFIX: poison/slow não eram decrementados no tutorial e vazavam para a gameplay
    if (game->player.slowTimer > 0.0f) game->player.slowTimer -= delta;
    for (int i = 0; i < MAX_BIOMINES; i++)
    {
        if (!game->bioMines[i].active) continue;
        game->bioMines[i].pulse += delta;
        if (game->bioMines[i].armTimer > 0.0f) game->bioMines[i].armTimer -= delta;
    }
    if (game->player.poisonTimer > 0.0f)
    {
        game->player.poisonTimer -= delta;
        // Dano de veneno suave também no tutorial (acumulador fracionário)
        game->poisonTickAccum += 6.0f * delta;
        if (game->poisonTickAccum >= 1.0f)
        {
            int dmg = (int)game->poisonTickAccum;
            game->poisonTickAccum -= (float)dmg;
            game->player.hp -= dmg;
            if (game->player.hp < 1) game->player.hp = 1; // tutorial nunca mata por veneno
        }
        if (GetRandomValue(0, 30) == 0)
            SpawnParticle(game, game->player.position, (Vector2){0, -20}, PURPLE, 4.0f, 0.5f);
    }
    if (game->slashAnimTimer > 0.0f) game->slashAnimTimer -= delta;
    if (game->hurtFlashTimer > 0.0f) game->hurtFlashTimer -= delta;
    UpdateDamageTexts(game, delta);

    // ========================================================================
    // PASSO 0: Andar para a esquerda (Treino de Mobilidade)
    // ========================================================================
    if (game->tutorialStep == 0)
    {
        // Avança quando passar de X=1000
        bool itemsRemaining = (game->player.position.x > SYRINGE_WIDTH - 600.0f);

        if (!itemsRemaining)
        {
            game->tutorialStep = 1;
            game->tutorialTimer = 0.0f;
            // Abre diálogo do passo 1
            dlg->active    = true;
            dlg->page      = 0;
            dlg->charShown = 0;
            dlg->charTimer = 0.0f;
        }
    }

    // ========================================================================
    // PASSO 1: Combate — spawn de bactéria móvel e atiradora
    // ========================================================================
    else if (game->tutorialStep == 1)
    {
        // Spawna a bactéria uma única vez no passo 1
        if (!game->tutorialEnemySpawned)
        {
            for (int i = 0; i < MAX_ENEMIES; i++)
            {
                if (!game->enemies[i].active)
                {
                    game->enemies[i].active          = true;
                    game->enemies[i].position        = (Vector2){ SYRINGE_WIDTH / 2.0f, SYRINGE_HEIGHT / 2.0f };
                    game->enemies[i].hp              = 35;
                    game->enemies[i].maxHp           = 35;
                    game->enemies[i].speed           = 85.0f;
                    game->enemies[i].type            = 0;
                    game->enemies[i].tier            = TIER_1;
                    game->enemies[i].state           = IDLE;
                    game->enemies[i].isRanged        = false; // controlada de forma customizada abaixo
                    game->enemies[i].isTutorialEnemy = true;
                    game->enemies[i].cooldownTimer   = 1.5f; // primeiro tiro após 1.5s
                    game->enemies[i].patrolTarget    = game->enemies[i].position;
                    game->enemies[i].patrolTimer     = 99.0f;
                    game->enemies[i].velSmooth       = (Vector2){ 0.0f, 0.0f };
                    game->enemies[i].animTime        = 0.0f;
                    game->enemies[i].attackAnim      = 0.0f;
                    game->enemies[i].spawnAnim       = 0.0f;
                    break;
                }
            }
            game->tutorialEnemySpawned = true;
            // Partículas de invocação
            for (int p = 0; p < 15; p++)
            {
                Vector2 vel = { (float)GetRandomValue(-80, 80), (float)GetRandomValue(-80, 80) };
                SpawnParticle(game, (Vector2){ SYRINGE_WIDTH/2.0f, SYRINGE_HEIGHT/2.0f }, vel, (Color){50,200,80,255}, 5.0f, 0.8f);
            }
        }

        // IA da bactéria tutorial e seu status de morte
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (game->enemies[i].active && game->enemies[i].isTutorialEnemy)
            {
                Enemy *e = &game->enemies[i];
                if (e->state == DEATH)
                {
                    e->cooldownTimer -= delta;
                    if (e->cooldownTimer <= 0.0f)
                    {
                        e->active = false;
                        // Spawna a célula de cura ao morrer!
                        SpawnPowerUpAt(game, e->position, HP_RECOVERY);
                    }
                    continue;
                }

                if (e->state == HURT)
                {
                    e->cooldownTimer -= delta;
                    if (e->cooldownTimer <= 0.0f) e->state = IDLE;
                }
                else if (e->state == ATTACK)
                {
                    e->chargeTimer -= delta;
                    if (e->chargeTimer <= 0.0f)
                    {
                        // Dispara uma toxina lenta de treino em direção ao jogador
                        Vector2 targetDir = Vector2Subtract(game->player.position, e->position);
                        targetDir = Vector2Normalize(targetDir);
                        Vector2 projVel = Vector2Scale(targetDir, 160.0f); // Tiro lento de 160 px/s

                        // Procura slot de projétil
                        for (int j = 0; j < MAX_PROJECTILES; j++)
                        {
                            if (!game->projectiles[j].active)
                            {
                                game->projectiles[j].active = true;
                                game->projectiles[j].position = e->position;
                                game->projectiles[j].velocity = projVel;
                                game->projectiles[j].type = PROJ_ACID_ARC;
                                game->projectiles[j].damage = 6;
                                game->projectiles[j].isPlayerProjectile = false;
                                game->projectiles[j].lifeTime = 8.0f;
                                game->projectiles[j].hitbox = (Rectangle){ e->position.x - 10, e->position.y - 10, 20, 20 };
                                break;
                            }
                        }
                        e->state = IDLE;
                    }
                }
                else
                {
                    // Persegue o jogador lentamente
                    Vector2 chaseDir = Vector2Subtract(game->player.position, e->position);
                    float dist = Vector2Length(chaseDir);
                    if (dist > 15.0f)
                    {
                        chaseDir = Vector2Normalize(chaseDir);
                        e->position = Vector2Add(e->position, Vector2Scale(chaseDir, e->speed * delta));
                        e->state = AGGRO;
                    }
                    else
                    {
                        e->state = IDLE;
                    }

                    // Ataca atirando a cada 3.2s se perto
                    e->cooldownTimer -= delta;
                    if (e->cooldownTimer <= 0.0f && dist < 400.0f)
                    {
                        e->state = ATTACK;
                        e->chargeTimer = 0.5f;     // Tempo de carregamento do tiro
                        e->cooldownTimer = 3.2f;   // Cooldown entre tiros
                    }
                }
            }
        }

        // Ataque com ESPAÇO ou clique segurado (Q está reservado para diálogo).
        // DEBOUNCE: o mesmo SPACE/clique que fechou a última página de diálogo NÃO
        // pode atacar. O latch (setado enquanto o diálogo estava ativo) só é
        // liberado quando o jogador SOLTA a tecla/botão; a bactéria recém-spawnada
        // só recebe dano após um comando NOVO do jogador.
        bool attackHeld = IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        if (game->attackInputLatched && !attackHeld) game->attackInputLatched = false;
        if (!game->attackInputLatched && attackHeld)
        {
            extern Vector2 g_virtualMouse;
            Vector2 worldMouse = GetScreenToWorld2D(g_virtualMouse, game->camera);
            PlayerAttack(game, worldMouse);
        }
        // Verifica se a bactéria tutorial foi eliminada
        bool algumVivo = false;
        for (int i = 0; i < MAX_ENEMIES; i++)
            if (game->enemies[i].active && game->enemies[i].isTutorialEnemy)
            { algumVivo = true; break; }

        if (!algumVivo && game->tutorialEnemySpawned)
        {
            game->tutorialStep  = 2;
            game->tutorialTimer = 0.0f;
            // Abre diálogo do passo 2
            dlg->active    = true;
            dlg->page      = 0;
            dlg->charShown = 0;
            dlg->charTimer = 0.0f;
        }
    }
    
    // ========================================================================
    // PASSO 2: Saída pela agulha (bocal da seringa)
    // ========================================================================
    else if (game->tutorialStep == 2)
    {
        Rectangle playerRect = {
            game->player.position.x - 18.0f,
            game->player.position.y - 18.0f,
            36.0f, 36.0f
        };

        if (!game->injectionCutscene && MapSeringa_CheckExit(playerRect))
        {
            game->injectionCutscene = true;
            game->injectionTimer = 0.0f;
            dlg->active = false;
        }

        if (game->injectionCutscene)
        {
            game->injectionTimer += delta;

            // Tremor de tela crescente durante a injeção
            game->screenShake = 0.5f + (game->injectionTimer / 1.5f) * 1.5f;

            // Empurra o jogador para a esquerda
            game->player.position.x -= 800.0f * delta;

            if (game->injectionTimer > 1.5f)
            {
                game->injectionCutscene = false;
                game->screenShake = 0.0f;

                // Limpa inimigos e partículas da seringa
                for (int i = 0; i < MAX_POWERUPS; i++) game->powerUps[i].active = false;
                InitParticlePool(game);

                // Tela de carregamento para o organismo (com efeito de compressão)
                // OBS: inTutorial ainda é true aqui — é o que ativa o syringeTransitionFX
                RequestLoadingScreen(game, LOAD_TO_GAMEPLAY, 3.0f);
            }
        }
    }

    // ========================================================================
    // ATUALIZA COLISÕES E PROJÉTEIS DENTRO DO TUTORIAL
    // ========================================================================
    // 1. Colisão Jogador <-> Bactéria
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (game->enemies[i].active && game->enemies[i].isTutorialEnemy && game->enemies[i].state != DEATH)
        {
            float distSqr = Vector2DistanceSqr(game->player.position, game->enemies[i].position);
            if (distSqr < 35.0f * 35.0f) // colidiu
            {
                HandlePlayerEnemyCollision(game, &game->enemies[i]);
            }
        }
    }

    // 2. Colisão Projétil <-> Jogador
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (game->projectiles[i].active)
        {
            game->projectiles[i].position = Vector2Add(
                game->projectiles[i].position,
                Vector2Scale(game->projectiles[i].velocity, delta)
            );
            game->projectiles[i].hitbox.x = game->projectiles[i].position.x - 10;
            game->projectiles[i].hitbox.y = game->projectiles[i].position.y - 10;

            Rectangle pRect = { game->player.position.x - 20, game->player.position.y - 20, 40, 40 };
            if (CheckCollisionRecs(game->projectiles[i].hitbox, pRect))
            {
                HandleProjectileCollision(game, &game->projectiles[i]);
            }

            if (game->projectiles[i].position.x < -100 || game->projectiles[i].position.x > SYRINGE_WIDTH + 100 ||
                game->projectiles[i].position.y < -100 || game->projectiles[i].position.y > SYRINGE_HEIGHT + 100)
            {
                game->projectiles[i].active = false;
            }
        }
    }

    // 3. Coleta de Power-ups (ampolas de vacina / esferas de treino e cura)
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (game->powerUps[i].active)
        {
            game->powerUps[i].pulseTimer += delta;
            float distSqr = Vector2DistanceSqr(game->player.position, game->powerUps[i].position);
            if (distSqr < 35.0f * 35.0f) // coletou
            {
                game->powerUps[i].active = false;
                game->player.score += 50;
                PlaySound(g_assets.sfxPickup);

                SpawnParticleExplosion(game, game->powerUps[i].position, YELLOW, 15, 60.0f, 160.0f, 4.5f, 0.6f);

                ApplyPowerUpEffect(game, game->powerUps[i].type);
            }
        }
    }

    // Atualiza câmera suavemente
    game->camera.target.x += (game->player.position.x - game->camera.target.x) * 0.10f;
    game->camera.target.y += (game->player.position.y - game->camera.target.y) * 0.10f;

    // Screen shake também no tutorial (golpes e cutscene de injeção)
    if (game->screenShake > 0.0f)
    {
        if (!game->injectionCutscene) // a cutscene mantém o tremor constante
        {
            game->screenShake -= delta * 1.5f;
            if (game->screenShake < 0.0f) game->screenShake = 0.0f;
        }
        float sh = (game->screenShake > 2.0f) ? 2.0f : game->screenShake; // limite p/ não enjoar
        game->camera.offset.x = (SCREEN_WIDTH / 2.0f) + (float)GetRandomValue(-15, 15) * sh;
        game->camera.offset.y = (SCREEN_HEIGHT / 2.0f) + (float)GetRandomValue(-15, 15) * sh;
    }
    else
    {
        game->camera.offset = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
    }

    // Squash & Stretch recovery (essencial para resetar após tomar dano)
    game->player.squashX = Lerp(game->player.squashX, 1.0f, 10.0f * delta);
    game->player.squashY = Lerp(game->player.squashY, 1.0f, 10.0f * delta);
}



// ============================================================================
// CICLO PRINCIPAL DE ATUALIZAÇÃO DA LOGICA
// ============================================================================
void UpdateGameplay(GameState *game, float delta)
{
    UpdateGrid(game);
    game->timeElapsed += delta;
    // Mantém os nomes/descrições das armas temáticas em sincronia com o Mundo.
    SetWeaponWorld(game->currentWorld);

    // ------------------------------------------------------------------------
    // MODO ADMIN: atalhos de teste (só funcionam com o modo admin ATIVO)
    // ------------------------------------------------------------------------
    if (game->adminMode)
    {
        // "." e "]" : mata todos os inimigos e conclui a fase (fluxo normal segue)
        if (IsKeyPressed(KEY_PERIOD) || IsKeyPressed(KEY_RIGHT_BRACKET))
        {
            for (int i = 0; i < MAX_ENEMIES; i++) game->enemies[i].active = false;
            for (int i = 0; i < MAX_CORES; i++) game->cores[i].active = false;
            game->bossShieldActive = false;
            game->bossCoresSpawned = false;
            game->enemiesRemaining = 0;
            ShowBanner(game, "[ADMIN] FASE LIMPA", "Avancando para o proximo passo...",
                       (Color){ 255, 210, 60, 255 }, 1.2f);
        }
        if (IsKeyPressed(KEY_LEFT_BRACKET) && game->wave > 1) game->wave--; // reduz wave
        if (IsKeyPressed(KEY_F1)) game->debugCollision = !game->debugCollision; // overlay de colisão (debug)
        if (IsKeyPressed(KEY_H)) game->player.hp = game->player.maxHp;      // cura total
        if (IsKeyPressed(KEY_L)) game->player.xp = game->player.xpNeeded;   // sobe de nível
        if (IsKeyPressed(KEY_P)) game->player.susPoints += 50;              // +SUS
        if (IsKeyPressed(KEY_K)) // mata só comuns, mantém chefe/mini chefe
        {
            for (int i = 0; i < MAX_ENEMIES; i++)
                if (game->enemies[i].active && game->enemies[i].tier != TIER_3_BOSS &&
                    game->enemies[i].tier != TIER_MINIBOSS)
                {
                    game->enemies[i].active = false;
                    if (game->enemiesRemaining > 0) game->enemiesRemaining--;
                }
        }
    }

    // Decaimento do screen shake
    if (game->screenShake > 0.0f)
    {
        game->screenShake -= delta * 1.5f;
        if (game->screenShake < 0.0f) game->screenShake = 0.0f;
    }

    // Decaimento da animação de slash
    if (game->slashAnimTimer > 0.0f)
    {
        game->slashAnimTimer -= delta;
    }

    // Decaimento do flash de dano
    if (game->hurtFlashTimer > 0.0f) game->hurtFlashTimer -= delta;

    // ------------------------------------------------------------------------
    // 1. ATUALIZA TIMERS E STATS DO JOGADOR
    // ------------------------------------------------------------------------
    if (game->player.speedTimer > 0.0f) game->player.speedTimer -= delta;
    if (game->player.shieldTimer > 0.0f) game->player.shieldTimer -= delta;
    if (game->player.attackBoostTimer > 0.0f) game->player.attackBoostTimer -= delta;
    if (game->player.attackCooldown > 0.0f) game->player.attackCooldown -= delta;
    if (game->player.damageCooldown > 0.0f) game->player.damageCooldown -= delta;
    if (game->player.poisonTimer > 0.0f) {
        game->player.poisonTimer -= delta;
        // BUGFIX: HP é int — "hp -= 8*delta" truncava e drenava ~60 HP/s.
        // Agora acumula a fração e aplica 8 de dano por segundo de fato.
        game->poisonTickAccum += 8.0f * delta;
        if (game->poisonTickAccum >= 1.0f) {
            int dmg = (int)game->poisonTickAccum;
            game->poisonTickAccum -= (float)dmg;
            game->player.hp -= dmg;
        }
        if (game->player.hp <= 0) {
            game->player.hp = 0;
            game->currentScreen = SCREEN_GAMEOVER;
            return;
        }
        if (GetRandomValue(0, 30) == 0) {
            SpawnParticle(game, game->player.position, (Vector2){0, -20}, PURPLE, 4.0f, 0.5f);
        }
    }
    if (game->player.slowTimer > 0.0f) game->player.slowTimer -= delta;

    // ---- Buffs da expansão (itens) ----
    if (game->player.maskTimer > 0.0f) game->player.maskTimer -= delta;
    if (game->player.distancingTimer > 0.0f) game->player.distancingTimer -= delta;
    if (game->player.regenTimer > 0.0f)
    {
        game->player.regenTimer -= delta;
        // Citocina: regenera ~12 HP/s (acumula fração porque hp é int).
        game->player.regenAccum += 12.0f * delta;
        if (game->player.regenAccum >= 1.0f)
        {
            int heal = (int)game->player.regenAccum;
            game->player.regenAccum -= (float)heal;
            game->player.hp += heal;
            if (game->player.hp > game->player.maxHp) game->player.hp = game->player.maxHp;
        }
        if (GetRandomValue(0, 20) == 0)
            SpawnParticle(game, game->player.position, (Vector2){ 0, -25 }, (Color){ 80, 230, 140, 255 }, 4.0f, 0.5f);
    }

    UpdateDamageTexts(game, delta);
    UpdateBanner(game, delta);

    // ------------------------------------------------------------------------
    // 2. MOVIMENTAÇÃO DO JOGADOR
    // ------------------------------------------------------------------------
    float currentSpeed = game->player.speed;
    if (game->player.speedTimer > 0.0f) currentSpeed *= 1.6f; // Buff de velocidade
    if (game->player.slowTimer > 0.0f) currentSpeed *= 0.5f;  // Debuff de velocidade

    Vector2 moveDir = { 0.0f, 0.0f };
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { moveDir.x += 1.0f; game->player.facingDir = 1; }
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  { moveDir.x -= 1.0f; game->player.facingDir = -1; }
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  moveDir.y += 1.0f;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    moveDir.y -= 1.0f;

    if (moveDir.x != 0.0f || moveDir.y != 0.0f)
    {
        game->player.isMoving = true;
        moveDir = Vector2Normalize(moveDir);
        game->player.position = Vector2Add(
            game->player.position,
            Vector2Scale(moveDir, currentSpeed * delta)
        );

        // Emite fumaça suave de movimento
        if (GetRandomValue(0, 10) == 0)
        {
            Vector2 smokeVel = { -moveDir.x * 30.0f, -moveDir.y * 30.0f };
            Color pCol = (game->player.speedTimer > 0.0f) ? YELLOW : LIGHTGRAY;
            SpawnParticle(game, game->player.position, smokeVel, Fade(pCol, 0.4f), 5.0f, 0.5f);
        }
    }
    else
    {
        game->player.isMoving = false;
    }

    // Atualiza Rastro (Trail)
    static float trailTimer = 0.0f;
    trailTimer += delta;
    if (trailTimer > 0.02f) {
        game->player.trail[game->player.trailIndex] = game->player.position;
        game->player.trailIndex = (game->player.trailIndex + 1) % 10;
        trailTimer = 0.0f;
    }

    // Squash & Stretch Lerp
    game->player.squashX = Lerp(game->player.squashX, 1.0f, 10.0f * delta);
    game->player.squashY = Lerp(game->player.squashY, 1.0f, 10.0f * delta);

    // O herói fica confinado DENTRO do corpo (a silhueta é a arena).
    MapBody_ApplyCollision(&game->player.position, 20.0f);

    // Velocidade suavizada do herói (para antecipação LIMITADA de mira da IA).
    if (delta > 0.0f)
    {
        Vector2 inst = Vector2Scale(Vector2Subtract(game->player.position, game->playerPrevPos), 1.0f / delta);
        game->playerVelSmooth = Vector2Lerp(game->playerVelSmooth, inst, 0.25f);
    }
    game->playerPrevPos = game->player.position;

    // ------------------------------------------------------------------------
    // 3. ENTRADA DE COMBATE E ARMAS
    // ------------------------------------------------------------------------
    // Troca de arma com feedback claro (mostra nome/efeito ou aviso de bloqueio).
    int wpnRequest = 0;
    if (IsKeyPressed(KEY_ONE))   wpnRequest = 1;
    if (IsKeyPressed(KEY_TWO))   wpnRequest = 2;
    if (IsKeyPressed(KEY_THREE)) wpnRequest = 3;
    if (IsKeyPressed(KEY_FOUR))  wpnRequest = 4;
    if (wpnRequest != 0)
    {
        if (wpnRequest == 1 && WeaponUnlocked(game, WEAPON_BIOBLADE) &&
            (game->player.equippedWeapon == 1 || game->player.equippedWeapon == WEAPON_BIOBLADE))
        {
            wpnRequest = (game->player.equippedWeapon == WEAPON_BIOBLADE) ? 1 : WEAPON_BIOBLADE;
        }
        WeaponInfo wi = GetWeaponInfo(wpnRequest);
        if (WeaponUnlocked(game, wpnRequest))
        {
            if (wpnRequest != game->player.equippedWeapon)
            {
                game->player.equippedWeapon = wpnRequest;
                PlaySound(g_assets.sfxPickup);
                ShowBanner(game, wi.name, wi.special, wi.color, 2.2f);
                SpawnParticleExplosion(game, game->player.position, wi.color, 10, 40.0f, 120.0f, 3.0f, 0.4f);
            }
        }
        else
        {
            // Arma bloqueada: requisito por ABATES (Lâmina Bioelétrica) ou por nível.
            const char *req = (wpnRequest == WEAPON_BIOBLADE)
                ? TextFormat("Desbloqueia com %d abates (%d/%d)", BIOBLADE_UNLOCK_KILLS,
                             game->totalEnemiesKilled, BIOBLADE_UNLOCK_KILLS)
                : TextFormat("Desbloqueia no Nivel %d", wi.unlockLevel);
            ShowBanner(game, TextFormat("%s BLOQUEADA", wi.name), req, (Color){ 200, 80, 80, 255 }, 2.0f);
        }
    }

    if (IsKeyPressed(KEY_E) && game->player.healthPotions > 0 && game->player.hp < game->player.maxHp) {
        game->player.healthPotions--;
        game->player.hp += game->player.maxHp / 2; // Cura 50%
        if (game->player.hp > game->player.maxHp) game->player.hp = game->player.maxHp;
        PlaySound(g_assets.sfxPickup);
        SpawnParticleExplosion(game, game->player.position, GREEN, 20, 50.0f, 150.0f, 4.0f, 0.8f);
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON))
    {
        if (DetonateAllBioMines(game))
            ShowBanner(game, "MINAS DE RNA DETONADAS", "Explosao em area ativada pelo botao direito", (Color){ 90, 255, 180, 255 }, 1.6f);
    }

    // Segurar (Down) em vez de pressionar (Pressed): permite fogo contínuo nas
    // armas automáticas (fuzil/lâmina); o attackCooldown de cada arma controla a
    // cadência, então segurar não dispara mais rápido que o permitido.
    if (IsKeyDown(KEY_SPACE) || IsMouseButtonDown(MOUSE_LEFT_BUTTON))
    {
        extern Vector2 g_virtualMouse;
        Vector2 worldMouse = GetScreenToWorld2D(g_virtualMouse, game->camera);
        PlayerAttack(game, worldMouse);
    }

    // Entrada de combate e fluxo de save serao tratados no rpg.c para capturar a screenshot

    // ------------------------------------------------------------------------
    // 4. ATUALIZA PARTÍCULAS
    // ------------------------------------------------------------------------
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            // Partículas com gravidade direcional e arraste (viscosidade)
            game->particles[i].velocity.y += 150.0f * delta;
            game->particles[i].velocity.x *= 0.96f; // Drag
            
            game->particles[i].position = Vector2Add(
                game->particles[i].position,
                Vector2Scale(game->particles[i].velocity, delta)
            );
            game->particles[i].lifeTime -= delta;
            if (game->particles[i].lifeTime <= 0.0f) FreeParticle(game, i);
        }
    }

    // ------------------------------------------------------------------------
    // 5. ATUALIZA POWER-UPS (COLETA)
    // ------------------------------------------------------------------------
    for (int i = 0; i < MAX_POWERUPS; i++)
    {
        if (game->powerUps[i].active)
        {
            game->powerUps[i].pulseTimer += delta;

            // Distância jogador -> item
            float distSqr = Vector2DistanceSqr(game->player.position, game->powerUps[i].position);
            if (distSqr < 35.0f * 35.0f) // Colidiu/Coletou
            {
                game->powerUps[i].active = false;
                game->player.score += 50;
                PlaySound(g_assets.sfxPickup);

                // Efeito radial de coleta
                SpawnParticleExplosion(game, game->powerUps[i].position, YELLOW, 15, 60.0f, 160.0f, 4.5f, 0.6f);

                // Aplica efeitos baseados no tipo do Power-up / item
                ApplyPowerUpEffect(game, game->powerUps[i].type);
            }
        }
    }

    // ------------------------------------------------------------------------
    // 6. ATUALIZA INIMIGOS E INTELIGÊNCIA ARTIFICIAL (IA)
    // ------------------------------------------------------------------------
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (!game->enemies[i].active) continue;

        Enemy *enemy = &game->enemies[i];

        // Comportamento do arquétipo (centralizado): a IA ramifica por `beh` em
        // vez de números mágicos de tipo espalhados (enxame/elite têm IA própria).
        const EnemyArchetype *arch = EnemyArchetypeFor(enemy->type);
        EnemyBehavior beh = arch ? arch->behavior : BEHAV_MELEE;

        // Relógio de animação por inimigo + "pop-in" de surgimento (tempo/estado,
        // sem aleatoriedade por frame). Posição inicial p/ medir o deslocamento.
        enemy->animTime += delta;
        if (enemy->spawnAnim < 1.0f) { enemy->spawnAnim += delta * 3.5f; if (enemy->spawnAnim > 1.0f) enemy->spawnAnim = 1.0f; }
        Vector2 animStartPos = enemy->position;

        if (enemy->state == DEATH)
        {
            enemy->cooldownTimer -= delta;
            // Recolhe a velocidade suavizada na morte (anima parando).
            enemy->velSmooth = Vector2Scale(enemy->velSmooth, 1.0f - 6.0f * delta);
            if (enemy->cooldownTimer <= 0.0f)
            {
                enemy->active = false;
            }
            continue;
        }

        // Decai o brilho do escudo de capsídeo (feedback ao tomar dano no escudo)
        if (enemy->shieldHitFlash > 0.0f) enemy->shieldHitFlash -= delta;

        // DISTANCIAMENTO SOCIAL (item): aura ao redor do herói empurra os
        // patógenos que entram no raio, impedindo a aproximação por um tempo.
        if (game->player.distancingTimer > 0.0f)
        {
            const float auraR = 175.0f;
            Vector2 away = Vector2Subtract(enemy->position, game->player.position);
            float d = Vector2Length(away);
            if (d < auraR && d > 0.01f)
            {
                float push = (auraR - d) * 0.5f; // empurra mais quanto mais perto
                enemy->position = Vector2Add(enemy->position, Vector2Scale(Vector2Normalize(away), push));
            }
        }

        float distSqrToPlayer = Vector2DistanceSqr(game->player.position, enemy->position);

        // --------------------------------------------------------------------
        // PERCEPÇÃO (dificuldade): detecta o jogador por distância e guarda a
        // última posição conhecida, perseguindo-a por um tempo mesmo sem ver.
        // --------------------------------------------------------------------
        float detRange = game->diff.detectionRange;
        if (detRange <= 1.0f) detRange = 480.0f; // segurança
        bool seesPlayer = (distSqrToPlayer < detRange * detRange);
        if (seesPlayer)
        {
            enemy->lastKnownPlayerPos = game->player.position;
            enemy->aggroMemory = game->diff.aggroMemoryTime;
        }
        else if (enemy->aggroMemory > 0.0f)
        {
            enemy->aggroMemory -= delta;
        }
        // Alvo de perseguição: o jogador (se visível) ou a última posição conhecida.
        Vector2 chaseTarget = seesPlayer ? game->player.position : enemy->lastKnownPlayerPos;

        // --------------------------------------------------------------------
        // ESQUIVA DE PROJÉTEIS (médio/difícil): dash lateral curto se um projétil
        // do jogador estiver vindo na direção do inimigo. Tanques esquivam pouco;
        // rápidos esquivam melhor; chefe não esquiva (usa escudo/resistência).
        // --------------------------------------------------------------------
        if (enemy->dodgeCooldown > 0.0f) enemy->dodgeCooldown -= delta;
        if (enemy->tier != TIER_3_BOSS && enemy->dodgeCooldown <= 0.0f &&
            game->diff.dodgeChance > 0.0f && enemy->state != DEATH)
        {
            float chance = game->diff.dodgeChance;
            if (enemy->type == 2) chance *= 0.30f;              // tanque esquiva pouco
            if (enemy->type == 1 || enemy->type == 3) chance *= 1.30f; // rápidos esquivam melhor
            if (beh == BEHAV_SWARM) chance *= 1.25f;            // enxame ágil esquiva mais
            if (beh == BEHAV_ELITE) chance *= 0.45f;            // elite resistente esquiva pouco
            if (enemy->tier == TIER_MINIBOSS) chance *= 0.40f;  // mini chefe esquiva pouco
            for (int p = 0; p < MAX_PROJECTILES; p++)
            {
                if (!game->projectiles[p].active || !game->projectiles[p].isPlayerProjectile) continue;
                Vector2 toEnemy = Vector2Subtract(enemy->position, game->projectiles[p].position);
                if (Vector2LengthSqr(toEnemy) > 170.0f * 170.0f) continue;
                Vector2 pv = game->projectiles[p].velocity;
                if (pv.x == 0.0f && pv.y == 0.0f) continue;
                pv = Vector2Normalize(pv);
                Vector2 te = Vector2Normalize(toEnemy);
                if (Vector2DotProduct(pv, te) < 0.70f) continue; // não está vindo na direção
                if (GetRandomValue(0, 100) < (int)(chance * 100.0f))
                {
                    Vector2 perp = { -pv.y, pv.x };
                    float side = (Vector2DotProduct(perp, toEnemy) >= 0.0f) ? 1.0f : -1.0f;
                    enemy->position = Vector2Add(enemy->position, Vector2Scale(perp, side * 52.0f));
                    if (enemy->position.x < 30.0f) enemy->position.x = 30.0f;
                    if (enemy->position.x > MAP_WIDTH - 30.0f) enemy->position.x = MAP_WIDTH - 30.0f;
                    if (enemy->position.y < 30.0f) enemy->position.y = 30.0f;
                    if (enemy->position.y > MAP_HEIGHT - 30.0f) enemy->position.y = MAP_HEIGHT - 30.0f;
                    enemy->dodgeCooldown = 0.8f;
                    SpawnParticleExplosion(game, enemy->position, (Color){ 200, 220, 255, 255 }, 5, 40.0f, 110.0f, 2.0f, 0.3f);
                }
                break;
            }
        }

        // Status updates
        if (enemy->poisonTimer > 0.0f && enemy->state != DEATH) {
            enemy->poisonTimer -= delta;
            // BUGFIX: "hp -= 15*delta" truncava para 0 todo frame (hp é int) e o
            // veneno em inimigos nunca causava dano. Agora acumula a fração e
            // aplica 15 de dano por segundo de fato.
            enemy->poisonAccum += 15.0f * delta;
            if (enemy->poisonAccum >= 1.0f) {
                int pdmg = (int)enemy->poisonAccum;
                enemy->poisonAccum -= (float)pdmg;
                enemy->hp -= pdmg;
            }
            // Pequena partícula tóxica para feedback visual do veneno
            if (GetRandomValue(0, 20) == 0)
                SpawnParticle(game, enemy->position, (Vector2){ 0, -25 }, (Color){ 150, 60, 200, 255 }, 3.0f, 0.4f);
            if (enemy->hp <= 0) {
                enemy->hp = 0;
                enemy->state = DEATH;
                enemy->cooldownTimer = 0.5f; // Death animation duration
                PlayEnemyDamageSfx(enemy->type, enemy->tier);
                RegisterEnemyKill(game, enemy);
            }
        }
        if (enemy->slowTimer > 0.0f) enemy->slowTimer -= delta;
        if (enemy->hitCooldown > 0.0f) enemy->hitCooldown -= delta;

        // --------------------------------------------------------------------
        // FASES DO CHEFE + INVOCAÇÃO DE LACAIOS (chefe e mini chefe)
        // Fase 0 (>66%): padrão.  Fase 1 (33-66%): mais agressivo, invoca.
        // Fase 2 (<33%): ESCUDO com Núcleos de Infecção + rajada radial.
        // --------------------------------------------------------------------
        int bossPhase = 0;
        float bossSpeedMult = 1.0f;
        bool isBossUnit = (enemy->tier == TIER_3_BOSS);
        bool isMiniBoss = (enemy->tier == TIER_MINIBOSS);

        if (isBossUnit && enemy->state != DEATH)
        {
            float pct = (float)enemy->hp / enemy->maxHp;
            bossPhase = (pct > 0.66f) ? 0 : (pct > 0.33f) ? 1 : 2;
            bossSpeedMult = (1.0f + (float)bossPhase * 0.35f) * game->diff.bossAggroMul;

            if (bossPhase != enemy->aiPhase)
            {
                enemy->aiPhase = bossPhase;
                game->screenShake = 0.7f;
                SpawnParticleExplosion(game, enemy->position, (Color){ 255, 60, 80, 255 }, 40, 120.0f, 320.0f, 5.0f, 0.8f);
                if (bossPhase == 2)
                {
                    if (!game->bossCoresSpawned) SpawnInfectionCores(game, enemy->position);
                    ShowBanner(game, "FASE 3: ESCUDO ATIVO!",
                               "Destrua os 4 NUCLEOS DE INFECCAO para baixar o escudo!",
                               (Color){ 120, 230, 255, 255 }, 4.0f);
                }
                else
                {
                    ShowBanner(game, "O CHEFE MUDOU DE FASE!",
                               "FASE 2: enfurecida e invocando mais lacaios!",
                               (Color){ 255, 80, 90, 255 }, 3.0f);
                }
            }
        }

        // Invocação de lacaios: chefe (fase>=1) e mini chefe, com LIMITE e dificuldade
        if ((isBossUnit && bossPhase >= 1) || isMiniBoss)
        {
            enemy->summonTimer -= delta;
            if (enemy->summonTimer <= 0.0f)
            {
                float baseInterval = isBossUnit ? ((bossPhase == 2) ? 5.0f : 8.0f) : 11.0f;
                float smul = (game->diff.summonMul <= 0.01f) ? 1.0f : game->diff.summonMul;
                enemy->summonTimer = baseInterval / smul;

                // Conta lacaios ativos para respeitar o teto (performance + justiça)
                int minions = 0;
                for (int k = 0; k < MAX_ENEMIES; k++)
                    if (game->enemies[k].active && game->enemies[k].isEscort) minions++;

                int want = isBossUnit ? ((bossPhase == 2) ? 3 : 2) : 1;
                float hmul = (game->diff.enemyHealthMul <= 0.01f) ? 1.0f : game->diff.enemyHealthMul;
                for (int s = 0; s < want && minions < MAX_BOSS_MINIONS; s++)
                {
                    for (int k = 0; k < MAX_ENEMIES; k++)
                    {
                        if (game->enemies[k].active) continue;
                        Enemy *m = &game->enemies[k];
                        // Lacaios podem nascer defendendo um Núcleo (se houver escudo)
                        Vector2 origin = enemy->position;
                        if (game->bossShieldActive && CoresAlive(game) > 0 && GetRandomValue(0, 1) == 0)
                        {
                            for (int c = 0; c < MAX_CORES; c++)
                                if (game->cores[c].active) { origin = game->cores[c].position; break; }
                        }
                        float a = (float)GetRandomValue(0, 360) * DEG2RAD;
                        // Lacaio invocado conforme o Mundo: vírus invoca UNIDADES
                        // MENORES (enxame); bactéria invoca bacilo atirador. Init
                        // centralizado garante todos os campos do slot reutilizado.
                        bool virusW = (game->currentWorld == WORLD_VIRUS);
                        int mtype = virusW ? ETYPE_VIRUS_SWARM : ETYPE_BACT_RANGED;
                        EnemyInitFromArchetype(m, mtype, game->wave, hmul);
                        m->position = (Vector2){ origin.x + cosf(a) * 110.0f, origin.y + sinf(a) * 110.0f };
                        m->active = true;
                        m->patrolTarget = m->position;
                        m->isEscort = true;
                        m->flankSign = (GetRandomValue(0, 1) ? 1.0f : -1.0f);
                        game->enemiesRemaining++;
                        SpawnParticleExplosion(game, m->position, DARKGRAY, 8, 40.0f, 120.0f, 2.0f, 0.4f);
                        minions++;
                        break;
                    }
                }
            }
        }

        // State Machine Update
        if (enemy->state == HURT) {
            enemy->cooldownTimer -= delta;
            if (enemy->cooldownTimer <= 0.0f) enemy->state = IDLE;
        }
        else if (enemy->state == ATTACK) {
            enemy->chargeTimer -= delta;
            if (enemy->chargeTimer <= 0.0f) {
                // Seleciona tipo de projétil baseado no tipo do inimigo
                ProjectileType ptype = PROJ_ACID_ARC;
                int dmg = 8;
                
                if (enemy->type == 1) { // Dengue (legado): picada espalhada
                    ptype = PROJ_BULLET_SPREAD;
                    dmg = 10;
                } else if (enemy->type == 2) { // KPC: tiro pesado
                    ptype = PROJ_VOID_BOLT;
                    dmg = 20;
                } else if (enemy->type == 4) { // TB: ácido moderado
                    ptype = PROJ_ACID_ARC;
                    dmg = 12;
                } else if (enemy->type == ETYPE_BACT_RANGED) { // Bactéria atiradora (bacilo)
                    ptype = PROJ_ACID_ARC;
                    dmg = 11;
                } else if (enemy->type == ETYPE_VIRUS_RANGED || enemy->type == ETYPE_VIRUS_ELITE ||
                           enemy->type == ETYPE_VIRUS_BOSS) { // Vírus atirador/elite/chefe: material viral
                    ptype = PROJ_VIRAL_SPORE;
                    dmg = (enemy->type == ETYPE_VIRUS_ELITE) ? 16 : 12;
                }
                
                // Antecipação LIMITADA de mira: lidera o alvo conforme a velocidade
                // do herói e a dificuldade (difícil lidera mais). Limitada (cap) para
                // continuar justo e esquivável — não é mira perfeita.
                float lead = (game->diff.reactionMul <= 0.6f) ? 0.34f
                           : (game->diff.reactionMul <= 0.8f) ? 0.22f : 0.12f;
                Vector2 aimTarget = game->player.position;
                if (enemy->isRanged) {
                    Vector2 off = Vector2Scale(game->playerVelSmooth, lead);
                    float m = Vector2Length(off);
                    if (m > 140.0f) off = Vector2Scale(off, 140.0f / m);
                    aimTarget = Vector2Add(aimTarget, off);
                }

                SpawnProjectile(game, enemy->position, aimTarget, ptype, dmg);
                PlaySound(g_assets.sfxEnemyShoot);

                // Dengue (type 1): leque de 3 projéteis (a "picada espalhada" que o
                // nome PROJ_BULLET_SPREAD sempre prometeu mas nunca entregava).
                // Os i-frames do jogador limitam o dano total — o leque serve para
                // ser mais difícil de esquivar, não para multiplicar o dano.
                if (enemy->type == 1) {
                    Vector2 sprL = { aimTarget.x - 70, aimTarget.y };
                    Vector2 sprR = { aimTarget.x + 70, aimTarget.y };
                    SpawnProjectile(game, enemy->position, sprL, ptype, dmg);
                    SpawnProjectile(game, enemy->position, sprR, ptype, dmg);
                }

                // KPC e Boss disparam múltiplos projéteis
                if (enemy->tier == TIER_3_BOSS) {
                    Vector2 off1 = { aimTarget.x + 100, aimTarget.y };
                    Vector2 off2 = { aimTarget.x - 100, aimTarget.y };
                    SpawnProjectile(game, enemy->position, off1, ptype, dmg);
                    SpawnProjectile(game, enemy->position, off2, ptype, dmg);

                    // FASE FINAL: rajada radial em todas as direções (bullet-hell leve)
                    if (bossPhase >= 2) {
                        for (int r = 0; r < 8; r++) {
                            float a = (float)r / 8.0f * 2.0f * PI;
                            Vector2 rt = { enemy->position.x + cosf(a) * 200.0f,
                                           enemy->position.y + sinf(a) * 200.0f };
                            SpawnProjectile(game, enemy->position, rt, ptype, dmg);
                        }
                    }
                }

                enemy->state = IDLE;
                // Cadência: KPC atira mais devagar; o chefe acelera por fase.
                // A dificuldade ajusta a reação (difícil reage mais rápido).
                float react = game->diff.reactionMul;
                if (enemy->tier == TIER_3_BOSS)
                    enemy->cooldownTimer = (2.4f - (float)bossPhase * 0.7f) * react; // 2.4/1.7/1.0 * reação
                else if (enemy->tier == TIER_MINIBOSS)
                    enemy->cooldownTimer = 1.8f * react;
                else
                    enemy->cooldownTimer = ((enemy->type == 2) ? 2.5f : 1.5f) * react;
            }
        }
        else if (enemy->isRanged && seesPlayer && enemy->cooldownTimer <= 0.0f) {
            enemy->state = ATTACK;
            enemy->chargeTimer = 0.6f * game->diff.reactionMul; // reage mais rápido no difícil
        }
        else if (seesPlayer || enemy->aggroMemory > 0.0f) {
            enemy->state = AGGRO; // persegue mesmo a última posição conhecida
        }
        else {
            enemy->state = IDLE;
            enemy->patrolTimer -= delta;
        }
        
        if (enemy->cooldownTimer > 0.0f && enemy->state != HURT && enemy->state != ATTACK) {
            enemy->cooldownTimer -= delta;
        }

        // Ações de Movimentação
        if (enemy->state == AGGRO)
        {
            // Persegue o jogador OU a última posição conhecida (perception)
            Vector2 chaseDir = Vector2Subtract(chaseTarget, enemy->position);
            float distSqrToPlayer = Vector2LengthSqr(chaseDir);
            chaseDir = Vector2Normalize(chaseDir);

            // Velocidade modulada pela dificuldade
            float currentSpeed = enemy->speed * (enemy->slowTimer > 0.0f ? 0.5f : 1.0f) * game->diff.enemySpeedMul;

            if (!enemy->isRanged) {
                // Inimigo melee: cerco + fuga com pouca vida. O ENXAME viral
                // investe mais rápido e nunca recua (ataca em grupo).
                float chaseMult = (enemy->tier == TIER_2) ? 1.25f : 1.05f;
                if (beh == BEHAV_SWARM) chaseMult = 1.45f;
                Vector2 perp = { -chaseDir.y, chaseDir.x };

                // Fuga: tipos frágeis recuam quando estão com pouca vida (mas não a escolta
                // nem o enxame). O limiar depende da dificuldade (fácil recua antes).
                bool frail = (enemy->type == 3 || enemy->type == 0);
                if (frail && !enemy->isEscort && enemy->hp <= (int)(enemy->maxHp * game->diff.retreatThreshold))
                    enemy->fleeTimer = 1.0f;

                if (enemy->fleeTimer > 0.0f) {
                    enemy->fleeTimer -= delta;
                    // Recua do jogador com leve desvio lateral (menos previsível)
                    Vector2 flee = Vector2Normalize(Vector2Add(Vector2Scale(chaseDir, -1.0f),
                                                               Vector2Scale(perp, enemy->flankSign * 0.5f)));
                    enemy->position = Vector2Add(enemy->position, Vector2Scale(flee, currentSpeed * 1.1f * delta));
                } else {
                    // Cerco: aproxima-se por um ângulo lateral quando longe, e reto quando perto.
                    // A intensidade do flanqueamento cresce com a dificuldade.
                    float flankAmount = (distSqrToPlayer > 130.0f * 130.0f) ? game->diff.flankAmount : 0.0f;
                    Vector2 desired = Vector2Normalize(Vector2Add(chaseDir, Vector2Scale(perp, enemy->flankSign * flankAmount)));
                    // Enxame: leve zigue-zague (investida imprevisível, difícil de acertar).
                    if (beh == BEHAV_SWARM)
                    {
                        float wig = sinf(enemy->animTime * 9.0f + enemy->flankSign) * 0.5f;
                        desired = Vector2Normalize(Vector2Add(desired, Vector2Scale(perp, wig)));
                    }
                    enemy->position = Vector2Add(enemy->position, Vector2Scale(desired, currentSpeed * chaseMult * delta));
                }
            } else {
                // IA Específica: Aedes aegypti (Type 1), KPC/Chefe (Type 2) e TB (Type 4)
                if (enemy->type == 1) { // Aedes aegypti (errático, tenta manter distância)
                    if (distSqrToPlayer > 300.0f * 300.0f) {
                        float timeFactor = GetTime() * 15.0f;
                        float angle = sinf(timeFactor) * 1.2f;
                        float nx = chaseDir.x * cosf(angle) - chaseDir.y * sinf(angle);
                        float ny = chaseDir.x * sinf(angle) + chaseDir.y * cosf(angle);
                        Vector2 zigzagDir = (Vector2){nx, ny};
                        enemy->position = Vector2Add(enemy->position, Vector2Scale(zigzagDir, currentSpeed * delta));
                    } else if (distSqrToPlayer < 200.0f * 200.0f) {
                        // Foge se o jogador chegar muito perto
                        enemy->position = Vector2Add(enemy->position, Vector2Scale(chaseDir, -currentSpeed * delta));
                    }
                } else if (enemy->type == 2) { // KPC / Superbactéria / Chefe (persegue mantendo média distância)
                    // O chefe acelera conforme muda de fase (bossSpeedMult).
                    if (distSqrToPlayer > 250.0f * 250.0f) {
                        enemy->position = Vector2Add(enemy->position, Vector2Scale(chaseDir, currentSpeed * 0.8f * bossSpeedMult * delta));
                    }
                } else if (beh == BEHAV_ELITE && fmodf(enemy->animTime, 7.0f) >= 4.0f) {
                    // ELITE/MUTANTE: ALTERNA comportamento. Por ~3s a cada 7s entra em
                    // INVESTIDA agressiva (avança como um bruto), depois volta ao kiting
                    // (ramo abaixo). Torna o elite reconhecível e diferente do atirador.
                    if (distSqrToPlayer > 90.0f * 90.0f)
                        enemy->position = Vector2Add(enemy->position, Vector2Scale(chaseDir, currentSpeed * 1.1f * delta));
                } else {
                    // RANGED GENÉRICO (bactéria/vírus atirador, elite em kiting, mini chefe):
                    // KITING + STRAFE. Mantém uma distância preferida e ORBITA o
                    // jogador (circle-strafe), reduzindo o tempo ocioso e ficando
                    // mais difícil de acertar. O sentido da órbita (flankSign) é
                    // coordenado e troca de tempos em tempos (determinístico).
                    float pref = (enemy->tier == TIER_MINIBOSS) ? 360.0f : 300.0f;
                    float d = sqrtf(distSqrToPlayer);
                    Vector2 perp = { -chaseDir.y, chaseDir.x };
                    Vector2 move = { 0.0f, 0.0f };
                    if (d > pref + 90.0f)      move = chaseDir;                        // aproxima
                    else if (d < pref - 90.0f) move = Vector2Scale(chaseDir, -1.0f);   // recua
                    float strafe = 0.7f + game->diff.flankAmount * 0.5f;              // orbita
                    move = Vector2Add(move, Vector2Scale(perp, enemy->flankSign * strafe));
                    if (Vector2LengthSqr(move) > 0.0001f) move = Vector2Normalize(move);
                    enemy->position = Vector2Add(enemy->position, Vector2Scale(move, currentSpeed * 0.9f * delta));
                    // Inverte o sentido da órbita ~a cada 5s (sem aleatoriedade por frame).
                    if (fmodf(enemy->animTime, 5.0f) < delta) enemy->flankSign = -enemy->flankSign;
                }
            }
        }
        else if (enemy->state == IDLE && !enemy->isRanged)
        {
            float currentSpeed = enemy->speed * (enemy->slowTimer > 0.0f ? 0.5f : 1.0f);
            Vector2 dir = Vector2Subtract(enemy->patrolTarget, enemy->position);
            if (Vector2LengthSqr(dir) > 10.0f * 10.0f)
            {
                dir = Vector2Normalize(dir);
                enemy->position = Vector2Add(enemy->position, Vector2Scale(dir, (currentSpeed * 0.4f) * delta));
            }
        }
        else if (enemy->state == IDLE)
        {
            float distSqrToTarget = Vector2DistanceSqr(enemy->position, enemy->patrolTarget);
            if (distSqrToTarget < 15.0f * 15.0f || enemy->patrolTimer <= 0.0f)
            {
                float angle = (float)GetRandomValue(0, 360) * DEG2RAD;
                float radius = (float)GetRandomValue(100, 300);
                enemy->patrolTarget.x = enemy->position.x + cosf(angle) * radius;
                enemy->patrolTarget.y = enemy->position.y + sinf(angle) * radius;
                enemy->patrolTimer = (float)GetRandomValue(3, 7);
            }
            Vector2 patrolDir = Vector2Normalize(Vector2Subtract(enemy->patrolTarget, enemy->position));
            enemy->position = Vector2Add(enemy->position, Vector2Scale(patrolDir, enemy->speed * delta));
        }

        // --------------------------------------------------------------------
        // TENDÊNCIA CENTRAL DO CHEFE (fase protegida) — proteção adicional.
        // Enquanto o escudo está ativo, o chefe é atraído suavemente para a
        // região central segura do tórax. Mantém-no perto dos núcleos/herói e
        // evita que knockback/separação o prendam nas extremidades do corpo.
        // NÃO é teleporte: mistura-se ao movimento da IA e é limitado, então o
        // chefe permanece agressivo. (A validação dos núcleos é independente.)
        // --------------------------------------------------------------------
        if (isBossUnit && game->bossShieldActive && enemy->state != DEATH)
        {
            Vector2 safe = MapBody_GetSafeCenter();
            Vector2 toC = Vector2Subtract(safe, enemy->position);
            float distC = Vector2Length(toC);
            if (distC > 220.0f)
            {
                Vector2 dirC = Vector2Scale(toC, 1.0f / distC);
                float ramp = (distC - 220.0f) / 600.0f;
                if (ramp > 1.0f) ramp = 1.0f;
                float pull = enemy->speed * bossSpeedMult * 0.45f * ramp;
                enemy->position = Vector2Add(enemy->position, Vector2Scale(dirC, pull * delta));
            }
        }

        // --------------------------------------------------------------------
        // FÍSICA DE COLISÃO DESLIZANTE (SEPARAÇÃO ENTRE INIMIGOS)
        // --------------------------------------------------------------------
        // Raio de corpo: tanques (KPC) e o ELITE/mutante viral (maiores) ocupam mais.
        float enemyRadius = (enemy->type == 2 || enemy->type == ETYPE_VIRUS_ELITE) ? 35.0f : 20.0f;
        int nearEnemies[MAX_ENEMIES];
        int nearCount = GetEnemiesInRadius(game, enemy->position, 70.0f, nearEnemies);
        for (int k = 0; k < nearCount; k++) {
            int j = nearEnemies[k];
            if (i != j && game->enemies[j].active && game->enemies[j].state != DEATH) {
                float otherRadius = (game->enemies[j].type == 2 || game->enemies[j].type == ETYPE_VIRUS_ELITE) ? 35.0f : 20.0f;
                float distSqr = Vector2DistanceSqr(enemy->position, game->enemies[j].position);
                float minDist = enemyRadius + otherRadius;
                if (distSqr < minDist * minDist && distSqr > 0.0001f) {
                    Vector2 separationDir = Vector2Subtract(enemy->position, game->enemies[j].position);
                    separationDir = Vector2Normalize(separationDir);
                    // Empurrão de separação ajustável
                    enemy->position = Vector2Add(enemy->position, Vector2Scale(separationDir, 60.0f * delta));
                }
            }
        }

        // Mantém os patógenos DENTRO do corpo (mesma arena do herói).
        MapBody_ApplyCollision(&enemy->position, enemyRadius);

        // --------------------------------------------------------------------
        // COLISÃO: DANO NO JOGADOR
        // --------------------------------------------------------------------
        // Aumentado a pedido do usuário para que não entrem no personagem.
        // O chefe (corpo de 140px) usa um alcance maior para que encostar nele doa.
        float colRange = (enemy->tier == TIER_3_BOSS) ? 110.0f
                       : (enemy->tier == TIER_MINIBOSS) ? 75.0f
                       : (enemy->type == 2 || enemy->type == ETYPE_VIRUS_ELITE) ? 65.0f : 45.0f;
        if (distSqrToPlayer < colRange * colRange)
        {
            HandlePlayerEnemyCollision(game, enemy);
        }

        // --------------------------------------------------------------------
        // ANIMAÇÃO (fim da iteração): velocidade real suavizada (lean/stretch) e
        // envelope de antecipação/recuo do ataque. Tudo derivado de estado/tempo.
        // --------------------------------------------------------------------
        if (delta > 0.0f)
        {
            Vector2 inst = Vector2Scale(Vector2Subtract(enemy->position, animStartPos), 1.0f / delta);
            enemy->velSmooth = Vector2Lerp(enemy->velSmooth, inst, 0.30f);
        }
        // attackAnim: sobe (antecipação) enquanto carrega o tiro; cai p/ recuo logo
        // após disparar (cooldown alto recém-iniciado); relaxa para 0 caso contrário.
        float targetAnim = 0.0f;
        if (enemy->state == ATTACK)               targetAnim = 1.0f;   // antecipação
        else if (enemy->isRanged && enemy->cooldownTimer > 0.0f && enemy->state == IDLE)
            targetAnim = -0.5f;                                        // recuo pós-disparo
        enemy->attackAnim = Lerp(enemy->attackAnim, targetAnim, 12.0f * delta);
    }

    // ------------------------------------------------------------------------
    // NÚCLEOS DE INFECÇÃO (escudo do chefe): animação + gestão do escudo
    // ------------------------------------------------------------------------
    for (int i = 0; i < MAX_CORES; i++)
    {
        if (!game->cores[i].active) continue;
        game->cores[i].pulse += delta;
        if (game->cores[i].hitFlash > 0.0f) game->cores[i].hitFlash -= delta;
    }
    // Se o chefe não estiver mais vivo, os núcleos e o escudo somem.
    {
        bool bossAlive = false;
        for (int i = 0; i < MAX_ENEMIES; i++)
            if (game->enemies[i].active && game->enemies[i].tier == TIER_3_BOSS && game->enemies[i].state != DEATH)
            { bossAlive = true; break; }
        if (!bossAlive && (game->bossShieldActive || CoresAlive(game) > 0))
        {
            for (int i = 0; i < MAX_CORES; i++) game->cores[i].active = false;
            game->bossShieldActive = false;
        }
    }

    // ------------------------------------------------------------------------
    // 6.5 ATUALIZA PROJÉTEIS
    // ------------------------------------------------------------------------
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (game->projectiles[i].active)
        {
            // Move + aplica limites de alcance/void (Fase 5). Se o rifle estourou
            // o alcance ou saiu para o void, apenas se dissipa (sem dano).
            if (!Projectile_Advance(&game->projectiles[i], delta)) {
                if (game->projectiles[i].isPlayerProjectile)
                    SpawnParticle(game, game->projectiles[i].position, (Vector2){ 0, 0 },
                                  WeaponSkinPrimary(game->player.weaponSkinId), 3.0f, 0.22f);
                continue;
            }

            if (game->projectiles[i].isPlayerProjectile) {
                // Diminui o tempo de vida para explosivos
                game->projectiles[i].lifeTime -= delta;

                if (game->projectiles[i].type != PROJ_PLAYER_GRENADE &&
                    TriggerBioMinesInRadius(game, game->projectiles[i].position, 22.0f))
                {
                    if (game->projectiles[i].type != PROJ_PLAYER_BFG)
                        game->projectiles[i].active = false;
                }
                if (!game->projectiles[i].active) continue;
                
                // Granada
                if (game->projectiles[i].type == PROJ_PLAYER_GRENADE && game->projectiles[i].lifeTime <= 0.0f) {
                    game->projectiles[i].active = false;
                    SpawnParticleExplosion(game, game->projectiles[i].position, ORANGE, 25, 150.0f, 400.0f, 4.0f, 0.6f);
                    PlaySound(g_assets.sfxGrenadeExplode);
                    game->screenShake = 0.5f;
                    
                    int collIndices[MAX_ENEMIES];
                    int collCount = GetEnemiesInRadius(game, game->projectiles[i].position, 180.0f, collIndices);
                    for (int k = 0; k < collCount; k++) {
                        int eIdx = collIndices[k];
                        if (game->enemies[eIdx].active && game->enemies[eIdx].state != DEATH) {
                            int applied = ApplyPlayerDamageToEnemy(game, &game->enemies[eIdx], game->projectiles[i].damage, false);
                            if (applied > 0) {
                                SpawnDamageText(game, game->enemies[eIdx].position, applied, ORANGE);
                                // A Granada Macrófago libera enzimas: aplica veneno (DoT)
                                // nos sobreviventes, dando-lhe um papel claro contra alvos tanques.
                                game->enemies[eIdx].poisonTimer = 2.5f;
                                if (game->enemies[eIdx].hp <= 0) {
                                    game->enemies[eIdx].hp = 0;
                                    game->enemies[eIdx].state = DEATH;
                                    game->enemies[eIdx].cooldownTimer = 0.5f;
                                    RegisterEnemyKill(game, &game->enemies[eIdx]);
                                } else {
                                    game->enemies[eIdx].state = HURT;
                                    game->enemies[eIdx].cooldownTimer = 0.25f;
                                }
                            }
                        }
                    }
                    // A explosão também danifica os Núcleos de Infecção do escudo
                    HitInfectionCores(game, game->projectiles[i].position, 180.0f, game->projectiles[i].damage);
                }
                else {
                    // Colisão normal de projéteis do player (exceto granada que explode no timer)
                    // OTIMIZAÇÃO: usa o spatial grid em vez de varrer todos os inimigos
                    if (game->projectiles[i].type != PROJ_PLAYER_GRENADE) {
                        Rectangle searchRect = {
                            game->projectiles[i].hitbox.x - 40.0f,
                            game->projectiles[i].hitbox.y - 40.0f,
                            game->projectiles[i].hitbox.width + 80.0f,
                            game->projectiles[i].hitbox.height + 80.0f
                        };
                        int hitIdx[MAX_ENEMIES];
                        int hitCount = GetEnemiesInRect(game, searchRect, hitIdx);
                        for (int k = 0; k < hitCount; k++) {
                            int j = hitIdx[k];
                            if (game->enemies[j].active && game->enemies[j].state != DEATH) {
                                // Hitbox proporcional ao tamanho do inimigo. O chefe é
                                // enorme (140px) — com raio fixo de 45 os projéteis
                                // atravessavam o corpo dele sem registrar acerto.
                                float eRadius = (game->enemies[j].tier == TIER_3_BOSS) ? 100.0f
                                              : (game->enemies[j].tier == TIER_MINIBOSS) ? 60.0f : 45.0f;
                                Rectangle eRect = { game->enemies[j].position.x - eRadius, game->enemies[j].position.y - eRadius, eRadius * 2, eRadius * 2 };
                                if (CheckCollisionRecs(game->projectiles[i].hitbox, eRect)) {
                                    bool isBFG = (game->projectiles[i].type == PROJ_PLAYER_BFG);
                                    // BFG não é desativado no primeiro hit (perfurante)
                                    if (!isBFG) {
                                        game->projectiles[i].active = false;
                                    }

                                    int effDmg = ScaleAffinityDamage(game->projectiles[i].damage, game->projectiles[i].type, game->enemies[j].type);
                                    int applied = ApplyPlayerDamageToEnemy(game, &game->enemies[j], effDmg, isBFG);
                                    if (applied > 0) {
                                        if (isBFG) PlaySound(g_assets.sfxBFGDamage);
                                        else PlayEnemyDamageSfx(game->enemies[j].type, game->enemies[j].tier);
                                        SpawnParticleExplosion(game, game->enemies[j].position, WeaponSkinPrimary(game->player.weaponSkinId), 10, 50.0f, 150.0f, 3.0f, 0.4f);
                                        SpawnDamageText(game, game->enemies[j].position, applied, WeaponSkinSecondary(game->player.weaponSkinId));

                                        if (game->enemies[j].hp <= 0) {
                                            game->enemies[j].hp = 0;
                                            game->enemies[j].state = DEATH;
                                            game->enemies[j].cooldownTimer = 0.5f;
                                            RegisterEnemyKill(game, &game->enemies[j]);
                                        } else {
                                            game->enemies[j].state = HURT;
                                            game->enemies[j].cooldownTimer = 0.25f;
                                        }
                                    }

                                    // Se não é BFG, sai do loop (1 hit por projétil)
                                    if (!isBFG) break;
                                }
                            }
                        }

                        // Projéteis diretos também destroem os Núcleos de Infecção
                        if (game->projectiles[i].active)
                        {
                            bool isBFG2 = (game->projectiles[i].type == PROJ_PLAYER_BFG);
                            if (HitInfectionCores(game, game->projectiles[i].position, 14.0f, game->projectiles[i].damage) && !isBFG2)
                                game->projectiles[i].active = false;
                        }
                    }
                }
            } else {
                // Colisão com jogador (Inimigo atirou)
                Rectangle pRect = { game->player.position.x - 22, game->player.position.y - 22, 44, 44 };
                if (CheckCollisionRecs(game->projectiles[i].hitbox, pRect))
                {
                    HandleProjectileCollision(game, &game->projectiles[i]);
                }
            }
            
            // BFG: expira pelo lifetime
            if (game->projectiles[i].type == PROJ_PLAYER_BFG && game->projectiles[i].lifeTime <= 0.0f) {
                game->projectiles[i].active = false;
                SpawnParticleExplosion(game, game->projectiles[i].position, GREEN, 30, 100.0f, 300.0f, 5.0f, 0.8f);
            }

            // Rifles retos: backstop por lifetime (proteção SECUNDÁRIA ao alcance).
            if (game->projectiles[i].active && game->projectiles[i].lifeTime <= 0.0f &&
                (game->projectiles[i].type == PROJ_PLAYER_PHAGE ||
                 game->projectiles[i].type == PROJ_PLAYER_VACCINE ||
                 game->projectiles[i].type == PROJ_PLAYER_RIFLE)) {
                game->projectiles[i].active = false;
            }
            
            // Remove se sair do mapa
            if (game->projectiles[i].active &&
                (game->projectiles[i].position.x < -200 || game->projectiles[i].position.x > MAP_WIDTH + 200 ||
                 game->projectiles[i].position.y < -200 || game->projectiles[i].position.y > MAP_HEIGHT + 200)) {
                game->projectiles[i].active = false;
            }
        }
    }

    // ------------------------------------------------------------------------
    // 7. SISTEMA DE LEVEL UP DO HERÓI
    // ------------------------------------------------------------------------
    if (game->player.xp >= game->player.xpNeeded)
    {
        // Consome XP e sobe de nível
        game->player.xp -= game->player.xpNeeded;
        game->player.level++;
        game->player.xpNeeded = (int)(game->player.xpNeeded * 1.5f);

        // Melhora atributos do herói
        game->player.maxHp += 15;
        game->player.hp = game->player.maxHp; // Cura total no level up
        game->player.attackPower += 6;
        game->player.speed += 10.0f;

        // Grande efeito visual festivo de Level Up (Verde e Gold)
        SpawnParticleExplosion(game, game->player.position, LIME, 30, 80.0f, 220.0f, 5.0f, 1.0f);
        SpawnParticleExplosion(game, game->player.position, GOLD, 20, 100.0f, 250.0f, 4.0f, 1.2f);
        PlaySound(g_assets.sfxPickup); // som festivo de level up

        game->screenShake = 0.5f;

        // Detecta desbloqueio de nova arma por nível e mostra banner adequado.
        int newMax = 1;
        for (int w = 2; w <= 4; w++)
            if (game->player.level >= WeaponUnlockLevel(w)) newMax = w;

        if (newMax > game->maxWeaponUnlocked)
        {
            game->maxWeaponUnlocked = newMax;
            WeaponInfo wi = GetWeaponInfo(newMax);
            ShowBanner(game, TextFormat("NOVA ARMA: %s", wi.name),
                       TextFormat("Tecla [%d] desbloqueada - %s", wi.key, wi.special),
                       wi.color, 3.4f);
        }
        else
        {
            ShowBanner(game, TextFormat("NIVEL %d", game->player.level),
                       "Atributos aumentados! Vida restaurada.",
                       GOLD, 2.4f);
        }
    }

    // ------------------------------------------------------------------------
    // 7.5 CONCLUSÃO DA ONDA (CENTRALIZADO)
    // BUGFIX: antes a onda só avançava se o último inimigo morresse pelo golpe
    // melee; mortes por projétil/granada/veneno deixavam o jogo travado.
    // ------------------------------------------------------------------------
    if (game->enemiesRemaining <= 0)
    {
        bool anyAlive = false;
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (game->enemies[i].active && game->enemies[i].state != DEATH)
            {
                anyAlive = true;
                break;
            }
        }
        if (!anyAlive)
        {
            game->wave++;
            if (game->wave > WAVES_PER_WORLD)
            {
                if (game->currentWorld == WORLD_BACTERIA)
                {
                    // Concluiu o Mundo das Bactérias: cutscene com o cientista
                    // (diálogo educativo) que leva ao Mundo dos Vírus (Fase 6).
                    game->worldCompleted = true;
                    game->currentScreen = SCREEN_WORLD_TRANSITION;
                    game->uiAnimTimer = 0.0f;
                    game->sceneDialog = (DialogState){ true, 0, 0, 0.0f }; // reinicia o diálogo
                }
                else
                {
                    // Concluiu o Mundo dos Vírus: Vitória final.
                    RequestLoadingScreen(game, LOAD_TO_VICTORY, 2.5f);
                }
            }
            else
            {
                game->currentScreen = SCREEN_QUIZ;
            }
            return;
        }
    }

    // ------------------------------------------------------------------------
    // 8. ATUALIZAÇÃO DA CÂMERA (SUAVE COM LERP + SHAKE)
    // ------------------------------------------------------------------------
    float targetX = game->player.position.x;
    float targetY = game->player.position.y;
    
    // Lerp da câmera em direção ao jogador
    game->camera.target.x += (targetX - game->camera.target.x) * 0.085f;
    game->camera.target.y += (targetY - game->camera.target.y) * 0.085f;

    // Aplica chacoalhar de tela na câmera se ativo
    if (game->screenShake > 0.0f)
    {
        game->camera.offset.x = (SCREEN_WIDTH / 2.0f) + (float)GetRandomValue(-15, 15) * game->screenShake;
        game->camera.offset.y = (SCREEN_HEIGHT / 2.0f) + (float)GetRandomValue(-15, 15) * game->screenShake;
    }
    else
    {
        game->camera.offset = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
    }
}

// ============================================================================
// PERSISTÊNCIA: SALVAR JOGO POR SLOT
// ============================================================================
typedef struct {
    GameState *gameSnapshot;
    int slot;
} SaveThreadData;

THREAD_RETURN SaveGameThread(void *lpParam)
{
    SaveThreadData *data = (SaveThreadData *)lpParam;
    GameState *game = data->gameSnapshot;
    int slot = data->slot;

    char path[64];
    sprintf(path, "Saves/save_slot_%d.txt", slot);

    FILE *arquivo = fopen(path, "w");
    if (arquivo != NULL)
    {
        // 0. Metadados do Slot para carregamento rápido
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char dateBuffer[32];
        strftime(dateBuffer, sizeof(dateBuffer), "%d/%m/%Y %H:%M", tm_info);

        fprintf(arquivo, "%s\n", (game->player.name[0] != '\0') ? game->player.name : "HERO");
        fprintf(arquivo, "%d\n", game->player.level);
        fprintf(arquivo, "%d\n", game->player.score);
        fprintf(arquivo, "%d\n", game->wave);
        fprintf(arquivo, "%s\n", dateBuffer);

        // 1. Dados do Jogador
        fprintf(arquivo, "%f\n", game->player.position.x);
        fprintf(arquivo, "%f\n", game->player.position.y);
        fprintf(arquivo, "%d\n", game->player.hp);
        fprintf(arquivo, "%d\n", game->player.maxHp);
        fprintf(arquivo, "%d\n", game->player.score);
        fprintf(arquivo, "%d\n", game->player.level);
        fprintf(arquivo, "%d\n", game->player.xp);
        fprintf(arquivo, "%d\n", game->player.xpNeeded);
        fprintf(arquivo, "%d\n", game->player.attackPower);
        fprintf(arquivo, "%f\n", game->player.speed);

        // 2. Estado do Mundo
        fprintf(arquivo, "%d\n", game->wave);
        fprintf(arquivo, "%d\n", game->totalEnemiesKilled);
        fprintf(arquivo, "%f\n", game->timeElapsed);

        // 3. Contagem e Estado dos Inimigos
        // Grava exatamente o número de linhas de inimigos que serão escritas
        // (inimigos em animação de morte ou do tutorial não são salvos)
        int savedEnemies = 0;
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (game->enemies[i].active && game->enemies[i].state != DEATH && !game->enemies[i].isTutorialEnemy)
                savedEnemies++;
        }
        fprintf(arquivo, "%d\n", savedEnemies);

        // Escreve cada inimigo ativo
        for (int i = 0; i < MAX_ENEMIES; i++)
        {
            if (game->enemies[i].active && game->enemies[i].state != DEATH && !game->enemies[i].isTutorialEnemy)
            {
                fprintf(arquivo, "%f %f %d %d %d %f %d %d\n",
                        game->enemies[i].position.x,
                        game->enemies[i].position.y,
                        game->enemies[i].hp,
                        game->enemies[i].maxHp,
                        game->enemies[i].type,
                        game->enemies[i].speed,
                        game->enemies[i].tier,
                        game->enemies[i].isRanged);
            }
        }

        // 4. Bloco extra (versão 2+): poções, pontos do SUS, arma, skins,
        //    dificuldade e (versão 3) o Mundo atual da campanha.
        fprintf(arquivo, "EXTRA %d %d %d %d %d %d %d\n",
                game->player.healthPotions,
                game->player.susPoints,
                game->player.equippedWeapon,
                game->player.skinId,
                game->player.weaponSkinId,
                game->difficulty,
                game->currentWorld);

        fclose(arquivo);
    }

    // Libera a memória do snapshot e dos parâmetros da thread
    free(game);
    free(data);
    THREAD_END();
}

void SalvarJogoSlot(GameState *game, int slot)
{
    // Efeito visual imediato na thread principal para dar feedback ao jogador
    SpawnParticleExplosion(game, game->player.position, GREEN, 15, 30.0f, 90.0f, 4.0f, 0.6f);

    // Snapshot para o Salvamento Assíncrono
    GameState *snapshot = (GameState *)malloc(sizeof(GameState));
    if (snapshot != NULL) {
        memcpy(snapshot, game, sizeof(GameState));
        
        SaveThreadData *data = (SaveThreadData *)malloc(sizeof(SaveThreadData));
        if (data != NULL) {
            data->gameSnapshot = snapshot;
            data->slot = slot;
            
            // Inicia a thread de salvamento em background usando _beginthread (C runtime)
            START_THREAD(SaveGameThread, data);
        } else {
            free(snapshot);
        }
    }
}

// ============================================================================
// PERSISTÊNCIA: CARREGAR JOGO POR SLOT
// ============================================================================
void CarregarJogoSlot(GameState *game, int slot)
{
    char path[64];
    sprintf(path, "Saves/save_slot_%d.txt", slot);

    FILE *arquivo = fopen(path, "r");
    if (arquivo != NULL)
    {
        // Reseta primeiro para evitar lixo nas partículas e power-ups
        float shakeOld = game->screenShake;
        GameScreen oldScreen = game->currentScreen;
        float tempMusicVol = game->musicVolume;
        float tempSfxVol = game->sfxVolume;
        int tempSkin = game->player.skinId;
        int tempWSkin = game->player.weaponSkinId;
        int tempCos[COS_SLOT_COUNT];   // cosméticos persistem ao carregar um save
        for (int s = 0; s < COS_SLOT_COUNT; s++) tempCos[s] = game->player.cosmetics[s];

        // Limpa estados de buffs temporários
        memset(game, 0, sizeof(GameState));

        game->currentScreen = oldScreen;
        game->screenShake = shakeOld;
        game->musicVolume = tempMusicVol;
        game->sfxVolume = tempSfxVol;
        game->player.skinId = tempSkin;
        game->player.weaponSkinId = tempWSkin;
        for (int s = 0; s < COS_SLOT_COUNT; s++) game->player.cosmetics[s] = tempCos[s];
        InitParticlePool(game);

        // Pular/Ler metadados iniciais
        char nameLine[32];
        if (fgets(nameLine, sizeof(nameLine), arquivo) != NULL)
        {
            nameLine[strcspn(nameLine, "\r\n")] = '\0';
            strncpy(game->player.name, nameLine, 15);
            game->player.name[15] = '\0';
        }
        // Consome as linhas de metadados (nível/score/onda já constam mais abaixo).
        // O retorno é verificado apenas para silenciar avisos do compilador.
        int dummyLevel = 0, dummyScore = 0, dummyWave = 0;
        char dummyDate[32];
        if (fscanf(arquivo, "%d\n", &dummyLevel) != 1) dummyLevel = 0;
        if (fscanf(arquivo, "%d\n", &dummyScore) != 1) dummyScore = 0;
        if (fscanf(arquivo, "%d\n", &dummyWave)  != 1) dummyWave = 0;
        (void)dummyLevel; (void)dummyScore; (void)dummyWave;
        if (fgets(dummyDate, sizeof(dummyDate), arquivo) != NULL)
        {
            // Apenas consome a linha da data
        }

        // 1. Dados do Jogador (com verificação básica de leitura)
        bool readOk = true;
        readOk &= (fscanf(arquivo, "%f\n", &game->player.position.x) == 1);
        readOk &= (fscanf(arquivo, "%f\n", &game->player.position.y) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->player.hp) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->player.maxHp) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->player.score) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->player.level) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->player.xp) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->player.xpNeeded) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->player.attackPower) == 1);
        readOk &= (fscanf(arquivo, "%f\n", &game->player.speed) == 1);

        // 2. Estado do Mundo
        readOk &= (fscanf(arquivo, "%d\n", &game->wave) == 1);
        readOk &= (fscanf(arquivo, "%d\n", &game->totalEnemiesKilled) == 1);
        readOk &= (fscanf(arquivo, "%f\n", &game->timeElapsed) == 1);

        // 3. Contagem e Inimigos
        int enemyCount = 0;
        readOk &= (fscanf(arquivo, "%d\n", &enemyCount) == 1);
        if (enemyCount < 0) enemyCount = 0;
        if (enemyCount > MAX_ENEMIES) enemyCount = MAX_ENEMIES;
        game->enemiesRemaining = enemyCount;

        for (int i = 0; i < enemyCount; i++)
        {
            int t = 0, isR = 0;
            if (fscanf(arquivo, "%f %f %d %d %d %f %d %d\n",
                    &game->enemies[i].position.x,
                    &game->enemies[i].position.y,
                    &game->enemies[i].hp,
                    &game->enemies[i].maxHp,
                    &game->enemies[i].type,
                    &game->enemies[i].speed,
                    &t,
                    &isR) != 8)
            {
                // Linha corrompida/faltando: ajusta a contagem e para a leitura
                game->enemiesRemaining = i;
                break;
            }
            if (t < 0 || t > TIER_MINIBOSS) t = TIER_1; // aceita mini chefe salvo
            game->enemies[i].tier = (EnemyTier)t;
            game->enemies[i].isRanged = (bool)isR;

            game->enemies[i].active = true;
            game->enemies[i].state = IDLE;
            game->enemies[i].patrolTarget = game->enemies[i].position;
            game->enemies[i].patrolTimer = 3.0f;
        }

        // 4. Bloco extra (versão 2+) — opcional para compatibilidade com saves antigos
        int potions = 3, sus = 0, weapon = 1, skin = game->player.skinId, wskin = game->player.weaponSkinId;
        int diffSaved = game->difficulty;
        int worldSaved = WORLD_BACTERIA; // saves antigos (sem este campo) começam no Mundo das Bactérias
        char extraTag[8] = { 0 };
        if (fscanf(arquivo, "%7s", extraTag) == 1 && strcmp(extraTag, "EXTRA") == 0)
        {
            // Tenta ler 7 valores (com dificuldade e Mundo); tolera saves antigos com 5 ou 6.
            int rd = fscanf(arquivo, "%d %d %d %d %d %d %d", &potions, &sus, &weapon, &skin, &wskin, &diffSaved, &worldSaved);
            if (rd < 5) { potions = 3; sus = 0; weapon = 1; }
        }
        if (diffSaved >= DIFFICULTY_EASY && diffSaved <= DIFFICULTY_HARD)
            game->difficulty = diffSaved;
        // Sanitiza o Mundo carregado (retrocompatível: vale WORLD_BACTERIA por padrão)
        game->currentWorld = (worldSaved == WORLD_VIRUS) ? WORLD_VIRUS : WORLD_BACTERIA;

        // Reconstrói o escudo de capsídeo dos inimigos virais carregados (o
        // escudo não é gravado no arquivo; é derivado do tipo/vida no Mundo 2).
        if (game->currentWorld == WORLD_VIRUS)
        {
            for (int i = 0; i < game->enemiesRemaining && i < MAX_ENEMIES; i++)
            {
                Enemy *e = &game->enemies[i];
                if (!e->active) continue;
                // Todos os vírus comuns/elite têm capsídeo; o CHEFE usa o sistema
                // de Núcleos de Infecção (não tem escudo de capsídeo próprio).
                if (EnemyIsViral(e->type) && e->tier != TIER_3_BOSS)
                {
                    e->shieldMaxHp = (e->maxHp / 3) + 20;
                    e->shieldHp = e->shieldMaxHp;
                    e->shieldActive = true;
                    e->shieldHitFlash = 0.0f;
                }
            }
        }
        ApplyDifficulty(game);
        game->player.healthPotions = (potions >= 0 && potions <= 99) ? potions : 3;
        game->player.susPoints = (sus >= 0) ? sus : 0;
        // BUGFIX: após carregar um save, equippedWeapon ficava 0 e o ataque não fazia nada
        game->player.equippedWeapon = (weapon >= 1 && weapon <= WEAPON_COUNT) ? weapon : 1;
        game->player.skinId = (skin >= 0 && skin < SKIN_COUNT) ? skin : 0;
        game->player.weaponSkinId = (wskin >= 0 && wskin < WEAPON_SKIN_COUNT) ? wskin : 0;

        // Sanitiza valores para evitar estados inválidos com arquivos corrompidos
        if (game->player.maxHp <= 0) game->player.maxHp = 100;
        if (game->player.hp <= 0 || game->player.hp > game->player.maxHp) game->player.hp = game->player.maxHp;
        if (game->player.xpNeeded <= 0) game->player.xpNeeded = 100;
        if (game->player.level <= 0) game->player.level = 1;
        if (game->player.attackPower <= 0) game->player.attackPower = 15;
        if (game->player.speed < 100.0f || game->player.speed > 1000.0f) game->player.speed = 280.0f;
        if (game->wave < 1 || game->wave > 5) game->wave = 1;

        // Recalcula as armas desbloqueadas a partir do nível carregado e garante
        // que a arma equipada não esteja bloqueada. A Lâmina Bioelétrica (5) deriva
        // dos abates salvos (totalEnemiesKilled) — sem campo novo no save.
        game->maxWeaponUnlocked = 1;
        for (int w = 2; w <= 4; w++)
            if (game->player.level >= WeaponUnlockLevel(w)) game->maxWeaponUnlocked = w;
        game->bioBladeAnnounced = (game->totalEnemiesKilled >= BIOBLADE_UNLOCK_KILLS);
        if (!WeaponUnlocked(game, game->player.equippedWeapon))
            game->player.equippedWeapon = game->maxWeaponUnlocked;
        if (game->player.position.x < 0.0f || game->player.position.x > MAP_WIDTH ||
            game->player.position.y < 0.0f || game->player.position.y > MAP_HEIGHT)
        {
            game->player.position = (Vector2){ MAP_WIDTH / 2.0f, MAP_HEIGHT / 2.0f };
        }
        if (!readOk)
        {
            // Save gravemente corrompido: garante pelo menos um estado jogável
            game->enemiesRemaining = 0;
        }

        // Atributos derivados que não vão para o arquivo
        game->player.squashX = 1.0f;
        game->player.squashY = 1.0f;
        game->player.facingDir = 1;
        for (int i = 0; i < 10; i++) game->player.trail[i] = game->player.position;
        game->player.damageCooldown = 1.5f; // breve invulnerabilidade pós-load

        // Câmera re-alinhada instantaneamente
        game->camera.target = game->player.position;
        game->camera.offset = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
        game->camera.zoom = 1.0f;
        game->camera.rotation = 0.0f;

        game->saveLoaded = true;
        fclose(arquivo);

        // Efeito visual de carregamento completo (Partículas ciano)
        SpawnParticleExplosion(game, game->player.position, SKYBLUE, 20, 50.0f, 150.0f, 4.0f, 0.7f);
    }
}

// ============================================================================
// PERSISTÊNCIA: CARREGAR METADADOS DE UM SLOT
// ============================================================================
SaveSlotMeta CarregarMetadadosSlot(int slot)
{
    SaveSlotMeta meta = { 0 };
    char path[64];
    sprintf(path, "Saves/save_slot_%d.txt", slot);

    FILE *arquivo = fopen(path, "r");
    if (arquivo != NULL)
    {
        meta.exists = true;
        
        // Linha 1: Nome do Jogador
        if (fgets(meta.name, sizeof(meta.name), arquivo) != NULL)
        {
            meta.name[strcspn(meta.name, "\r\n")] = '\0';
        }
        
        // Linhas 2-4: Nível, Score e Onda (meta já é zero-inicializado).
        // O retorno é verificado para silenciar avisos e tolerar saves parciais.
        if (fscanf(arquivo, "%d\n", &meta.level) != 1) meta.level = 0;
        if (fscanf(arquivo, "%d\n", &meta.score) != 1) meta.score = 0;
        if (fscanf(arquivo, "%d\n", &meta.wave)  != 1) meta.wave = 0;

        // Linha 5: Data
        if (fgets(meta.date, sizeof(meta.date), arquivo) != NULL)
        {
            meta.date[strcspn(meta.date, "\r\n")] = '\0';
        }
        
        fclose(arquivo);
    }
    else
    {
        meta.exists = false;
    }
    return meta;
}

// ============================================================================
// TELA DE CARREGAMENTO (LOADING SCREEN)
// ============================================================================
void RequestLoadingScreen(GameState *game, LoadTarget target, float duration)
{
    // Descarrega o mapa e entidades anteriores para otimização
    for (int i = 0; i < MAX_ENEMIES; i++) game->enemies[i].active = false;
    for (int i = 0; i < MAX_POWERUPS; i++) game->powerUps[i].active = false;
    InitParticlePool(game);
    for (int i = 0; i < MAX_PROJECTILES; i++) game->projectiles[i].active = false;
    for (int i = 0; i < MAX_BIOMINES; i++) game->bioMines[i].active = false;
    for (int i = 0; i < MAX_DAMAGE_TEXTS; i++) game->damageTexts[i].active = false;
    // Limpa o escudo/núcleos do chefe ao trocar de cena
    for (int i = 0; i < MAX_CORES; i++) game->cores[i].active = false;
    game->bossShieldActive = false;
    game->bossCoresSpawned = false;

    // Efeito especial de "injeção": ativo apenas na transição tutorial -> gameplay
    game->syringeTransitionFX = (game->inTutorial && target == LOAD_TO_GAMEPLAY);

    // Se estivermos saindo do tutorial, reseta inTutorial
    if (target == LOAD_TO_GAMEPLAY)
    {
        game->inTutorial = false;
    }

    game->currentScreen = SCREEN_LOADING;
    game->loadTarget = target;
    game->loadingTimer = 0.0f;
    game->loadingDuration = duration;
    int tipCount = GetLoadingTipCount();
    game->loadingTip = tipCount > 0 ? GetRandomValue(0, tipCount - 1) : 0;
    game->screenShake = 0.0f;
}

void UpdateTelaLoading(GameState *game, float delta)
{
    game->loadingTimer += delta;

    // Emite algumas partículas decorativas biológicas/de rede na tela de carregamento
    if (GetRandomValue(0, 15) == 0)
    {
        Vector2 pos = { (float)GetRandomValue(0, SCREEN_WIDTH), SCREEN_HEIGHT + 10.0f };
        Vector2 vel = { (float)GetRandomValue(-20, 20), (float)GetRandomValue(-50, -20) };
        Color col = (GetRandomValue(0, 1) == 0) ? (Color){ 0, 229, 255, 255 } : (Color){ 0, 200, 100, 255 };
        SpawnParticle(game, pos, vel, col, (float)GetRandomValue(2, 4), (float)GetRandomValue(2, 4));
    }

    // Atualiza partículas decorativas da tela de loading
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        if (game->particles[i].active)
        {
            game->particles[i].position.y += game->particles[i].velocity.y * delta;
            game->particles[i].position.x += game->particles[i].velocity.x * delta;
            game->particles[i].lifeTime -= delta;
            if (game->particles[i].lifeTime <= 0.0f) FreeParticle(game, i);
        }
    }

    if (game->loadingTimer >= game->loadingDuration)
    {
        // O efeito de injeção termina LIMPO: nada de tremor residual
        game->syringeTransitionFX = false;
        game->screenShake = 0.0f;

        // Se houver slot de save especificado para carregar, carrega-o agora.
        // O arquivo é texto pequeno: carregamento síncrono é instantâneo e
        // elimina a antiga thread com WaitTime (causa de travamentos/races).
        if (game->loadSlot > 0)
        {
            int slot = game->loadSlot;
            game->loadSlot = 0; // reseta
            CarregarJogoSlot(game, slot);
            game->currentScreen = SCREEN_GAMEPLAY;
            game->saveLoaded = true;
            strcpy(game->notificationMsg, "GAME LOADED!");
            game->timeElapsed = 0.0f;
            return;
        }

        // Caso contrário, faz o carregamento padrão do destino
        switch (game->loadTarget)
        {
            case LOAD_TO_TUTORIAL:
                InitTutorial(game);
                break;

            case LOAD_TO_GAMEPLAY:
                game->inTutorial = false;
                game->currentScreen = SCREEN_GAMEPLAY;

                // ------------------------------------------------------------
                // BUGFIX: estado do jogador totalmente saneado ao entrar no
                // organismo (antes o veneno/dano do tutorial vazava e matava
                // o jogador instantaneamente).
                // ------------------------------------------------------------
                game->player.hp = game->player.maxHp;     // HP cheio
                game->player.poisonTimer = 0.0f;          // sem veneno residual
                game->player.slowTimer = 0.0f;
                game->player.speedTimer = 0.0f;
                game->player.shieldTimer = 0.0f;
                game->player.attackBoostTimer = 0.0f;
                game->player.attackCooldown = 0.0f;       // pode atacar de imediato
                game->player.damageCooldown = 2.0f;       // 2s de invulnerabilidade inicial
                game->poisonTickAccum = 0.0f;
                game->player.squashX = 1.0f;
                game->player.squashY = 1.0f;
                if (game->player.equippedWeapon < 1 || game->player.equippedWeapon > 4)
                    game->player.equippedWeapon = 1;

                // Posiciona jogador no centro do organismo (posição segura;
                // a onda spawna inimigos a pelo menos 450px de distância)
                game->player.position = (Vector2){ MAP_WIDTH / 2.0f, MAP_HEIGHT / 2.0f };
                for (int i = 0; i < 10; i++) game->player.trail[i] = game->player.position;

                // Reinicia a câmera (sem tremor herdado da cutscene)
                game->camera.target = game->player.position;
                game->camera.offset = (Vector2){ SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f };
                game->camera.zoom = 1.0f;
                game->camera.rotation = 0.0f;
                game->screenShake = 0.0f;

                // Inicia wave
                StartNextWave(game);
                break;

            case LOAD_TO_MENU:
                game->currentScreen = SCREEN_MENU;
                break;

            case LOAD_TO_GAMEOVER:
                game->currentScreen = SCREEN_GAMEOVER;
                break;

            case LOAD_TO_VICTORY:
                game->currentScreen = SCREEN_VICTORY;
                game->sceneDialog = (DialogState){ true, 0, 0, 0.0f }; // cena final do cientista
                break;
        }
    }
}

// ============================================================================
// TEXTOS DE DIÁLOGO DO TUTORIAL (PAGINADOS)
// ============================================================================
void GetTutorialDialogText(int step, int page, const char **line1, const char **line2, const char **line3)
{
    *line1 = "";
    *line2 = "";
    *line3 = "";
    if (step == 0)
    {
        if (page == 0)
        {
            *line1 = "Voce e um Anticorpo convocado para salvar um paciente infectado no DF.";
            *line2 = "Sua missao: eliminar patogenos e proteger o organismo.";
            *line3 = "[Q ou ESPACO para continuar]";
        }
        else
        {
            *line1 = "Voce esta dentro de uma Seringa de Vacina, pronto para ser injetado.";
            *line2 = "Mova-se com WASD ou as SETAS.";
            *line3 = "Avance para a ESQUERDA, em direcao a agulha, para calibrar os sensores.";
        }
    }
    else if (step == 1)
    {
        if (page == 0)
        {
            *line1 = "Alerta! Uma bacteria de treino (vacina atenuada) foi inserida.";
            *line2 = "Seu corpo precisa aprender a reconhecer e destruir essa ameaca.";
            *line3 = "[Q ou ESPACO para continuar]";
        }
        else
        {
            *line1 = "ATAQUE com o BOTAO ESQUERDO DO MOUSE ou ESPACO (o golpe acerta ao redor).";
            *line2 = "Cuidado: ela dispara toxinas. Esquive-se andando!";
            *line3 = "Ao derrota-la, colete a cura que ela deixa cair.";
        }
    }
    else if (step == 2)
    {
        if (page == 0)
        {
            *line1 = "Excelente! Assinatura do patogeno registrada com sucesso.";
            *line2 = "No organismo: troque de arma com 1-4 e use pocao de vida com E.";
            *line3 = "[Q ou ESPACO para continuar]";
        }
        else
        {
            *line1 = "Siga ate o BOCAL DA AGULHA, a esquerda (piscando em verde).";
            *line2 = "Ao entrar, voce sera injetado na corrente sanguinea do paciente.";
            *line3 = "Boa sorte, Anticorpo. O Distrito Federal conta com voce!";
        }
    }
}

// ============================================================================
// TRANSIÇÃO ENTRE MUNDOS (Fase 6): Bactérias -> Vírus — CENA DO CIENTISTA
// Substitui a antiga tela puramente textual por um diálogo com o cientista (mesma
// arte do tutorial, em destaque) que celebra a vitória sobre a KPC, reforça o uso
// correto de antibióticos/higiene e introduz o Mundo dos Vírus. Texto em páginas
// com typewriter; na última, inicia o Mundo 2 preservando o progresso do jogador.
// ============================================================================
static const char *const WORLD1_PAGES[] = {
    "Excelente trabalho, agente! A pneumonia bacteriana foi contida e a "
    "Superbacteria KPC nao vai mais avancar pelo organismo.",

    "Mas preste atencao: bacterias como a KPC sao perigosas justamente porque "
    "resistem a muitos antibioticos. Nada de automedicacao. Antibiotico so com "
    "prescricao e sempre ate o fim do tratamento indicado. E nunca subestime a "
    "higiene das maos: nos hospitais, ela salva vidas.",

    "Ainda nao acabou. Os sensores detectaram uma nova onda de invasores. Agora "
    "nao sao bacterias, e sim VIRUS: menores, mais rapidos e muito diferentes "
    "entre si.",

    "Cinco ameacas virais surgirao progressivamente, ate o temivel CORONAVIRUS na "
    "onda final. Todas tem CAPSIDEO: use a Lamina Bioeletrica para romper o escudo. "
    "Prepare-se, Anticorpo: o organismo ainda precisa de voce.",
};
#define WORLD1_PAGE_COUNT ((int)(sizeof(WORLD1_PAGES) / sizeof(WORLD1_PAGES[0])))

void UpdateTelaTransicao(GameState *game)
{
    game->uiAnimTimer += GetFrameTime();
    if (game->uiAnimTimer < 0.35f) return; // breve espera de entrada antes de aceitar input

    int r = ScientistDialogAdvance(&game->sceneDialog,
                                   WORLD1_PAGES[game->sceneDialog.page], WORLD1_PAGE_COUNT,
                                   SCIENTIST_VOICE_WORLD1, game->sfxVolume);
    if (r != 2) return; // ainda digitando ou lendo as paginas

    // Diálogo concluido: inicia o Mundo dos Vírus mantendo nível, armas e skins.
    game->sceneDialog.active = false;
    game->currentWorld = WORLD_VIRUS;
    game->worldCompleted = false;
    game->wave = 1;

    // Limpa o estado de combate do mundo anterior.
    for (int i = 0; i < MAX_ENEMIES; i++)    game->enemies[i].active = false;
    for (int i = 0; i < MAX_POWERUPS; i++)   game->powerUps[i].active = false;
    for (int i = 0; i < MAX_PROJECTILES; i++) game->projectiles[i].active = false;
    for (int i = 0; i < MAX_BIOMINES; i++) game->bioMines[i].active = false;
    for (int i = 0; i < MAX_CORES; i++)      game->cores[i].active = false;
    game->bossShieldActive = false;
    game->bossCoresSpawned = false;
    game->enemiesRemaining = 0;

    // Restaura o herói e remove debuffs/itens temporários.
    game->player.hp = game->player.maxHp;
    game->player.poisonTimer = 0.0f; game->player.slowTimer = 0.0f;
    game->player.damageCooldown = 0.0f; game->poisonTickAccum = 0.0f;
    game->player.maskTimer = 0.0f; game->player.distancingTimer = 0.0f;
    game->player.regenTimer = 0.0f; game->player.regenAccum = 0.0f;
    game->player.position = (Vector2){ MAP_WIDTH / 2.0f, MAP_HEIGHT / 2.0f };

    SetWeaponWorld(game->currentWorld);
    StartNextWave(game);
    game->currentScreen = SCREEN_GAMEPLAY;
}

void DrawTelaTransicao(GameState *game, Font font)
{
    // Fundo escuro com leve gradiente azulado (entrada no "mundo viral").
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (Color){ 6, 10, 22, 255 });
    DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                           Fade((Color){ 20, 40, 80, 255 }, 0.5f), Fade(BLACK, 0.9f));

    float entry = UIEase(game->uiAnimTimer / 0.5f);
    if (entry > 1.0f) entry = 1.0f;

    // Título grande no topo.
    DrawTitleText(font, "MUNDO 1 CONCLUIDO: BACTERIAS DERROTADAS",
                  SCREEN_WIDTH / 2.0f, 56.0f, 28.0f, Fade(GOLD, entry));

    // Cena do cientista: círculo grande (mesma arte do tutorial) + caixa de
    // diálogo grande com efeito typewriter (texto com wrap, nunca vaza da caixa).
    DrawScientistDialog(font, &game->sceneDialog, "TRANSMISSAO // ALERTA VIRAL",
                        WORLD1_PAGES[game->sceneDialog.page], WORLD1_PAGE_COUNT,
                        (Color){ 120, 200, 255, 255 }, entry);
}
