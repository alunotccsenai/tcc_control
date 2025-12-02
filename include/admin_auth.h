/**
 * @file admin_auth.h
 * @brief Sistema de autenticação administrativa para ESP32
 * @date 24/11/2025
 * 
 * Gerencia autenticação por PIN para acesso às configurações do sistema.
 * 
 * Características:
 * - PIN de 4 dígitos armazenado em NVS (Preferences)
 * - Controle de tentativas falhadas
 * - Bloqueio temporário após múltiplas falhas
 * - Timeout de sessão automático
 * - Recuperação via Serial Monitor
 * 
 * Uso:
 *   AdminAuth auth;
 *   auth.begin();
 *   if (auth.validate("1234")) {
 *       // Acesso concedido
 *   }
 */

#ifndef ADMIN_AUTH_H
#define ADMIN_AUTH_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ════════════════════════════════════════════════════════════════
// 📊 ESTRUTURA DE ESTADO
// ════════════════════════════════════════════════════════════════

struct AdminAuthState {
    bool authenticated;              // Está autenticado?
    bool enabled;                    // Sistema ativo?
    uint8_t failed_attempts;         // Tentativas falhadas consecutivas
    unsigned long lockout_until;     // Timestamp de fim do bloqueio (millis)
    unsigned long last_activity;     // Última atividade (para timeout)
    char current_pin[ADMIN_PIN_LENGTH + 1];  // PIN atual (+ null terminator)
};

// ════════════════════════════════════════════════════════════════
// 🔐 CLASSE AdminAuth
// ════════════════════════════════════════════════════════════════

class AdminAuth {
public:
    // ──────────────────────────────────────────────────────────────
    // CONSTRUTOR E INICIALIZAÇÃO
    // ──────────────────────────────────────────────────────────────
    
    AdminAuth();
    ~AdminAuth();
    
    /**
     * @brief Inicializa o sistema de autenticação
     */
    void begin();
    
    // ──────────────────────────────────────────────────────────────
    // AUTENTICAÇÃO
    // ──────────────────────────────────────────────────────────────
    
    /**
     * @brief Valida um PIN
     * @param pin PIN de 4 dígitos a validar
     * @return true se o PIN está correto
     */
    bool validate(const char* pin);
    
    /**
     * @brief Verifica se está autenticado
     * @return true se autenticado
     */
    bool isAuthenticated() const;
    
    /**
     * @brief Define estado de autenticação
     * @param auth true para autenticar, false para logout
     */
    void setAuthenticated(bool auth);
    
    /**
     * @brief Faz logout (limpa autenticação)
     */
    void logout();
    
    // ──────────────────────────────────────────────────────────────
    // GERENCIAMENTO DE PIN
    // ──────────────────────────────────────────────────────────────
    
    /**
     * @brief Altera o PIN admin
     * @param currentPin PIN atual (para validação)
     * @param newPin Novo PIN de 4 dígitos
     * @return true se alterado com sucesso
     */
    bool changePin(const char* currentPin, const char* newPin);
    
    /**
     * @brief Reseta PIN para o padrão
     * @return true se resetado com sucesso
     */
    bool resetPin();
    
    /**
     * @brief Obtém o PIN atual (apenas para debug)
     * @return PIN mascarado (ex: "****")
     */
    String getMaskedPin() const;
    
    // ──────────────────────────────────────────────────────────────
    // CONTROLE DE BLOQUEIO
    // ──────────────────────────────────────────────────────────────
    
    /**
     * @brief Verifica se está bloqueado
     * @return true se bloqueado
     */
    bool isLocked() const;
    
    /**
     * @brief Obtém tempo restante de bloqueio
     * @return Segundos restantes (0 se não bloqueado)
     */
    uint32_t getLockoutTimeRemaining() const;
    
    /**
     * @brief Registra tentativa falhada
     */
    void recordFailedAttempt();
    
    /**
     * @brief Reseta contador de tentativas
     */
    void resetAttempts();
    
    /**
     * @brief Obtém número de tentativas falhadas
     * @return Número de tentativas
     */
    uint8_t getFailedAttempts() const;
    
    /**
     * @brief Obtém tentativas restantes
     * @return Número de tentativas restantes
     */
    uint8_t getRemainingAttempts() const;
    
    // ──────────────────────────────────────────────────────────────
    // CONFIGURAÇÃO
    // ──────────────────────────────────────────────────────────────
    
    /**
     * @brief Habilita/desabilita sistema de autenticação
     * @param enabled true para habilitar
     */
    void setEnabled(bool enabled);
    
    /**
     * @brief Verifica se sistema está habilitado
     * @return true se habilitado
     */
    bool isEnabled() const;
    
    // ──────────────────────────────────────────────────────────────
    // SESSÃO E TIMEOUT
    // ──────────────────────────────────────────────────────────────
    
    /**
     * @brief Atualiza timestamp de última atividade
     */
    void updateActivity();
    
    /**
     * @brief Verifica timeout de sessão
     * @return true se sessão expirou
     */
    bool checkTimeout();
    
    /**
     * @brief Obtém tempo restante de sessão
     * @return Segundos restantes (0 se não autenticado)
     */
    uint32_t getSessionTimeRemaining() const;
    
    // ──────────────────────────────────────────────────────────────
    // PERSISTÊNCIA
    // ──────────────────────────────────────────────────────────────
    
    /**
     * @brief Carrega configuração do NVS
     * @return true se carregou com sucesso
     */
    bool load();
    
    /**
     * @brief Salva configuração no NVS
     * @return true se salvou com sucesso
     */
    bool save();
    
    // ──────────────────────────────────────────────────────────────
    // DEBUG E DIAGNÓSTICO
    // ──────────────────────────────────────────────────────────────
    
    /**
     * @brief Imprime estado atual no Serial
     */
    void printStatus() const;
    
    /**
     * @brief Obtém estado completo
     * @return Estrutura AdminAuthState
     */
    AdminAuthState getState() const;

private:
    Preferences preferences;
    AdminAuthState state;
    
    // Helpers internos
    bool validatePinFormat(const char* pin) const;
    bool isEmergencyPin(const char* pin) const;
    void lockAccount();
    void unlockAccount();
};

// ════════════════════════════════════════════════════════════════
// 🌍 INSTÂNCIA GLOBAL (declarada em main.cpp)
// ════════════════════════════════════════════════════════════════

extern AdminAuth adminAuth;

#endif // ADMIN_AUTH_H
