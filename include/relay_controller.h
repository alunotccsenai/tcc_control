/**
 * @file relay_controller.h
 * @brief Controlador de relé para porta/fechadura
 * @version 1.1.0
 * @date 2025-11-28
 * 
 * Gerencia acionamento do relé GPIO19 (v5.1.0) para controle de acesso.
 * Suporta destravamento temporizado e permanente.
 * 
 * ATUALIZADO: GPIO20 → GPIO19 conforme pinagem v5.1.0
 */

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "pins.h"

// ═══════════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES
// ═══════════════════════════════════════════════════════════════════════

#define RELAY_DEFAULT_UNLOCK_TIME   5000    // 5 segundos (ms)
#define RELAY_ACTIVE_HIGH           true    // true=HIGH destrava, false=LOW destrava

// ═══════════════════════════════════════════════════════════════════════
// CLASSE RELAYCONTROLLER
// ═══════════════════════════════════════════════════════════════════════

class RelayController {
public:
    /**
     * @brief Construtor
     */
    RelayController();
    
    /**
     * @brief Inicializa GPIO do relé
     */
    void begin();
    
    /**
     * @brief Destravar porta (temporizado)
     * @param duration Duração em ms (padrão: 5000ms)
     */
    void unlock(uint32_t duration = RELAY_DEFAULT_UNLOCK_TIME);
    
    /**
     * @brief Destravar permanentemente
     */
    void unlockPermanent();
    
    /**
     * @brief Travar porta
     */
    void lock();
    
    /**
     * @brief Verifica se porta está destrancada
     * @return true se destrancada
     */
    bool isUnlocked();
    
    /**
     * @brief Atualizar estado (chamar no loop)
     * Gerencia destravamento temporizado
     */
    void update();
    
    /**
     * @brief Log de acionamento
     * @param method Método de autenticação (ex: "PIN", "RFID", "BIO")
     * @param user Nome do usuário
     */
    void logAccess(const char* method, const char* user);

private:
    bool unlocked;
    bool temporaryUnlock;
    uint32_t unlockStartTime;
    uint32_t unlockDuration;
    
    /**
     * @brief Ativa relé (porta abre)
     */
    void activateRelay();
    
    /**
     * @brief Desativa relé (porta fecha)
     */
    void deactivateRelay();
};

#endif // RELAY_CONTROLLER_H
