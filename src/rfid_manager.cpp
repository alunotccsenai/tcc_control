/**
 * @file rfid_manager.cpp
 * @brief Implementação do gerenciador RFID/NFC PN532
 */

#include "rfid_manager.h"
#include "config.h"
#include "pins.h"
#include <SPI.h>

// ════════════════════════════════════════════════════════════════
// INSTÂNCIA GLOBAL
// ════════════════════════════════════════════════════════════════

RFIDManager rfidManager;

// ════════════════════════════════════════════════════════════════
// CONSTRUTOR/DESTRUTOR
// ════════════════════════════════════════════════════════════════

RFIDManager::RFIDManager() {
    card_count = 0;
    log_count = 0;
    last_read_time = 0;
    enrollState = RFID_IDLE;
    pn532 = nullptr;
}

RFIDManager::~RFIDManager() {
    if (pn532) delete pn532;
    preferences.end();
}

// ════════════════════════════════════════════════════════════════
// INICIALIZAÇÃO
// ════════════════════════════════════════════════════════════════

bool RFIDManager::init() {
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║     INICIALIZANDO RFID MANAGER (PN532)       ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    
    // ════════════════════════════════════════════════════════════════════════════
    // 🟢 HARDWARE CONECTADO - CÓDIGO HABILITADO v5.1.1
    // ════════════════════════════════════════════════════════════════════════════
    
    Serial.println("🔧 Inicializando PN532 via SPI...");
    Serial.println("📋 PINAGEM SPI:");
    Serial.printf("   • SCK  → GPIO%d (FSPICLK) - Compartilhado com Display/Touch\n", RFID_SCK_PIN);
    Serial.printf("   • MOSI → GPIO%d (FSPID)   - Compartilhado com Display/Touch\n", RFID_MOSI_PIN);
    Serial.printf("   • MISO → GPIO%d (FSPIQ)   - Compartilhado APENAS com Touch\n", RFID_MISO_PIN);
    Serial.printf("   • NSS  → GPIO%d            - EXCLUSIVO para PN532\n", PN532_SS_PIN);
    Serial.printf("   • RST  → GPIO%d            - %s\n", PN532_RST_PIN, 
                  PN532_RST_PIN == -1 ? "Não conectado (opcional)" : "Conectado");
    Serial.println("⚠️  DIP Switch: CH1 (I0) = OFF, CH2 (I1) = ON (Modo SPI)");
    
    // ✅ v6.0.5: SEGUIR CÓDIGO FUNCIONAL - Passar &SPI no construtor
    Serial.println("🔧 Criando instância PN532 (SPI)...");
    pn532 = new Adafruit_PN532(PN532_SS_PIN, &SPI);  // ⭐ CRÍTICO: Passar &SPI
    
    // ✅ v6.0.5: PASSO 1 - Configurar GPIO21 (CS/SS) ANTES do begin()
    Serial.println("🔧 Configurando pino CS do PN532...");
    pinMode(PN532_SS_PIN, OUTPUT);
    digitalWrite(PN532_SS_PIN, HIGH);  // Desativar PN532 inicialmente
    delay(10);  // Estabilização do pino
    
    // ✅ v6.0.5: PASSO 2 - Delay para estabilização do barramento SPI
    Serial.println("   ⏱️  Aguardando estabilização (100ms)...");
    delay(100);  // PN532 precisa ~7ms para sair do Power Down mode
    
    // ✅ v6.0.5: PASSO 3 - Inicializar PN532 usando begin() da biblioteca
    Serial.println("🔧 Chamando pn532->begin()...");
    pn532->begin();  // ⭐ CRÍTICO: Biblioteca faz todo o wakeup automaticamente!
    
    // ✅ v6.0.5: PASSO 4 - Delay adicional após begin()
    Serial.println("   ⏱️  Aguardando PN532 entrar em modo SPI (200ms)...");
    delay(200);  // Permite que o PN532 entre em modo de comunicação SPI
    
    // ✅ v6.0.5: PASSO 5 - Verificar firmware (com retry - até 3 tentativas)
    Serial.println("🔧 Verificando firmware do PN532...");
    uint32_t versiondata = 0;
    for (int attempt = 1; attempt <= 3 && !versiondata; attempt++) {
        if (attempt > 1) {
            Serial.printf("   🔄 Tentativa %d/3...\n", attempt);
            delay(500);  // Aguardar antes de nova tentativa
        }
        versiondata = pn532->getFirmwareVersion();
    }
    
    if (!versiondata) {
        Serial.println("❌ PN532 não encontrado após 3 tentativas!");  // v6.0.5: Corrigido
        Serial.println("\n🔍 CHECKLIST DE VERIFICAÇÃO:");
        Serial.println("═══════════════════════════════════════");
        Serial.printf("   1️⃣ PINAGEM SPI:\n");
        Serial.printf("      • NSS (CS):  GPIO%d\n", PN532_SS_PIN);
        Serial.printf("      • SCK:       GPIO%d\n", RFID_SCK_PIN);
        Serial.printf("      • MOSI:      GPIO%d\n", RFID_MOSI_PIN);
        Serial.printf("      • MISO:      GPIO%d\n", RFID_MISO_PIN);
        Serial.println();
        Serial.println("   2️⃣ DIP SWITCH:");
        Serial.println("      • CH1 (I0) = OFF → LOW (0)");
        Serial.println("      • CH2 (I1) = ON  → HIGH (1)");
        Serial.println("      • Resultado: Modo SPI ✅");
        Serial.println();
        Serial.println("   3️⃣ ALIMENTAÇÃO:");
        Serial.println("      • VCC: 3.3V ou 5V (medido com multímetro)");
        Serial.println("      • GND: Conectado");
        Serial.println("      • Tensão estável (sem quedas)");
        Serial.println();
        Serial.println("   4️⃣ HARDWARE:");
        Serial.println("      • Módulo PN532 físico conectado");
        Serial.println("      • Fios bem soldados/conectados");
        Serial.println("      • LED do PN532 aceso (se houver)");
        Serial.println("═══════════════════════════════════════");
        Serial.println("✅ Sistema continuará sem RFID\n");
        delete pn532;
        pn532 = nullptr;
        return false;
    }
    
    // Exibir versão do firmware
    Serial.print("✅ PN532 conectado! Firmware v");
    Serial.print((versiondata >> 24) & 0xFF, DEC);
    Serial.print('.');
    Serial.println((versiondata >> 16) & 0xFF, DEC);
    
    // Configurar para leitura de cartões Mifare
    Serial.println("🔧 Configurando para modo Mifare...");
    pn532->SAMConfig();
    Serial.println("✅ PN532 configurado para Mifare/NTAG/Ultralight");
    
    // Carregar dados do NVS
    Serial.println("🔧 Carregando dados do NVS...");
    loadFromNVS();
    loadLogsFromNVS();
    
    Serial.printf("✅ %d cartão(s) cadastrado(s)\n", card_count);
    Serial.printf("✅ %d log(s) de acesso\n", log_count);
    Serial.println("╚══════════════════════════════════════════════╝\n");
    
    return true;
}

bool RFIDManager::isHardwareConnected() {
    if (!pn532) return false;
    uint32_t versiondata = pn532->getFirmwareVersion();
    return (versiondata != 0);
}

// ════════════════════════════════════════════════════════════════
// LEITURA DE CARTÕES
// ════════════════════════════════════════════════════════════════

bool RFIDManager::detectCard() {
    if (!pn532) return false;
    
    uint8_t uid[7];
    uint8_t uidLength;
    
    // Timeout rápido para não bloquear
    return pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50);
}

bool RFIDManager::readCard(uint8_t* uid, uint8_t* uid_length) {
    if (!pn532) {
        Serial.println("❌ PN532 não inicializado");
        return false;
    }
    
    // Debounce: não ler se foi lido recentemente (< 1s)
    if (millis() - last_read_time < 1000) {
        return false;
    }
    
    // Ler cartão com timeout de 1 segundo
    bool success = pn532->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, uid_length, 1000);
    
    if (success) {
        last_read_time = millis();
        Serial.print("📇 Cartão detectado: ");
        Serial.println(uidToString(uid, *uid_length));
    }
    
    return success;
}

// ════════════════════════════════════════════════════════════════
// GERENCIAMENTO DE CARTÕES
// ════════════════════════════════════════════════════════════════

bool RFIDManager::addCard(uint8_t* uid, uint8_t uid_length, const char* name) {
    // Verificar se já existe
    if (findCardIndex(uid, uid_length) >= 0) {
        Serial.println("❌ Cartão já cadastrado!");
        return false;
    }
    
    // Verificar limite
    if (card_count >= MAX_RFID_CARDS) {
        Serial.println("❌ Memória cheia! Máximo: 50 cartões");
        return false;
    }
    
    // Adicionar novo cartão
    RFIDCard* card = &cards[card_count];
    memset(card, 0, sizeof(RFIDCard));
    
    memcpy(card->uid, uid, uid_length);
    card->uid_length = uid_length;
    strncpy(card->name, name, RFID_NAME_LENGTH - 1);
    card->timestamp = millis() / 1000;  // Unix timestamp
    card->active = true;
    card->access_count = 0;
    card->last_access = 0;
    
    card_count++;
    saveToNVS();
    
    Serial.printf("✅ Cartão cadastrado: %s (%s)\n", name, uidToString(uid, uid_length).c_str());
    
    return true;
}

bool RFIDManager::removeCard(int index) {
    if (index < 0 || index >= card_count) {
        Serial.println("❌ Índice inválido");
        return false;
    }
    
    Serial.printf("🗑️ Removendo: %s\n", cards[index].name);
    
    // Mover todos os cartões subsequentes
    for (int i = index; i < card_count - 1; i++) {
        cards[i] = cards[i + 1];
    }
    
    card_count--;
    saveToNVS();
    
    Serial.println("✅ Cartão removido");
    return true;
}

bool RFIDManager::removeCardByUID(uint8_t* uid, uint8_t uid_length) {
    int index = findCardIndex(uid, uid_length);
    if (index < 0) return false;
    return removeCard(index);
}

bool RFIDManager::editCardName(int index, const char* new_name) {
    if (index < 0 || index >= card_count) return false;
    
    strncpy(cards[index].name, new_name, RFID_NAME_LENGTH - 1);
    cards[index].name[RFID_NAME_LENGTH - 1] = '\0';
    saveToNVS();
    
    Serial.printf("✏️ Nome alterado: %s\n", new_name);
    return true;
}

bool RFIDManager::toggleCardActive(int index) {
    if (index < 0 || index >= card_count) return false;
    
    cards[index].active = !cards[index].active;
    saveToNVS();
    
    Serial.printf("🔄 Cartão %s: %s\n", 
                  cards[index].name, 
                  cards[index].active ? "ATIVADO" : "DESATIVADO");
    return true;
}

// ════════════════════════════════════════════════════════════════
// AUTENTICAÇÃO
// ════════════════════════════════════════════════════════════════

bool RFIDManager::isCardAuthorized(uint8_t* uid, uint8_t uid_length) {
    int index = findCardIndex(uid, uid_length);
    
    if (index < 0) {
        Serial.println("❌ Cartão não cadastrado");
        logAccess(uid, uid_length, "Desconhecido", false);
        return false;
    }
    
    RFIDCard* card = &cards[index];
    
    if (!card->active) {
        Serial.printf("❌ Cartão desativado: %s\n", card->name);
        logAccess(uid, uid_length, card->name, false);
        return false;
    }
    
    // Atualizar estatísticas
    card->access_count++;
    card->last_access = millis() / 1000;
    saveToNVS();
    
    Serial.printf("✅ Acesso autorizado: %s\n", card->name);
    logAccess(uid, uid_length, card->name, true);
    
    return true;
}

int RFIDManager::findCardIndex(uint8_t* uid, uint8_t uid_length) {
    for (int i = 0; i < card_count; i++) {
        if (cards[i].uid_length == uid_length) {
            if (compareUID(cards[i].uid, uid, uid_length)) {
                return i;
            }
        }
    }
    return -1;
}

bool RFIDManager::compareUID(uint8_t* uid1, uint8_t* uid2, uint8_t len) {
    for (uint8_t i = 0; i < len; i++) {
        if (uid1[i] != uid2[i]) return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════
// CONSULTAS
// ════════════════════════════════════════════════════════════════

int RFIDManager::getCardCount() {
    return card_count;
}

int RFIDManager::getActiveCardCount() {
    int count = 0;
    for (int i = 0; i < card_count; i++) {
        if (cards[i].active) count++;
    }
    return count;
}

RFIDCard* RFIDManager::getCard(int index) {
    if (index < 0 || index >= card_count) return nullptr;
    return &cards[index];
}

String RFIDManager::uidToString(uint8_t* uid, uint8_t uid_length) {
    String uidString = "";
    for (uint8_t i = 0; i < uid_length; i++) {
        if (uid[i] < 0x10) uidString += "0";
        uidString += String(uid[i], HEX);
        if (i < uid_length - 1) uidString += ":";
    }
    uidString.toUpperCase();
    return uidString;
}

void RFIDManager::listCards() {
    Serial.println("\n╔══════════════════════════════════════════════╗");
    Serial.println("║          CARTÕES RFID CADASTRADOS            ║");
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf("║ Total: %d/%d                                  ║\n", card_count, MAX_RFID_CARDS);
    Serial.println("╠══════════════════════════════════════════════╣");
    
    for (int i = 0; i < card_count; i++) {
        RFIDCard* card = &cards[i];
        Serial.printf("║ [%02d] %-18s %s       ║\n", 
                      i + 1,
                      card->name,
                      card->active ? "✓" : "✗");
        Serial.printf("║      UID: %-35s║\n", 
                      uidToString(card->uid, card->uid_length).c_str());
        Serial.printf("║      Acessos: %-4d  Último: %-12lu ║\n",
                      card->access_count,
                      card->last_access);
        
        if (i < card_count - 1) {
            Serial.println("╠──────────────────────────────────────────────╣");
        }
    }
    
    Serial.println("╚══════════════════════════════════════════════╝\n");
}

// ════════════════════════════════════════════════════════════════
// LOGS DE ACESSO
// ════════════════════════════════════════════════════════════════

void RFIDManager::logAccess(uint8_t* uid, uint8_t uid_length, const char* name, bool granted) {
    if (log_count >= MAX_ACCESS_LOGS) {
        // Remover log mais antigo (FIFO)
        for (int i = 0; i < MAX_ACCESS_LOGS - 1; i++) {
            logs[i] = logs[i + 1];
        }
        log_count = MAX_ACCESS_LOGS - 1;
    }
    
    AccessLog* log = &logs[log_count];
    memcpy(log->uid, uid, uid_length);
    log->uid_length = uid_length;
    strncpy(log->name, name, RFID_NAME_LENGTH - 1);
    log->timestamp = millis() / 1000;
    log->granted = granted;
    
    log_count++;
    saveLogsToNVS();
    
    Serial.printf("📝 Log: %s - %s %s\n", 
                  name, 
                  uidToString(uid, uid_length).c_str(),
                  granted ? "✅" : "❌");
}

int RFIDManager::getLogCount() {
    return log_count;
}

AccessLog* RFIDManager::getLog(int index) {
    if (index < 0 || index >= log_count) return nullptr;
    return &logs[index];
}

void RFIDManager::clearLogs() {
    log_count = 0;
    saveLogsToNVS();
    Serial.println("🗑️ Logs limpos");
}

String RFIDManager::logsToJSON() {
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = 0; i < log_count; i++) {
        AccessLog* log = &logs[i];
        JsonObject obj = array.createNestedObject();
        obj["uid"] = uidToString(log->uid, log->uid_length);
        obj["name"] = log->name;
        obj["timestamp"] = log->timestamp;
        obj["granted"] = log->granted;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// ════════════════════════════════════════════════════════════════
// IMPORTAÇÃO/EXPORTAÇÃO
// ════════════════════════════════════════════════════════════════

String RFIDManager::exportToJSON() {
    DynamicJsonDocument doc(8192);
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = 0; i < card_count; i++) {
        RFIDCard* card = &cards[i];
        JsonObject obj = array.createNestedObject();
        obj["uid"] = uidToString(card->uid, card->uid_length);
        obj["name"] = card->name;
        obj["timestamp"] = card->timestamp;
        obj["active"] = card->active;
        obj["access_count"] = card->access_count;
        obj["last_access"] = card->last_access;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool RFIDManager::importFromJSON(const String& json) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.print("❌ Erro ao importar JSON: ");
        Serial.println(error.c_str());
        return false;
    }
    
    JsonArray array = doc.as<JsonArray>();
    int imported = 0;
    
    for (JsonObject obj : array) {
        if (card_count >= MAX_RFID_CARDS) break;
        
        String uidStr = obj["uid"];
        // Parse UID string "XX:XX:XX:XX"
        uint8_t uid[8];
        uint8_t uid_len = 0;
        
        int pos = 0;
        while (pos < uidStr.length() && uid_len < 8) {
            String byteStr = uidStr.substring(pos, pos + 2);
            uid[uid_len++] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
            pos += 3;  // "XX:"
        }
        
        // Verificar duplicata
        if (findCardIndex(uid, uid_len) >= 0) continue;
        
        RFIDCard* card = &cards[card_count];
        memcpy(card->uid, uid, uid_len);
        card->uid_length = uid_len;
        strncpy(card->name, obj["name"], RFID_NAME_LENGTH - 1);
        card->timestamp = obj["timestamp"];
        card->active = obj["active"];
        card->access_count = obj["access_count"];
        card->last_access = obj["last_access"];
        
        card_count++;
        imported++;
    }
    
    saveToNVS();
    Serial.printf("✅ Importados %d cartões\n", imported);
    return true;
}

void RFIDManager::clearAll() {
    card_count = 0;
    saveToNVS();
    Serial.println("🗑️ Todos os cartões removidos");
}

// ════════════════════════════════════════════════════════════════
// MÁQUINA DE ESTADOS - CADASTRO
// ════════════════════════════════════════════════════════════════

void RFIDManager::startEnrollment() {
    if (!isHardwareConnected()) {
        enrollState = RFID_ERROR_HARDWARE;
        return;
    }
    
    if (card_count >= MAX_RFID_CARDS) {
        enrollState = RFID_ERROR_FULL;
        return;
    }
    
    enrollState = RFID_WAITING_CARD;
    Serial.println("🔵 Aguardando cartão RFID...");
}

void RFIDManager::cancelEnrollment() {
    enrollState = RFID_IDLE;
    Serial.println("❌ Cadastro cancelado");
}

void RFIDManager::processEnrollment() {
    switch (enrollState) {
        case RFID_WAITING_CARD:
            if (readCard(tempUID, &tempUIDLength)) {
                // Verificar se já existe
                if (findCardIndex(tempUID, tempUIDLength) >= 0) {
                    enrollState = RFID_ERROR_DUPLICATE;
                    Serial.println("❌ Cartão já cadastrado!");
                } else {
                    enrollState = RFID_CARD_READ;
                    Serial.println("✅ Cartão lido! Aguardando nome...");
                }
            }
            break;
            
        case RFID_CARD_READ:
            // Aguarda nome ser inserido externamente
            // (será chamado addCard() após inserir nome)
            break;
            
        default:
            // Estados de erro ou sucesso - reset automático após delay
            break;
    }
}

String RFIDManager::getEnrollStateString() {
    switch (enrollState) {
        case RFID_IDLE: return "Inativo";
        case RFID_WAITING_CARD: return "Aproxime o cartao...";
        case RFID_READING: return "Lendo...";
        case RFID_CARD_READ: return "Cartao lido! Digite o nome";
        case RFID_SAVING: return "Salvando...";
        case RFID_SUCCESS: return "Cadastrado com sucesso!";
        case RFID_ERROR_DUPLICATE: return "Erro: Cartao ja existe";
        case RFID_ERROR_FULL: return "Erro: Memoria cheia (50)";
        case RFID_ERROR_READ: return "Erro: Falha na leitura";
        case RFID_ERROR_HARDWARE: return "Erro: PN532 desconectado";
        default: return "Desconhecido";
    }
}

// ════════════════════════════════════════════════════════════════
// PERSISTÊNCIA NVS
// ════════════════════════════════════════════════════════════════

void RFIDManager::loadFromNVS() {
    preferences.begin("rfid_cards", true);  // Read-only
    
    card_count = preferences.getInt("count", 0);
    
    for (int i = 0; i < card_count; i++) {
        String key = "card_" + String(i);
        preferences.getBytes(key.c_str(), &cards[i], sizeof(RFIDCard));
    }
    
    preferences.end();
}

void RFIDManager::saveToNVS() {
    preferences.begin("rfid_cards", false);  // Read-write
    
    preferences.putInt("count", card_count);
    
    for (int i = 0; i < card_count; i++) {
        String key = "card_" + String(i);
        preferences.putBytes(key.c_str(), &cards[i], sizeof(RFIDCard));
    }
    
    preferences.end();
}

void RFIDManager::loadLogsFromNVS() {
    preferences.begin("rfid_logs", true);
    
    log_count = preferences.getInt("count", 0);
    
    for (int i = 0; i < log_count; i++) {
        String key = "log_" + String(i);
        preferences.getBytes(key.c_str(), &logs[i], sizeof(AccessLog));
    }
    
    preferences.end();
}

void RFIDManager::saveLogsToNVS() {
    preferences.begin("rfid_logs", false);
    
    preferences.putInt("count", log_count);
    
    for (int i = 0; i < log_count; i++) {
        String key = "log_" + String(i);
        preferences.putBytes(key.c_str(), &logs[i], sizeof(AccessLog));
    }
    
    preferences.end();
}