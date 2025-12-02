/**
 * @file rfid_handlers_simple.h
 * @brief Handlers HTTP simplificados para endpoints RFID
 * @version 1.0.0 (simplificado para evitar stack overflow)
 * @date 2025-11-27
 * 
 * Endpoints REST profissionais para gerenciamento de cartões RFID/NFC.
 * Versão otimizada com funções separadas para reduzir uso de stack.
 */

#ifndef RFID_HANDLERS_SIMPLE_H
#define RFID_HANDLERS_SIMPLE_H

#include <Arduino.h>

// Forward declaration (será definido onde este header for incluído)
class AsyncWebServer;
class AsyncWebServerRequest;
class RFIDStorage;
class Adafruit_PN532;

// ═══════════════════════════════════════════════════════════════════════
// CONFIGURAÇÃO DOS ENDPOINTS
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Configura todos os endpoints RFID no servidor
 * @param server Instância do AsyncWebServer
 * 
 * Endpoints criados:
 * - GET    /api/rfid/status      - Status do PN532
 * - POST   /api/rfid/register    - Cadastrar cartão
 * - DELETE /api/rfid/delete/:uid - Remover cartão
 * - GET    /api/rfid/list        - Listar cartões
 * - GET    /api/rfid/stats       - Estatísticas
 */
void setupRFIDEndpointsSimple(AsyncWebServer& server);

// ═══════════════════════════════════════════════════════════════════════
// HANDLERS INDIVIDUAIS (funções separadas para reduzir stack)
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief GET /api/rfid/status
 * Retorna status do módulo PN532 e quantidade de cartões
 */
void handleRFIDStatus(AsyncWebServerRequest *request);

/**
 * @brief GET /api/rfid/list
 * Lista todos os cartões cadastrados com metadados
 */
void handleRFIDList(AsyncWebServerRequest *request);

/**
 * @brief GET /api/rfid/stats
 * Estatísticas do sistema RFID
 */
void handleRFIDStats(AsyncWebServerRequest *request);

/**
 * @brief POST /api/rfid/register (body handler)
 * Cadastra novo cartão RFID
 */
void handleRFIDRegister(AsyncWebServerRequest *request, uint8_t *data, size_t len);

/**
 * @brief DELETE /api/rfid/delete/:uid
 * Remove cartão por UID
 */
void handleRFIDDelete(AsyncWebServerRequest *request);

// ═══════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Envia resposta JSON de erro
 * @param request Request HTTP
 * @param message Mensagem de erro
 * @param code Código HTTP (padrão: 400)
 */
void sendRFIDError(AsyncWebServerRequest *request, const char* message, int code = 400);

/**
 * @brief Envia resposta JSON de sucesso
 * @param request Request HTTP
 * @param message Mensagem de sucesso
 */
void sendRFIDSuccess(AsyncWebServerRequest *request, const char* message);

#endif // RFID_HANDLERS_SIMPLE_H
