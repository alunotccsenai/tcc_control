/**
 * @file rfid_storage.h
 * @brief Sistema de armazenamento persistente para cartões RFID (LittleFS)
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * Gerencia metadados de cartões RFID cadastrados usando LittleFS.
 * Sincronizado com RFIDManager para armazenamento permanente.
 */

#ifndef RFID_STORAGE_H
#define RFID_STORAGE_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <vector>

// ═══════════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES
// ═══════════════════════════════════════════════════════════════════════

#define RFID_STORAGE_FILE   "/rfid_cards.json"
#define MAX_RFID_CARDS      50

// ═══════════════════════════════════════════════════════════════════════
// ESTRUTURAS
// ═══════════════════════════════════════════════════════════════════════

/**
 * @brief Estrutura de metadados de cartão RFID
 */
struct RFIDCard {
    String uid;                 // UID do cartão (formato: XX:XX:XX:XX)
    String userName;            // Nome do usuário
    uint32_t registeredAt;      // Timestamp de cadastro
    uint32_t lastAccess;        // Timestamp do último acesso
    uint16_t accessCount;       // Contador de acessos
    bool active;                // Ativo/inativo
};

// ═══════════════════════════════════════════════════════════════════════
// CLASSE RFIDSTORAGE
// ═══════════════════════════════════════════════════════════════════════

class RFIDStorage {
public:
    /**
     * @brief Construtor
     */
    RFIDStorage();
    
    /**
     * @brief Inicializa LittleFS e carrega dados
     * @return true se inicializou com sucesso
     */
    bool begin();
    
    /**
     * @brief Adiciona novo cartão
     * @param uid UID do cartão (formato XX:XX:XX:XX)
     * @param userName Nome do usuário
     * @return true se adicionado com sucesso
     */
    bool addCard(const String& uid, const String& userName);
    
    /**
     * @brief Remove cartão por UID
     * @param uid UID do cartão
     * @return true se removido com sucesso
     */
    bool removeCard(const String& uid);
    
    /**
     * @brief Atualiza nome do usuário
     * @param uid UID do cartão
     * @param newName Novo nome
     * @return true se atualizado com sucesso
     */
    bool updateUserName(const String& uid, const String& newName);
    
    /**
     * @brief Atualiza timestamp de último acesso
     * @param uid UID do cartão
     * @return true se atualizado com sucesso
     */
    bool updateLastAccess(const String& uid);
    
    /**
     * @brief Verifica se cartão está cadastrado
     * @param uid UID do cartão
     * @return true se está cadastrado e ativo
     */
    bool isCardRegistered(const String& uid);
    
    /**
     * @brief Busca nome do usuário por UID
     * @param uid UID do cartão
     * @return Nome do usuário (vazio se não encontrado)
     */
    String getUserName(const String& uid);
    
    /**
     * @brief Retorna todos os cartões cadastrados
     * @return Vetor com todos os cartões
     */
    std::vector<RFIDCard> getAllCards();
    
    /**
     * @brief Retorna quantidade de cartões cadastrados
     * @return Número de cartões
     */
    int count();
    
    /**
     * @brief Limpa todos os cartões (CUIDADO!)
     * @return true se limpou com sucesso
     */
    bool clearAll();
    
    /**
     * @brief Exporta dados em JSON
     * @return String JSON com todos os cartões
     */
    String exportJSON();
    
    /**
     * @brief Importa dados de JSON
     * @param json String JSON
     * @return true se importou com sucesso
     */
    bool importJSON(const String& json);

private:
    std::vector<RFIDCard> cards;
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
     * @brief Busca índice do cartão por UID
     * @param uid UID do cartão
     * @return Índice (-1 se não encontrado)
     */
    int findCardIndex(const String& uid);
};

#endif // RFID_STORAGE_H
