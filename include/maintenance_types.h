/**
 * @file maintenance_types.h
 * @brief Tipos e estruturas para sistema de requisição de manutenção
 * @version 1.0.0
 * @date 2025-11-24
 * 
 * Este header define todas as estruturas de dados, enumerações e funções
 * auxiliares para o sistema de requisição de manutenção.
 */

#ifndef MAINTENANCE_TYPES_H
#define MAINTENANCE_TYPES_H

#include <Arduino.h>

// ════════════════════════════════════════════════════════════════
// ENUMERAÇÕES
// ════════════════════════════════════════════════════════════════

/**
 * @brief Locais disponíveis para requisição de manutenção
 * @note Índice 0 = "Selecione..." (inválido)
 */
typedef enum {
    LOCAL_NONE = 0,                    // Não selecionado
    LOCAL_SALA_ELETRONICA_DIGITAL,     // 1
    LOCAL_SALA_ELETRONICA_ANALOGICA,   // 2
    LOCAL_SALA_PNEUMATICA,             // 3
    LOCAL_SALA_ELETRICA,               // 4
    LOCAL_OUTRO,                       // 5
    LOCAL_MAX                          // Limite
} LocalManutencao;

/**
 * @brief Níveis de prioridade da requisição
 * @note Afeta ordem de atendimento e cor visual
 */
typedef enum {
    PRIORIDADE_NONE = 0,          // Não selecionada
    PRIORIDADE_BAIXA,             // 1 - Verde  - Pode aguardar
    PRIORIDADE_MEDIA,             // 2 - Amarelo - Resolver em breve
    PRIORIDADE_ALTA,              // 3 - Laranja - Urgente
    PRIORIDADE_CRITICA,           // 4 - Vermelho - Emergência
    PRIORIDADE_MAX                // Limite
} PrioridadeManutencao;

/**
 * @brief Status da requisição no sistema
 */
typedef enum {
    STATUS_PENDENTE = 0,          // Criada, aguardando envio
    STATUS_ENVIADA,               // E-mail enviado com sucesso
    STATUS_ERRO_ENVIO,            // Falha no envio (tentar novamente)
    STATUS_ATENDIDA,              // Manutenção realizada
    STATUS_CANCELADA              // Cancelada pelo usuário
} StatusRequisicao;

// ════════════════════════════════════════════════════════════════
// ESTRUTURA PRINCIPAL
// ════════════════════════════════════════════════════════════════

/**
 * @brief Estrutura completa de uma requisição de manutenção
 * @note Tamanho aproximado: 384 bytes
 * @warning Mantenha alinhamento para compatibilidade NVS
 */
typedef struct __attribute__((packed)) {
    // ===== IDENTIFICAÇÃO =====
    uint32_t id;                    // ID único (auto-incremento)
    char uuid[37];                  // UUID v4 (opcional, formato: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx)
    
    // ===== TIMESTAMP =====
    time_t timestamp;               // Unix timestamp (segundos desde 1970-01-01)
    char datetime[20];              // Formato: "YYYY-MM-DD HH:MM:SS"
    
    // ===== DADOS DO FORMULÁRIO =====
    char problema[201];             // Descrição do problema (200 chars + \0)
    LocalManutencao local;          // Enum do local selecionado
    char local_nome[50];            // Nome do local por extenso
    PrioridadeManutencao prioridade; // Enum da prioridade
    char prioridade_nome[30];       // Nome da prioridade por extenso
    char contato[51];               // Contato opcional (50 chars + \0)
    
    // ===== METADADOS DO SISTEMA =====
    char ip_origem[16];             // IP do ESP32 (formato: xxx.xxx.xxx.xxx)
    char mac_address[18];           // MAC address (formato: XX:XX:XX:XX:XX:XX)
    uint8_t versao_firmware;        // Versão do firmware (ex: 1, 2, 3...)
    uint8_t padding1;               // Padding para alinhamento
    uint16_t padding2;              // Padding para alinhamento
    
    // ===== STATUS E CONTROLE =====
    StatusRequisicao status;        // Status atual da requisição
    bool email_enviado;             // Flag: e-mail foi enviado com sucesso?
    uint8_t tentativas_envio;       // Número de tentativas de envio
    uint8_t padding3;               // Padding
    time_t ultima_tentativa;        // Timestamp da última tentativa de envio
    
} MaintenanceRequest;

// ════════════════════════════════════════════════════════════════
// FUNÇÕES AUXILIARES
// ════════════════════════════════════════════════════════════════

/**
 * @brief Converte enum de local para string legível
 * @param local Enum LocalManutencao
 * @return String com nome do local
 */
inline const char* localToString(LocalManutencao local) {
    switch(local) {
        case LOCAL_SALA_ELETRONICA_DIGITAL:   return "Sala - Eletrônica Digital";
        case LOCAL_SALA_ELETRONICA_ANALOGICA: return "Sala - Eletrônica Analógica";
        case LOCAL_SALA_PNEUMATICA:           return "Sala - Pneumática";
        case LOCAL_SALA_ELETRICA:             return "Sala - Elétrica";
        case LOCAL_OUTRO:                     return "Outro";
        default:                              return "Não especificado";
    }
}

/**
 * @brief Converte enum de prioridade para string legível
 * @param prioridade Enum PrioridadeManutencao
 * @return String com nome da prioridade
 */
inline const char* prioridadeToString(PrioridadeManutencao prioridade) {
    switch(prioridade) {
        case PRIORIDADE_BAIXA:    return "Baixa - Pode aguardar";
        case PRIORIDADE_MEDIA:    return "Média - Resolver em breve";
        case PRIORIDADE_ALTA:     return "Alta - Urgente";
        case PRIORIDADE_CRITICA:  return "Crítica - Emergência";
        default:                  return "Não especificada";
    }
}

/**
 * @brief Converte enum de prioridade para cor hexadecimal
 * @param prioridade Enum PrioridadeManutencao
 * @return String com código de cor HTML (ex: "#10B981")
 */
inline const char* prioridadeToColor(PrioridadeManutencao prioridade) {
    switch(prioridade) {
        case PRIORIDADE_BAIXA:    return "#10B981";  // Verde
        case PRIORIDADE_MEDIA:    return "#FBBF24";  // Amarelo
        case PRIORIDADE_ALTA:     return "#F97316";  // Laranja
        case PRIORIDADE_CRITICA:  return "#EF4444";  // Vermelho
        default:                  return "#6B7280";  // Cinza
    }
}

/**
 * @brief Converte enum de prioridade para cor LVGL
 * @param prioridade Enum PrioridadeManutencao
 * @return Código de cor em formato uint32_t para LVGL
 */
inline uint32_t prioridadeToColorLVGL(PrioridadeManutencao prioridade) {
    switch(prioridade) {
        case PRIORIDADE_BAIXA:    return 0x10B981;  // Verde
        case PRIORIDADE_MEDIA:    return 0xFBBF24;  // Amarelo
        case PRIORIDADE_ALTA:     return 0xF97316;  // Laranja
        case PRIORIDADE_CRITICA:  return 0xEF4444;  // Vermelho
        default:                  return 0x6B7280;  // Cinza
    }
}

/**
 * @brief Converte enum de status para string legível
 * @param status Enum StatusRequisicao
 * @return String com nome do status
 */
inline const char* statusToString(StatusRequisicao status) {
    switch(status) {
        case STATUS_PENDENTE:     return "Pendente";
        case STATUS_ENVIADA:      return "Enviada";
        case STATUS_ERRO_ENVIO:   return "Erro no envio";
        case STATUS_ATENDIDA:     return "Atendida";
        case STATUS_CANCELADA:    return "Cancelada";
        default:                  return "Desconhecido";
    }
}

/**
 * @brief Inicializa uma requisição com valores padrão
 * @param req Ponteiro para a estrutura MaintenanceRequest
 */
inline void inicializarRequisicao(MaintenanceRequest* req) {
    if (req == nullptr) return;
    
    // Zera toda a estrutura
    memset(req, 0, sizeof(MaintenanceRequest));
    
    // Define valores padrão
    req->id = 0;
    req->timestamp = 0;
    req->local = LOCAL_NONE;
    req->prioridade = PRIORIDADE_NONE;
    req->status = STATUS_PENDENTE;
    req->email_enviado = false;
    req->tentativas_envio = 0;
    req->ultima_tentativa = 0;
    req->versao_firmware = 1;
}

/**
 * @brief Valida se uma requisição está completa e válida
 * @param req Ponteiro para a estrutura MaintenanceRequest
 * @return true se válida, false caso contrário
 */
inline bool validarRequisicao(const MaintenanceRequest* req) {
    if (req == nullptr) return false;
    
    // 1. Problema deve ter pelo menos 10 caracteres
    if (strlen(req->problema) < 10) {
        Serial.println("❌ Validação: Problema muito curto");
        return false;
    }
    
    // 2. Local deve ser selecionado (não pode ser NONE)
    if (req->local == LOCAL_NONE || req->local >= LOCAL_MAX) {
        Serial.println("❌ Validação: Local não selecionado");
        return false;
    }
    
    // 3. Prioridade deve ser selecionada
    if (req->prioridade == PRIORIDADE_NONE || req->prioridade >= PRIORIDADE_MAX) {
        Serial.println("❌ Validação: Prioridade não selecionada");
        return false;
    }
    
    // 4. Timestamp deve ser válido (> 0)
    if (req->timestamp == 0) {
        Serial.println("❌ Validação: Timestamp inválido");
        return false;
    }
    
    // 5. Contato é opcional, mas se preenchido deve ter min 3 chars
    if (req->contato[0] != '\0' && strlen(req->contato) < 3) {
        Serial.println("⚠️ Aviso: Contato muito curto (ignorado)");
        // Não invalida, apenas avisa
    }
    
    Serial.println("✅ Validação: Requisição OK");
    return true;
}

/**
 * @brief Imprime requisição no Serial Monitor (debug)
 * @param req Ponteiro para a estrutura MaintenanceRequest
 */
inline void printRequisicao(const MaintenanceRequest* req) {
    if (req == nullptr) return;
    
    Serial.println("\n╔══════════════════════════════════════════════╗");
    Serial.println("║       REQUISIÇÃO DE MANUTENÇÃO              ║");
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf( "║ ID:          #%05u                         ║\n", req->id);
    Serial.printf( "║ Data/Hora:   %-30s║\n", req->datetime);
    Serial.printf( "║ Local:       %-30s║\n", req->local_nome);
    Serial.printf( "║ Prioridade:  %-30s║\n", req->prioridade_nome);
    Serial.printf( "║ Status:      %-30s║\n", statusToString(req->status));
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf( "║ Problema:                                    ║\n");
    Serial.printf( "║ %s\n", req->problema);
    Serial.println("╠══════════════════════════════════════════════╣");
    
    if (strlen(req->contato) > 0) {
        Serial.printf("║ Contato:     %-30s║\n", req->contato);
    }
    
    Serial.printf( "║ IP:          %-30s║\n", req->ip_origem);
    Serial.printf( "║ MAC:         %-30s║\n", req->mac_address);
    Serial.printf( "║ E-mail:      %s                           ║\n", 
                   req->email_enviado ? "Enviado ✅" : "Pendente ⏳");
    Serial.println("╚══════════════════════════════════════════════╝\n");
}

#endif // MAINTENANCE_TYPES_H