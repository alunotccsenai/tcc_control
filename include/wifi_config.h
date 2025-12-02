/**
 * @file wifi_config.h
 * @brief Configurações e estruturas para Wi-Fi e API REST
 * @date 22/11/2025
 * @version 1.0
 * 
 * Sistema completo de gerenciamento Wi-Fi para ESP32
 * - Modo Station (conectar a redes existentes)
 * - Modo Access Point (portal de configuração)
 * - API REST com 4 endpoints
 * - Persistência de credenciais em NVS
 * - Reconexão automática
 * - mDNS para acesso por nome
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

/* ============================================================================
 * VARIÁVEIS GLOBAIS WI-FI
 * ========================================================================== */

// Servidor Web
extern WebServer server;

// Armazenamento persistente
extern Preferences wifiPrefs;

// Status da conexão
extern bool wifiConnected;
extern bool wifiAPMode;
extern String currentSSID;
extern String currentPassword;
extern unsigned long lastWiFiCheck;

/* ============================================================================
 * ESTRUTURAS DE DADOS
 * ========================================================================== */

/**
 * @brief Informações de rede Wi-Fi escaneada
 */
struct WiFiNetworkInfo {
    String ssid;            // Nome da rede
    int32_t rssi;           // Intensidade do sinal (dBm)
    uint8_t encryption;     // Tipo de criptografia
    int32_t channel;        // Canal Wi-Fi
    String bssid;           // MAC address do AP
};

/**
 * @brief Status da conexão Wi-Fi atual
 */
struct WiFiStatusInfo {
    bool connected;         // Se está conectado
    String ssid;            // Nome da rede conectada
    String ip;              // Endereço IP
    String mac;             // MAC address
    int32_t rssi;           // Intensidade do sinal
    String gateway;         // Gateway padrão
    String dns;             // Servidor DNS
};

/* ============================================================================
 * FUNÇÕES WI-FI - DECLARAÇÕES
 * ========================================================================== */

/**
 * @brief Inicializa sistema Wi-Fi
 * 
 * Carrega credenciais salvas, tenta conectar ou inicia modo AP
 * Configura servidor web e API REST
 */
void setupWiFi();

/**
 * @brief Tenta conectar a uma rede Wi-Fi
 * @param ssid Nome da rede
 * @param password Senha da rede
 * @return true se conectou com sucesso
 */
bool connectToWiFi(const String& ssid, const String& password);

/**
 * @brief Inicia modo Access Point
 * 
 * Cria rede Wi-Fi própria para configuração
 * Portal web acessível em 192.168.4.1
 */
void startAPMode();

/**
 * @brief Verifica status da conexão e reconecta se necessário
 * 
 * Deve ser chamado no loop() principal
 * Reconecta automaticamente se perder conexão
 */
void checkWiFiConnection();

/**
 * @brief Carrega credenciais salvas da memória NVS
 * @return true se encontrou credenciais salvas
 */
bool loadSavedCredentials();

/**
 * @brief Salva credenciais na memória persistente (NVS)
 * @param ssid Nome da rede
 * @param password Senha da rede
 */
void saveCredentials(const String& ssid, const String& password);

/**
 * @brief Remove credenciais salvas da memória
 */
void clearSavedCredentials();

/**
 * @brief Obtém tipo de criptografia como string
 * @param encryptionType Tipo de criptografia (enum)
 * @return String com nome da criptografia (open, wep, wpa, wpa2, etc)
 */
String getEncryptionType(uint8_t encryptionType);

/* ============================================================================
 * API REST - DECLARAÇÕES
 * ========================================================================== */

/**
 * @brief Configura rotas da API REST
 * 
 * Endpoints:
 * - GET  /api/wifi/scan       - Escaneia redes disponíveis
 * - POST /api/wifi/connect    - Conecta a uma rede
 * - GET  /api/wifi/status     - Retorna status da conexão
 * - POST /api/wifi/disconnect - Desconecta da rede
 * - GET  /                    - Portal de configuração HTML
 */
void setupAPIRoutes();

/**
 * @brief Verifica autenticação via API Key
 * @return true se autenticado (header X-API-Key válido)
 */
bool checkAPIKey();

/**
 * @brief Envia headers CORS para requisições OPTIONS
 */
void handleCORS();

/* ============================================================================
 * HANDLERS DA API - DECLARAÇÕES
 * ========================================================================== */

/**
 * @brief GET /api/wifi/scan - Escaneia redes Wi-Fi disponíveis
 * 
 * Retorna JSON com array de redes encontradas
 * Exemplo de resposta:
 * {
 *   "networks": [
 *     {
 *       "ssid": "MinhaRede",
 *       "rssi": -45,
 *       "encryption": "wpa2",
 *       "channel": 6,
 *       "bssid": "AA:BB:CC:DD:EE:FF"
 *     }
 *   ]
 * }
 */
void handleWiFiScan();

/**
 * @brief POST /api/wifi/connect - Conecta a uma rede Wi-Fi
 * 
 * Body JSON:
 * {
 *   "ssid": "MinhaRede",
 *   "password": "minhaSenha"
 * }
 * 
 * Resposta JSON:
 * {
 *   "success": true,
 *   "message": "Conectado com sucesso",
 *   "ip": "192.168.1.100",
 *   "rssi": -45
 * }
 */
void handleWiFiConnect();

/**
 * @brief GET /api/wifi/status - Retorna status da conexão atual
 * 
 * Resposta JSON:
 * {
 *   "connected": true,
 *   "ssid": "MinhaRede",
 *   "ip": "192.168.1.100",
 *   "mac": "AA:BB:CC:DD:EE:FF",
 *   "rssi": -45,
 *   "gateway": "192.168.1.1",
 *   "dns": "8.8.8.8"
 * }
 */
void handleWiFiStatus();

/**
 * @brief POST /api/wifi/disconnect - Desconecta da rede atual
 * 
 * Remove credenciais salvas e inicia modo AP
 * 
 * Resposta JSON:
 * {
 *   "success": true
 * }
 */
void handleWiFiDisconnect();

/**
 * @brief GET / - Página inicial (portal de configuração)
 * 
 * Retorna HTML completo do portal de configuração
 * Interface web para escanear e conectar a redes Wi-Fi
 */
void handleRoot();

/**
 * @brief 404 - Página não encontrada
 */
void handleNotFound();

#endif // WIFI_CONFIG_H
