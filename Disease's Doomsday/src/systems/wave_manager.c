#include "wave_manager.h"
#include "../../include/gameplay.h"
#include "../../Assets/Maps/map_body.h"
#include "raymath.h"
#include <math.h>

// Posiciona um ponto de spawn DENTRO do corpo, longe do jogador (>= 600 px).
// Distância aumentada com o mapa maior (~2x área) para espalhar a horda.
static Vector2 PickSpawnFarFromPlayer(GameState *game)
{
    return MapBody_FindSpawnPoint(MapBody_RandomPointInside(game->player.position, 600.0f), 70.0f);
}

// Configura o chefe final (Superbactéria KPC) num índice específico
static void ConfigureBoss(GameState *game, int idx)
{
    Enemy *b = &game->enemies[idx];
    // Visual do chefe conforme o Mundo: Superbactéria KPC (Mundo 1) ou chefe
    // viral coronavírus (Mundo 2). O "escudo do chefe" usa o sistema de Núcleos
    // de Infecção (InfectionCore) — o chefe NÃO tem capsídeo próprio.
    int type = (game->currentWorld == WORLD_VIRUS) ? ETYPE_VIRUS_BOSS : ETYPE_KPC;
    EnemyInitFromArchetype(b, type, game->wave, 1.0f); // inicializa TODOS os campos do slot
    // Spawn validado: chefe nasce inteiramente dentro do corpo, com folga das
    // paredes (não fica meio "para fora" nem preso numa passagem estreita).
    b->position = MapBody_FindSpawnPoint(PickSpawnFarFromPlayer(game), 80.0f);
    b->active = true;
    b->patrolTarget = b->position;
    b->tier = TIER_3_BOSS;
    b->maxHp = 1400 + game->wave * 100;
    b->hp = b->maxHp;
    b->speed = 70.0f;
    b->isRanged = true;
    b->summonTimer = 6.0f;
    // O escudo do chefe é via Núcleos de Infecção (sem capsídeo próprio).
    b->shieldActive = false; b->shieldHp = 0; b->shieldMaxHp = 0; b->shieldHitFlash = 0.0f;
}

// Configura um MINI CHEFE da onda (entre o elite e o chefe final), escalando
// proceduralmente conforme a fase aumenta. NÃO morre num tiro de arma forte.
static void ConfigureMiniBoss(GameState *game, int idx)
{
    Enemy *m = &game->enemies[idx];
    // Mini chefe temático por Mundo e ONDA: bactéria tanque KPC (Mundo 1) ou, no
    // Mundo dos Vírus, líder do arquétipo da onda — o ELITE/mutante entra como
    // mini chefe a partir da onda 3 (ver VirusMiniBossType). O comportamento
    // (isRanged) vem do arquétipo; aqui só promovemos a tier de mini chefe.
    bool virus = (game->currentWorld == WORLD_VIRUS);
    int type = virus ? VirusMiniBossType(game->wave) : ETYPE_KPC;
    EnemyInitFromArchetype(m, type, game->wave, 1.0f); // inicializa TODOS os campos do slot
    m->position = MapBody_FindSpawnPoint(PickSpawnFarFromPlayer(game), 60.0f);
    m->active = true;
    m->patrolTarget = m->position;
    m->tier = TIER_MINIBOSS;
    m->maxHp = 300 + game->wave * 220;   // wave1=520 ... wave4=1180
    m->hp = m->maxHp;
    m->speed = 90.0f + game->wave * 6.0f;
    m->cooldownTimer = 1.2f;
    m->flankSign = (GetRandomValue(0, 1) ? 1.0f : -1.0f);
    m->summonTimer = 8.0f;        // invoca lacaios ocasionalmente
    // Capsídeo reforçado do mini chefe viral (sobrepõe o valor do arquétipo).
    if (virus) { m->shieldMaxHp = 120 + game->wave * 30; m->shieldHp = m->shieldMaxHp; m->shieldActive = true; }
    else       { m->shieldMaxHp = 0; m->shieldHp = 0; m->shieldActive = false; }
    m->shieldHitFlash = 0.0f;
}

// Spawna um lacaio de escolta numa posição (perto do mini chefe/chefe).
static void SpawnEscortMinion(GameState *game, Vector2 pos)
{
    for (int k = 0; k < MAX_ENEMIES; k++)
    {
        if (game->enemies[k].active) continue;
        Enemy *e = &game->enemies[k];
        // Escolta temática por Mundo: vírus sorteia entre enxame/melee/atirador
        // (mais variedade que antes); bactéria entre coco/bacilo. Init centralizado
        // garante todos os campos; capsídeo vem do arquétipo (vírus).
        bool virus = (game->currentWorld == WORLD_VIRUS);
        int type;
        if (virus)
        {
            // Escolta respeita a progressão da onda (só tipos já introduzidos).
            int vt[4]; int vn = VirusWaveTypes(game->wave, vt);
            type = vt[GetRandomValue(0, vn - 1)];
        }
        else type = (GetRandomValue(0, 1) == 1) ? ETYPE_BACT_RANGED : ETYPE_BACT_MELEE;
        EnemyInitFromArchetype(e, type, game->wave, 1.0f); // hmul aplicado pelo loop global
        e->position = MapBody_FindSpawnPoint(pos, 40.0f);
        e->active = true;
        e->patrolTarget = pos;
        e->tier = TIER_2;                       // escolta padronizada
        e->maxHp = 18 + game->wave * 5;         // escolta é leve
        e->hp = e->maxHp;
        e->speed = e->isRanged ? 200.0f : 250.0f;
        e->flankSign = (GetRandomValue(0, 1) ? 1.0f : -1.0f);
        e->isEscort = true;
        game->enemiesRemaining++;
        return;
    }
}

void StartNextWave(GameState *game)
{
    // ------------------------------------------------------------------
    // ONDA FINAL (5): Confronto garantido com o Chefe (Superbactéria KPC)
    // Antes o chefe só aparecia por acaso (chance < 10%), tornando o clímax
    // imprevisível. Agora a última onda é sempre uma batalha de chefe com um
    // grupo curado de lacaios — desafiador, porém justo.
    // ------------------------------------------------------------------
    bool bossWave = (game->wave >= 5);

    // Aumenta quantidade a cada onda. Horda comum ampliada (x4 -> x6) para
    // acompanhar o mapa maior (~2x area) e nao deixar o corpo vazio. O total
    // ativo (horda + mini chefe + escolta) fica < MAX_ENEMIES em todas as ondas.
    int numEnemies = bossWave ? 13 : (8 + game->wave * 6); // 1 chefe + 12 lacaios na final
    if (numEnemies > MAX_ENEMIES) numEnemies = MAX_ENEMIES;

    game->enemiesRemaining = numEnemies;

    int startIdx = 0;
    if (bossWave)
    {
        ConfigureBoss(game, 0); // O chefe é sempre o inimigo 0
        startIdx = 1;
    }

    // ------------------------------------------------------------------------
    // COMPOSIÇÃO DA HORDA COMUM — centralizada por arquétipo (sem blocos
    // duplicados). Cada Mundo tem sua progressão de tipos por onda:
    //  - WORLD_VIRUS: composição PLANEJADA por onda (VirusWaveBag): onda 1 enxame;
    //    onda 2 +envelopado; onda 3 +atirador; onda 4 combinação completa. Na onda
    //    do chefe, lacaios = enxame/melee/atirador controlados.
    //  - WORLD_BACTERIA: composição original preservada (balança do Mundo 1).
    // ------------------------------------------------------------------------
    int vbag[16]; int vbn = 0;
    if (game->currentWorld == WORLD_VIRUS) vbn = VirusWaveBag(game->wave, vbag, 16);

    Vector2 bossPos = bossWave ? game->enemies[0].position : (Vector2){ 0, 0 };
    for (int i = startIdx; i < numEnemies; i++)
    {
        Vector2 spawnPos;
        bool escort = false;
        if (bossWave)
        {
            float ang = (float)(i - startIdx) / (float)(numEnemies - startIdx) * 2.0f * PI;
            float ring = 170.0f + (float)((i - startIdx) % 3) * 55.0f;
            spawnPos.x = bossPos.x + cosf(ang) * ring;
            spawnPos.y = bossPos.y + sinf(ang) * ring;
            // Mantém o anel de escolta dentro do corpo.
            spawnPos = MapBody_FindSpawnPoint(spawnPos, 45.0f);
            escort = true;
        }
        else
        {
            spawnPos = PickSpawnFarFromPlayer(game);
        }

        // Tipo deste slot conforme o Mundo/onda (composição determinística no
        // Mundo dos Vírus -> garante presença dos arquétipos esperados por onda).
        int type;
        if (game->currentWorld == WORLD_VIRUS)
        {
            if (bossWave)
            {
                int et[3] = { ETYPE_VIRUS_SWARM, ETYPE_VIRUS_MELEE, ETYPE_VIRUS_RANGED };
                type = et[(i - startIdx) % 3];
            }
            else type = (vbn > 0) ? vbag[(i - startIdx) % vbn] : ETYPE_VIRUS_SWARM;
        }
        else // WORLD_BACTERIA — composição original (preserva o Mundo 1)
        {
            int randVal = GetRandomValue(0, 99);
            if (randVal < 50 || game->wave == 1)     type = ETYPE_BACT_MELEE;
            else if (randVal < 85 || game->wave < 3) type = ETYPE_BACT_RANGED;
            else                                     type = ETYPE_KPC; // elite a partir da onda 3
        }

        // Inicializa TODOS os campos do slot a partir do arquétipo (capsídeo,
        // stats, animação). hmul é aplicado pelo loop de dificuldade adiante.
        Enemy *e = &game->enemies[i];
        EnemyInitFromArchetype(e, type, game->wave, 1.0f);
        e->position = spawnPos;
        e->active = true;
        e->patrolTarget = spawnPos;
        e->patrolTimer = (float)GetRandomValue(2, 5);
        e->cooldownTimer = (float)GetRandomValue(1, 3); // stagger inicial entre inimigos
        e->flankSign = (GetRandomValue(0, 1) == 0) ? -1.0f : 1.0f;
        e->isEscort = escort;
    }

    // ------------------------------------------------------------------
    // MINI CHEFE: cada onda comum (1-4) ganha um mini chefe como evento
    // principal, acompanhado de uma pequena escolta. A onda 5 já tem o
    // CHEFE final, então não recebe mini chefe.
    // ------------------------------------------------------------------
    if (!bossWave)
    {
        for (int k = 0; k < MAX_ENEMIES; k++)
        {
            if (!game->enemies[k].active)
            {
                ConfigureMiniBoss(game, k);
                game->enemiesRemaining++;
                // Escolta ao redor do mini chefe (aumenta por onda)
                int escorts = 2 + game->wave / 2;
                for (int e = 0; e < escorts; e++)
                {
                    float ang = (float)e / (float)escorts * 2.0f * PI;
                    Vector2 ep = { game->enemies[k].position.x + cosf(ang) * 140.0f,
                                   game->enemies[k].position.y + sinf(ang) * 140.0f };
                    ep = MapBody_FindSpawnPoint(ep, 42.0f); // dentro do corpo, com margem
                    SpawnEscortMinion(game, ep);
                }
                break;
            }
        }
    }

    // Garante que existam alguns power-ups espalhados no mapa no início da onda
    int powerUpsCount = 4 + game->wave;
    if (powerUpsCount > 10) powerUpsCount = 10;
    for (int i = 0; i < powerUpsCount; i++)
    {
        // Power-up validado: dentro do corpo e com folga das paredes (coletável).
        Vector2 itemPos = MapBody_FindSpawnPoint(MapBody_RandomPointInside(game->player.position, 0.0f), 30.0f);
        SpawnPowerUpAt(game, itemPos, -1); // Tipo aleatório, dentro do corpo
    }

    // ------------------------------------------------------------------
    // DIFICULDADE: escala a vida de TODOS os inimigos recém-spawnados.
    // (dano e velocidade são aplicados em tempo real na IA/combate.)
    // ------------------------------------------------------------------
    float hmul = game->diff.enemyHealthMul;
    if (hmul <= 0.01f) hmul = 1.0f; // segurança caso a dificuldade não tenha sido aplicada
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        if (game->enemies[i].active && !game->enemies[i].isTutorialEnemy)
        {
            int hp = (int)(game->enemies[i].maxHp * hmul);
            if (hp < 1) hp = 1;
            game->enemies[i].maxHp = hp;
            game->enemies[i].hp = hp;
            // Inicializa os campos de ANIMAÇÃO transitórios (evita usar valores
            // antigos de um slot reaproveitado). animTime recebe um pequeno
            // deslocamento por índice para dessincronizar o bobbing entre inimigos.
            game->enemies[i].velSmooth = (Vector2){ 0.0f, 0.0f };
            game->enemies[i].animTime  = (float)(i % 8) * 0.25f;
            game->enemies[i].attackAnim = 0.0f;
            game->enemies[i].spawnAnim  = 0.0f;
        }
    }

    // Partículas azuis de invocação de nova onda
    for (int p = 0; p < 30; p++)
    {
        Vector2 vel = { (float)GetRandomValue(-150, 150), (float)GetRandomValue(-150, 150) };
        SpawnParticle(game, game->player.position, vel, SKYBLUE, 6.0f, 1.2f);
    }

    // Banner de aviso da nova horda / chefe (texto por Mundo).
    bool virusWorld = (game->currentWorld == WORLD_VIRUS);
    if (bossWave)
    {
        if (virusWorld)
            ShowBanner(game, "CHEFE VIRAL: CAPSIDEO REFORCADO",
                       "Quebre o escudo (capsideo) antes de atacar a vida!",
                       (Color){ 120, 200, 255, 255 }, 4.0f);
        else
            ShowBanner(game, "CHEFE: SUPERBACTERIA KPC",
                       "Resistente a antibioticos! Use a Granada e a BFG.",
                       (Color){ 255, 70, 90, 255 }, 4.0f);
        game->screenShake = 0.8f;
    }
    else
    {
        const char *sub;
        if (virusWorld)
            sub = (game->wave == 4)
                ? "Virus com escudo! Quebre o capsideo. O CHEFE viral se aproxima!"
                : "Virus com capsideo: quebre o escudo para eliminar.";
        else
            sub = (game->wave == 4)
                ? "MINI CHEFE presente! E a proxima horda traz o CHEFE final!"
                : "Um MINI CHEFE lidera esta horda. Elimine todos para avancar.";
        ShowBanner(game, TextFormat("HORDA %d / 5", game->wave), sub,
                   (Color){ 0, 229, 255, 255 }, 3.4f);
    }
}
