/**
 * @file biometric_storage.cpp
 * @brief Implementação do sistema de armazenamento biométrico
 * @version 1.0.0
 * @date 2025-11-27
 */

#include "biometric_storage.h"

// ═══════════════════════════════════════════════════════════════════════
// CONSTRUTOR
// ═══════════════════════════════════════════════════════════════════════

BiometricStorage::BiometricStorage() : initialized(false) {
    // Reservar espaço para evitar realocações
    users.reserve(MAX_FINGERPRINTS);
}

// ═══════════════════════════════════════════════════════════════════════
// INICIALIZAÇÃO
// ═══════════════════════════════════════════════════════════════════════

bool BiometricStorage::begin() {
    Serial.println("🔧 [BiometricStorage] Inicializando...");
    
    // Inicializar LittleFS (se ainda não foi inicializado)
    if (!LittleFS.begin(true)) {  // true = formatar se falhar
        Serial.println("❌ [BiometricStorage] Erro ao inicializar LittleFS");
        return false;
    }
    
    Serial.println("✅ [BiometricStorage] LittleFS inicializado");
    
    // Carregar dados
    if (!load()) {
        Serial.println("⚠️  [BiometricStorage] Nenhum arquivo encontrado (primeira inicialização)");
        // Criar arquivo vazio
        save();
    }
    
    initialized = true;
    Serial.printf("✅ [BiometricStorage] Pronto! %d usuário(s) carregado(s)\n", users.size());
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// GERENCIAMENTO DE USUÁRIOS
// ═══════════════════════════════════════════════════════════════════════

bool BiometricStorage::addUser(const BiometricUser& user) {
    if (!initialized) {
        Serial.println("❌ [BiometricStorage] Não inicializado");
        return false;
    }
    
    // Verificar se slot já existe
    if (findUserIndex(user.slotId) >= 0) {
        Serial.printf("⚠️  [BiometricStorage] Slot %d já cadastrado\n", user.slotId);
        return false;
    }
    
    // Verificar limite
    if (users.size() >= MAX_FINGERPRINTS) {
        Serial.println("❌ [BiometricStorage] Limite de usuários atingido");
        return false;
    }
    
    // Adicionar ao vetor
    users.push_back(user);
    
    // Salvar
    if (!save()) {
        Serial.println("❌ [BiometricStorage] Erro ao salvar");
        users.pop_back();  // Remover se falhou
        return false;
    }
    
    Serial.printf("✅ [BiometricStorage] Usuário %s cadastrado (Slot %d)\n", 
        user.userName.c_str(), user.slotId);
    
    return true;
}

bool BiometricStorage::removeUser(uint16_t slotId) {
    if (!initialized) return false;
    
    int index = findUserIndex(slotId);
    if (index < 0) {
        Serial.printf("⚠️  [BiometricStorage] Slot %d não encontrado\n", slotId);
        return false;
    }
    
    // Remover do vetor
    users.erase(users.begin() + index);
    
    // Salvar
    if (!save()) {
        Serial.println("❌ [BiometricStorage] Erro ao salvar");
        return false;
    }
    
    Serial.printf("✅ [BiometricStorage] Slot %d removido\n", slotId);
    return true;
}

bool BiometricStorage::updateUserName(uint16_t slotId, const String& newName) {
    if (!initialized) return false;
    
    int index = findUserIndex(slotId);
    if (index < 0) return false;
    
    users[index].userName = newName;
    
    return save();
}

bool BiometricStorage::updateLastAccess(uint16_t slotId, uint16_t confidence) {
    if (!initialized) return false;
    
    int index = findUserIndex(slotId);
    if (index < 0) return false;
    
    users[index].lastAccess = millis();
    users[index].accessCount++;
    users[index].confidence = confidence;
    
    return save();
}

// ═══════════════════════════════════════════════════════════════════════
// CONSULTAS
// ═══════════════════════════════════════════════════════════════════════

BiometricUser* BiometricStorage::getUserBySlot(uint16_t slotId) {
    if (!initialized) return nullptr;
    
    int index = findUserIndex(slotId);
    if (index < 0) return nullptr;
    
    return &users[index];
}

std::vector<BiometricUser> BiometricStorage::getAllUsers() {
    return users;
}

int BiometricStorage::count() {
    return users.size();
}

uint16_t BiometricStorage::getNextFreeSlot() {
    // Criar set com slots ocupados
    bool occupied[MAX_FINGERPRINTS + 1] = {false};
    
    for (const BiometricUser& user : users) {
        if (user.slotId <= MAX_FINGERPRINTS) {
            occupied[user.slotId] = true;
        }
    }
    
    // Encontrar primeiro slot livre (começando do 1)
    for (uint16_t i = 1; i <= MAX_FINGERPRINTS; i++) {
        if (!occupied[i]) {
            return i;
        }
    }
    
    // Nenhum slot livre
    return MAX_FINGERPRINTS + 1;
}

bool BiometricStorage::clearAll() {
    if (!initialized) return false;
    
    users.clear();
    return save();
}

// ═══════════════════════════════════════════════════════════════════════
// IMPORTAÇÃO/EXPORTAÇÃO
// ═══════════════════════════════════════════════════════════════════════

String BiometricStorage::exportJSON() {
    if (!initialized) return "{}";
    
    StaticJsonDocument<4096> doc;
    JsonArray array = doc.createNestedArray("users");
    
    for (const BiometricUser& user : users) {
        JsonObject obj = array.createNestedObject();
        obj["slotId"] = user.slotId;
        obj["userId"] = user.userId;
        obj["userName"] = user.userName;
        obj["registeredAt"] = user.registeredAt;
        obj["lastAccess"] = user.lastAccess;
        obj["accessCount"] = user.accessCount;
        obj["confidence"] = user.confidence;
        obj["active"] = user.active;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool BiometricStorage::importJSON(const String& json) {
    if (!initialized) return false;
    
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.printf("❌ [BiometricStorage] Erro ao parsear JSON: %s\n", error.c_str());
        return false;
    }
    
    JsonArray array = doc["users"];
    if (array.isNull()) {
        Serial.println("❌ [BiometricStorage] Array 'users' não encontrado");
        return false;
    }
    
    // Limpar dados atuais
    users.clear();
    
    // Importar novos dados
    for (JsonObject obj : array) {
        BiometricUser user;
        user.slotId = obj["slotId"];
        user.userId = obj["userId"].as<String>();
        user.userName = obj["userName"].as<String>();
        user.registeredAt = obj["registeredAt"];
        user.lastAccess = obj["lastAccess"];
        user.accessCount = obj["accessCount"];
        user.confidence = obj["confidence"];
        user.active = obj["active"];
        
        users.push_back(user);
    }
    
    Serial.printf("✅ [BiometricStorage] Importados %d usuário(s)\n", users.size());
    
    return save();
}

// ═══════════════════════════════════════════════════════════════════════
// PERSISTÊNCIA (LittleFS)
// ═══════════════════════════════════════════════════════════════════════

bool BiometricStorage::load() {
    // Verificar se arquivo existe
    if (!LittleFS.exists(BIOMETRIC_STORAGE_FILE)) {
        Serial.printf("⚠️  [BiometricStorage] Arquivo %s não existe\n", BIOMETRIC_STORAGE_FILE);
        return false;
    }
    
    // Abrir arquivo
    File file = LittleFS.open(BIOMETRIC_STORAGE_FILE, "r");
    if (!file) {
        Serial.println("❌ [BiometricStorage] Erro ao abrir arquivo");
        return false;
    }
    
    // Ler conteúdo
    String content = file.readString();
    file.close();
    
    if (content.length() == 0) {
        Serial.println("⚠️  [BiometricStorage] Arquivo vazio");
        return false;
    }
    
    // Parsear JSON
    StaticJsonDocument<4096> doc;
    DeserializationError error = deserializeJson(doc, content);
    
    if (error) {
        Serial.printf("❌ [BiometricStorage] Erro ao parsear JSON: %s\n", error.c_str());
        return false;
    }
    
    // Carregar dados
    JsonArray array = doc["users"];
    if (array.isNull()) {
        Serial.println("⚠️  [BiometricStorage] Array 'users' não encontrado");
        return false;
    }
    
    users.clear();
    
    for (JsonObject obj : array) {
        BiometricUser user;
        user.slotId = obj["slotId"];
        user.userId = obj["userId"].as<String>();
        user.userName = obj["userName"].as<String>();
        user.registeredAt = obj["registeredAt"];
        user.lastAccess = obj["lastAccess"];
        user.accessCount = obj["accessCount"];
        user.confidence = obj["confidence"];
        user.active = obj["active"];
        
        users.push_back(user);
    }
    
    Serial.printf("✅ [BiometricStorage] Carregados %d usuário(s)\n", users.size());
    
    return true;
}

bool BiometricStorage::save() {
    // Criar JSON
    StaticJsonDocument<4096> doc;
    JsonArray array = doc.createNestedArray("users");
    
    for (const BiometricUser& user : users) {
        JsonObject obj = array.createNestedObject();
        obj["slotId"] = user.slotId;
        obj["userId"] = user.userId;
        obj["userName"] = user.userName;
        obj["registeredAt"] = user.registeredAt;
        obj["lastAccess"] = user.lastAccess;
        obj["accessCount"] = user.accessCount;
        obj["confidence"] = user.confidence;
        obj["active"] = user.active;
    }
    
    // Abrir arquivo para escrita
    File file = LittleFS.open(BIOMETRIC_STORAGE_FILE, "w");
    if (!file) {
        Serial.println("❌ [BiometricStorage] Erro ao abrir arquivo para escrita");
        return false;
    }
    
    // Serializar JSON direto no arquivo
    if (serializeJson(doc, file) == 0) {
        Serial.println("❌ [BiometricStorage] Erro ao serializar JSON");
        file.close();
        return false;
    }
    
    file.close();
    
    Serial.printf("💾 [BiometricStorage] Salvos %d usuário(s)\n", users.size());
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// HELPERS PRIVADOS
// ═══════════════════════════════════════════════════════════════════════

int BiometricStorage::findUserIndex(uint16_t slotId) {
    for (size_t i = 0; i < users.size(); i++) {
        if (users[i].slotId == slotId) {
            return i;
        }
    }
    return -1;
}
