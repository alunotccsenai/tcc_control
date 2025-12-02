/**
 * @file rfid_manager.h
 * @brief Gerenciador de cartões RFID/NFC com PN532
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * PINAGEM PN532 (SPI - Compartilhado com Display):
 *   SCK     → GPIO12 (FSPICLK) - Compartilhado
 *   MOSI    → GPIO11 (FSPID)   - Compartilhado
 *   MISO    → GPIO13 (FSPIQ)   - Compartilhado
 *   NSS/CS  → GPIO21            - EXCLUSIVO PN532
 *   VCC     → 3.3V-5V
 *   GND     → GND
 * 
 * DIP Switch: CH1=ON, CH2=OFF (modo SPI)
 * 
 * Sistema completo de gerenciamento de cartões RFID com:
 * - Cadastro com nome personalizado
 * - Edição de nomes após cadastro
 * - Armazenamento em NVS (até 50 cartões)
 * - Log de acessos com timestamp
 * - Exportação/importação via JSON
 * - Suporte: Mifare Classic, Ultralight, NTAG, FeliCa
 */

#ifndef RFID_MANAGER_H
#define RFID_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_PN532.h>

// ════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES
// ════════════════════════════════════════════════��═══════════════

#define MAX_RFID_CARDS      50      // Máximo de cartões cadastrados
#define RFID_UID_LENGTH     8       // Comprimento do UID (até 7 bytes + 1 null)
#define RFID_NAME_LENGTH    20      // Comprimento do nome
#define MAX_ACCESS_LOGS     100     // Máximo de logs de acesso

// ════════════════════════════════════════════════════════════════
// ESTRUTURAS
// ════════════════════════════════════════════════════════════════

/**
 * @brief Estrutura de um cartão RFID cadastrado
 */
typedef struct __attribute__((packed)) {
    uint8_t uid[RFID_UID_LENGTH];   // UID do cartão
    uint8_t uid_length;              // Comprimento real do UID (4 ou 7)
    char name[RFID_NAME_LENGTH];     // Nome/descrição do usuário
    uint32_t timestamp;              // Data de cadastro (Unix timestamp)
    bool active;                     // Ativo/inativo
    uint16_t access_count;           // Contador de acessos
    uint32_t last_access;            // Último acesso (Unix timestamp)
} RFIDCard;

/**
 * @brief Log de acesso RFID
 */
typedef struct __attribute__((packed)) {
    uint8_t uid[RFID_UID_LENGTH];   // UID do cartão
    uint8_t uid_length;              // Comprimento do UID
    char name[RFID_NAME_LENGTH];     // Nome do usuário
    uint32_t timestamp;              // Data/hora do acesso
    bool granted;                    // Acesso concedido/negado
} AccessLog;

/**
 * @brief Estados da máquina de cadastro RFID
 */
enum RFIDEnrollState {
    RFID_IDLE,                  // Aguardando comando
    RFID_WAITING_CARD,          // Aguardando aproximar cartão
    RFID_READING,               // Lendo UID
    RFID_CARD_READ,             // Cartão lido, aguardando nome
    RFID_SAVING,                // Salvando no NVS
    RFID_SUCCESS,               // Cadastrado com sucesso
    RFID_ERROR_DUPLICATE,       // Erro: cartão já cadastrado
    RFID_ERROR_FULL,            // Erro: memória cheia
    RFID_ERROR_READ,            // Erro: falha na leitura
    RFID_ERROR_HARDWARE         // Erro: PN532 desconectado
};

// ════════════════════════════════════════════════════════════════
// CLASSE PRINCIPAL
// ════════════════════════════════════════════════════════════════

class RFIDManager {
public:
    // ���══ CONSTRUTOR/DESTRUTOR ═══
    RFIDManager();
    ~RFIDManager();
    
    // ═══ INICIALIZAÇÃO ═══
    bool init();                        // Inicializa PN532 e carrega NVS
    bool isHardwareConnected();         // Verifica se PN532 está conectado
    
    // ═══ GERENCIAMENTO DE CARTÕES ═══
    bool addCard(uint8_t* uid, uint8_t uid_length, const char* name);
    bool removeCard(int index);         // Remove por índice
    bool removeCardByUID(uint8_t* uid, uint8_t uid_length);
    bool editCardName(int index, const char* new_name);
    bool toggleCardActive(int index);   // Ativa/desativa cartão
    
    // ═══ AUTENTICAÇÃO ═══
    bool isCardAuthorized(uint8_t* uid, uint8_t uid_length);
    int findCardIndex(uint8_t* uid, uint8_t uid_length);
    
    // ═══ LEITURA DE CARTÕES ═══
    bool detectCard();                  // Verifica se há cartão presente
    bool readCard(uint8_t* uid, uint8_t* uid_length);  // Lê UID
    
    // ═══ CONSULTAS ═══
    int getCardCount();
    int getActiveCardCount();
    RFIDCard* getCard(int index);
    String uidToString(uint8_t* uid, uint8_t uid_length);
    void listCards();                   // Imprime no Serial
    
    // ═══ LOGS DE ACESSO ═══
    void logAccess(uint8_t* uid, uint8_t uid_length, const char* name, bool granted);
    int getLogCount();
    AccessLog* getLog(int index);
    void clearLogs();
    String logsToJSON();                // Exporta logs em JSON
    
    // ═══ IMPORTAÇÃO/EXPORTAÇÃO ═══
    String exportToJSON();              // Exporta todos os cartões
    bool importFromJSON(const String& json);
    void clearAll();                    // Remove todos os cartões
    
    // ═══ MÁQUINA DE ESTADOS (CADASTRO) ═══
    RFIDEnrollState enrollState;
    uint8_t tempUID[RFID_UID_LENGTH];   // UID temporário durante cadastro
    uint8_t tempUIDLength;
    void startEnrollment();             // Inicia processo de cadastro
    void cancelEnrollment();            // Cancela cadastro
    void processEnrollment();           // Processa estado atual (chamar no loop)
    String getEnrollStateString();      // Retorna string do estado atual
    
private:
    Adafruit_PN532 *pn532;              // Instância do PN532
    Preferences preferences;
    RFIDCard cards[MAX_RFID_CARDS];
    AccessLog logs[MAX_ACCESS_LOGS];
    int card_count;
    int log_count;
    uint32_t last_read_time;            // Debounce de leitura
    
    void loadFromNVS();
    void saveToNVS();
    void loadLogsFromNVS();
    void saveLogsToNVS();
    bool compareUID(uint8_t* uid1, uint8_t* uid2, uint8_t len);
};

// ════════════════════════════════════════════════════════════════
// INSTÂNCIA GLOBAL
// ════════════════════════════════════════════════════════════════

extern RFIDManager rfidManager;

#endif // RFID_MANAGER_H
