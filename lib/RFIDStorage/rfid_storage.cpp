/**
 * @file rfid_storage.cpp
 * @brief Implementação do sistema de armazenamento RFID
 * @version 1.0.0
 * @date 2025-11-27
 */

#include "rfid_storage.h"

// ═══════════════════════════════════════════════════════════════════════
// CONSTRUTOR
// ═══════════════════════════════════════════════════════════════════════

RFIDStorage::RFIDStorage() : initialized(false) {
    // Reservar espaço para evitar realocações
    cards.reserve(MAX_RFID_CARDS);
}

// ═══════════════════════════════════════════════════════════════════════
// INICIALIZAÇÃO
// ═══════════════════════════════════════════════════════════════════════

bool RFIDStorage::begin() {
    Serial.println("🔧 [RFIDStorage] Inicializando...");
    
    // Inicializar LittleFS
    if (!LittleFS.begin(true)) {  // true = formatar se falhar
        Serial.println("❌ [RFIDStorage] Erro ao inicializar LittleFS");
        return false;
    }
    
    Serial.println("✅ [RFIDStorage] LittleFS inicializado");
    
    // Carregar dados
    if (!load()) {
        Serial.println("⚠️  [RFIDStorage] Nenhum arquivo encontrado (primeira inicialização)");
        // Criar arquivo vazio
        save();
    }
    
    initialized = true;
    Serial.printf("✅ [RFIDStorage] Pronto! %d cartão(s) carregado(s)\n", cards.size());
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// GERENCIAMENTO DE CARTÕES
// ═══════════════════════════════════════════════════════════════════════

bool RFIDStorage::addCard(const String& uid, const String& userName) {
    if (!initialized) {
        Serial.println("❌ [RFIDStorage] Não inicializado");
        return false;
    }
    
    // Verificar se já existe
    if (findCardIndex(uid) >= 0) {
        Serial.printf("⚠️  [RFIDStorage] Cartão %s já cadastrado\n", uid.c_str());
        return false;
    }
    
    // Verificar limite
    if (cards.size() >= MAX_RFID_CARDS) {
        Serial.println("❌ [RFIDStorage] Limite de cartões atingido");
        return false;
    }
    
    // Criar novo cartão
    RFIDCard newCard;
    newCard.uid = uid;
    newCard.userName = userName;
    newCard.registeredAt = millis();
    newCard.lastAccess = 0;
    newCard.accessCount = 0;
    newCard.active = true;
    
    // Adicionar ao vetor
    cards.push_back(newCard);
    
    // Salvar
    if (!save()) {
        Serial.println("❌ [RFIDStorage] Erro ao salvar");
        cards.pop_back();  // Remover se falhou
        return false;
    }
    
    Serial.printf("✅ [RFIDStorage] Cartão %s cadastrado (%s)\n", 
        uid.c_str(), userName.c_str());
    
    return true;
}

bool RFIDStorage::removeCard(const String& uid) {
    if (!initialized) return false;
    
    int index = findCardIndex(uid);
    if (index < 0) {
        Serial.printf("⚠️  [RFIDStorage] Cartão %s não encontrado\n", uid.c_str());
        return false;
    }
    
    // Remover do vetor
    cards.erase(cards.begin() + index);
    
    // Salvar
    if (!save()) {
        Serial.println("❌ [RFIDStorage] Erro ao salvar");
        return false;
    }
    
    Serial.printf("✅ [RFIDStorage] Cartão %s removido\n", uid.c_str());
    return true;
}

bool RFIDStorage::updateUserName(const String& uid, const String& newName) {
    if (!initialized) return false;
    
    int index = findCardIndex(uid);
    if (index < 0) return false;
    
    cards[index].userName = newName;
    
    return save();
}

bool RFIDStorage::updateLastAccess(const String& uid) {
    if (!initialized) return false;
    
    int index = findCardIndex(uid);
    if (index < 0) return false;
    
    cards[index].lastAccess = millis();
    cards[index].accessCount++;
    
    return save();
}

// ═══════════════════════════════════════════════════════════════════════
// CONSULTAS
// ═══════════════════════════════════════════════════════════════════════

bool RFIDStorage::isCardRegistered(const String& uid) {
    if (!initialized) return false;
    
    int index = findCardIndex(uid);
    if (index < 0) return false;
    
    return cards[index].active;
}

String RFIDStorage::getUserName(const String& uid) {
    if (!initialized) return "";
    
    int index = findCardIndex(uid);
    if (index < 0) return "";
    
    return cards[index].userName;
}

std::vector<RFIDCard> RFIDStorage::getAllCards() {
    return cards;
}

int RFIDStorage::count() {
    return cards.size();
}

bool RFIDStorage::clearAll() {
    if (!initialized) return false;
    
    cards.clear();
    return save();
}

// ═══════════════════════════════════════════════════════════════════════
// IMPORTAÇÃO/EXPORTAÇÃO
// ═══════════════════════════════════════════════════════════════════════

String RFIDStorage::exportJSON() {
    if (!initialized) return "{}";
    
    StaticJsonDocument<4096> doc;
    JsonArray array = doc.createNestedArray("cards");
    
    for (const RFIDCard& card : cards) {
        JsonObject obj = array.createNestedObject();
        obj["uid"] = card.uid;
        obj["userName"] = card.userName;
        obj["registeredAt"] = card.registeredAt;
        obj["lastAccess"] = card.lastAccess;
        obj["accessCount"] = card.accessCount;
        obj["active"] = card.active;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool RFIDStorage::importJSON(const String& json) {
    if (!initialized) return false;
    
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.printf("❌ [RFIDStorage] Erro ao parsear JSON: %s\n", error.c_str());
        return false;
    }
    
    JsonArray array = doc["cards"];
    if (array.isNull()) {
        Serial.println("❌ [RFIDStorage] Array 'cards' não encontrado");
        return false;
    }
    
    // Limpar dados atuais
    cards.clear();
    
    // Importar novos dados
    for (JsonObject obj : array) {
        RFIDCard card;
        card.uid = obj["uid"].as<String>();
        card.userName = obj["userName"].as<String>();
        card.registeredAt = obj["registeredAt"];
        card.lastAccess = obj["lastAccess"];
        card.accessCount = obj["accessCount"];
        card.active = obj["active"];
        
        cards.push_back(card);
    }
    
    Serial.printf("✅ [RFIDStorage] Importados %d cartão(s)\n", cards.size());
    
    return save();
}

// ═══════════════════════════════════════════════════════════════════════
// PERSISTÊNCIA (LittleFS)
// ═══════════════════════════════════════════════════════════════════════

bool RFIDStorage::load() {
    // Verificar se arquivo existe
    if (!LittleFS.exists(RFID_STORAGE_FILE)) {
        Serial.printf("⚠️  [RFIDStorage] Arquivo %s não existe\n", RFID_STORAGE_FILE);
        return false;
    }
    
    // Abrir arquivo
    File file = LittleFS.open(RFID_STORAGE_FILE, "r");
    if (!file) {
        Serial.println("❌ [RFIDStorage] Erro ao abrir arquivo");
        return false;
    }
    
    // Ler conteúdo
    String content = file.readString();
    file.close();
    
    if (content.length() == 0) {
        Serial.println("⚠️  [RFIDStorage] Arquivo vazio");
        return false;
    }
    
    // Parsear JSON
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, content);
    
    if (error) {
        Serial.printf("❌ [RFIDStorage] Erro ao parsear JSON: %s\n", error.c_str());
        return false;
    }
    
    // Carregar dados
    JsonArray array = doc["cards"];
    if (array.isNull()) {
        Serial.println("⚠️  [RFIDStorage] Array 'cards' não encontrado");
        return false;
    }
    
    cards.clear();
    
    for (JsonObject obj : array) {
        RFIDCard card;
        card.uid = obj["uid"].as<String>();
        card.userName = obj["userName"].as<String>();
        card.registeredAt = obj["registeredAt"];
        card.lastAccess = obj["lastAccess"];
        card.accessCount = obj["accessCount"];
        card.active = obj["active"];
        
        cards.push_back(card);
    }
    
    Serial.printf("✅ [RFIDStorage] Carregados %d cartão(s)\n", cards.size());
    
    return true;
}

bool RFIDStorage::save() {
    // Criar JSON
    StaticJsonDocument<4096> doc;
    JsonArray array = doc.createNestedArray("cards");
    
    for (const RFIDCard& card : cards) {
        JsonObject obj = array.createNestedObject();
        obj["uid"] = card.uid;
        obj["userName"] = card.userName;
        obj["registeredAt"] = card.registeredAt;
        obj["lastAccess"] = card.lastAccess;
        obj["accessCount"] = card.accessCount;
        obj["active"] = card.active;
    }
    
    // Abrir arquivo para escrita
    File file = LittleFS.open(RFID_STORAGE_FILE, "w");
    if (!file) {
        Serial.println("❌ [RFIDStorage] Erro ao abrir arquivo para escrita");
        return false;
    }
    
    // Serializar JSON direto no arquivo
    if (serializeJson(doc, file) == 0) {
        Serial.println("❌ [RFIDStorage] Erro ao serializar JSON");
        file.close();
        return false;
    }
    
    file.close();
    
    Serial.printf("💾 [RFIDStorage] Salvos %d cartão(s)\n", cards.size());
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// HELPERS PRIVADOS
// ═══════════════════════════════════════════════════════════════════════

int RFIDStorage::findCardIndex(const String& uid) {
    for (size_t i = 0; i < cards.size(); i++) {
        if (cards[i].uid.equalsIgnoreCase(uid)) {
            return i;
        }
    }
    return -1;
}
