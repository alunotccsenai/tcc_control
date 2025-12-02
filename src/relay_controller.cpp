/**
 * @file relay_controller.cpp
 * @brief Implementação do controlador de relé
 * @version 1.1.0
 * @date 2025-11-28
 * 
 * ATUALIZADO: GPIO20 → GPIO19 conforme pinagem v5.1.0
 */

#include "relay_controller.h"

// ═══════════════════════════════════════════════════════════════════════
// CONSTRUTOR
// ═══════════════════════════════════════════════════════════════════════

RelayController::RelayController() 
    : unlocked(false), 
      temporaryUnlock(false),
      unlockStartTime(0),
      unlockDuration(0) {
}

// ═══════════════════════════════════════════════════════════════════════
// INICIALIZAÇÃO
// ═══════════════════════════════════════════════════════════════════════

void RelayController::begin() {
    Serial.println("🔧 [RelayController] Inicializando...");
    
    // Configurar GPIO como saída
    pinMode(RELAY_PIN, OUTPUT);
    
    // Garantir que começa trancada
    lock();
    
    Serial.printf("✅ [RelayController] GPIO%d configurado (Porta TRANCADA)\n", RELAY_PIN);
}

// ═══════════════════════════════════════════════════════════════════════
// CONTROLE DO RELÉ
// ═══════════════════════════════════════════════════════════════════════

void RelayController::unlock(uint32_t duration) {
    Serial.printf("🔓 [RelayController] Destrancando porta (%dms)\n", duration);
    
    activateRelay();
    
    unlocked = true;
    temporaryUnlock = true;
    unlockStartTime = millis();
    unlockDuration = duration;
}

void RelayController::unlockPermanent() {
    Serial.println("🔓 [RelayController] Destrancando porta (PERMANENTE)");
    
    activateRelay();
    
    unlocked = true;
    temporaryUnlock = false;
}

void RelayController::lock() {
    Serial.println("🔒 [RelayController] Trancando porta");
    
    deactivateRelay();
    
    unlocked = false;
    temporaryUnlock = false;
}

bool RelayController::isUnlocked() {
    return unlocked;
}

// ═══════════════════════════════════════════════════════════════════════
// UPDATE (LOOP)
// ═══════════════════════════════════════════════════════════════════════

void RelayController::update() {
    // Verificar se é destravamento temporizado
    if (temporaryUnlock && unlocked) {
        // Verificar se tempo expirou
        if (millis() - unlockStartTime >= unlockDuration) {
            Serial.println("⏱️  [RelayController] Timer expirado - Trancando porta");
            lock();
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// LOGGING
// ═══════════════════════════════════════════════════════════════════════

void RelayController::logAccess(const char* method, const char* user) {
    Serial.printf("📝 [RelayController] Acesso: %s | Usuário: %s\n", method, user);
}

// ═══════════════════════════════════════════════════════════════════════
// HELPERS PRIVADOS
// ═══════════════════════════════════════════════════════════════════════

void RelayController::activateRelay() {
    if (RELAY_ACTIVE_HIGH) {
        digitalWrite(RELAY_PIN, HIGH);  // HIGH = destravar
        Serial.printf("  ⚡ GPIO%d = HIGH (Relé ATIVADO)\n", RELAY_PIN);
    } else {
        digitalWrite(RELAY_PIN, LOW);   // LOW = destravar
        Serial.printf("  ⚡ GPIO%d = LOW (Relé ATIVADO)\n", RELAY_PIN);
    }
}

void RelayController::deactivateRelay() {
    if (RELAY_ACTIVE_HIGH) {
        digitalWrite(RELAY_PIN, LOW);   // LOW = trancar
        Serial.printf("  ⚡ GPIO%d = LOW (Relé DESATIVADO)\n", RELAY_PIN);
    } else {
        digitalWrite(RELAY_PIN, HIGH);  // HIGH = trancar
        Serial.printf("  ⚡ GPIO%d = HIGH (Relé DESATIVADO)\n", RELAY_PIN);
    }
}
