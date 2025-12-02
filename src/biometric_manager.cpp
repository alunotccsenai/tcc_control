/**
 * @file biometric_manager.cpp
 * @brief Implementação do gerenciador de biometria AS608
 */

#include "biometric_manager.h"
#include "config.h"
#include "pins.h"

// ════════════════════════════════════════════════════════════════
// INSTÂNCIA GLOBAL
// ════════════════════════════════════════════════════════════════

BiometricManager bioManager;

// ════════════════════════════════════════════════════════════════
// CONSTRUTOR/DESTRUTOR
// ════════════════════════════════════════════════════════════════

BiometricManager::BiometricManager() {
    finger_count = 0;
    log_count = 0;
    last_verify_time = 0;
    enrollState = BIO_IDLE;
    finger = nullptr;
}

BiometricManager::~BiometricManager() {
    if (finger) delete finger;
    preferences.end();
}

// ════════════════════════════════════════════════════════════════
// INICIALIZAÇÃO
// ════════════════════════════════════════════════════════════════

bool BiometricManager::init() {
    Serial.println("╔══════════════════════════════════════════════╗");
    Serial.println("║   INICIALIZANDO BIOMETRIC MANAGER (AS608)    ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    
    // ════════════════════════════════════════════════════════════════════════════
    // 🟢 HARDWARE CONECTADO - CÓDIGO HABILITADO v5.1.1
    // ════════════════════════════════════════════════════════════════════════════
    
    // ⚠️ CRÍTICO: AS608 consome 120mA nominal e picos de 150mA na inicialização
    // Isso pode causar brown-out no VDD3P3 do ESP32-S3
    Serial.println("⚡ ATENÇÃO: AS608 consome até 150mA (pico)");
    Serial.println("⚡ Aguardando estabilização da alimentação...");
    delay(1000);  // ⭐ CRÍTICO: Esperar alimentação estabilizar ANTES de ligar sensor
    
    // Inicializar Serial2 para AS608
    Serial.println("🔧 Inicializando UART2...");
    Serial.printf("   • RX ESP32: GPIO%d → TX AS608 (Blue)\n", BIO_RX_PIN);
    Serial.printf("   • TX ESP32: GPIO%d → RX AS608 (Green)\n", BIO_TX_PIN);
    Serial.printf("   • Baudrate: %d bps\n", BIO_BAUDRATE);
    
    Serial2.begin(BIO_BAUDRATE, SERIAL_8N1, BIO_RX_PIN, BIO_TX_PIN);
    delay(500);  // Aguardar estabilização da UART
    
    // Criar instância do sensor
    Serial.println("🔧 Criando instância do sensor...");
    finger = new Adafruit_Fingerprint(&Serial2);
    
    // Verificar conexão
    Serial.println("🔧 Verificando conexão com AS608...");
    Serial.println("⚡ ATENÇÃO: LED azul do sensor vai ligar (pico de corrente)");
    delay(200);  // ⭐ Esperar antes do pico de corrente
    
    if (!finger->verifyPassword()) {
        Serial.println("❌ AS608 não encontrado! Verifique:");
        Serial.printf("   - RX: GPIO%d → TX sensor (Blue wire)\n", BIO_RX_PIN);
        Serial.printf("   - TX: GPIO%d → RX sensor (Green wire)\n", BIO_TX_PIN);
        Serial.printf("   - Baudrate: %d bps\n", BIO_BAUDRATE);
        Serial.println("   - Alimentação: 3.3V (Red wire) e GND (Black wire)");
        Serial.println("   - IMPORTANTE: TX/RX devem estar CRUZADOS!");
        Serial.println("✅ Sistema continuará sem Biometria\n");
        delete finger;
        finger = nullptr;
        return false;
    }
    
    Serial.println("✅ AS608 conectado com sucesso!");
    delay(200);  // ⭐ Esperar após pico de corrente do LED
    
    // Obter parâmetros do sensor
    Serial.println("🔧 Lendo parâmetros do sensor...");
    finger->getParameters();
    delay(100);  // ⭐ Esperar após leitura de parâmetros
    Serial.printf("✅ Capacidade: %d templates\n", finger->capacity);
    Serial.printf("✅ Segurança: Level %d\n", finger->security_level);
    Serial.printf("✅ Tamanho pacote: %d bytes\n", finger->packet_len);
    Serial.printf("✅ Baudrate: %d bps\n", finger->baud_rate);
    
    // Contar templates no sensor
    Serial.println("🔧 Contando templates no sensor...");
    uint16_t sensor_count = getSensorTemplateCount();
    Serial.printf("✅ Templates no sensor: %d\n", sensor_count);
    delay(100);  // ⭐ Esperar após contagem
    
    // Carregar metadados do NVS
    Serial.println("🔧 Carregando metadados do NVS...");
    loadFromNVS();
    loadLogsFromNVS();
    
    Serial.printf("✅ %d metadados carregados\n", finger_count);
    Serial.printf("✅ %d logs carregados\n", log_count);
    Serial.println("╚══════════════════════════════════════════════╝\n");
    
    return true;
}

bool BiometricManager::isHardwareConnected() {
    if (!finger) return false;
    return finger->verifyPassword();
}

uint16_t BiometricManager::getSensorTemplateCount() {
    if (!finger) return 0;
    
    finger->getTemplateCount();
    return finger->templateCount;
}

// ════════════════════════════════════════════════════════════════
// GERENCIAMENTO DE DIGITAIS
// ════════════════════════════════════════════════════════════════

bool BiometricManager::addFingerprint(uint16_t id, const char* name) {
    // Verificar se ID já existe
    if (findFingerprintIndex(id) >= 0) {
        Serial.println("❌ ID já cadastrado!");
        return false;
    }
    
    // Verificar limite
    if (finger_count >= MAX_FINGERPRINTS) {
        Serial.println("❌ Limite de metadados atingido!");
        return false;
    }
    
    // Adicionar metadados
    Fingerprint* fp = &fingerprints[finger_count];
    memset(fp, 0, sizeof(Fingerprint));
    
    fp->id = id;
    strncpy(fp->name, name, FINGER_NAME_LENGTH - 1);
    fp->timestamp = millis() / 1000;
    fp->active = true;
    fp->access_count = 0;
    fp->last_access = 0;
    fp->confidence = 0;
    
    finger_count++;
    saveToNVS();
    
    Serial.printf("✅ Metadados cadastrados: ID=%d, Nome=%s\n", id, name);
    
    return true;
}

bool BiometricManager::deleteFingerprint(int index) {
    if (index < 0 || index >= finger_count) {
        Serial.println("❌ Índice inválido");
        return false;
    }
    
    Fingerprint* fp = &fingerprints[index];
    Serial.printf("🗑️ Removendo: ID=%d, Nome=%s\n", fp->id, fp->name);
    
    // Remover do sensor
    if (finger && finger->deleteModel(fp->id) == FINGERPRINT_OK) {
        Serial.println("✅ Template removido do sensor");
    } else {
        Serial.println("⚠️ Falha ao remover do sensor (metadados serão removidos)");
    }
    
    // Remover metadados
    for (int i = index; i < finger_count - 1; i++) {
        fingerprints[i] = fingerprints[i + 1];
    }
    
    finger_count--;
    saveToNVS();
    
    Serial.println("✅ Metadados removidos");
    return true;
}

bool BiometricManager::deleteFingerprintByID(uint16_t id) {
    int index = findFingerprintIndex(id);
    if (index < 0) return false;
    return deleteFingerprint(index);
}

bool BiometricManager::editFingerprintName(int index, const char* new_name) {
    if (index < 0 || index >= finger_count) return false;
    
    strncpy(fingerprints[index].name, new_name, FINGER_NAME_LENGTH - 1);
    fingerprints[index].name[FINGER_NAME_LENGTH - 1] = '\0';
    saveToNVS();
    
    Serial.printf("✏️ Nome alterado: ID=%d → %s\n", fingerprints[index].id, new_name);
    return true;
}

bool BiometricManager::toggleFingerprintActive(int index) {
    if (index < 0 || index >= finger_count) return false;
    
    fingerprints[index].active = !fingerprints[index].active;
    saveToNVS();
    
    Serial.printf("🔄 ID=%d (%s): %s\n", 
                  fingerprints[index].id,
                  fingerprints[index].name,
                  fingerprints[index].active ? "ATIVADO" : "DESATIVADO");
    return true;
}

// ════════════════════════════════════════════════════════════════
// AUTENTICAÇÃO
// ════════════════════════════════════════════════════════════════

int BiometricManager::verifyFingerprint(uint16_t &id, uint16_t &confidence) {
    if (!finger) return -1;
    
    // Debounce: não verificar se foi lido recentemente (< 2s)
    if (millis() - last_verify_time < 2000) {
        return -1;
    }
    
    // 1. Capturar imagem
    uint8_t p = finger->getImage();
    if (p != FINGERPRINT_OK) return -1;
    
    // 2. Converter para template
    p = finger->image2Tz();
    if (p != FINGERPRINT_OK) return -1;
    
    // 3. Buscar no banco
    p = finger->fingerSearch();
    if (p != FINGERPRINT_OK) return -1;
    
    // 4. Digital encontrada!
    id = finger->fingerID;
    confidence = finger->confidence;
    last_verify_time = millis();
    
    Serial.printf("🔍 Digital encontrada: ID=%d, Confiança=%d\n", id, confidence);
    
    return id;
}

bool BiometricManager::isFingerprintAuthorized(uint16_t id) {
    int index = findFingerprintIndex(id);
    
    if (index < 0) {
        Serial.printf("❌ ID=%d não cadastrado\n", id);
        logAccess(id, "Desconhecido", 0, false);
        return false;
    }
    
    Fingerprint* fp = &fingerprints[index];
    
    if (!fp->active) {
        Serial.printf("❌ ID=%d desativado (%s)\n", id, fp->name);
        logAccess(id, fp->name, 0, false);
        return false;
    }
    
    // Atualizar estatísticas
    fp->access_count++;
    fp->last_access = millis() / 1000;
    saveToNVS();
    
    Serial.printf("✅ Acesso autorizado: %s (ID=%d)\n", fp->name, id);
    logAccess(id, fp->name, fp->confidence, true);
    
    return true;
}

int BiometricManager::findFingerprintIndex(uint16_t id) {
    for (int i = 0; i < finger_count; i++) {
        if (fingerprints[i].id == id) {
            return i;
        }
    }
    return -1;
}

// ════════════════════════════════════════════════════════════════
// AUTENTICAÇÃO CONTÍNUA (NOVO v6.0.22)
// ════════════════════════════════════════════════════════════════

/**
 * @brief Verifica se há dedo no sensor (modo rápido)
 * @return true se há dedo detectado
 */
bool BiometricManager::hasFingerOnSensor() {
    if (!finger) return false;
    
    uint8_t p = finger->getImage();
    return (p == FINGERPRINT_OK);
}

/**
 * @brief Verifica digital e retorna resultado (modo simplificado)
 * @return true se digital foi reconhecida
 * 
 * USO:
 *   if (bioManager.verifyFinger()) {
 *       uint16_t id = bioManager.getLastMatchedID();
 *       uint16_t conf = bioManager.getLastConfidence();
 *       Serial.printf("Reconhecido: ID=%d, Confiança=%d\n", id, conf);
 *   }
 */
bool BiometricManager::verifyFinger() {
    if (!finger) return false;
    
    // 1. Capturar imagem
    uint8_t p = finger->getImage();
    if (p != FINGERPRINT_OK) {
        return false; // Sem dedo ou erro
    }
    
    Serial.println("🔍 [VERIFY] Imagem capturada");
    
    // 2. Converter para template
    p = finger->image2Tz();
    if (p != FINGERPRINT_OK) {
        Serial.printf("❌ [VERIFY] Erro ao processar imagem: %d\n", p);
        return false;
    }
    
    Serial.println("✅ [VERIFY] Template gerado");
    
    // 3. Buscar no banco (1:N) - USAR FAST SEARCH (mais rápido)
    p = finger->fingerFastSearch();
    
    Serial.printf("🔍 [VERIFY] fingerFastSearch() retornou: %d\n", p);
    
    if (p == FINGERPRINT_OK) {
        // ✅ DIGITAL RECONHECIDA!
        uint16_t id = finger->fingerID;
        uint16_t confidence = finger->confidence;
        
        Serial.printf("✅ [VERIFY] Match encontrado! ID=%d, Confiança=%d\n", id, confidence);
        
        // Atualizar cache
        last_verify_time = millis();
        
        // Buscar informações do usuário
        int index = findFingerprintIndex(id);
        
        Serial.printf("🔍 [VERIFY] Buscando metadados... index=%d\n", index);
        
        if (index >= 0) {
            Fingerprint* fp = &fingerprints[index];
            
            Serial.printf("📋 [VERIFY] Metadados: Nome='%s', Ativo=%d\n", fp->name, fp->active);
            
            // Verificar se está ativo
            if (!fp->active) {
                Serial.printf("🔒 Digital reconhecida mas DESATIVADA: %s (ID=%d)\n", 
                              fp->name, id);
                logAccess(id, fp->name, confidence, false);
                return false;
            }
            
            // ✅ ACESSO AUTORIZADO
            fp->access_count++;
            fp->last_access = millis() / 1000;
            fp->confidence = confidence;
            saveToNVS();
            
            Serial.printf("✅ Acesso concedido: %s (ID=%d, Confiança=%d)\n", 
                          fp->name, id, confidence);
            
            logAccess(id, fp->name, confidence, true);
            
            return true;
            
        } else {
            // Digital no sensor mas sem metadados no NVS
            Serial.printf("⚠️  Digital reconhecida (ID=%d) mas sem metadados\n", id);
            logAccess(id, "Sem nome", confidence, false);
            return false;
        }
        
    } else if (p == FINGERPRINT_NOTFOUND) {
        // ❌ DIGITAL NÃO CADASTRADA
        Serial.println("❌ [VERIFY] Digital não reconhecida (FINGERPRINT_NOTFOUND)");
        return false;
        
    } else {
        // ⚠️ ERRO NA BUSCA
        Serial.printf("❌ [VERIFY] Erro na busca: %d\n", p);
        return false;
    }
}

/**
 * @brief Retorna último ID reconhecido
 */
uint16_t BiometricManager::getLastMatchedID() {
    if (!finger) return 0;
    return finger->fingerID;
}

/**
 * @brief Retorna última confiança
 */
uint16_t BiometricManager::getLastConfidence() {
    if (!finger) return 0;
    return finger->confidence;
}

// ════════════════════════════════════════════════════════════════
// CONSULTAS
// ════════════════════════════════════════════════════════════════

int BiometricManager::getCount() {
    return finger_count;
}

int BiometricManager::getActiveCount() {
    int count = 0;
    for (int i = 0; i < finger_count; i++) {
        if (fingerprints[i].active) count++;
    }
    return count;
}

Fingerprint* BiometricManager::getFingerprint(int index) {
    if (index < 0 || index >= finger_count) return nullptr;
    return &fingerprints[index];
}

void BiometricManager::listFingerprints() {
    Serial.println("\n╔══════════════════════════════════════════════╗");
    Serial.println("║       IMPRESSÕES DIGITAIS CADASTRADAS        ║");
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf("║ Total: %d/%d                                 ║\n", finger_count, MAX_FINGERPRINTS);
    Serial.println("╠══════════════════════════════════════════════╣");
    
    for (int i = 0; i < finger_count; i++) {
        Fingerprint* fp = &fingerprints[i];
        Serial.printf("║ [%03d] ID=%03d %-18s %s       ║\n", 
                      i + 1,
                      fp->id,
                      fp->name,
                      fp->active ? "✓" : "✗");
        Serial.printf("║       Acessos: %-4d  Último: %-12lu║\n",
                      fp->access_count,
                      fp->last_access);
        Serial.printf("║       Confiança: %-3d                       ║\n",
                      fp->confidence);
        
        if (i < finger_count - 1) {
            Serial.println("╠──────────────────────────────────────────────╣");
        }
    }
    
    Serial.println("╚══════════════════════════════════════════════╝\n");
}

// ════════════════════════════════════════════════════════════════
// LOGS DE ACESSO
// ════════════════════════════════════════════════════════════════

void BiometricManager::logAccess(uint16_t id, const char* name, uint16_t confidence, bool granted) {
    if (log_count >= MAX_BIO_LOGS) {
        // Remover log mais antigo (FIFO)
        for (int i = 0; i < MAX_BIO_LOGS - 1; i++) {
            logs[i] = logs[i + 1];
        }
        log_count = MAX_BIO_LOGS - 1;
    }
    
    BiometricLog* log = &logs[log_count];
    log->id = id;
    strncpy(log->name, name, FINGER_NAME_LENGTH - 1);
    log->timestamp = millis() / 1000;
    log->confidence = confidence;
    log->granted = granted;
    
    log_count++;
    saveLogsToNVS();
    
    Serial.printf("📝 Log: ID=%d %s [%d] %s\n", 
                  id, name, confidence,
                  granted ? "✅" : "❌");
}

int BiometricManager::getLogCount() {
    return log_count;
}

BiometricLog* BiometricManager::getLog(int index) {
    if (index < 0 || index >= log_count) return nullptr;
    return &logs[index];
}

void BiometricManager::clearLogs() {
    log_count = 0;
    saveLogsToNVS();
    Serial.println("🗑️ Logs limpos");
}

String BiometricManager::logsToJSON() {
    DynamicJsonDocument doc(4096);
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = 0; i < log_count; i++) {
        BiometricLog* log = &logs[i];
        JsonObject obj = array.createNestedObject();
        obj["id"] = log->id;
        obj["name"] = log->name;
        obj["timestamp"] = log->timestamp;
        obj["confidence"] = log->confidence;
        obj["granted"] = log->granted;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// ════════════════════════════════════════════════════════════════
// IMPORTAÇÃO/EXPORTAÇÃO
// ════════════════════════════════════════════════════════════════

String BiometricManager::exportToJSON() {
    DynamicJsonDocument doc(8192);
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = 0; i < finger_count; i++) {
        Fingerprint* fp = &fingerprints[i];
        JsonObject obj = array.createNestedObject();
        obj["id"] = fp->id;
        obj["name"] = fp->name;
        obj["timestamp"] = fp->timestamp;
        obj["active"] = fp->active;
        obj["access_count"] = fp->access_count;
        obj["last_access"] = fp->last_access;
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool BiometricManager::importFromJSON(const String& json) {
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
        if (finger_count >= MAX_FINGERPRINTS) break;
        
        uint16_t id = obj["id"];
        
        // Verificar duplicata
        if (findFingerprintIndex(id) >= 0) continue;
        
        Fingerprint* fp = &fingerprints[finger_count];
        fp->id = id;
        strncpy(fp->name, obj["name"], FINGER_NAME_LENGTH - 1);
        fp->timestamp = obj["timestamp"];
        fp->active = obj["active"];
        fp->access_count = obj["access_count"];
        fp->last_access = obj["last_access"];
        
        finger_count++;
        imported++;
    }
    
    saveToNVS();
    Serial.printf("✅ Importados %d metadados\n", imported);
    return true;
}

void BiometricManager::clearAll() {
    clearAllTemplates();
    finger_count = 0;
    saveToNVS();
    Serial.println("🗑️ Todos os dados removidos");
}

void BiometricManager::clearAllTemplates() {
    if (!finger) return;
    
    finger->emptyDatabase();
    Serial.println("🗑️ Banco de templates limpo");
}

// ════════════════════════════════════════════════════════════════
// MÁQUINA DE ESTADOS - CADASTRO
// ════════════════════════════════════════════════════════════════

void BiometricManager::startEnrollment() {
    if (!isHardwareConnected()) {
        enrollState = BIO_ERROR_HARDWARE;
        return;
    }
    
    if (finger_count >= MAX_FINGERPRINTS) {
        enrollState = BIO_ERROR_FULL;
        return;
    }
    
    // Obter próximo ID livre
    tempID = getFreeID();
    if (tempID == 0) {
        enrollState = BIO_ERROR_FULL;
        return;
    }
    
    enrollState = BIO_WAITING_FINGER_1;
    enrollStartTime = millis();
    Serial.printf("🔵 Iniciando cadastro - ID=%d\n", tempID);
    Serial.println("👆 Posicione o dedo (1/2)...");
}

void BiometricManager::cancelEnrollment() {
    enrollState = BIO_IDLE;
    Serial.println("❌ Cadastro cancelado");
}

void BiometricManager::processEnrollment() {
    // Timeout global de 10 segundos por etapa
    if (enrollState != BIO_IDLE && 
        enrollState != BIO_SUCCESS &&
        enrollState != BIO_AWAITING_NAME &&
        millis() - enrollStartTime > ENROLL_TIMEOUT) {
        enrollState = BIO_ERROR_TIMEOUT;
        Serial.println("❌ Timeout! Tente novamente");
        return;
    }
    
    uint8_t p;
    
    switch (enrollState) {
        case BIO_WAITING_FINGER_1:
            p = finger->getImage();
            if (p == FINGERPRINT_OK) {
                enrollState = BIO_READING_1;
            }
            break;
            
        case BIO_READING_1:
            p = finger->image2Tz(1);
            if (p == FINGERPRINT_OK) {
                Serial.println("✅ 1ª leitura OK!");
                Serial.println("🖐️ Remova o dedo...");
                enrollState = BIO_REMOVE_FINGER;
                enrollStartTime = millis();
            } else {
                enrollState = BIO_ERROR_SENSOR;
            }
            break;
            
        case BIO_REMOVE_FINGER:
            // Aguardar remover dedo
            p = finger->getImage();
            if (p == FINGERPRINT_NOFINGER) {
                Serial.println("👆 Posicione novamente (2/2)...");
                enrollState = BIO_WAITING_FINGER_2;
                enrollStartTime = millis();
            }
            break;
            
        case BIO_WAITING_FINGER_2:
            p = finger->getImage();
            if (p == FINGERPRINT_OK) {
                enrollState = BIO_READING_2;
            }
            break;
            
        case BIO_READING_2:
            p = finger->image2Tz(2);
            if (p == FINGERPRINT_OK) {
                Serial.println("✅ 2ª leitura OK!");
                enrollState = BIO_COMPARING;
            } else {
                enrollState = BIO_ERROR_SENSOR;
            }
            break;
            
        case BIO_COMPARING:
            // Comparar as duas leituras
            p = finger->createModel();
            if (p == FINGERPRINT_OK) {
                Serial.println("✅ Leituras correspondem!");
                enrollState = BIO_CREATING_MODEL;
            } else if (p == FINGERPRINT_ENROLLMISMATCH) {
                enrollState = BIO_ERROR_NO_MATCH;
                Serial.println("❌ Leituras não correspondem!");
            } else {
                enrollState = BIO_ERROR_SENSOR;
            }
            break;
            
        case BIO_CREATING_MODEL:
            // Salvar no sensor
            p = finger->storeModel(tempID);
            if (p == FINGERPRINT_OK) {
                Serial.printf("✅ Salvo no sensor! ID=%d\n", tempID);
                enrollState = BIO_AWAITING_NAME;
                Serial.println("📝 Digite o nome do usuário...");
            } else {
                enrollState = BIO_ERROR_SENSOR;
            }
            break;
            
        case BIO_AWAITING_NAME:
            // Aguarda nome ser inserido externamente
            // (será chamado addFingerprint() após inserir nome)
            break;
            
        default:
            // Estados de erro ou sucesso
            break;
    }
}

String BiometricManager::getEnrollStateString() {
    switch (enrollState) {
        case BIO_IDLE: return "Inativo";
        case BIO_WAITING_FINGER_1: return "Posicione o dedo (1/2)";
        case BIO_READING_1: return "Lendo 1/2...";
        case BIO_REMOVE_FINGER: return "Remova o dedo";
        case BIO_WAITING_FINGER_2: return "Posicione novamente (2/2)";
        case BIO_READING_2: return "Lendo 2/2...";
        case BIO_COMPARING: return "Comparando leituras...";
        case BIO_CREATING_MODEL: return "Criando modelo...";
        case BIO_STORING: return "Salvando...";
        case BIO_AWAITING_NAME: return "Digite o nome";
        case BIO_SUCCESS: return "Cadastrado com sucesso!";
        case BIO_ERROR_TIMEOUT: return "Erro: Timeout";
        case BIO_ERROR_NO_MATCH: return "Erro: Digitais nao correspondem";
        case BIO_ERROR_DUPLICATE: return "Erro: Digital ja existe";
        case BIO_ERROR_FULL: return "Erro: Memoria cheia (127)";
        case BIO_ERROR_SENSOR: return "Erro: Falha no sensor";
        case BIO_ERROR_HARDWARE: return "Erro: AS608 desconectado";
        default: return "Desconhecido";
    }
}

int BiometricManager::getEnrollProgress() {
    switch (enrollState) {
        case BIO_IDLE: return 0;
        case BIO_WAITING_FINGER_1: return 10;
        case BIO_READING_1: return 20;
        case BIO_REMOVE_FINGER: return 35;
        case BIO_WAITING_FINGER_2: return 50;
        case BIO_READING_2: return 65;
        case BIO_COMPARING: return 80;
        case BIO_CREATING_MODEL: return 90;
        case BIO_STORING: return 95;
        case BIO_AWAITING_NAME: return 99;
        case BIO_SUCCESS: return 100;
        default: return 0;
    }
}

// ════════════════════════════════════════════════════════════════
// FUNÇÕES AUXILIARES PRIVADAS
// ════════════════════════════════════════════════════════════════

uint16_t BiometricManager::getFreeID() {
    // Buscar primeiro ID livre (1-127)
    for (uint16_t id = 1; id <= 127; id++) {
        if (!isIDUsed(id)) {
            return id;
        }
    }
    return 0;  // Nenhum ID disponível
}

bool BiometricManager::isIDUsed(uint16_t id) {
    for (int i = 0; i < finger_count; i++) {
        if (fingerprints[i].id == id) {
            return true;
        }
    }
    return false;
}

// ════════════════════════════════════════════════════════════════
// PERSISTÊNCIA NVS
// ════════════════════════════════════════════════════════════════

void BiometricManager::loadFromNVS() {
    preferences.begin("fingerprints", true);
    
    finger_count = preferences.getInt("count", 0);
    
    for (int i = 0; i < finger_count; i++) {
        String key = "fp_" + String(i);
        preferences.getBytes(key.c_str(), &fingerprints[i], sizeof(Fingerprint));
    }
    
    preferences.end();
}

void BiometricManager::saveToNVS() {
    preferences.begin("fingerprints", false);
    
    preferences.putInt("count", finger_count);
    
    for (int i = 0; i < finger_count; i++) {
        String key = "fp_" + String(i);
        preferences.putBytes(key.c_str(), &fingerprints[i], sizeof(Fingerprint));
    }
    
    preferences.end();
}

void BiometricManager::loadLogsFromNVS() {
    preferences.begin("bio_logs", true);
    
    log_count = preferences.getInt("count", 0);
    
    for (int i = 0; i < log_count; i++) {
        String key = "log_" + String(i);
        preferences.getBytes(key.c_str(), &logs[i], sizeof(BiometricLog));
    }
    
    preferences.end();
}

void BiometricManager::saveLogsToNVS() {
    preferences.begin("bio_logs", false);
    
    preferences.putInt("count", log_count);
    
    for (int i = 0; i < log_count; i++) {
        String key = "log_" + String(i);
        preferences.putBytes(key.c_str(), &logs[i], sizeof(BiometricLog));
    }
    
    preferences.end();
}