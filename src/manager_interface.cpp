/**
 * @file manager_interface.cpp
 * @brief Implementação da interface para os managers
 * @version 1.0.0
 * @date 2025-11-27
 */

#include "manager_interface.h"
#include "rfid_manager.h"
#include "biometric_manager.h"

// Instâncias externas (definidas no main.cpp)
extern RFIDManager rfidManager;
extern BiometricManager bioManager;

// ═══════════════════════════════════════════════════════════════════════
// IMPLEMENTAÇÕES RFID
// ═══════════════════════════════════════════════════════════════════════

bool rfidHardwareConnected() {
    return rfidManager.isHardwareConnected();
}

// ═══════════════════════════════════════════════════════════════════════
// IMPLEMENTAÇÕES BIOMÉTRICAS
// ═══════════════════════════════════════════════════════════════════════

bool bioHardwareConnected() {
    return bioManager.isHardwareConnected();
}

int bioSensorTemplateCount() {
    return bioManager.getSensorTemplateCount();
}
