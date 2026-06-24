#ifndef PLAYER_H
#define PLAYER_H

#include "raylib.h"
#include <stdbool.h>

// Slots do guarda-roupa modular (cosméticos). Cada slot guarda o índice do item
// equipado; o índice 0 é sempre "nenhum/padrão". O catálogo (nomes, descrições,
// desbloqueios) vive em Assets/@models/cosmetics.{h,c} — DATA-DRIVEN, sem
// condicionais espalhadas. memset(0) no GameState => tudo no padrão (back-compat
// com saves/config antigos).
typedef enum CosmeticSlot {
    COS_HELMET = 0, // Capacete
    COS_FACE,       // Máscara / acessório facial
    COS_CHEST,      // Peitoral
    COS_ARMS,       // Braços / luvas
    COS_LEGS,       // Calças
    COS_BOOTS,      // Botas
    COS_BACK,       // Acessório traseiro
    COS_FX,         // Efeito visual
    COS_SLOT_COUNT
} CosmeticSlot;

typedef struct Player
{
    char name[16];        // Nome do jogador
    Vector2 position;
    float speed;
    int hp;
    int maxHp;
    int score;
    int level;
    int xp;
    int xpNeeded;
    int attackPower;
    int susPoints; // Pontos para upgrades (Pontos do SUS)
    
    // Timers de Buffs e Debuffs (ativos se > 0)
    float speedTimer;
    float shieldTimer;
    float attackBoostTimer;
    float poisonTimer; // Dano ao longo do tempo
    float slowTimer;   // Redução de velocidade

    // ---- Buffs da expansão (itens dropáveis) ----
    float maskTimer;       // Máscara Hospitalar (Mundo 1): reduz dano recebido
    float distancingTimer; // Distanciamento Social (Mundo 1): aura que repele inimigos
    float regenTimer;      // Citocina de Estabilização: regeneração de vida ao longo do tempo
    float regenAccum;      // acumulador fracionário da regeneração (hp é int)
    float supremeTimer;    // Orbe Supremo: velocidade/dano/escudo reforçados por pouco tempo
    
    // Combate
    float attackCooldown;  // Tempo até o próximo ataque
    float damageCooldown;  // I-frames: invencibilidade após tomar dano (0.5s)
    
    // Animação
    int facingDir; // 1 para Direita, -1 para Esquerda
    bool isMoving;
    float squashX;
    float squashY;
    Vector2 trail[10];
    int trailIndex;
    
    // Armas
    int equippedWeapon; // 1 = Lâmina, 2 = Fuzil, 3 = Granada, 4 = Vacina BFG

    int healthPotions; // Poções de vida

    // Skins (visuais, sem efeito de balanceamento)
    int skinId;       // 0 = Padrão, 1 = Médica, 2 = Infectada (paleta/material)
    int weaponSkinId; // 0 = Padrão, 1 = Plasma, 2 = Tóxica

    // Guarda-roupa modular: item equipado por slot (0 = nenhum/padrão).
    int cosmetics[COS_SLOT_COUNT];

} Player;

// Forward declaration of GameState
struct GameState;

// Player logic
void PlayerAttack(struct GameState *game, Vector2 worldMousePos);

#endif // PLAYER_H
