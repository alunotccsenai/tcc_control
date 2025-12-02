/**
 * @file biometric_manager.h
 * @brief Gerenciador de impressões digitais AS608
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * PINAGEM AS608 (UART2):
 *   RX ESP32 ← TX AS608 (Blue)  → GPIO01 (RTC_GPIO1, ADC1_CH0)
 *   TX ESP32 → RX AS608 (Green) → GPIO02 (RTC_GPIO2, ADC1_CH1)
 *   VCC (Red)   → 3.3V (VDD3P3)
 *   GND (Black) → GND
 * 
 * Baud Rate: 57600 bps
 * Capacidade: 300 impressões digitais
 * 
 * Sistema completo de gerenciamento de biometria com:
 * - Cadastro com 2 leituras + nome personalizado
 * - Edição de nomes após cadastro
 * - Armazenamento no sensor AS608 + metadados em NVS
 * - Log de acessos com timestamp
 * - Exportação/importação de metadados via JSON
 */

#ifndef BIOMETRIC_MANAGER_H
#define BIOMETRIC_MANAGER_H

#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES
// ════════════════════════════════════════════════════════════════

#define MAX_FINGERPRINTS    127             // Limite do AS608
#define FINGER_NAME_LENGTH  20              // Comprimento do nome
#define MAX_BIO_LOGS        100             // Máximo de logs de acesso
#define ENROLL_TIMEOUT      10000           // Timeout de cadastro (10s)

// ════════════════════════════════════════════════════════════════
// ESTRUTURAS
// ════════════════════════════════════════════════════════════════

/**
 * @brief Estrutura de impressão digital cadastrada (metadados)
 */
typedef struct __attribute__((packed)) {
    uint16_t id;                        // ID no sensor (1-127)
    char name[FINGER_NAME_LENGTH];      // Nome do usuário
    uint32_t timestamp;                 // Data de cadastro (Unix timestamp)
    bool active;                        // Ativo/inativo
    uint16_t access_count;              // Contador de acessos
    uint32_t last_access;               // Último acesso (Unix timestamp)
    uint16_t confidence;                // Confiança da última leitura (0-255)
} Fingerprint;

/**
 * @brief Log de acesso biométrico
 */
typedef struct __attribute__((packed)) {
    uint16_t id;                        // ID da digital
    char name[FINGER_NAME_LENGTH];      // Nome do usuário
    uint32_t timestamp;                 // Data/hora do acesso
    uint16_t confidence;                // Confiança da leitura (0-255)
    bool granted;                       // Acesso concedido/negado
} BiometricLog;

/**
 * @brief Estados da máquina de cadastro de digital
 */
enum BiometricEnrollState {
    BIO_IDLE,                       // Aguardando comando
    BIO_WAITING_FINGER_1,           // Aguardando 1ª leitura
    BIO_READING_1,                  // Processando 1ª imagem
    BIO_REMOVE_FINGER,              // Remover dedo (entre leituras)
    BIO_WAITING_FINGER_2,           // Aguardando 2ª leitura
    BIO_READING_2,                  // Processando 2ª imagem
    BIO_COMPARING,                  // Comparando leituras
    BIO_CREATING_MODEL,             // Criando modelo
    BIO_STORING,                    // Salvando no sensor
    BIO_AWAITING_NAME,              // Aguardando nome do usuário
    BIO_SUCCESS,                    // Cadastro concluído
    BIO_ERROR_TIMEOUT,              // Erro: timeout
    BIO_ERROR_NO_MATCH,             // Erro: leituras não correspondem
    BIO_ERROR_DUPLICATE,            // Erro: digital já cadastrada
    BIO_ERROR_FULL,                 // Erro: memória cheia (127)
    BIO_ERROR_SENSOR,               // Erro: falha no sensor
    BIO_ERROR_HARDWARE              // Erro: AS608 desconectado
};

// ════════════════════════════════════════════════════════════════
// CLASSE PRINCIPAL
// ════════════════════════════════════════════════════════════════

class BiometricManager {
public:
    // ═══ CONSTRUTOR/DESTRUTOR ═══
    BiometricManager();
    ~BiometricManager();
    
    // ═══ INICIALIZAÇÃO ═══
    bool init();                        // Inicializa AS608 e carrega NVS
    bool isHardwareConnected();         // Verifica se AS608 está respondendo
    uint16_t getSensorTemplateCount();  // Quantidade no sensor
    
    // ═══ GERENCIAMENTO DE DIGITAIS ═══
    bool addFingerprint(uint16_t id, const char* name);  // Adiciona metadados
    bool deleteFingerprint(int index);  // Remove por índice (sensor + NVS)
    bool deleteFingerprintByID(uint16_t id);
    bool editFingerprintName(int index, const char* new_name);
    bool toggleFingerprintActive(int index);  // Ativa/desativa
    
    // ═══ AUTENTICAÇÃO ═══
    int verifyFingerprint(uint16_t &id, uint16_t &confidence);  // -1=não encontrado
    bool isFingerprintAuthorized(uint16_t id);
    int findFingerprintIndex(uint16_t id);
    
    // ═══ AUTENTICAÇÃO CONTÍNUA (NOVO v6.0.22) ═══
    bool verifyFinger();                    // Verifica digital (retorna true se reconhecida)
    uint16_t getLastMatchedID();            // Retorna último ID reconhecido
    uint16_t getLastConfidence();           // Retorna última confiança
    bool hasFingerOnSensor();               // Verifica se há dedo no sensor
    
    // ═══ CONSULTAS ═══
    int getCount();                     // Total de metadados
    int getActiveCount();
    Fingerprint* getFingerprint(int index);
    void listFingerprints();            // Imprime no Serial
    
    // ═══ LOGS DE ACESSO ═══
    void logAccess(uint16_t id, const char* name, uint16_t confidence, bool granted);
    int getLogCount();
    BiometricLog* getLog(int index);
    void clearLogs();
    String logsToJSON();                // Exporta logs em JSON
    
    // ═══ IMPORTAÇÃO/EXPORTAÇÃO ═══
    String exportToJSON();              // Exporta metadados (não exporta templates!)
    bool importFromJSON(const String& json);
    void clearAll();                    // Remove tudo (sensor + NVS)
    void clearAllTemplates();           // Limpa banco do sensor
    
    // ═══ MÁQUINA DE ESTADOS (CADASTRO) ═══
    BiometricEnrollState enrollState;
    uint16_t tempID;                    // ID temporário durante cadastro
    uint32_t enrollStartTime;           // Timestamp de início
    void startEnrollment();             // Inicia processo
    void cancelEnrollment();            // Cancela
    void processEnrollment();           // Processa estado (chamar no loop)
    String getEnrollStateString();      // Retorna string do estado
    int getEnrollProgress();            // Retorna progresso 0-100%
    
private:
    Adafruit_Fingerprint *finger;
    Preferences preferences;
    Fingerprint fingerprints[MAX_FINGERPRINTS];
    BiometricLog logs[MAX_BIO_LOGS];
    int finger_count;
    int log_count;
    uint32_t last_verify_time;          // Debounce de verificação
    
    void loadFromNVS();
    void saveToNVS();
    void loadLogsFromNVS();
    void saveLogsToNVS();
    uint16_t getFreeID();               // Retorna próximo ID livre (1-127)
    bool isIDUsed(uint16_t id);         // Verifica se ID está em uso
};

// ════════════════════════════════════════════════════════════════
// INSTÂNCIA GLOBAL
// ════════════════════════════════════════════════════════════════

extern BiometricManager bioManager;

#endif // BIOMETRIC_MANAGER_H