/**
 * @file manager_interface.h
 * @brief Interface para os managers RFID e Biométrico (evita conflitos de estruturas)
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * Este header fornece acesso aos métodos dos managers sem incluir suas definições
 * de estruturas, evitando conflitos com os headers de storage.
 */

#ifndef MANAGER_INTERFACE_H
#define MANAGER_INTERFACE_H

#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════
// INTERFACE RFID MANAGER
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Verifica se o hardware RFID (PN532) está conectado
 * @return true se conectado
 */
bool rfidHardwareConnected();

// ═══════════════════════════════════════════════════════════════════════
// INTERFACE BIOMETRIC MANAGER
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Verifica se o hardware biométrico (AS608) está conectado
 * @return true se conectado
 */
bool bioHardwareConnected();

/**
 * @brief Retorna quantidade de templates no sensor AS608
 * @return Número de templates (0-127)
 */
int bioSensorTemplateCount();

#endif // MANAGER_INTERFACE_H
