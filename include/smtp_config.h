/**
 * @file smtp_config.h
 * @brief Configurações SMTP para envio de e-mails
 * @version 1.0.0
 * @date 2025-11-24
 * 
 * ⚠️ ATENÇÃO: Este arquivo contém credenciais sensíveis!
 * - NÃO fazer commit deste arquivo no Git
 * - Adicionar ao .gitignore
 * - Usar senha de app do Gmail (não a senha da conta)
 * 
 * COMO OBTER SENHA DE APP DO GMAIL:
 * 1. Acesse: https://myaccount.google.com/security
 * 2. Ative "Verificação em duas etapas"
 * 3. Vá em "Senhas de app"
 * 4. Crie nova senha: App = "Outro" → "ESP32 Controle Acesso"
 * 5. Copie a senha gerada (16 caracteres)
 * 6. Cole abaixo em SMTP_PASSWORD
 */

#ifndef SMTP_CONFIG_H
#define SMTP_CONFIG_H

// ════════════════════════════════════════════════════════════════
// SERVIDOR SMTP
// ════════════════════════════════════════════════════════════════

/**
 * @brief Host do servidor SMTP
 * @note Gmail: smtp.gmail.com
 * @note Outlook: smtp.office365.com
 * @note Yahoo: smtp.mail.yahoo.com
 */
#define SMTP_HOST     "smtp.gmail.com"

/**
 * @brief Porta do servidor SMTP
 * @note 587 = STARTTLS (recomendado)
 * @note 465 = SSL/TLS
 * @note 25  = Não criptografado (não recomendado)
 */
#define SMTP_PORT     587

// ════════════════════════════════════════════════════════════════
// AUTENTICAÇÃO (REMETENTE)
// ════════════════════════════════════════════════════════════════

/**
 * @brief E-mail da conta que ENVIA as notificações
 * @example "controlacesso.notificacoes@gmail.com"
 */
#define SMTP_EMAIL    "seu_email@gmail.com"  // ⚠️ CONFIGURAR

/**
 * @brief Senha de app do Gmail (16 dígitos)
 * @warning NÃO usar a senha da conta! Usar senha de app!
 * @example "abcd efgh ijkl mnop" (copiar do Gmail)
 */
#define SMTP_PASSWORD "xxxx xxxx xxxx xxxx"  // ⚠️ CONFIGURAR

/**
 * @brief Nome do remetente que aparece no e-mail
 */
#define SMTP_NAME     "Sistema Controle Acesso"

// ════════════════════════════════════════════════════════════════
// DESTINATÁRIO (EQUIPE DE MANUTENÇÃO)
// ════════════════════════════════════════════════════════════════

/**
 * @brief E-mail da equipe que RECEBE as notificações
 * @example "manutencao@suaempresa.com"
 */
#define RECIPIENT_EMAIL "manutencao@empresa.com"  // ⚠️ CONFIGURAR

/**
 * @brief Nome do destinatário
 */
#define RECIPIENT_NAME  "Equipe de Manutenção"

// ════════════════════════════════════════════════════════════════
// CONFIGURAÇÕES AVANÇADAS
// ════════════════════════════════════════════════════════════════

/**
 * @brief Prefixo do assunto do e-mail
 * @note Facilita filtros e regras no servidor de e-mail
 */
#define EMAIL_SUBJECT_PREFIX  "[MANUTENÇÃO]"

/**
 * @brief Timeout para conexão SMTP (milissegundos)
 * @note 30000ms = 30 segundos
 */
#define EMAIL_TIMEOUT_MS      30000

/**
 * @brief Número máximo de tentativas de reenvio
 * @note Se falhar, requisição fica pendente para retry automático
 */
#define EMAIL_MAX_RETRIES     3

/**
 * @brief Intervalo entre tentativas de reenvio (segundos)
 * @note 300s = 5 minutos
 */
#define EMAIL_RETRY_INTERVAL  300

/**
 * @brief Ativar debug SMTP no Serial Monitor
 * @note true = logs detalhados, false = apenas erros
 */
#define SMTP_DEBUG_ENABLED    true

/**
 * @brief Validar certificado SSL/TLS
 * @note true = mais seguro, false = aceita certificados inválidos
 * @warning Para testes, pode usar false se tiver problemas de certificado
 */
#define SMTP_VALIDATE_CERT    false  // false para testes

// ════════════════════════════════════════════════════════════════
// TEMPLATE DE E-MAIL
// ════════════════════════════════════════════════════════════════

/**
 * @brief Rodapé do e-mail (aparece em todos os e-mails)
 */
#define EMAIL_FOOTER \
    "<hr style='border: 0; border-top: 1px solid #ddd; margin: 20px 0;'>" \
    "<p style='text-align: center; color: #9CA3AF; font-size: 12px;'>" \
    "Sistema de Controle de Acesso ESP32-S3<br>" \
    "Este é um e-mail automático. Não responda.<br>" \
    "Em caso de dúvidas, entre em contato com a equipe de TI." \
    "</p>"

#endif // SMTP_CONFIG_H
