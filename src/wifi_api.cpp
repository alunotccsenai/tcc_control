/**
 * @file wifi_api.cpp
 * @brief Implementação da API REST para gerenciamento Wi-Fi
 * @date 22/11/2025
 * @version 1.0
 * 
 * Sistema completo de gerenciamento Wi-Fi:
 * - Conexão a redes Wi-Fi existentes
 * - Modo Access Point para configuração
 * - API REST com 4 endpoints
 * - Portal web de configuração
 * - Persistência de credenciais
 * - Reconexão automática
 */

#include "config.h"
#include "wifi_config.h"

#if WIFI_ENABLED && WIFI_MDNS_ENABLED
#include <ESPmDNS.h>
#endif

/* ============================================================================
 * VARIÁVEIS GLOBAIS
 * ========================================================================== */

WebServer server(WEBSERVER_PORT);
Preferences wifiPrefs;

bool wifiConnected = false;
bool wifiAPMode = false;
String currentSSID = "";
String currentPassword = "";
unsigned long lastWiFiCheck = 0;

/* ============================================================================
 * INICIALIZAÇÃO WI-FI
 * ========================================================================== */

void setupWiFi() {
    Serial.println("\n[WiFi] Inicializando sistema Wi-Fi...");
    
    // Configurar hostname
    WiFi.setHostname(WIFI_HOSTNAME);
    
    // Tentar carregar credenciais salvas
    bool hasSavedCredentials = loadSavedCredentials();
    
    if (hasSavedCredentials && currentSSID.length() > 0) {
        Serial.println("[WiFi] Credenciais encontradas. Tentando conectar...");
        if (connectToWiFi(currentSSID, currentPassword)) {
            wifiConnected = true;
            Serial.println("[WiFi] ✓ Conectado à rede salva!");
        } else {
            Serial.println("[WiFi] ✗ Falha ao conectar. Iniciando modo AP...");
            startAPMode();
        }
    } else {
        Serial.println("[WiFi] Nenhuma credencial salva. Iniciando modo AP...");
        startAPMode();
    }
    
    // Configurar rotas da API
    setupAPIRoutes();
    
    // Iniciar servidor web
    server.begin();
    Serial.printf("[WiFi] Servidor HTTP iniciado na porta %d\n", WEBSERVER_PORT);
    
    #if WIFI_MDNS_ENABLED
    if (MDNS.begin(WIFI_MDNS_NAME)) {
        Serial.printf("[WiFi] mDNS iniciado: http://%s.local\n", WIFI_MDNS_NAME);
        MDNS.addService("http", "tcp", WEBSERVER_PORT);
    }
    #endif
}

/* ============================================================================
 * CONEXÃO WI-FI
 * ========================================================================== */

bool connectToWiFi(const String& ssid, const String& password) {
    Serial.printf("[WiFi] Conectando a: %s\n", ssid.c_str());
    
    // Configurar modo Station
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    // Desabilitar power saving para melhor performance
    WiFi.setSleep(false);
    
    // Iniciar conexão
    WiFi.begin(ssid.c_str(), password.c_str());
    
    // Aguardar conexão com timeout
    int attempts = 0;
    int maxAttempts = WIFI_CONNECT_TIMEOUT * 2; // 2 checks por segundo
    
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        delay(500);
        Serial.print(".");
        attempts++;
        
        // A cada 10 tentativas, verificar se deve abortar
        if (attempts % 10 == 0) {
            Serial.printf("\n[WiFi] Tentativa %d/%d...\n", attempts/2, WIFI_CONNECT_TIMEOUT);
        }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        wifiAPMode = false;
        currentSSID = ssid;
        currentPassword = password;
        
        Serial.println("\n[WiFi] ✓ Conectado com sucesso!");
        Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());
        Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());
        
        // Salvar credenciais
        saveCredentials(ssid, password);
        
        return true;
    } else {
        Serial.println("\n[WiFi] ✗ Falha na conexão!");
        Serial.printf("[WiFi] Status: %d\n", WiFi.status());
        wifiConnected = false;
        return false;
    }
}

/* ============================================================================
 * MODO ACCESS POINT
 * ========================================================================== */

void startAPMode() {
    Serial.println("[WiFi] Iniciando modo Access Point...");
    
    // Desconectar de qualquer rede
    WiFi.disconnect();
    delay(100);
    
    // Configurar modo AP
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Configurar IP estático (definido em config.h)
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);
    
    // Iniciar AP
    bool apStarted = WiFi.softAP(
        WIFI_AP_SSID,
        WIFI_AP_PASSWORD,
        WIFI_AP_CHANNEL,
        WIFI_AP_HIDDEN,
        WIFI_AP_MAX_CLIENTS
    );
    
    if (apStarted) {
        wifiAPMode = true;
        wifiConnected = false;
        
        Serial.println("[WiFi] ✓ Access Point iniciado!");
        Serial.printf("[WiFi] SSID: %s\n", WIFI_AP_SSID);
        Serial.printf("[WiFi] Senha: %s\n", WIFI_AP_PASSWORD);
        Serial.printf("[WiFi] IP: %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("[WiFi] Acesse: http://%s\n", WiFi.softAPIP().toString().c_str());
    } else {
        Serial.println("[WiFi] ✗ Erro ao iniciar Access Point!");
    }
}

/* ============================================================================
 * VERIFICAÇÃO DE CONEXÃO
 * ========================================================================== */

void checkWiFiConnection() {
    // Verificar apenas a cada WIFI_CHECK_INTERVAL
    if (millis() - lastWiFiCheck < WIFI_CHECK_INTERVAL) {
        return;
    }
    
    lastWiFiCheck = millis();
    
    // Se está em modo AP, não fazer nada
    if (wifiAPMode) {
        return;
    }
    
    // Verificar status
    if (WiFi.status() != WL_CONNECTED && wifiConnected) {
        Serial.println("[WiFi] ⚠ Conexão perdida!");
        wifiConnected = false;
        
        #if WIFI_AUTO_CONNECT
        if (currentSSID.length() > 0) {
            Serial.println("[WiFi] Tentando reconectar...");
            WiFi.reconnect();
            delay(WIFI_RETRY_DELAY);
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[WiFi] ✓ Reconectado!");
                wifiConnected = true;
            } else {
                Serial.println("[WiFi] ✗ Falha ao reconectar. Iniciando modo AP...");
                startAPMode();
            }
        }
        #endif
    } else if (WiFi.status() == WL_CONNECTED && !wifiConnected) {
        wifiConnected = true;
        Serial.println("[WiFi] ✓ Conexão restabelecida!");
    }
}

/* ============================================================================
 * PERSISTÊNCIA DE CREDENCIAIS
 * ========================================================================== */

bool loadSavedCredentials() {
    wifiPrefs.begin("wifi", true); // Read-only
    
    currentSSID = wifiPrefs.getString("ssid", "");
    currentPassword = wifiPrefs.getString("password", "");
    
    wifiPrefs.end();
    
    if (currentSSID.length() > 0) {
        Serial.printf("[WiFi] Credenciais carregadas: SSID='%s'\n", currentSSID.c_str());
        return true;
    }
    
    Serial.println("[WiFi] Nenhuma credencial salva encontrada.");
    return false;
}

void saveCredentials(const String& ssid, const String& password) {
    wifiPrefs.begin("wifi", false); // Read-write
    
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("password", password);
    
    wifiPrefs.end();
    
    Serial.printf("[WiFi] Credenciais salvas: SSID='%s'\n", ssid.c_str());
}

void clearSavedCredentials() {
    wifiPrefs.begin("wifi", false);
    
    wifiPrefs.remove("ssid");
    wifiPrefs.remove("password");
    
    wifiPrefs.end();
    
    currentSSID = "";
    currentPassword = "";
    
    Serial.println("[WiFi] Credenciais removidas.");
}

/* ============================================================================
 * UTILITÁRIOS
 * ========================================================================== */

String getEncryptionType(uint8_t encryptionType) {
    switch (encryptionType) {
        case WIFI_AUTH_OPEN:
            return "open";
        case WIFI_AUTH_WEP:
            return "wep";
        case WIFI_AUTH_WPA_PSK:
            return "wpa";
        case WIFI_AUTH_WPA2_PSK:
            return "wpa2";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "wpa/wpa2";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "wpa2-enterprise";
        case WIFI_AUTH_WPA3_PSK:
            return "wpa3";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "wpa2/wpa3";
        default:
            return "unknown";
    }
}

/* ============================================================================
 * CONFIGURAÇÃO DE ROTAS API
 * ========================================================================== */

void setupAPIRoutes() {
    // ⚠️ IMPORTANTE: Configurar WebServer para aceitar headers customizados
    const char* headerKeys[] = {"X-API-Key"};
    size_t headerKeysSize = sizeof(headerKeys) / sizeof(char*);
    server.collectHeaders(headerKeys, headerKeysSize);
    
    Serial.println("[API] Headers customizados configurados: X-API-Key");
    
    // Preflight CORS (OPTIONS)
    server.on("/api/wifi/scan", HTTP_OPTIONS, handleCORS);
    server.on("/api/wifi/connect", HTTP_OPTIONS, handleCORS);
    server.on("/api/wifi/status", HTTP_OPTIONS, handleCORS);
    server.on("/api/wifi/disconnect", HTTP_OPTIONS, handleCORS);
    
    // Rotas da API
    server.on("/api/wifi/scan", HTTP_GET, handleWiFiScan);
    server.on("/api/wifi/connect", HTTP_POST, handleWiFiConnect);
    server.on("/api/wifi/status", HTTP_GET, handleWiFiStatus);
    server.on("/api/wifi/disconnect", HTTP_POST, handleWiFiDisconnect);
    
    // Página principal
    server.on("/", HTTP_GET, handleRoot);
    
    // 404
    server.onNotFound(handleNotFound);
    
    Serial.println("[API] Rotas configuradas:");
    Serial.println("  GET  /api/wifi/scan");
    Serial.println("  POST /api/wifi/connect");
    Serial.println("  GET  /api/wifi/status");
    Serial.println("  POST /api/wifi/disconnect");
}

/* ============================================================================
 * AUTENTICAÇÃO E CORS
 * ========================================================================== */

bool checkAPIKey() {
    Serial.println("[API] Verificando autenticação...");
    Serial.printf("[API] API_KEY esperada: '%s' (length=%d)\n", API_KEY, strlen(API_KEY));
    
    // Listar TODOS os headers recebidos para debug
    Serial.println("[API] Headers recebidos:");
    for (int i = 0; i < server.headers(); i++) {
        Serial.printf("  [%d] %s: %s\n", i, server.headerName(i).c_str(), server.header(i).c_str());
    }
    
    if (!server.hasHeader("X-API-Key")) {
        server.send(401, "application/json", "{\"error\":\"API Key obrigatória\"}");
        Serial.println("[API] ✗ Requisição sem API Key");
        Serial.println("[API] ✗ Header X-API-Key não encontrado!");
        return false;
    }
    
    String apiKey = server.header("X-API-Key");
    Serial.printf("[API] API_KEY recebida: '%s' (length=%d)\n", apiKey.c_str(), apiKey.length());
    
    if (apiKey != API_KEY) {
        server.send(403, "application/json", "{\"error\":\"API Key inválida\"}");
        Serial.println("[API] ✗ API Key inválida");
        Serial.printf("[API] ✗ Esperado: '%s'\n", API_KEY);
        Serial.printf("[API] ✗ Recebido: '%s'\n", apiKey.c_str());
        return false;
    }
    
    Serial.println("[API] ✓ API Key válida!");
    return true;
}

void handleCORS() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
    server.send(200);
}

/* ============================================================================
 * HANDLERS DA API
 * ========================================================================== */

void handleWiFiScan() {
    Serial.println("[API] GET /api/wifi/scan");
    
    // Verificar autenticação
    if (!checkAPIKey()) return;
    
    // Escanear redes
    Serial.println("[WiFi] Escaneando redes...");
    int networksFound = WiFi.scanNetworks();
    
    // Criar JSON de resposta
    StaticJsonDocument<4096> doc;
    JsonArray networks = doc.createNestedArray("networks");
    
    for (int i = 0; i < networksFound; i++) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["rssi"] = WiFi.RSSI(i);
        network["encryption"] = getEncryptionType(WiFi.encryptionType(i));
        network["channel"] = WiFi.channel(i);
        network["bssid"] = WiFi.BSSIDstr(i);
    }
    
    String response;
    serializeJson(doc, response);
    
    // Enviar resposta com CORS
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
    
    Serial.printf("[WiFi] %d redes encontradas\n", networksFound);
    
    // Limpar resultado do scan
    WiFi.scanDelete();
}

void handleWiFiConnect() {
    Serial.println("[API] POST /api/wifi/connect");
    
    // Verificar autenticação
    if (!checkAPIKey()) return;
    
    // Parsear JSON
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));
    
    if (error) {
        server.send(400, "application/json", "{\"error\":\"JSON inválido\"}");
        Serial.println("[API] ✗ JSON inválido");
        return;
    }
    
    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();
    
    Serial.printf("[WiFi] Tentando conectar: SSID='%s'\n", ssid.c_str());
    
    // Tentar conectar
    bool success = connectToWiFi(ssid, password);
    
    // Resposta
    StaticJsonDocument<256> response;
    response["success"] = success;
    response["message"] = success ? "Conectado com sucesso" : "Falha na conexão";
    
    if (success) {
        response["ip"] = WiFi.localIP().toString();
        response["rssi"] = WiFi.RSSI();
    }
    
    String responseStr;
    serializeJson(response, responseStr);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", responseStr);
}

void handleWiFiStatus() {
    Serial.println("[API] GET /api/wifi/status");
    
    // Verificar autenticação
    if (!checkAPIKey()) return;
    
    // Criar JSON de status
    StaticJsonDocument<512> doc;
    
    doc["connected"] = (WiFi.status() == WL_CONNECTED);
    doc["ap_mode"] = wifiAPMode;
    
    if (WiFi.status() == WL_CONNECTED) {
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["mac"] = WiFi.macAddress();
        doc["rssi"] = WiFi.RSSI();
        doc["gateway"] = WiFi.gatewayIP().toString();
        doc["dns"] = WiFi.dnsIP().toString();
    } else if (wifiAPMode) {
        doc["ap_ssid"] = WIFI_AP_SSID;
        doc["ap_ip"] = WiFi.softAPIP().toString();
        doc["ap_clients"] = WiFi.softAPgetStationNum();
    }
    
    String response;
    serializeJson(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleWiFiDisconnect() {
    Serial.println("[API] POST /api/wifi/disconnect");
    
    // Verificar autenticação
    if (!checkAPIKey()) return;
    
    // Desconectar
    WiFi.disconnect();
    wifiConnected = false;
    
    // Limpar credenciais salvas
    clearSavedCredentials();
    
    // Iniciar modo AP
    startAPMode();
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"success\":true}");
    
    Serial.println("[WiFi] Desconectado e modo AP iniciado");
}

void handleRoot() {
    String html = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <title>ESP32 - Controle de Acesso Wi-Fi</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body { 
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #0a0a0a 0%, #1a1a2e 100%);
      color: #fff;
      padding: 20px;
      min-height: 100vh;
    }
    .container { max-width: 600px; margin: 0 auto; }
    h1 { 
      font-size: 28px;
      margin-bottom: 8px;
      display: flex;
      align-items: center;
      gap: 12px;
    }
    .subtitle { 
      color: #06b6d4;
      font-size: 14px;
      margin-bottom: 30px;
      opacity: 0.9;
    }
    .card {
      background: rgba(26, 26, 46, 0.9);
      border: 1px solid #374151;
      border-radius: 12px;
      padding: 24px;
      margin-bottom: 20px;
      box-shadow: 0 4px 6px rgba(0, 0, 0, 0.3);
    }
    h3 {
      margin-bottom: 16px;
      font-size: 18px;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .network {
      background: rgba(10, 10, 10, 0.6);
      border: 2px solid #374151;
      border-radius: 8px;
      padding: 16px;
      margin: 12px 0;
      cursor: pointer;
      transition: all 0.2s ease;
    }
    .network:hover {
      background: rgba(42, 42, 62, 0.6);
      border-color: #06b6d4;
      transform: translateY(-2px);
    }
    .network.selected {
      border-color: #06b6d4;
      background: rgba(6, 182, 212, 0.15);
      box-shadow: 0 0 0 2px rgba(6, 182, 212, 0.2);
    }
    .network-name {
      font-weight: 600;
      font-size: 16px;
      margin-bottom: 6px;
    }
    .network-details {
      color: #9ca3af;
      font-size: 13px;
    }
    input, button {
      width: 100%;
      padding: 14px 16px;
      margin: 10px 0;
      border-radius: 8px;
      border: 1px solid #374151;
      font-size: 15px;
      transition: all 0.2s ease;
    }
    input {
      background: rgba(10, 10, 10, 0.6);
      color: #fff;
    }
    input:focus {
      outline: none;
      border-color: #06b6d4;
      box-shadow: 0 0 0 3px rgba(6, 182, 212, 0.1);
    }
    button {
      background: linear-gradient(135deg, #06b6d4 0%, #0891b2 100%);
      color: #fff;
      font-weight: 600;
      cursor: pointer;
      border: none;
    }
    button:hover:not(:disabled) { 
      background: linear-gradient(135deg, #0891b2 0%, #0e7490 100%);
      transform: translateY(-1px);
      box-shadow: 0 4px 12px rgba(6, 182, 212, 0.4);
    }
    button:active:not(:disabled) {
      transform: translateY(0);
    }
    button:disabled {
      background: #374151;
      cursor: not-allowed;
      opacity: 0.6;
    }
    .status {
      padding: 12px 16px;
      border-radius: 8px;
      margin: 12px 0;
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .status.success {
      background: rgba(34, 197, 94, 0.15);
      border: 1px solid #22c55e;
      color: #22c55e;
    }
    .status.error {
      background: rgba(239, 68, 68, 0.15);
      border: 1px solid #ef4444;
      color: #ef4444;
    }
    .status.info {
      background: rgba(59, 130, 246, 0.15);
      border: 1px solid #3b82f6;
      color: #3b82f6;
    }
    .loading {
      display: inline-block;
      width: 16px;
      height: 16px;
      border: 2px solid rgba(255,255,255,0.3);
      border-top-color: #fff;
      border-radius: 50%;
      animation: spin 0.6s linear infinite;
    }
    @keyframes spin {
      to { transform: rotate(360deg); }
    }
    .empty-state {
      text-align: center;
      padding: 40px 20px;
      color: #6b7280;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Controle de Acesso</h1>
    <div class="subtitle">Configuracao de Rede Wi-Fi</div>
    
    <div class="card">
      <h3>Redes Disponiveis</h3>
      <button onclick="scanNetworks()" id="scanBtn">
        Escanear Redes
      </button>
      <div id="networks"></div>
    </div>
    
    <div class="card">
      <h3>Conectar a Rede</h3>
      <input type="text" id="ssid" placeholder="Nome da Rede (SSID)" readonly>
      <input type="password" id="password" placeholder="Senha da Rede">
      <button onclick="connectWiFi()" id="connectBtn">
        Conectar a Rede
      </button>
      <div id="status"></div>
    </div>
  </div>
  
  <script>
    const API_KEY = ')HTMLPAGE" + String(API_KEY) + R"HTMLPAGE(';
    console.log('[Portal] API_KEY configurada:', API_KEY ? '✓ OK' : '✗ Faltando');
    console.log('[Portal] API_KEY valor:', API_KEY);
    console.log('[Portal] API_KEY length:', API_KEY.length);
    
    async function scanNetworks() {
      const btn = document.getElementById('scanBtn');
      const container = document.getElementById('networks');
      
      btn.disabled = true;
      btn.innerHTML = '<span class="loading"></span> Escaneando...';
      container.innerHTML = '';
      
      console.log('[Scan] Enviando requisicao com API_KEY:', API_KEY);
      
      try {
        const res = await fetch('/api/wifi/scan', {
          headers: { 'X-API-Key': API_KEY }
        });
        
        console.log('[Scan] Status:', res.status);
        console.log('[Scan] Headers enviados:', { 'X-API-Key': API_KEY });
        
        const data = await res.json();
        console.log('[Scan] Resposta:', data);
        
        if (data.networks && data.networks.length > 0) {
          data.networks.forEach(net => {
            const div = document.createElement('div');
            div.className = 'network';
            div.innerHTML = `
              <div class="network-name">${net.ssid}</div>
              <div class="network-details">
                Sinal: ${net.rssi} dBm | ${net.encryption.toUpperCase()} | Canal ${net.channel}
              </div>
            `;
            div.onclick = () => {
              document.querySelectorAll('.network').forEach(n => n.classList.remove('selected'));
              div.classList.add('selected');
              document.getElementById('ssid').value = net.ssid;
              document.getElementById('password').focus();
            };
            container.appendChild(div);
          });
        } else {
          container.innerHTML = '<div class="empty-state">Nenhuma rede encontrada</div>';
        }
      } catch (e) {
        container.innerHTML = '<div class="status error">Erro ao escanear: ' + e + '</div>';
      }
      
      btn.disabled = false;
      btn.innerHTML = 'Escanear Redes';
    }
    
    async function connectWiFi() {
      const ssid = document.getElementById('ssid').value;
      const password = document.getElementById('password').value;
      const btn = document.getElementById('connectBtn');
      const status = document.getElementById('status');
      
      if (!ssid) {
        status.innerHTML = '<div class="status error">Selecione uma rede primeiro</div>';
        return;
      }
      
      btn.disabled = true;
      btn.innerHTML = '<span class="loading"></span> Conectando...';
      status.innerHTML = '<div class="status info">Conectando a rede...</div>';
      
      try {
        const res = await fetch('/api/wifi/connect', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'X-API-Key': API_KEY
          },
          body: JSON.stringify({ ssid, password })
        });
        
        const data = await res.json();
        
        if (data.success) {
          status.innerHTML = `<div class="status success">
            Conectado com sucesso!<br>
            <small>IP: ${data.ip} | Sinal: ${data.rssi} dBm</small>
          </div>`;
          setTimeout(() => {
            status.innerHTML += '<div class="status info">Recarregando pagina...</div>';
            setTimeout(() => location.reload(), 2000);
          }, 3000);
        } else {
          status.innerHTML = '<div class="status error">' + data.message + '</div>';
        }
      } catch (e) {
        status.innerHTML = '<div class="status error">Erro: ' + e + '</div>';
      }
      
      btn.disabled = false;
      btn.innerHTML = 'Conectar a Rede';
    }
    
    // Escanear ao carregar pagina
    window.onload = () => scanNetworks();
  </script>
</body>
</html>
)HTMLPAGE";
    
    server.send(200, "text/html", html);
}

void handleNotFound() {
    StaticJsonDocument<128> doc;
    doc["error"] = "Rota não encontrada";
    doc["path"] = server.uri();
    
    String response;
    serializeJson(doc, response);
    
    server.send(404, "application/json", response);
}