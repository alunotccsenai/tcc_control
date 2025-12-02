/**
 * @file rfid_handlers_simple.cpp
 * @brief Implementação dos handlers HTTP para RFID
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * NOTA: Este arquivo é um STUB que será ativado quando
 * o webserver AsyncWebServer for integrado ao projeto.
 * 
 * Por enquanto, contém apenas placeholders para compilação.
 */

#include "rfid_handlers_simple.h"

// ═══════════════════════════════════════════════════════════════════════
// NOTA: IMPLEMENTAÇÃO STUB
// ═══════════════════════════════════════════════════════════════════════
// 
// Estas funções serão implementadas quando:
// 1. AsyncWebServer estiver integrado ao main.cpp
// 2. RFIDStorage estiver funcional
// 3. PN532 estiver conectado e testado
//
// Por enquanto, apenas compilam sem erro.
// ═══════════════════════════════════════════════════════════════════════

void setupRFIDEndpointsSimple(AsyncWebServer& server) {
    // Stub - será implementado quando AsyncWebServer estiver ativo
    Serial.println("⚠️  [RFID Handlers] STUB - Aguardando integração com AsyncWebServer");
}

void handleRFIDStatus(AsyncWebServerRequest *request) {
    // Stub
}

void handleRFIDList(AsyncWebServerRequest *request) {
    // Stub
}

void handleRFIDStats(AsyncWebServerRequest *request) {
    // Stub
}

void handleRFIDRegister(AsyncWebServerRequest *request, uint8_t *data, size_t len) {
    // Stub
}

void handleRFIDDelete(AsyncWebServerRequest *request) {
    // Stub
}

void sendRFIDError(AsyncWebServerRequest *request, const char* message, int code) {
    // Stub
}

void sendRFIDSuccess(AsyncWebServerRequest *request, const char* message) {
    // Stub
}
