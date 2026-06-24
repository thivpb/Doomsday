#include "player.h"
#include "../../include/game.h"
#include "../../include/gameplay.h"
#include "../../include/spatial_grid.h"
#include "../../include/asset_manager.h"
#include "../../src/entities/projectiles.h"
#include "../../Assets/@models/weapons_model.h"
#include "raymath.h"

void PlayerAttack(GameState *game, Vector2 worldMousePos)
{
    if (game->player.attackCooldown > 0.0f) return;

    // BUGFIX: garante arma válida (após load de save antigo, equippedWeapon
    // podia ficar 0 e o ataque silenciosamente não fazia nada)
    if (game->player.equippedWeapon < 1 || game->player.equippedWeapon > WEAPON_COUNT)
        game->player.equippedWeapon = 1;

    int wpn = game->player.equippedWeapon;
    int danoBase = game->player.attackPower;
    if (game->player.attackBoostTimer > 0.0f) danoBase *= 2; // Buff de ataque dobra dano

    Color skinPrim = WeaponSkinPrimary(game->player.weaponSkinId);
    Color skinSec  = WeaponSkinSecondary(game->player.weaponSkinId);

    if (wpn == 1 || wpn == WEAPON_BIOBLADE) {
        Vector2 aimDir = Vector2Normalize(Vector2Subtract(worldMousePos, game->player.position));
        if (aimDir.x == 0.0f && aimDir.y == 0.0f)
            aimDir = (Vector2){ (float)game->player.facingDir, 0.0f };

        int combo = game->meleeComboStep % ((wpn == WEAPON_BIOBLADE) ? 3 : 2);
        int animKind = (combo == 0) ? 1 : (combo == 1) ? 2 : 3;
        float hitRadius = (animKind == 1) ? 175.0f : (animKind == 2) ? 155.0f : 145.0f;

        game->player.attackCooldown = (animKind == 3) ? 0.30f : 0.24f;
        PlayWeaponAttackSfx(wpn);

        game->slashAnimTimer = game->player.attackCooldown;
        game->slashAnimPos = game->player.position;
        game->slashAnimRadius = hitRadius;
        game->slashAnimKind = animKind;
        game->slashAnimDir = aimDir;
        game->meleeComboStep++;

        // Efeitos visuais: explosão de partículas de slash (cores da skin da arma)
        SpawnParticleExplosion(game, game->player.position, skinSec, (animKind == 3) ? 18 : 10, 100.0f, 250.0f, 4.0f, 0.4f);
        SpawnParticleExplosion(game, game->player.position, skinPrim, (animKind == 1) ? 6 : 10, 150.0f, 300.0f, 3.5f, 0.35f);

        // Câmera dá uma leve chacoalhada no ataque
        game->screenShake = (animKind == 3) ? 0.38f : 0.22f;

        TriggerBioMinesInRadius(game, game->player.position, hitRadius + 18.0f);

        int collIndices[MAX_ENEMIES];
        int collCount = GetEnemiesInRadius(game, game->player.position, hitRadius, collIndices);

        for (int k = 0; k < collCount; k++)
        {
            int i = collIndices[k];
            if (!game->enemies[i].active || game->enemies[i].state == DEATH) continue;

            Vector2 toEnemy = Vector2Subtract(game->enemies[i].position, game->player.position);
            float dist = Vector2Length(toEnemy);
            Vector2 enemyDir = (dist > 0.01f) ? Vector2Scale(toEnemy, 1.0f / dist) : aimDir;
            if (animKind == 1 && dist > 58.0f && Vector2DotProduct(aimDir, enemyDir) < 0.62f) continue;
            if (animKind == 2 && dist > 65.0f && Vector2DotProduct(aimDir, enemyDir) < -0.20f) continue;

            // Acertou! Causa dano (com proteções anti-melt para chefes/escudo)
            int danoTotal = 15 + danoBase;
            if (animKind == 1) danoTotal += 5;
            if (animKind == 3) danoTotal += 8;
            // Lâmina Bioelétrica (arma 5): descarrega corrente no capsídeo — dano
            // muito maior contra o ESCUDO viral, quebrando-o bem mais rápido. Vale
            // em QUALQUER Mundo (a arma é desbloqueada por abates na campanha).
            if (wpn == WEAPON_BIOBLADE && game->enemies[i].shieldActive && game->enemies[i].shieldHp > 0)
                danoTotal *= 3;
            game->enemies[i].lastHitWeaponSlot = 1;
            int applied = ApplyPlayerDamageToEnemy(game, &game->enemies[i], danoTotal, false);
            if (applied <= 0) continue; // bloqueado pelo escudo do chefe
            PlayEnemyDamageSfx(game->enemies[i].type, game->enemies[i].tier);
            SpawnDamageText(game, game->enemies[i].position, applied, skinSec);

            // Empurrão (Knockback) na direção oposta ao jogador (chefes resistem)
            float knockMag = (game->enemies[i].tier == TIER_3_BOSS) ? 8.0f
                           : (game->enemies[i].tier == TIER_MINIBOSS) ? 22.0f : 55.0f;
            Vector2 knockbackDir = Vector2Subtract(game->enemies[i].position, game->player.position);
            if (knockbackDir.x == 0.0f && knockbackDir.y == 0.0f) knockbackDir = (Vector2){ 1.0f, 0.0f };
            knockbackDir = Vector2Normalize(knockbackDir);

            // Empurra o inimigo a uma distância segura
            game->enemies[i].position = Vector2Add(game->enemies[i].position, Vector2Scale(knockbackDir, knockMag));

            // Partículas de sangue/dano no local do inimigo
            Color hitColor = (game->enemies[i].type == 2) ? MAROON : RED;
            SpawnParticleExplosion(game, game->enemies[i].position, hitColor, 15, 80.0f, 180.0f, 3.0f, 0.5f);

            // Se o inimigo morreu
            if (game->enemies[i].hp <= 0)
            {
                game->enemies[i].hp = 0;
                game->enemies[i].state = DEATH;
                game->enemies[i].cooldownTimer = 0.5f; // Duração da animação de morte

                // Partículas de morte e ganho de xp
                SpawnParticleExplosion(game, game->enemies[i].position, GOLD, 20, 50.0f, 150.0f, 4.0f, 0.7f);

                // Durante o tutorial NÃO contabilizamos onda nem score real
                if (game->inTutorial)
                {
                    // Apenas efeito visual de vitória na seringa
                    SpawnParticleExplosion(game, game->enemies[i].position, (Color){0, 220, 120, 255}, 15, 60.0f, 140.0f, 4.0f, 0.7f);
                    continue;
                }

                // --- Lógica normal de jogo (fora do tutorial) ---
                // Recompensa centralizada (XP/score/contador/drop) idêntica à das
                // demais armas. A conclusão da onda é verificada no fim do
                // UpdateGameplay (vale para todas as armas).
                RegisterEnemyKill(game, &game->enemies[i]);
            }
            else
            {
                // Se o inimigo não morreu, entra em estado de ferimento (stun/flash)
                game->enemies[i].state = HURT;
                game->enemies[i].cooldownTimer = 0.25f; // flash/stun de 0.25s
            }
        }
        // O golpe em área também destroi os Núcleos de Infecção do escudo do chefe
        HitInfectionCores(game, game->player.position, hitRadius, 15 + danoBase);
    }
    else if (wpn == 2 || wpn == WEAPON_RIFLE_EVOLVED) {
        // Cadência (Fase 5): 0.28s — antes 0.15s permitia spray contínuo. Agora
        // o alcance (~1050px) e a cadência limitam a "limpeza" sem mirar.
        game->player.attackCooldown = (wpn == WEAPON_RIFLE_EVOLVED) ? 0.24f : 0.28f;
        PlayWeaponAttackSfx(wpn);
        game->screenShake = 0.1f;
        // Arma de projétil do Mundo atual: Rifle de Bacteriófagos (Mundo 1, bônus
        // vs. bactérias) ou Rifle de Vacina (Mundo 2, bônus vs. vírus).
        ProjectileType rt = (wpn == WEAPON_RIFLE_EVOLVED) ? PROJ_PLAYER_RIFLE_EVOLVED :
                            (game->currentWorld == WORLD_VIRUS) ? PROJ_PLAYER_VACCINE : PROJ_PLAYER_PHAGE;
        SpawnProjectile(game, game->player.position, worldMousePos, rt, 8 + danoBase);
    }
    else if (wpn == 3 || wpn == WEAPON_RNA_LAUNCHER) {
        game->player.attackCooldown = (wpn == WEAPON_RNA_LAUNCHER) ? 0.68f : 0.75f;
        PlayWeaponAttackSfx(wpn);
        Vector2 dir = Vector2Normalize(Vector2Subtract(worldMousePos, game->player.position));
        if (dir.x == 0.0f && dir.y == 0.0f) dir = (Vector2){ (float)game->player.facingDir, 0.0f };
        Vector2 minePos = (wpn == WEAPON_RNA_LAUNCHER)
            ? Vector2Add(game->player.position, Vector2Scale(dir, 310.0f))
            : Vector2Add(game->player.position, Vector2Scale(dir, 52.0f));
        if (wpn == WEAPON_RNA_LAUNCHER)
            PlantBioMineTimed(game, minePos, 58 + danoBase, RNA_LAUNCHER_FUSE);
        else
            PlantBioMine(game, minePos, 48 + danoBase);
        game->screenShake = 0.12f;
    }
    else if (wpn == 4 || wpn == WEAPON_BFG_EVOLVED) {
        game->player.attackCooldown = (wpn == WEAPON_BFG_EVOLVED) ? 5.8f : 5.0f;
        PlayWeaponAttackSfx(wpn);
        game->screenShake = 0.8f;
        SpawnProjectile(game, game->player.position, worldMousePos,
                        (wpn == WEAPON_BFG_EVOLVED) ? PROJ_PLAYER_BFG_EVOLVED : PROJ_PLAYER_BFG,
                        100 + danoBase);
    }
}

// ============================================================================
// DADOS DAS ARMAS (fonte única usada por HUD, Arsenal e Tutorial)
// ============================================================================
// Mundo atual para nomear as armas temáticas (definido por SetWeaponWorld).
static int s_weaponWorld = WORLD_BACTERIA;
void SetWeaponWorld(int world)
{
    s_weaponWorld = (world == WORLD_VIRUS) ? WORLD_VIRUS : WORLD_BACTERIA;
    SetWeaponModelWorld(s_weaponWorld); // legado: modelo melee do slot 1 já não depende do Mundo
}

WeaponInfo GetWeaponInfo(int weapon)
{
    bool virus = (s_weaponWorld == WORLD_VIRUS);
    switch (weapon)
    {
        case 2:
            // Arma de projétil temática por Mundo (mesmas estatísticas-base).
            if (virus) return (WeaponInfo){
                "Rifle de Vacina", "Doses de imunizacao em linha reta.",
                "+60% de dano contra VIRUS", "Imuniza: eficaz contra virus (vacinas treinam o sistema imune).",
                8, "Rapida (0.28s)", 0.28f, 2, 2, (Color){ 120, 200, 255, 255 }, 1050.0f };
            return (WeaponInfo){
                "Rifle de Bacteriofagos", "Dispara bacteriofagos em linha reta.",
                "+60% de dano contra BACTERIAS", "Bacteriofagos sao virus que infectam bacterias; otimo no Mundo 1.",
                8, "Rapida (0.28s)", 0.28f, 2, 2, (Color){ 120, 255, 160, 255 }, 1050.0f };
        case 3: return (WeaponInfo){
            "Desestabilizador de RNA", "Planta minas biológicas no chão.",
            "Mouse 2 detona todas as minas", "Controle tatico: prepare armadilhas e exploda hordas no momento certo.",
            48, "Tatica (0.75s)", 0.75f, 3, 3, (Color){ 90, 255, 180, 255 } };
        case 4: return (WeaponInfo){
            "BFG Imunologico", "Projetil pesado que ATRAVESSA inimigos.",
            "Perfurante (atravessa todos)", "Limpa fileiras inteiras; guarde para hordas e chefe.",
            100, "Muito lenta", 5.0f, 4, 4, (Color){ 120, 255, 160, 255 } };
        case WEAPON_RIFLE_EVOLVED:
            return (WeaponInfo){
                "Rifle Vetorial Replicante", "Dispara vetores instaveis que se duplicam no primeiro impacto.",
                "Primeiro acerto gera um projetil extra de mesmo dano", "Evolucao do slot 2: pressione 2 para alternar depois de 30 abates com o rifle.",
                10, "Muito rapida (0.24s)", 0.24f, 2, 2, (Color){ 255, 170, 90, 255 }, 1050.0f };
        case WEAPON_RNA_LAUNCHER:
            return (WeaponInfo){
                "Lanca-Minas de RNA", "Arremessa minas biologicas a distancia.",
                "Minas explodem em 6s ou com Mouse 2", "Evolucao do slot 3: cobre area sem precisar plantar no proprio pe.",
                52, "Tatica (0.68s)", 0.68f, 3, 3, (Color){ 210, 120, 255, 255 } };
        case WEAPON_BFG_EVOLVED:
            return (WeaponInfo){
                "BFG Imunologico Omega", "Projetil pesado que atravessa e detona no fim do trajeto.",
                "Explosao final de alto dano em area", "Evolucao do slot 4: limpa corredores e pune grupos no ponto de impacto final.",
                115, "Devastadora", 5.8f, 4, 4, (Color){ 255, 230, 90, 255 } };
        case WEAPON_BIOBLADE:
            // Arma melee desbloqueável (30 abates): a antiga "Escalpelizador
            // Estatico" promovida a arma própria, disponível na campanha inteira.
            // Descarrega corrente no capsídeo -> quebra escudos em qualquer Mundo.
            return (WeaponInfo){
                "Lamina Bioeletrica", "Golpe em 360 graus que descarrega corrente no capsideo.",
                "Quebra ESCUDOS (capsideo) muito mais rapido", "Desbloqueia com 30 abates; depois alterne no slot 1.",
                15, "Combo rapido", 0.24f, 1, 1, (Color){ 180, 120, 255, 255 } };
        case 1:
        default:
            // Arma corpo a corpo inicial — sempre disponível, em qualquer Mundo.
            return (WeaponInfo){
                "Espada-Seringa", "Combo corpo a corpo com estocada e corte.",
                "Alterna alcance frontal e corte lateral", "Defesa pessoal; depois de 30 abates, o slot 1 alterna para a Lamina.",
                15, "Combo rapido", 0.24f, 1, 1, (Color){ 0, 229, 255, 255 } };
    }
}

// Desbloqueio unificado (troca em jogo + arsenal). Armas 1-4 seguem a progressão
// de nível (maxWeaponUnlocked); a Lâmina Bioelétrica (5) abre por ABATES.
bool WeaponUnlocked(GameState *game, int weapon)
{
    if (game->adminMode && game->adminUnlockWeapons)
        return (weapon >= 1 && weapon <= WEAPON_COUNT);
    int slot = WeaponSlotForId(weapon);
    if (WeaponIsEvolution(weapon))
        return WeaponKillCountForSlot(game, slot) >= WEAPON_EVOLVE_KILLS;
    return (weapon >= 1 && weapon <= game->maxWeaponUnlocked);
}

int WeaponUnlockLevel(int weapon)
{
    return GetWeaponInfo(weapon).unlockLevel;
}

const char *WeaponName(int weapon)
{
    return GetWeaponInfo(weapon).name;
}

int WeaponSlotForId(int weapon)
{
    if (weapon == WEAPON_BIOBLADE) return 1;
    if (weapon == WEAPON_RIFLE_EVOLVED) return 2;
    if (weapon == WEAPON_RNA_LAUNCHER) return 3;
    if (weapon == WEAPON_BFG_EVOLVED) return 4;
    if (weapon < 1) return 1;
    if (weapon > WEAPON_SLOT_COUNT) return WEAPON_SLOT_COUNT;
    return weapon;
}

int WeaponEvolutionForSlot(int slot)
{
    switch (slot)
    {
        case 1: return WEAPON_BIOBLADE;
        case 2: return WEAPON_RIFLE_EVOLVED;
        case 3: return WEAPON_RNA_LAUNCHER;
        case 4: return WEAPON_BFG_EVOLVED;
        default: return 1;
    }
}

int WeaponBaseForSlot(int slot)
{
    if (slot < 1) return 1;
    if (slot > WEAPON_SLOT_COUNT) return WEAPON_SLOT_COUNT;
    return slot;
}

bool WeaponIsEvolution(int weapon)
{
    return weapon == WEAPON_BIOBLADE || weapon == WEAPON_RIFLE_EVOLVED ||
           weapon == WEAPON_RNA_LAUNCHER || weapon == WEAPON_BFG_EVOLVED;
}

int WeaponKillCountForSlot(GameState *game, int slot)
{
    if (!game) return 0;
    if (slot < 1 || slot > WEAPON_SLOT_COUNT) return 0;
    int kills = game->weaponKills[slot - 1];
    if (slot == 1 && kills <= 0 && game->totalEnemiesKilled > 0)
        kills = game->totalEnemiesKilled;
    return kills;
}

int WeaponKillsToEvolve(GameState *game, int slot)
{
    int left = WEAPON_EVOLVE_KILLS - WeaponKillCountForSlot(game, slot);
    return (left > 0) ? left : 0;
}
