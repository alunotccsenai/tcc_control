/**
 * @file biometric_storage.h
 * @brief Sistema de armazenamento persistente para metadados biométricos (LittleFS)
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * Gerencia metadados de impressões digitais cadastradas usando LittleFS.
 * Sincronizado com BiometricManager para armazenamento permanente.
 */

#ifndef BIOMETRIC_STORAGE_H
#define BIOMETRIC_STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES
// ═══════════════════════════════════════════════════════════════════════

#define BIOMETRIC_STORAGE_FILE  "/biometric_users.json"
#define MAX_FINGERPRINTS        127

// ═══════════════════════════════════════════════════════════════════════
// ESTRUTURAS
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Estrutura de metadados de usuário biométrico
 */
struct BiometricUser {
    uint16_t slotId;            // ID no sensor AS608 (1-127)
    String userId;              // ID único do usuário
    String userName;            // Nome do usuário
    uint32_t registeredAt;      // Timestamp de cadastro
    uint32_t lastAccess;        // Timestamp do último acesso
    uint16_t accessCount;       // Contador de acessos
    uint16_t confidence;        // Confiança da última leitura (0-255)
    bool active;                // Ativo/inativo
};

// ═══════════════════════════════════════════════════════════════════════
// CLASSE BIOMETRICSTORAGE
// ═══════════════════════════════════════════════════════════════════════

class BiometricStorage {
public:
    /**
     * @brief Construtor
     */
    BiometricStorage();
    
    /**
     * @brief Inicializa LittleFS e carrega dados
     * @return true se inicializou com sucesso
     */
    bool begin();
    
    /**
     * @brief Adiciona novo usuário
     * @param user Estrutura do usuário
     * @return true se adicionado com sucesso
     */
    bool addUser(const BiometricUser& user);
    
    /**
     * @brief Remove usuário por slot ID
     * @param slotId ID do slot no sensor
     * @return true se removido com sucesso
     */
    bool removeUser(uint16_t slotId);
    
    /**
     * @brief Atualiza nome do usuário
     * @param slotId ID do slot
     * @param newName Novo nome
     * @return true se atualizado com sucesso
     */
    bool updateUserName(uint16_t slotId, const String& newName);
    
    /**
     * @brief Atualiza último acesso
     * @param slotId ID do slot
     * @param confidence Confiança da leitura
     * @return true se atualizado com sucesso
     */
    bool updateLastAccess(uint16_t slotId, uint16_t confidence);
    
    /**
     * @brief Busca usuário por slot ID
     * @param slotId ID do slot
     * @return Ponteiro para usuário (nullptr se não encontrado)
     */
    BiometricUser* getUserBySlot(uint16_t slotId);
    
    /**
     * @brief Retorna todos os usuários
     * @return Vetor com todos os usuários
     */
    std::vector<BiometricUser> getAllUsers();
    
    /**
     * @brief Retorna quantidade de usuários
     * @return Número de usuários
     */
    int count();
    
    /**
     * @brief Retorna próximo slot livre
     * @return ID do próximo slot livre (1-127, ou >127 se cheio)
     */
    uint16_t getNextFreeSlot();
    
    /**
     * @brief Limpa todos os usuários (CUIDADO!)
     * @return true se limpou com sucesso
     */
    bool clearAll();
    
    /**
     * @brief Exporta dados em JSON
     * @return String JSON com todos os usuários
     */
    String exportJSON();
    
    /**
     * @brief Importa dados de JSON
     * @param json String JSON
     * @return true se importou com sucesso
     */
    bool importJSON(const String& json);

private:
    std::vector<BiometricUser> users;
    bool initialized;
    
    /**
     * @brief Carrega dados do arquivo
     * @return true se carregou com sucesso
     */
    bool load();
    
    /**
     * @brief Salva dados no arquivo
     * @return true se salvou com sucesso
     */
    bool save();
    
    /**
     * @brief Busca índice do usuário por slot ID
     * @param slotId ID do slot
     * @return Índice (-1 se não encontrado)
     */
    int findUserIndex(uint16_t slotId);
};

#endif // BIOMETRIC_STORAGE_H
