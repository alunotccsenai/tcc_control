/**
 * @file config.h
 * @brief Configurações do projeto ESP32S3 Freenove com LVGL
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ============================================================================
 * INFORMAÇÕES DO PROJETO
 * ========================================================================== */
#define PROJECT_NAME        "ESP32S3 Access Control"
#define FIRMWARE_VERSION    "1.0.0-LVGL"
#define HARDWARE_MODEL      "Freenove ESP32S3-WROOM-N8R8"

/* ============================================================================
 * CONFIGURAÇÕES DE DISPLAY
 * ========================================================================== */
#define SCREEN_WIDTH        480
#define SCREEN_HEIGHT       320
#define SCREEN_ROTATION     1       // 0=0°, 1=90°, 2=180°, 3=270°

/* ============================================================================
 * CALIBRAÇÃO TOUCH XPT2046 - VALORES FINAIS DE PRODUÇÃO ✅
 * ============================================================================
 * Data de finalização: 21/11/2025
 * Hardware: ESP32-S3-WROOM-N8R8 + XPT2046 + LCD TFT 3.5" (480x320)
 * 
 * Mapeamento validado e testado em produção:
 *   x = map(p.x, TOUCH_MAX_X, TOUCH_MIN_X, 0, SCREEN_WIDTH);  ← Invertido
 *   y = map(p.y, TOUCH_MAX_Y, TOUCH_MIN_Y, 0, SCREEN_HEIGHT); ← Invertido
 * 
 * Ambos eixos (X e Y) precisam inversão MAX→MIN para funcionamento correto.
 * 
 * VALORES FINAIS FIXADOS (não alterar sem recalibração completa):
 *   MIN_X: 400  |  MAX_X: 3950  (Range: 3550)
 *   MIN_Y: 330  |  MAX_Y: 3650  (Range: 3320)
 * 
 * Status: ✅ CALIBRADO E VALIDADO EM PRODUÇÃO
 * ========================================================================== */

/* Modo de calibração e diagnóstico */
#define TOUCH_CALIBRATION_MODE      1       // 1=Ativar logging RAW detalhado
#define TOUCH_MAPPING_MODE          0       // Modo de mapeamento (FIXADO):
                                            // 0 = Normal (MIN→MAX, MIN→MAX) ✅ PRODUÇÃO
                                            // 1 = Invertido X (MAX→MIN, MIN→MAX)
                                            // 2 = Invertido Y (MIN→MAX, MAX→MIN)
                                            // 3 = Ambos invertidos (MAX→MIN, MAX→MIN)

/* ⭐ VALORES FINAIS DE CALIBRAÇÃO - FIXADOS PARA PRODUÇÃO ⭐ */
#define TOUCH_MIN_X         400     // ✅ CALIBRADO: Valor RAW p.x mínimo
#define TOUCH_MAX_X         3950    // ✅ CALIBRADO: Valor RAW p.x máximo
#define TOUCH_MIN_Y         330     // ✅ CALIBRADO: Valor RAW p.y mínimo
#define TOUCH_MAX_Y         3650    // ✅ CALIBRADO: Valor RAW p.y máximo

/* ============================================================================
 * CONFIGURAÇÕES NFC/RFID PN532 - PINAGEM CORRETA ESP32-S3-WROOM-N8R8
 * ============================================================================
 * Módulo: PN532 NFC/RFID Reader/Writer
 * Interface: SPI (DIP Switch: CH1=ON, CH2=OFF)
 * 
 * PINAGEM SPI (Compartilhada com Display e Touch):
 *   SCK  (Clock)      → GPIO12 (Pin 30) - FSPICLK - Compartilhado
 *   MOSI (Master Out) → GPIO11 (Pin 28) - FSPID   - Compartilhado
 *   MISO (Master In)  → GPIO13 (Pin 29) - FSPIQ   - Compartilhado
 *   NSS/CS (Select)   → GPIO21          - RTC_GPIO21 - **EXCLUSIVO PN532**
 * 
 * CONFIGURAÇÃO DIP SWITCH PN532:
 *   Posição 1 (I2C/SPI): ON  (1=LOW)  → Seleciona modo SPI
 *   Posição 2 (HSU):     OFF (0=HIGH) → Desabilita UART
 * 
 * NOTAS:
 *   - SCK/MOSI/MISO são compartilhados com ILI9488 (Display) e XPT2046 (Touch)
 *   - GPIO21 (NSS/CS) é exclusivo para PN532 (não compartilhado)
 *   - Reset não é utilizado (pode ser omitido ou usar GPIO47 se necessário)
 * ========================================================================== */
#define PN532_ENABLED           1       // 1=Habilitado, 0=Desabilitado

/* Pinos do PN532 (SPI) */
#define PN532_SS_PIN            21      // GPIO21 - SPI Chip Select (NSS) - EXCLUSIVO
#define PN532_RST_PIN           -1      // Não utilizado (pode usar GPIO47 se necessário)

#define PN532_TIMEOUT           100     // Timeout de leitura (ms)
#define PN532_MAX_RETRY         3       // Tentativas de leitura
#define PN532_READ_INTERVAL     500     // Intervalo entre leituras (ms)

/* Tipos de cartões suportados */
#define PN532_SUPPORT_MIFARE    1       // Mifare Classic 1K/4K
#define PN532_SUPPORT_ULTRALIGHT 1      // Mifare Ultralight
#define PN532_SUPPORT_NTAG      1       // NTAG203/213/215/216
#define PN532_SUPPORT_FELICA    1       // FeliCa (opcional)

/* Chave padrão Mifare Classic (FF FF FF FF FF FF) */
#define MIFARE_DEFAULT_KEY      { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

/* ============================================================================
 * CONFIGURAÇÕES DE WIFI - SISTEMA COMPLETO
 * ========================================================================== */
#define WIFI_ENABLED            1       // 1=Habilitado, 0=Desabilitado
#define WIFI_AUTO_CONNECT       1       // Reconectar automaticamente
#define WIFI_CONNECT_TIMEOUT    20      // Timeout de conexão (segundos)
#define WIFI_RETRY_DELAY        5000    // Delay entre tentativas (ms)
#define WIFI_CHECK_INTERVAL     5000    // Intervalo de verificação (ms)

/* Credenciais padrão (pode ser alterado via interface) */
#define WIFI_DEFAULT_SSID       ""      // Deixar vazio para modo AP
#define WIFI_DEFAULT_PASSWORD   ""      // Deixar vazio para modo AP

/* Access Point (modo de configuração) */
#define WIFI_AP_SSID            "ESP32_ControlAccess"
#define WIFI_AP_PASSWORD        "12345678"
#define WIFI_AP_CHANNEL         1       // Canal Wi-Fi (1-13)
#define WIFI_AP_MAX_CLIENTS     4       // Máximo de clientes simultâneos
#define WIFI_AP_HIDDEN          0       // 0=Visível, 1=Oculto

/* Servidor Web (API REST) */
#define WEBSERVER_ENABLED       1       // 1=Habilitado, 0=Desabilitado
#define WEBSERVER_PORT          80      // Porta HTTP
#define API_KEY                 "ESP32_ACCESS_2025_SECURE_KEY"  // Chave de autenticação

/* Hostname e DNS */
#define WIFI_HOSTNAME           "esp32-control-access"
#define WIFI_MDNS_ENABLED       1       // 0=Desabilitado, 1=Habilitado
#define WIFI_MDNS_NAME          "controlacesso"  // http://controlacesso.local

/* ============================================================================
 * CONFIGURAÇÕES SENSOR BIOMÉTRICO AS608 - PINAGEM CORRETA ESP32-S3-WROOM-N8R8
 * ============================================================================
 * Módulo: AS608 Fingerprint Sensor
 * Interface: UART2 (Serial2)
 * 
 * PINAGEM UART (Conexão Direta):
 *   RX do ESP32 → TX do AS608 (Blue)  → GPIO01 (RX de ESP32) - RTC_GPIO1, ADC1_CH0
 *   TX do ESP32 → RX do AS608 (Green) → GPIO02 (TX de ESP32) - RTC_GPIO2, ADC1_CH1
 *   VCC (Red)   → 3.3V                → Alimentação: 3.3V-5V - VDD3P3
 *   GND (Black) → GND                 → Terra comum
 * 
 * CONFIGURAÇÃO UART:
 *   Porta Serial: UART2 (Serial2)
 *   Baud Rate: 57600 bps (padrão AS608)
 *   Data bits: 8
 *   Parity: None
 *   Stop bits: 1
 * 
 * NOTAS:
 *   - GPIO01/GPIO02 são pinos RTC dedicados para UART2
 *   - Tensão de operação: 3.3V (compatível com ESP32-S3)
 *   - Corrente típica: 50-100mA (picos até 150mA durante scan)
 *   - Capacidade: até 300 impressões digitais
 *   - Taxa de reconhecimento: <1 segundo
 * ========================================================================== */
#define AS608_ENABLED           1       // 1=Habilitado, 0=Desabilitado

/* Pinos do AS608 (UART2) */
#define AS608_RX_PIN            1       // GPIO01 - RX do ESP32 ← TX do AS608 (Blue)
#define AS608_TX_PIN            2       // GPIO02 - TX do ESP32 → RX do AS608 (Green)
#define AS608_UART              2       // UART2 (Serial2)
#define AS608_BAUDRATE          57600   // Baud rate padrão

/* Configurações do sensor */
#define AS608_TIMEOUT           5000    // Timeout de operações (ms)
#define AS608_MAX_CAPACITY      300     // Capacidade máxima de IDs
#define AS608_SECURITY_LEVEL    3       // Nível de segurança (1-5, 3=médio)
#define AS608_SCAN_TIMEOUT      10000   // Timeout para scan de dedo (ms)
#define AS608_MATCH_THRESHOLD   50      // Threshold de matching (0-255)

/* ============================================================================
 * FEATURES HABILITADAS
 * ========================================================================== */
#define FEATURE_BIOMETRIC   1       // Sensor AS608
#define FEATURE_RFID        1       // Leitor RFID
#define FEATURE_WIFI        1       // WiFi
#define FEATURE_MAINTENANCE 1       // Sistema de manutenção

/* ============================================================================
 * CONFIGURAÇÕES DE SEGURANÇA
 * ========================================================================== */
#define DEFAULT_MASTER_PIN      "1234"
#define MAX_FAILED_ATTEMPTS     3
#define LOCKOUT_TIME            30000       // 30 segundos em ms
#define ACCESS_GRANT_TIME       5000        // 5 segundos em ms

/* ============================================================================
 * CONFIGURAÇÕES DE CONTROLE DE ACESSO (RELÉ)
 * ============================================================================
 * Sistema de acionamento do relé para porta/fechadura.
 * 
 * Hardware: Relé conectado ao GPIO19 ou GPIO20
 * Lógica: HIGH = Destravar, LOW = Trancar (padrão)
 * Tempo de destravamento: 5 segundos (temporizado)
 * 
 * Data: 27/11/2025
 * ========================================================================== */

/* Habilitar/desabilitar controle de relé */
#define RELAY_ENABLED               1           // 1=Habilitado, 0=Desabilitado

/* Configurações de tempo */
#define RELAY_UNLOCK_TIME           5000        // Tempo destrancado (5s)
#define RELAY_ACTIVE_LEVEL          HIGH        // HIGH=destravar, LOW=trancar

/* ============================================================================
 * CONFIGURAÇÕES DE AUTENTICAÇÃO ADMIN
 * ============================================================================
 * Sistema de proteção por PIN para acesso às configurações do sistema.
 * 
 * Características:
 * - PIN de 4 dígitos (padrão: 9999)
 * - 3 tentativas antes de bloquear
 * - Bloqueio de 5 minutos após falhas
 * - Armazenamento seguro em NVS
 * - Recuperação via Serial Monitor
 * 
 * Data: 24/11/2025
 * ========================================================================== */

/* Ativar/desativar sistema de autenticação */
#define ADMIN_AUTH_ENABLED          1           // 1=Habilitado, 0=Desabilitado

/* Configurações de PIN */
#define ADMIN_PIN_DEFAULT           "9999"      // PIN padrão inicial
#define ADMIN_PIN_LENGTH            4           // Tamanho fixo do PIN
#define ADMIN_PIN_MIN_VALUE         0           // Mínimo: 0000
#define ADMIN_PIN_MAX_VALUE         9999        // Máximo: 9999

/* Configurações de segurança */
#define ADMIN_MAX_ATTEMPTS          3           // Tentativas permitidas
#define ADMIN_LOCKOUT_TIME          300000      // 5 minutos em ms (5 * 60 * 1000)
#define ADMIN_SESSION_TIMEOUT       300000      // Tempo de sessão: 5 minutos
#define ADMIN_AUTO_LOGOUT           1           // 1=Logout automático, 0=Sessão permanente

/* Configurações de armazenamento */
#define ADMIN_NVS_NAMESPACE         "admin"     // Namespace no NVS
#define ADMIN_NVS_KEY_PIN           "pin"       // Chave para PIN
#define ADMIN_NVS_KEY_ENABLED       "enabled"   // Chave para flag enabled
#define ADMIN_NVS_KEY_ATTEMPTS      "attempts"  // Chave para tentativas
#define ADMIN_NVS_KEY_LOCKOUT       "lockout"   // Chave para tempo de bloqueio

/* Recuperação de emergência */
#define ADMIN_EMERGENCY_PIN         "0000"      // PIN de emergência (sempre funciona)
#define ADMIN_SERIAL_RESET_CMD      "RESET_ADMIN_PIN"  // Comando via Serial
#define ADMIN_ALLOW_EMERGENCY       1           // 1=Permitir PIN emergência, 0=Não

#endif // CONFIG_H