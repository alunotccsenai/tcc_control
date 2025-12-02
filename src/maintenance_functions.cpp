/**
 * @file maintenance_functions.cpp
 * @brief Implementação das funções do sistema de requisição de manutenção
 * @version 1.0.0
 * @date 2025-11-24
 * 
 * Este arquivo contém todas as funções auxiliares para o sistema de
 * requisição de manutenção, incluindo validação, salvamento e envio de e-mail.
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <lvgl.h>
#include "maintenance_types.h"
#include "smtp_config.h"

// Referências externas
extern lv_obj_t * manut_keyboard;
extern lv_obj_t * manut_label_status;
extern lv_obj_t * campo_com_foco;
extern lv_obj_t * manut_textarea_problema;
extern lv_obj_t * manut_dropdown_local;
extern lv_obj_t * manut_dropdown_prioridade;
extern lv_obj_t * manut_textarea_contato;
extern MaintenanceRequest currentRequest;
extern uint32_t maintenance_id_counter;

// Forward declaration do enum Screen do main.cpp
enum Screen {
    SCREEN_HOME = 0
};

extern void mudar_tela(Screen screen);
extern lv_obj_t* lv_scr_act();

// Cores (definidas no main)
#define COLOR_ERROR      0xF44336
#define COLOR_SUCCESS    0x4CAF50
#define COLOR_WARNING    0xFF9800

// ════════════════════════════════════════════════════════════════
// FUNÇÕES AUXILIARES
// ════════════════════════════════════════════════════════════════

/**
 * @brief Evento do teclado (detecta OK/Cancel para fechar)
 */
void evento_teclado_manut(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    // Fecha o teclado quando o usuário clica em OK ou Cancel
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (manut_keyboard) {
            lv_obj_add_flag(manut_keyboard, LV_OBJ_FLAG_HIDDEN);
            Serial.println("⌨️ Teclado virtual fechado");
        }
        campo_com_foco = NULL;
    }
}

/**
 * @brief Evento quando campo perde foco (fecha teclado)
 */
void evento_defocus_campo_manut(lv_event_t * e) {
    // Fecha teclado quando campo perde foco
    if (manut_keyboard) {
        lv_obj_add_flag(manut_keyboard, LV_OBJ_FLAG_HIDDEN);
        Serial.println("⌨️ Teclado virtual fechado (defocus)");
    }
    campo_com_foco = NULL;
}

/**
 * @brief Evento quando um campo recebe foco (abre teclado)
 */
void evento_foco_campo_manut(lv_event_t * e) {
    lv_obj_t * ta = lv_event_get_target(e);
    campo_com_foco = ta;
    
    if (manut_keyboard == NULL) {
        manut_keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(manut_keyboard, 480, 140);
        lv_obj_set_style_bg_color(manut_keyboard, lv_color_hex(0x1a1a2e), 0);
        
        // ⭐ ADICIONA EVENTO PARA FECHAR O TECLADO
        lv_obj_add_event_cb(manut_keyboard, evento_teclado_manut, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(manut_keyboard, evento_teclado_manut, LV_EVENT_CANCEL, NULL);
    }
    
    // Posiciona teclado na parte inferior
    lv_obj_clear_flag(manut_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(manut_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(manut_keyboard, ta);
    
    Serial.println("⌨️ Teclado virtual aberto");
}

/**
 * @brief Mostra mensagem de erro no formulário
 */
void mostrar_erro_manutencao(const char* mensagem) {
    if (manut_label_status != NULL) {
        lv_label_set_text(manut_label_status, mensagem);
        lv_obj_set_style_text_color(manut_label_status, lv_color_hex(COLOR_ERROR), 0);
        lv_obj_clear_flag(manut_label_status, LV_OBJ_FLAG_HIDDEN);
    }
    Serial.printf("❌ Erro: %s\\n", mensagem);
}

/**
 * @brief Mostra mensagem de status no formulário
 */
void mostrar_status_manutencao(const char* mensagem, uint32_t cor) {
    if (manut_label_status != NULL) {
        lv_label_set_text(manut_label_status, mensagem);
        lv_obj_set_style_text_color(manut_label_status, lv_color_hex(cor), 0);
        lv_obj_clear_flag(manut_label_status, LV_OBJ_FLAG_HIDDEN);
    }
    Serial.printf("ℹ️ Status: %s\\n", mensagem);
}

/**
 * @brief Evento cancelar requisição (limpa campos e volta)
 */
void evento_cancelar_requisicao(lv_event_t * e) {
    Serial.println("❌ Cancelando requisição");
    
    // Limpa todos os campos
    if (manut_textarea_problema) lv_textarea_set_text(manut_textarea_problema, "");
    if (manut_dropdown_local) lv_dropdown_set_selected(manut_dropdown_local, 0);
    if (manut_dropdown_prioridade) lv_dropdown_set_selected(manut_dropdown_prioridade, 0);
    if (manut_textarea_contato) lv_textarea_set_text(manut_textarea_contato, "");
    
    // Esconde teclado e mensagens
    if (manut_keyboard) lv_obj_add_flag(manut_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (manut_label_status) lv_obj_add_flag(manut_label_status, LV_OBJ_FLAG_HIDDEN);
    
    // Volta para HOME
    mudar_tela(SCREEN_HOME); // SCREEN_HOME = 0
}

/**
 * @brief Salva requisição no NVS (memória não-volátil)
 */
bool salvar_requisicao_nvs(const MaintenanceRequest* req) {
    Preferences prefs;
    
    if (!prefs.begin("manutencao", false)) {
        Serial.println("❌ Erro ao abrir namespace NVS 'manutencao'");
        return false;
    }
    
    // Gera chave única: req_00001, req_00002, etc
    char key[16];
    snprintf(key, sizeof(key), "req_%05u", req->id);
    
    // Salva estrutura completa como blob binário
    size_t written = prefs.putBytes(key, req, sizeof(MaintenanceRequest));
    
    bool success = (written == sizeof(MaintenanceRequest));
    
    if (success) {
        // Atualiza contador global
        prefs.putUInt("req_counter", req->id);
        
        // Incrementa contador de pendentes se não enviado
        if (!req->email_enviado) {
            uint16_t pending = prefs.getUInt("pending_count", 0);
            prefs.putUInt("pending_count", pending + 1);
        }
        
        Serial.printf("✅ Requisição #%05u salva no NVS (chave: %s)\\n", req->id, key);
    } else {
        Serial.printf("❌ Erro ao salvar no NVS: esperado %u bytes, gravado %u bytes\\n", 
                     sizeof(MaintenanceRequest), written);
    }
    
    prefs.end();
    return success;
}

/**
 * @brief Monta corpo do e-mail em HTML profissional
 */
String montar_corpo_email_html(const MaintenanceRequest* req) {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><style>";
    
    // CSS inline
    html += "body{font-family:Arial,sans-serif;background:#f3f4f6;margin:0;padding:20px;}";
    html += ".container{max-width:600px;margin:0 auto;background:white;border-radius:8px;overflow:hidden;box-shadow:0 4px 6px rgba(0,0,0,0.1);}";
    html += ".header{background:#1a1a2e;color:#FBBF24;padding:25px;text-align:center;}";
    html += ".header h2{margin:0;font-size:24px;}";
    html += ".header p{margin:5px 0 0 0;opacity:0.8;font-size:14px;}";
    html += ".content{padding:25px;}";
    html += ".field{margin:18px 0;border-left:4px solid #E5E7EB;padding-left:15px;}";
    html += ".field-label{font-weight:bold;color:#6B7280;font-size:11px;text-transform:uppercase;margin-bottom:6px;}";
    html += ".field-value{color:#1F2937;font-size:15px;background:#F9FAFB;padding:12px;border-radius:6px;}";
    html += ".priority-badge{display:inline-block;padding:10px 18px;border-radius:6px;color:white;font-weight:bold;}";
    html += ".footer{text-align:center;padding:20px;color:#9CA3AF;font-size:12px;border-top:1px solid #E5E7EB;}";
    html += "</style></head><body><div class='container'>";
    
    // Header
    html += "<div class='header'>";
    html += "<h2>🔧 REQUISIÇÃO DE MANUTENÇÃO</h2>";
    html += "<p>Requisição #";
    html += req->id;
    html += "</p>";
    html += "</div>";
    
    // Content
    html += "<div class='content'>";
    
    // Prioridade com destaque
    html += "<div class='field' style='border-left-color:";
    html += prioridadeToColor(req->prioridade);
    html += ";'><div class='field-label'>PRIORIDADE</div>";
    html += "<span class='priority-badge' style='background:";
    html += prioridadeToColor(req->prioridade);
    html += ";'>";
    html += prioridadeToString(req->prioridade);
    html += "</span></div>";
    
    // Local
    html += "<div class='field'><div class='field-label'>LOCAL</div>";
    html += "<div class='field-value'>";
    html += req->local_nome;
    html += "</div></div>";
    
    // Problema/Defeito
    html += "<div class='field'><div class='field-label'>PROBLEMA / DEFEITO RELATADO</div>";
    html += "<div class='field-value'>";
    html += req->problema;
    html += "</div></div>";
    
    // Contato (se fornecido)
    if (strlen(req->contato) > 0) {
        html += "<div class='field'><div class='field-label'>CONTATO</div>";
        html += "<div class='field-value'>";
        html += req->contato;
        html += "</div></div>";
    }
    
    // Data e Hora
    html += "<div class='field'><div class='field-label'>DATA E HORA</div>";
    html += "<div class='field-value'>";
    html += "📅 ";
    html += req->datetime;
    html += "</div></div>";
    
    // Informações do Sistema
    html += "<div class='field'><div class='field-label'>INFORMAÇÕES DO SISTEMA</div>";
    html += "<div class='field-value'>";
    html += "🌐 <strong>IP:</strong> ";
    html += req->ip_origem;
    html += "<br>";
    html += "🔌 <strong>MAC:</strong> ";
    html += req->mac_address;
    html += "<br>";
    html += "💾 <strong>Firmware:</strong> v";
    html += req->versao_firmware;
    html += "</div></div>";
    
    html += "</div>";  // Fecha content
    
    // Footer
    html += "<div class='footer'>";
    html += "<strong>Sistema de Controle de Acesso ESP32-S3</strong><br>";
    html += "Este é um e-mail automático.<br>";
    html += "Em caso de dúvidas, contate a equipe de TI.";
    html += "</div>";
    
    html += "</div></body></html>";
    
    return html;
}

/**
 * @brief Envia e-mail via SMTP (Gmail)
 * @return true se enviado com sucesso
 */
bool enviar_email_smtp(const MaintenanceRequest* req) {
    // Verifica se Wi-Fi está habilitado
    #ifndef WIFI_ENABLED
    Serial.println("⚠️ Wi-Fi desabilitado, e-mail não será enviado");
    return false;
    #endif
    
    // Verifica conexão Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ Wi-Fi não conectado");
        return false;
    }
    
    Serial.println("📧 Iniciando envio de e-mail via SMTP...");
    
    // ⭐ v6.0.54: Carregar configuração do NVS
    String recipient, smtp_email, smtp_pass;
    Preferences prefs;
    prefs.begin("email_config", true);
    bool configured = prefs.getBool("configured", false);
    
    if (configured) {
        recipient = prefs.getString("recipient", "");
        smtp_email = prefs.getString("smtp_email", "");
        smtp_pass = prefs.getString("smtp_password", "");
        prefs.end();
        
        Serial.println("📧 Usando configuração de e-mail do NVS");
        Serial.printf("   Remetente: %s\n", smtp_email.c_str());
        Serial.printf("   Destinatário: %s\n", recipient.c_str());
    } else {
        prefs.end();
        // Fallback para valores padrão de smtp_config.h
        recipient = RECIPIENT_EMAIL;
        smtp_email = SMTP_EMAIL;
        smtp_pass = SMTP_PASSWORD;
        
        Serial.println("⚠️ Usando configuração padrão de smtp_config.h");
        Serial.println("💡 Configure em: CONFIG → E-MAIL");
    }
    
    // Validação
    if (smtp_email.isEmpty() || smtp_pass.isEmpty() || recipient.isEmpty()) {
        Serial.println("❌ Configuração de e-mail incompleta!");
        return false;
    }
    
    // Configuração da sessão SMTP
    SMTPSession smtp;
    ESP_Mail_Session session;
    
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = smtp_email.c_str();
    session.login.password = smtp_pass.c_str();
    session.login.user_domain = "";
    
    // Configuração da mensagem
    SMTP_Message message;
    
    // Remetente
    message.sender.name = SMTP_NAME;
    message.sender.email = smtp_email.c_str();
    
    // Assunto
    String subject = EMAIL_SUBJECT_PREFIX;
    subject += " Requisição #";
    subject += req->id;
    subject += " - ";
    subject += prioridadeToString(req->prioridade);
    message.subject = subject.c_str();
    
    // Destinatário
    message.addRecipient("Manutenção", recipient.c_str());
    
    // Corpo HTML
    String htmlBody = montar_corpo_email_html(req);
    message.html.content = htmlBody.c_str();
    message.html.charSet = "utf-8";
    message.html.transfer_encoding = Content_Transfer_Encoding::enc_qp;
    
    // Configurações avançadas
    message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
    
    // Debug
    smtp.debug(SMTP_DEBUG_ENABLED ? 1 : 0);
    
    // Conecta ao servidor SMTP
    Serial.printf("📡 Conectando a %s:%d...\\n", SMTP_HOST, SMTP_PORT);
    
    if (!smtp.connect(&session)) {
        Serial.println("❌ Falha ao conectar ao servidor SMTP");
        Serial.print("Erro: ");
        Serial.println(smtp.errorReason());
        return false;
    }
    
    Serial.println("✅ Conectado ao servidor SMTP");
    
    // Envia e-mail
    Serial.println("📨 Enviando e-mail...");
    
    if (!MailClient.sendMail(&smtp, &message)) {
        Serial.println("❌ Falha ao enviar e-mail");
        Serial.print("Erro: ");
        Serial.println(smtp.errorReason());
        smtp.closeSession();
        return false;
    }
    
    Serial.println("✅ E-mail enviado com sucesso!");
    Serial.printf("   Para: %s\\n", RECIPIENT_EMAIL);
    Serial.printf("   Assunto: %s\\n", subject.c_str());
    
    // Fecha sessão
    smtp.closeSession();
    
    return true;
}

/**
 * @brief Evento principal: enviar requisição
 */
void evento_enviar_requisicao(lv_event_t * e) {
    Serial.println("\\n═══════════════════════════════════════════════════");
    Serial.println("  📨 ENVIANDO REQUISIÇÃO DE MANUTENÇÃO");
    Serial.println("═══════════════════════════════════════════════════");
    
    // Esconde teclado se estiver aberto
    if (manut_keyboard) lv_obj_add_flag(manut_keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // ═════════ COLETA DE DADOS ═════════
    const char* problema = manut_textarea_problema ? lv_textarea_get_text(manut_textarea_problema) : "";
    uint16_t local_idx = manut_dropdown_local ? lv_dropdown_get_selected(manut_dropdown_local) : 0;
    uint16_t prior_idx = manut_dropdown_prioridade ? lv_dropdown_get_selected(manut_dropdown_prioridade) : 0;
    const char* contato = manut_textarea_contato ? lv_textarea_get_text(manut_textarea_contato) : "";
    
    Serial.println("\\n📋 Dados coletados:");
    Serial.printf("   Problema: '%s' (%d chars)\\n", problema, strlen(problema));
    Serial.printf("   Local: índice %d\\n", local_idx);
    Serial.printf("   Prioridade: índice %d\\n", prior_idx);
    Serial.printf("   Contato: '%s'\\n", strlen(contato) > 0 ? contato : "(vazio)");
    
    // ═════════ VALIDAÇÃO ═════════
    Serial.println("\\n✓ Validando dados...");
    
    if (strlen(problema) < 10) {
        mostrar_erro_manutencao("Problema muito curto (min 10)");
        Serial.println("❌ Validação falhou: problema muito curto");
        return;
    }
    
    if (local_idx == 0) {
        mostrar_erro_manutencao("Selecione o local");
        Serial.println("❌ Validação falhou: local não selecionado");
        return;
    }
    
    if (prior_idx == 0) {
        mostrar_erro_manutencao("Selecione a prioridade");
        Serial.println("❌ Validação falhou: prioridade não selecionada");
        return;
    }
    
    Serial.println("✅ Validação OK!");
    
    // ═════════ PREENCHE ESTRUTURA ═════════
    Serial.println("\\n📝 Preenchendo estrutura...");
    inicializarRequisicao(&currentRequest);
    
    // ID único
    Preferences prefs;
    prefs.begin("manutencao", false);
    maintenance_id_counter = prefs.getUInt("req_counter", 0) + 1;
    prefs.end();
    
    currentRequest.id = maintenance_id_counter;
    Serial.printf("   ID: #%05u\\n", currentRequest.id);
    
    // Dados do formulário
    strncpy(currentRequest.problema, problema, 200);
    currentRequest.problema[200] = '\0';
    
    currentRequest.local = (LocalManutencao)local_idx;
    strncpy(currentRequest.local_nome, localToString(currentRequest.local), 49);
    
    currentRequest.prioridade = (PrioridadeManutencao)prior_idx;
    strncpy(currentRequest.prioridade_nome, prioridadeToString(currentRequest.prioridade), 29);
    
    if (strlen(contato) > 0) {
        strncpy(currentRequest.contato, contato, 50);
    }
    
    // Timestamp
    currentRequest.timestamp = time(nullptr);
    if (currentRequest.timestamp > 0) {
        struct tm timeinfo;
        localtime_r(&currentRequest.timestamp, &timeinfo);
        strftime(currentRequest.datetime, sizeof(currentRequest.datetime), 
                 "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        strcpy(currentRequest.datetime, "2025-11-24 00:00:00");
    }
    
    // Metadados
    #ifdef WIFI_ENABLED
    WiFi.localIP().toString().toCharArray(currentRequest.ip_origem, 16);
    WiFi.macAddress().toCharArray(currentRequest.mac_address, 18);
    #else
    strcpy(currentRequest.ip_origem, "0.0.0.0");
    strcpy(currentRequest.mac_address, "00:00:00:00:00:00");
    #endif
    
    currentRequest.versao_firmware = 1;
    currentRequest.status = STATUS_PENDENTE;
    currentRequest.email_enviado = false;
    currentRequest.tentativas_envio = 0;
    
    // Validação final
    if (!validarRequisicao(&currentRequest)) {
        mostrar_erro_manutencao("Dados invalidos");
        return;
    }
    
    // ═════════ SALVA NO NVS ═════════
    Serial.println("\\n💾 Salvando no NVS...");
    
    if (!salvar_requisicao_nvs(&currentRequest)) {
        mostrar_erro_manutencao("Erro ao salvar");
        return;
    }
    
    Serial.println("✅ Salvo no NVS!");
    
    // Imprime resumo
    printRequisicao(&currentRequest);
    
    // ═════════ ENVIA E-MAIL ═════════
    mostrar_status_manutencao("Enviando...", COLOR_WARNING);
    lv_task_handler();  // Atualiza UI
    
    bool email_enviado = enviar_email_smtp(&currentRequest);
    
    if (email_enviado) {
        // Atualiza status
        currentRequest.email_enviado = true;
        currentRequest.status = STATUS_ENVIADA;
        currentRequest.tentativas_envio = 1;
        currentRequest.ultima_tentativa = time(nullptr);
        salvar_requisicao_nvs(&currentRequest);
        
        mostrar_status_manutencao("✅ Enviada!", COLOR_SUCCESS);
        Serial.println("\\n✅ SUCESSO COMPLETO!");
    } else {
        // Marca como erro mas mantém salva
        currentRequest.status = STATUS_ERRO_ENVIO;
        currentRequest.tentativas_envio = 1;
        currentRequest.ultima_tentativa = time(nullptr);
        salvar_requisicao_nvs(&currentRequest);
        
        mostrar_status_manutencao("⚠️ Salva localmente", COLOR_WARNING);
        Serial.println("\\n⚠️ E-mail falhou, mas salvo no NVS");
    }
    
    Serial.println("═══════════════════════════════════════════════════\\n");
    
    // Aguarda 2.5s e volta
    delay(2500);
    mudar_tela(SCREEN_HOME); // SCREEN_HOME
}