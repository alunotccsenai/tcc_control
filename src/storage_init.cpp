/**
 * @file storage_init.cpp
 * @brief Inicialização dos sistemas de armazenamento (isolado para evitar conflitos)
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * Este arquivo contém apenas as inicializações dos storages RFID e Biométrico.
 * Está separado do main.cpp para evitar conflitos de definição de estruturas
 * entre rfid_storage.h (String-based RFIDCard) e rfid_manager.h (array-based RFIDCard).
 */

#include <Arduino.h>
#include <rfid_storage.h>
#include <biometric_storage.h>

// ═══════════════════════════════════════════════════════════════════════
// Instâncias globais dos storages (definidas AQUI para evitar conflitos)
// ═══════════════════════════════════════════════════════════════════════
RFIDStorage rfidStorage;         // Storage persistente RFID (LittleFS)
BiometricStorage bioStorage;     // Storage persistente Biometria (LittleFS)

/**
 * @brief Inicializa o storage RFID
 * @return true se inicializado com sucesso
 */
bool initRfidStorage() {
    if (rfidStorage.begin()) {
        Serial.printf("✅ RFID Storage: %d cartão(s) cadastrado(s)\n", rfidStorage.count());
        return true;
    } else {
        Serial.println("⚠️ RFID Storage não disponível");
        return false;
    }
}

/**
 * @brief Inicializa o storage biométrico
 * @return true se inicializado com sucesso
 */
bool initBioStorage() {
    if (bioStorage.begin()) {
        Serial.printf("✅ Biometric Storage: %d usuário(s) cadastrado(s)\n", bioStorage.count());
        return true;
    } else {
        Serial.println("⚠️ Biometric Storage não disponível");
        return false;
    }
}
