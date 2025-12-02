#include <Arduino.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include "esp_task_wdt.h"
#include "config.h"
#include "calibration.h"  // ⭐ Sistema de calibração
#include "admin_auth.h"   // ⭐ Sistema de autenticação admin
#include "maintenance_types.h"  // ⭐ NOVO: Tipos do sistema de manutenção

// ═══════════════════════════════════════════════════════════════════════
// ⭐ IMPORTANTE: Ordem de includes para evitar conflitos de estruturas
// ═══════════════════════════════════════════════════════════════════════
// PROBLEMA: rfid_storage.h define: struct RFIDCard { String uid; ... }
//           rfid_manager.h define: typedef struct { uint8_t uid[]; ... } RFIDCard;
// 
// SOLUÇÃO: Incluir APENAS os managers aqui. Os storages serão incluídos
//          no final do arquivo (depois do #include <ESP_Mail_Client.h>)
// ═══════════════════════════════════════════════════════════════════════

#include "rfid_manager.h"       // ⭐ Gerenciador RFID
#include "biometric_manager.h"  // ⭐ Gerenciador Biometria
#include "relay_controller.h"   // ⭐ Controlador de relé
#include "virtual_keyboard.h"   // ⭐ v6.0.9: Teclado Virtual Unificado (lv_keyboard)
#include "biometric_storage.h"  // ⭐ v6.0.22: Storage biométrico (lib/BiometricStorage)

// ⭐ DECLARAÇÕES FORWARD: Funções de manutenção (implementadas em maintenance_functions.cpp)
void evento_foco_campo_manut(lv_event_t * e);
void evento_defocus_campo_manut(lv_event_t * e);
void mostrar_erro_manutencao(const char* mensagem);
void mostrar_status_manutencao(const char* mensagem, uint32_t cor);
void evento_cancelar_requisicao(lv_event_t * e);
void evento_enviar_requisicao(lv_event_t * e);
bool salvar_requisicao_nvs(const MaintenanceRequest* req);
String montar_corpo_email_html(const MaintenanceRequest* req);
bool enviar_email_smtp(const MaintenanceRequest* req);

// ⭐ NOVO: Biblioteca PN532 NFC/RFID
#if PN532_ENABLED
#include <Adafruit_PN532.h>
#include <Preferences.h>  // Para salvar UIDs na Flash
#endif

// ⭐ NOVO: Sistema Wi-Fi completo
#if WIFI_ENABLED
#include "wifi_config.h"
#include <ESPmDNS.h>
#endif

// ⭐ NOVO: Cliente SMTP para envio de e-mails
#include <ESP_Mail_Client.h>

// ⭐ NOVO: Interface para inicialização de storages (evita conflito de estruturas)
#include "storage_init.h"

// Forward declarations dos storages (definições em storage_init.cpp)
class RFIDStorage;
// ⭐ BiometricStorage agora tem include completo acima (biometric_storage.h)

// ========================================
// CONSTANTES DO SISTEMA
// ========================================

static const uint32_t screenWidth  = 480;
static const uint32_t screenHeight = 320;

// ⭐ Calibração do Touch agora está em config.h
// TOUCH_MIN_X, TOUCH_MAX_X, TOUCH_MIN_Y, TOUCH_MAX_Y

// Cores do tema (EXATAS DO App.tsx)
#define COLOR_BG_DARK    0x0A0A0A  // bg-[#0a0a0a]
#define COLOR_BG_MEDIUM  0x1A1A2E  // bg-[#1a1a2e]
#define COLOR_BG_LIGHT   0x2A2A3E  // hover:bg-[#2a2a3e]
#define COLOR_BORDER     0x374151  // border-gray-700

#define COLOR_BLUE       0x2563EB  // bg-blue-600
#define COLOR_PURPLE     0x9333EA  // bg-purple-600
#define COLOR_CYAN       0x0891B2  // bg-cyan-600
#define COLOR_ORANGE     0xEA580C  // bg-orange-600

#define COLOR_SUCCESS    0x4CAF50
#define COLOR_ERROR      0xF44336
#define COLOR_WARNING    0xFF9800
#define COLOR_ACCENT     0x3B82F6  // blue-400

// ========================================
// GERENCIADOR DE TELAS
// ========================================

enum Screen {
    SCREEN_HOME,         // Acesso PIN
    SCREEN_BIOMETRIC,    // Gerenciamento Bio
    SCREEN_RFID,         // Gerenciamento RFID
    SCREEN_MAINTENANCE,  // Manutenção
    SCREEN_CONTROLS,     // Ajuda/Controles
    SCREEN_SETTINGS,     // Configurações
    SCREEN_CALIBRATION,  // ⭐ Calibração Touch
    SCREEN_ADMIN_AUTH    // ⭐ NOVA TELA: Autenticação Admin
};

enum MaintenanceSubScreen {
    MAINT_REQUEST,       // Nova requisição
    MAINT_HISTORY        // Histórico
};

enum SettingsSubScreen {
    SETTINGS_CALIBRATION,  // Calibração do touch
    SETTINGS_WIFI,         // Configurações Wi-Fi
    SETTINGS_RFID,         // ⭐ NOVO: Cadastro RFID
    SETTINGS_BIOMETRIC,    // ⭐ NOVO: Cadastro Biometria
    SETTINGS_EMAIL         // ⭐ NOVO: Configuração de E-mail
};

Screen currentScreen = SCREEN_HOME;
MaintenanceSubScreen maintenanceSubScreen = MAINT_REQUEST;
SettingsSubScreen settingsSubScreen = SETTINGS_CALIBRATION;

// Estados globais
String currentPin = "";
const String correctPin = "1234";
bool accessGranted = false;
String statusMessage = "";

// ⭐ NOVO: Variáveis de cadastro RFID/Bio
String enroll_name_input = "";          // Nome digitado para cadastro
lv_obj_t * enroll_keyboard = nullptr;   // Teclado para entrada de nome
lv_obj_t * enroll_textarea = nullptr;   // Campo de texto para nome
lv_obj_t * enroll_status_label = nullptr;  // Label de status do cadastro
bool enrolling_rfid = false;            // Flag: cadastrando RFID?
bool enrolling_bio = false;             // Flag: cadastrando biometria?

// ⭐ v6.0.9: Teclado virtual agora usa virtual_keyboard.h (código removido - 400 linhas deletadas!)

// ⭐ NOVO v5.2.0: Variáveis de cadastro RFID (novo fluxo)
static lv_obj_t * rfid_list_container = nullptr;
static lv_obj_t * rfid_status_label = nullptr;
static bool rfid_enrolling = false;
static String rfid_temp_name = "";
static uint8_t rfid_temp_uid[7];
static uint8_t rfid_temp_uid_length = 0;

// ⭐ NOVO v5.2.0: Variáveis de cadastro BIOMETRIA (novo fluxo)
static lv_obj_t * bio_list_container = nullptr;
static lv_obj_t * bio_status_label = nullptr;
static bool bio_enrolling = false;
static String bio_temp_name = "";
static int bio_enroll_step = 0;
static uint16_t bio_temp_id = 0;

// ⭐ NOVO v6.0.24: Mensagens temporárias na HOME
static lv_obj_t * home_message_label = nullptr;
static uint32_t home_message_timer = 0;
static const uint32_t HOME_MESSAGE_DURATION = 3000; // 3 segundos

// ⭐ NOVO v6.0.55: WiFi Scanner com lista de redes
static lv_obj_t * wifi_scan_list = nullptr;
static String selected_ssid = "";
static int8_t selected_rssi = 0;

// Estrutura para passar dados da rede WiFi selecionada
struct NetworkData {
    String ssid;
    int32_t rssi;
    wifi_auth_mode_t encryption;
};

// ⭐ NOVO v6.0.25: Controle de modo de autenticação na HOME
enum AuthMode {
    AUTH_NONE,      // Sem autenticação ativa
    AUTH_AUTO_BIO,  // Biometria automática (padrão na HOME)
    AUTH_PIN,       // Aguardando PIN
    AUTH_BIO_MANUAL,// Aguardando biometria após clicar botão BIO
    AUTH_RFID       // Aguardando RFID após clicar botão RFID
};
static AuthMode currentAuthMode = AUTH_AUTO_BIO;
static uint32_t authModeStartTime = 0;
static const uint32_t AUTH_TIMEOUT = 10000; // 10 segundos timeout

// Touch
bool touch_conectado = false;
unsigned long ultimo_aviso = 0;
int16_t touch_press_x = -1;  // ⭐ Posição X do press inicial
int16_t touch_press_y = -1;  // ⭐ Posição Y do press inicial
bool touch_in_gap = false;   // ⭐ Flag se press começou em gap

// ⭐ NOVO: PN532 NFC/RFID
#if PN532_ENABLED
  // ⚠️ CORREÇÃO v5.1.1: Usar construtor sem RST quando PN532_RST_PIN = -1
  // Isso evita erro "[E][esp32-hal-gpio.c:102] __pinMode(): Invalid pin selected"
  #if PN532_RST_PIN == -1
    // Construtor SPI sem RST (PN532 não requer RST para funcionar)
    Adafruit_PN532 nfc(PN532_SS_PIN);
  #else
    // Construtor SPI com RST
    Adafruit_PN532 nfc(PN532_SS_PIN, PN532_RST_PIN);
  #endif
  
Preferences rfidPrefs;  // Preferences para salvar UIDs
boolean nfcReady = false;
unsigned long lastRfidRead = 0;
String lastCardUID = "";  // Para anti-bounce
int totalCards = 0;       // Total de cartões cadastrados

// Estrutura para armazenar informações do cartão
struct CardInfo {
    String uid;
    String name;
    bool active;
    unsigned long lastAccess;
};
#endif

// Objetos LVGL globais
lv_obj_t * header_status_dot = NULL;
lv_obj_t * header_signal = NULL;
lv_obj_t * auth_display_box = NULL;   // ⭐ v6.0.37: Box único para PIN/BIO/RFID (modelo PIN)
lv_obj_t * auth_display_label = NULL; // ⭐ v6.0.37: Label único (troca texto/cor/tamanho dinamicamente)
lv_obj_t * pin_box = NULL;            // ⭐ Compatibilidade (aponta para auth_display_box)
lv_obj_t * bio_box = NULL;            // ⭐ Compatibilidade (aponta para auth_display_box)
lv_obj_t * rfid_box = NULL;           // ⭐ Compatibilidade (aponta para auth_display_box)
lv_obj_t * pin_display_label = NULL;
lv_obj_t * bio_display_label = NULL;  // ⭐ Compatibilidade (aponta para auth_display_label)
lv_obj_t * rfid_display_label = NULL; // ⭐ Compatibilidade (aponta para auth_display_label)
lv_obj_t * content_container = NULL;
lv_obj_t * nav_buttons[6] = {NULL};
lv_obj_t * calibration_label = NULL;  // ⭐ Label para mostrar coordenadas do touch

// ⭐ NOVO: Objetos LVGL para tela RFID (REMOVIDO - Variáveis agora são static locais)

// ⭐ NOVO: Sistema de autenticação admin
AdminAuth adminAuth;  // Instância global
String adminPinInput = "";              // PIN sendo digitado
lv_obj_t * adminPinDisplay = NULL;      // Label que mostra ****
lv_obj_t * adminMessageLabel = NULL;    // Mensagem de status
bool adminAuthInProgress = false;       // Autenticação em andamento

// ⭐ NOVO v1.0.0: Controlador de relé e sistemas de storage
#if RELAY_ENABLED
RelayController relayController;        // Controlador de relé GPIO19/20
#endif

// ⭐ Storages são definidos em storage_init.cpp (evita conflito de includes)
extern RFIDStorage rfidStorage;         // Storage persistente RFID (definido em storage_init.cpp)
extern BiometricStorage bioStorage;     // Storage persistente Biometria (definido em storage_init.cpp)

// ⭐ NOVO: Sistema de requisição de manutenção
MaintenanceRequest currentRequest;      // Requisição atual
lv_obj_t * manut_textarea_problema = NULL;
lv_obj_t * manut_dropdown_local = NULL;
lv_obj_t * manut_dropdown_prioridade = NULL;
lv_obj_t * manut_textarea_contato = NULL;
lv_obj_t * manut_keyboard = NULL;
lv_obj_t * manut_label_status = NULL;
lv_obj_t * campo_com_foco = NULL;
uint32_t maintenance_id_counter = 0;

// ========================================
// CONFIGURAÇÃO LOVYANGFX (VALIDADA 21/10/2025)
// ========================================

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_SPI       _bus_instance;
    lgfx::Light_PWM     _light_instance;

public:
    LGFX(void) {
        // Configuração SPI
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 27000000;  // 27MHz (validado)
            cfg.freq_read  = 16000000;
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            
            // PINAGEM VALIDADA 21/10/2025
            cfg.pin_sclk = 12;
            cfg.pin_mosi = 11;
            cfg.pin_miso = -1;  // Não usado
            cfg.pin_dc   = 17;
            
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        
        // Configuração Painel ILI9488
        {
            auto cfg = _panel_instance.config();
            
            cfg.pin_cs           = 10;
            cfg.pin_rst          = 18;
            cfg.pin_busy         = -1;
            
            // Resolução física
            cfg.memory_width     = 320;
            cfg.memory_height    = 480;
            cfg.panel_width      = 320;
            cfg.panel_height     = 480;
            
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 0;
            
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = false;
            cfg.invert           = false;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;
            
            _panel_instance.config(cfg);
        }
        
        // Configuração BACKLIGHT
        {
            auto cfg = _light_instance.config();
            
            cfg.pin_bl = 5;  // GPIO 5 (validado)
            cfg.invert = false;
            cfg.freq   = 44100;
            cfg.pwm_channel = 1;
            
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        
        setPanel(&_panel_instance);
    }
};

LGFX tft;

// ========================================
// TOUCH XPT2046
// ========================================

#define TOUCH_CS   9
#define TOUCH_IRQ  4

XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

static lv_indev_drv_t indev_drv;
lv_indev_t * indev_touchpad;

// Buffers LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[screenWidth * 20];  // 20 linhas (validado)

// ========================================
// CALLBACKS LVGL
// ========================================

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    esp_task_wdt_reset();
    
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    
    lv_disp_flush_ready(disp);
    esp_task_wdt_reset();
}

void my_touchpad_read(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
    data->state = LV_INDEV_STATE_REL;
    
    if (touch.touched()) {
        TS_Point p = touch.getPoint();
        
        // ⚠️ FILTRO: Ignorar valores inválidos (8191 = touch desconectado)
        if (p.x >= 8000 || p.y >= 8000 || p.x == 0 || p.y == 0) {
            if (!touch_conectado && (millis() - ultimo_aviso > 5000)) {
                Serial.println("⚠️  Touch: valores inválidos (8191) - ignorados");
            }
            data->state = LV_INDEV_STATE_REL;
            esp_task_wdt_reset();
            return;
        }
        
        // Touch VÁLIDO
        if (!touch_conectado) {
            Serial.println("✅ TOUCH CONECTADO E FUNCIONANDO!");
            touch_conectado = true;
        }
        
        // 🔍 LOG RAW DESABILITADO (calibração finalizada)
        // static unsigned long ultimo_log_raw = 0;
        // if (millis() - ultimo_log_raw > 500) {
        //     Serial.printf("📍 RAW: p.x=%d, p.y=%d\n", p.x, p.y);
        //     ultimo_log_raw = millis();
        // }
        
        // ⭐ NOVA CALIBRAÇÃO (21/11/2025): Sistema inteligente de calibração
        int16_t x_raw = p.x;
        int16_t y_raw = p.y;
        int16_t x, y;
        
        // Aplicar transformação de calibração
        calibrar_coordenadas(x_raw, y_raw, x, y);
        
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
        
        // ⭐ DEBUG DETALHADO (ordem corrigida: botões numéricos ANTES de header)
        // ⚠️ ATENÇÃO: content_container tem offset Y=20 (abaixo do header)
        // Botões criados em Y=5 relativo = Y=25 absoluto na tela
        const char* regiao = "DESCONHECIDA";
        
        // Prioridade 1: Botões numéricos (Y=25-209 absoluto) - ALINHADO 75px
        // ⚠️ Offset +26px já foi aplicado ao Y antes desta detecção
        // Altura: 4 linhas × 50px + 3 gaps × 4px = 212px → Y=25+212=237
        // Largura: 3 colunas × 75px + 2 gaps × 4px = 234px (alinhado!)
        if (y >= 25 && y <= 237 && x >= 240) {
            // Detectar qual botão numérico (ajustar para coordenadas relativas)
            int y_rel = y - 25;  // Coordenada relativa ao início do teclado
            int x_rel = x - 240; // Coordenada relativa ao início do teclado (base_x)
            
            int row = y_rel / 54; // 54 = 50px botão + 4px spacing_y
            int col = x_rel / 79; // 79 = 75px botão + 4px spacing_x (ALINHADO!)
            
            int y_dentro_linha = y_rel % 54;  // Posição dentro da linha (0-53)
            int x_dentro_coluna = x_rel % 79; // Posição dentro da coluna (0-78)
            
            // ⚠️ Verificar se está no BOTÃO ou no GAP
            if (y_dentro_linha < 50 && x_dentro_coluna < 75) {
                // Toque dentro do botão (não no gap)
                // ⚠️ NÃO resetar touch_in_gap aqui! Flag é "sticky" até release completo
                int btn_num = row * 3 + col;
                if (btn_num >= 0 && btn_num < 12) {
                    const char* nums[] = {"1","2","3","4","5","6","7","8","9","*","0","#"};
                    static char buf[32];
                    snprintf(buf, sizeof(buf), "BOTÃO [%s]", nums[btn_num]);
                    regiao = buf;
                } else {
                    regiao = "TECLADO";
                }
            } else if (y_dentro_linha >= 50) {
                // Toque no gap vertical (entre linhas) - btn_h=50px
                touch_in_gap = true;   // ⭐ Setar flag de gap
                regiao = "GAP VERTICAL";
            } else {
                // Toque no gap horizontal (entre colunas) - btn_w=75px
                touch_in_gap = true;   // ⭐ Setar flag de gap
                regiao = "GAP HORIZONTAL";
            }
        }
        // Prioridade 2: Botões de controle linha 1 (CLR, DEL)
        else if (y >= 230 && y <= 265 && x >= 240) {
            if (x >= 240 && x <= 351) regiao = "BOTÃO CLR";
            else if (x >= 357 && x <= 468) regiao = "BOTÃO DEL";
            else regiao = "ÁREA CONTROLE";
        }
        // Prioridade 3: Botões de controle linha 2 (BIO, RFID, OK) - 75px cada
        else if (y >= 271 && y <= 306 && x >= 240) {
            if (x >= 240 && x <= 315) regiao = "BOTÃO BIO";
            else if (x >= 319 && x <= 394) regiao = "BOTÃO RFID";
            else if (x >= 398 && x <= 479) regiao = "BOTÃO OK";
            else regiao = "ÁREA CONTROLE 2";
        }
        // Prioridade 4: Navegação (coluna esquerda, Y=134-300)
        // nav_area: Y=134, altura=166px
        else if (x < 234 && y >= 134) regiao = "NAVEGAÇÃO";
        // Prioridade 5: Display (coluna esquerda, Y=0-133)
        else if (x < 234 && y < 134) regiao = "DISPLAY PIN";
        // Prioridade 6: Header
        else if (y < 20) regiao = "HEADER";
        else regiao = "ÁREA LIVRE";
        
        Serial.printf("✅ Touch: x=%d, y=%d → %s\n", x, y, regiao);
    } else {
        // ⭐ Touch released - resetar flag de gap
        touch_in_gap = false;
    }
    
    esp_task_wdt_reset();
}

// ========================================
// PROTÓTIPOS DE FUNÇÕES
// ========================================

void mudar_tela(Screen nova_tela);
void atualizar_navegacao();
void criar_header();
void criar_conteudo_home();
void show_home_message(const char* message, uint32_t color); // ⭐ v6.0.24: Mensagens temporárias na HOME
void criar_conteudo_biometric();
void criar_conteudo_rfid();
void criar_conteudo_maintenance();
void criar_conteudo_controls();      // ⭐ NOVA: Tela de ajuda
void criar_conteudo_settings();
// ⭐ v6.0.9: Removido - agora usa virtual_keyboard.h
void processar_cadastro_biometrico();  // ⭐ v5.2.0: Máquina de estados bio
void criar_settings_calibration();   // ⭐ NOVA: Sub-aba calibração
void criar_settings_wifi();          // ⭐ NOVA: Sub-aba Wi-Fi
void criar_settings_rfid();          // ⭐ NOVO: Sub-aba RFID
void criar_settings_biometric();     // ⭐ NOVO: Sub-aba Biometria
void criar_settings_email();         // ⭐ NOVO: Sub-aba E-mail
void criar_conteudo_calibration();  // ⭐ NOVA TELA

// Funções de calibração via Serial
void processar_preset(String cmd);
void processar_comando_calibracao(String cmd);

// ⭐ NOVO: Funções PN532 NFC/RFID
#if PN532_ENABLED
void initPN532();
void processRFID();
String uidToString(uint8_t *uid, uint8_t uidLength);
boolean isCardAuthorized(String uid);
void saveCard(String uid, String name = "");
void removeCard(String uid);
void listCards();
void clearAllCards();
int getCardCount();
void grantAccess(const char* method, const char* id);
void denyAccess(const char* reason);
void updateRFIDUI(String message, bool success);
void processar_comando_rfid(String cmd);
#endif

// ⭐ NOVO: Funções Admin Auth
void criar_conteudo_admin_auth();
void criar_admin_locked_screen(uint32_t remaining_seconds);
void criar_teclado_admin(lv_obj_t * parent);
void admin_keypad_clicked(lv_event_t * e);
void atualizar_admin_pin_display();
void admin_validate_pin();

// ════════════════════════════════════════════════════════════════
// 🔐 IMPLEMENTAÇÃO DA CLASSE AdminAuth
// ════════════════════════════════════════════════════════════════

AdminAuth::AdminAuth() {
    // Inicializa estado padrão
    state.authenticated = false;
    state.enabled = ADMIN_AUTH_ENABLED;
    state.failed_attempts = 0;
    state.lockout_until = 0;
    state.last_activity = 0;
    strcpy(state.current_pin, ADMIN_PIN_DEFAULT);
}

AdminAuth::~AdminAuth() {
    preferences.end();
}

void AdminAuth::begin() {
    Serial.println("[AdminAuth] Inicializando sistema de autenticação...");
    
    // Carregar configurações do NVS
    if (load()) {
        Serial.println("[AdminAuth] ✓ Configurações carregadas do NVS");
    } else {
        Serial.println("[AdminAuth] ⚠ Usando configurações padrão");
        strcpy(state.current_pin, ADMIN_PIN_DEFAULT);
        state.enabled = ADMIN_AUTH_ENABLED;
        save();  // Salva padrões no NVS
    }
    
    printStatus();
}

bool AdminAuth::validate(const char* pin) {
    // 1. Validar formato
    if (!validatePinFormat(pin)) {
        Serial.println("[AdminAuth] ❌ Formato de PIN inválido");
        return false;
    }
    
    // 2. Verificar bloqueio
    if (isLocked()) {
        uint32_t remaining = getLockoutTimeRemaining();
        Serial.printf("[AdminAuth] 🔒 Conta bloqueada! Restam %u segundos\n", remaining);
        return false;
    }
    
    // 3. Verificar PIN de emergência
#if ADMIN_ALLOW_EMERGENCY
    if (isEmergencyPin(pin)) {
        Serial.println("[AdminAuth] 🚨 PIN de emergência aceito!");
        state.authenticated = true;
        resetAttempts();
        updateActivity();
        return true;
    }
#endif
    
    // 4. Validar PIN
    if (strcmp(pin, state.current_pin) == 0) {
        Serial.println("[AdminAuth] ✅ PIN correto! Acesso concedido.");
        state.authenticated = true;
        resetAttempts();
        updateActivity();
        save();
        return true;
    } else {
        Serial.printf("[AdminAuth] ❌ PIN incorreto! (recebido: %s)\n", pin);
        recordFailedAttempt();
        return false;
    }
}

bool AdminAuth::isAuthenticated() const {
    return state.authenticated;
}

void AdminAuth::setAuthenticated(bool auth) {
    state.authenticated = auth;
    if (auth) {
        updateActivity();
    } else {
        state.last_activity = 0;
    }
    Serial.printf("[AdminAuth] Autenticação: %s\n", auth ? "ATIVADA" : "DESATIVADA");
}

void AdminAuth::logout() {
    state.authenticated = false;
    state.last_activity = 0;
    Serial.println("[AdminAuth] 🚪 Logout realizado");
}

bool AdminAuth::changePin(const char* currentPin, const char* newPin) {
    // Validar PIN atual
    if (strcmp(currentPin, state.current_pin) != 0) {
        Serial.println("[AdminAuth] ❌ PIN atual incorreto");
        return false;
    }
    
    // Validar formato do novo PIN
    if (!validatePinFormat(newPin)) {
        Serial.println("[AdminAuth] ❌ Novo PIN com formato inválido");
        return false;
    }
    
    // Alterar PIN
    strcpy(state.current_pin, newPin);
    save();
    Serial.println("[AdminAuth] ✅ PIN alterado com sucesso");
    return true;
}

bool AdminAuth::resetPin() {
    strcpy(state.current_pin, ADMIN_PIN_DEFAULT);
    save();
    Serial.printf("[AdminAuth] 🔄 PIN resetado para: %s\n", ADMIN_PIN_DEFAULT);
    return true;
}

String AdminAuth::getMaskedPin() const {
    return "****";
}

bool AdminAuth::isLocked() const {
    if (state.lockout_until == 0) return false;
    return (millis() < state.lockout_until);
}

uint32_t AdminAuth::getLockoutTimeRemaining() const {
    if (!isLocked()) return 0;
    return (state.lockout_until - millis()) / 1000;  // Segundos
}

void AdminAuth::recordFailedAttempt() {
    state.failed_attempts++;
    Serial.printf("[AdminAuth] ⚠ Tentativa falhada #%d/%d\n", 
                  state.failed_attempts, ADMIN_MAX_ATTEMPTS);
    
    if (state.failed_attempts >= ADMIN_MAX_ATTEMPTS) {
        lockAccount();
    }
    
    save();
}

void AdminAuth::resetAttempts() {
    if (state.failed_attempts > 0) {
        Serial.println("[AdminAuth] ✓ Tentativas resetadas");
        state.failed_attempts = 0;
        save();
    }
}

uint8_t AdminAuth::getFailedAttempts() const {
    return state.failed_attempts;
}

uint8_t AdminAuth::getRemainingAttempts() const {
    if (state.failed_attempts >= ADMIN_MAX_ATTEMPTS) return 0;
    return ADMIN_MAX_ATTEMPTS - state.failed_attempts;
}

void AdminAuth::setEnabled(bool enabled) {
    state.enabled = enabled;
    save();
    Serial.printf("[AdminAuth] Sistema de autenticação: %s\n", 
                  enabled ? "HABILITADO" : "DESABILITADO");
}

bool AdminAuth::isEnabled() const {
    return state.enabled;
}

void AdminAuth::updateActivity() {
    state.last_activity = millis();
}

bool AdminAuth::checkTimeout() {
#if ADMIN_AUTO_LOGOUT
    if (!state.authenticated) return false;
    
    unsigned long elapsed = millis() - state.last_activity;
    if (elapsed > ADMIN_SESSION_TIMEOUT) {
        Serial.println("[AdminAuth] ⏱ Timeout de sessão! Fazendo logout...");
        logout();
        return true;
    }
#endif
    return false;
}

uint32_t AdminAuth::getSessionTimeRemaining() const {
    if (!state.authenticated) return 0;
    unsigned long elapsed = millis() - state.last_activity;
    if (elapsed > ADMIN_SESSION_TIMEOUT) return 0;
    return (ADMIN_SESSION_TIMEOUT - elapsed) / 1000;  // Segundos
}

bool AdminAuth::load() {
    preferences.begin(ADMIN_NVS_NAMESPACE, true);  // Read-only
    
    // Carregar PIN
    String pin = preferences.getString(ADMIN_NVS_KEY_PIN, "");
    if (pin.length() == ADMIN_PIN_LENGTH) {
        strcpy(state.current_pin, pin.c_str());
    } else {
        preferences.end();
        return false;
    }
    
    // Carregar flags
    state.enabled = preferences.getBool(ADMIN_NVS_KEY_ENABLED, ADMIN_AUTH_ENABLED);
    state.failed_attempts = preferences.getUChar(ADMIN_NVS_KEY_ATTEMPTS, 0);
    
    // Carregar lockout (não persiste entre reboots)
    state.lockout_until = 0;
    
    preferences.end();
    return true;
}

bool AdminAuth::save() {
    preferences.begin(ADMIN_NVS_NAMESPACE, false);  // Read-write
    
    preferences.putString(ADMIN_NVS_KEY_PIN, state.current_pin);
    preferences.putBool(ADMIN_NVS_KEY_ENABLED, state.enabled);
    preferences.putUChar(ADMIN_NVS_KEY_ATTEMPTS, state.failed_attempts);
    
    preferences.end();
    
    Serial.println("[AdminAuth] 💾 Configurações salvas no NVS");
    return true;
}

void AdminAuth::printStatus() const {
    Serial.println("\n╔══════════════════════════════════════════════╗");
    Serial.println("║       ADMIN AUTH - STATUS ATUAL              ║");
    Serial.println("╠══════════════════════════════════════════════╣");
    Serial.printf("║ Sistema:       %s                     ║\n", 
                  state.enabled ? "HABILITADO  " : "DESABILITADO");
    Serial.printf("║ Autenticado:   %s                          ║\n", 
                  state.authenticated ? "SIM" : "NÃO");
    Serial.printf("║ PIN Atual:     ****                          ║\n");
    Serial.printf("║ Tentativas:    %d/%d                          ║\n", 
                  state.failed_attempts, ADMIN_MAX_ATTEMPTS);
    Serial.printf("║ Bloqueado:     %s                          ║\n", 
                  isLocked() ? "SIM" : "NÃO");
    if (isLocked()) {
        Serial.printf("║ Desbloq. em:   %u segundos                  ║\n", 
                      getLockoutTimeRemaining());
    }
    Serial.println("╚══════════════════════════════════════════════╝\n");
}

AdminAuthState AdminAuth::getState() const {
    return state;
}

// ──────────────────────────────────────────────────────────────
// MÉTODOS PRIVADOS
// ──────────────────────────────────────────────────────────────

bool AdminAuth::validatePinFormat(const char* pin) const {
    if (pin == NULL) return false;
    if (strlen(pin) != ADMIN_PIN_LENGTH) return false;
    
    // Verificar se todos são dígitos
    for (int i = 0; i < ADMIN_PIN_LENGTH; i++) {
        if (!isdigit(pin[i])) return false;
    }
    
    return true;
}

bool AdminAuth::isEmergencyPin(const char* pin) const {
#if ADMIN_ALLOW_EMERGENCY
    return (strcmp(pin, ADMIN_EMERGENCY_PIN) == 0);
#else
    return false;
#endif
}

void AdminAuth::lockAccount() {
    state.lockout_until = millis() + ADMIN_LOCKOUT_TIME;
    Serial.printf("[AdminAuth] 🔒 CONTA BLOQUEADA por %d segundos!\n", 
                  ADMIN_LOCKOUT_TIME / 1000);
}

void AdminAuth::unlockAccount() {
    state.lockout_until = 0;
    state.failed_attempts = 0;
    Serial.println("[AdminAuth] 🔓 Conta desbloqueada");
}

// ========================================
// SETUP
// ========================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("SISTEMA DE CONTROLE DE ACESSO");
    Serial.println("ESP32-S3 + LVGL 8.3.11 + Touch XPT2046");
    Serial.println("Configuração validada: 21/10/2025");
    Serial.println("========================================\n");

    // Watchdog
    esp_task_wdt_init(30, true);
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();
    Serial.println("✅ Watchdog 30s configurado");

    // Inicializar display
    Serial.println("🖥️  Inicializando display ILI9488...");
    tft.init();
    tft.setRotation(1); // Landscape
    tft.setBrightness(200);
    tft.fillScreen(TFT_BLACK);
    delay(100);  // ⭐ Aguardar estabilização do SPI
    esp_task_wdt_reset();
    Serial.println("✅ Display ILI9488 480x320 OK");

    // Inicializar touch
    Serial.println("👆 Inicializando touch XPT2046...");
    touch.begin();
    touch.setRotation(1);
    esp_task_wdt_reset();
    Serial.println("✅ Touch XPT2046 inicializado");
    
    // Carregar calibração do touch
    Serial.println("📐 Carregando calibração do touchscreen...");
    carregar_calibracao();
    imprimir_status_calibracao();
    Serial.println("\n💡 Digite 'HELP' no Serial Monitor para comandos de calibração\n");

    // Inicializar LVGL
    Serial.println("🎨 Inicializando LVGL...");
    lv_init();
    esp_task_wdt_reset();

    // Configurar display buffer
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, screenWidth * 20);

    // Registrar display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    esp_task_wdt_reset();

    // Registrar input device (touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
    esp_task_wdt_reset();
    
    Serial.println("✅ LVGL configurado");
    
    // ⭐ NOVO: Inicializar sistema Wi-Fi
    #if WIFI_ENABLED
    Serial.println("📡 Inicializando sistema Wi-Fi...");
    setupWiFi();
    esp_task_wdt_reset();
    Serial.println("✅ Sistema Wi-Fi configurado");
    #endif
    
    // ⭐ NOVO: Inicializar autenticação admin
    #if ADMIN_AUTH_ENABLED
    Serial.println("🔐 Inicializando autenticação admin...");
    adminAuth.begin();
    esp_task_wdt_reset();
    Serial.println("✅ Sistema de autenticação configurado");
    #endif
    
    // ⭐ NOVO v1.0.0: Inicializar sistemas de storage (LittleFS)
    Serial.println("💾 Inicializando sistemas de armazenamento...");
    
    // Inicializar storages (funções em storage_init.cpp)
    initRfidStorage();
    esp_task_wdt_reset();
    
    initBioStorage();
    esp_task_wdt_reset();
    
    // ⭐ NOVO: Inicializar gerenciador RFID
    Serial.println("📇 Inicializando gerenciador RFID...");
    if (rfidManager.init()) {
        Serial.println("✅ Gerenciador RFID configurado");
    } else {
        Serial.println("⚠️ RFID não disponível (continuando sem RFID)");
    }
    
    // ⭐ CRÍTICO: REINICIALIZAR display após init do RFID
    // O PN532 corrompe o registro MADCTL do ILI9488 via SPI compartilhado
    Serial.println("🔧 REINICIALIZANDO display após PN532...");
    delay(200);  // Aguardar SPI estabilizar completamente
    
    // Reinicializar display (necessário para corrigir MADCTL corrompido)
    tft.init();
    tft.setRotation(1);  // LANDSCAPE
    tft.setBrightness(200);
    tft.fillScreen(TFT_BLACK);
    delay(100);
    
    Serial.println("✅ Display REINICIALIZADO pós-RFID (corrupção MADCTL corrigida)");
    
    esp_task_wdt_reset();
    
    // ═══════════════════════════════════════════════════════════════════════
    // ⭐ v6.0.10: PROTOCOLO ROBUSTO ANTI-BROWN-OUT (AS608)
    // ═══════════════════════════════════════════════════════════════════════
    // PROBLEMA: AS608 consome até 150mA durante init, causando:
    //   1. Queda de tensão no VDD3P3 (< 2.7V)
    //   2. ILI9488 corrompe e entra em ALLPON (tela branca)
    //   3. ESP32-S3 pode resetar (brown-out detector)
    //
    // SOLUÇÃO: Protocolo de 5 etapas:
    //   1. DESLIGAR display (backlight OFF)
    //   2. Inicializar AS608 (permitir pico sem vulnerabilidade)
    //   3. Aguardar estabilização LONGA (2s)
    //   4. REINICIALIZAR display COMPLETO (não só fillScreen)
    //   5. Verificar estado e tentar recovery se necessário
    // ═══════════════════════════════════════════════════════════════════════
    
    Serial.println("\n╔════════════════���═════════════════════════════╗");
    Serial.println("║   PROTOCOLO ANTI-BROWN-OUT v6.0.10          ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    
    // ═══ ETAPA 1: DESLIGAR DISPLAY ═══
    Serial.println("📴 ETAPA 1/5: Desligando display antes do AS608...");
    tft.setBrightness(0);  // ⭐ BACKLIGHT OFF (economiza ~50mA)
    delay(100);  // Espera backlight desligar
    Serial.println("   ✅ Display desligado (backlight OFF)");
    
    // ═══ ETAPA 2: INICIALIZAR AS608 (COM DISPLAY SEGURO) ═══
    Serial.println("👆 ETAPA 2/5: Inicializando AS608...");
    Serial.println("   ⚡ ATENÇÃO: Pico de 150mA esperado (display protegido)");
    
    bool bio_ok = bioManager.init();
    
    if (bio_ok) {
        Serial.println("   ✅ AS608 inicializado com sucesso");
    } else {
        Serial.println("   ⚠️ AS608 não disponível");
    }
    
    // ═══ ETAPA 3: ESTABILIZAÇÃO LONGA ═══
    Serial.println("⏳ ETAPA 3/5: Aguardando estabilização elétrica...");
    Serial.println("   • VDD3P3 recuperando de pico");
    Serial.println("   • Capacitores recarregando");
    delay(2000);  // ⭐ 2 segundos (antes: 1s - insuficiente!)
    Serial.println("   ✅ Alimentação estabilizada");
    
    // ═══ ETAPA 4: REINICIALIZAÇÃO COMPLETA DO DISPLAY ═══
    Serial.println("🔧 ETAPA 4/5: REINICIALIZANDO display completo...");
    
    // 4.1: Tentar reinit básico
    tft.init();  // ⭐ Reseta registradores internos
    delay(50);
    
    // 4.2: Configurar novamente
    tft.setRotation(1);  // LANDSCAPE
    delay(50);
    
    // 4.3: Reativar backlight
    tft.setBrightness(200);
    delay(50);
    
    // 4.4: Limpar tela (sai de ALLPON se estiver)
    tft.fillScreen(TFT_BLACK);
    delay(100);
    
    Serial.println("   ✅ Display reinicializado");
    
    // ═══ ETAPA 5: VERIFICAÇÃO E RECOVERY ═══
    Serial.println("🔍 ETAPA 5/5: Verificando estado do display...");
    
    // Teste: desenhar pixel branco e verificar se display responde
    tft.drawPixel(0, 0, TFT_WHITE);
    delay(10);
    tft.drawPixel(0, 0, TFT_BLACK);
    
    Serial.println("   ✅ Display respondendo corretamente");
    
    if (bio_ok) {
        Serial.println("\n✅ Gerenciador Biometria configurado");
        Serial.println("✅ Display protegido e restaurado");
        
        // ═══════════════════════════════════════════════════════════════════════
        // ⭐ NOVO v6.0.22: Inicializar BiometricStorage (JSON em LittleFS)
        // ═══════════════════════════════════════════════════════════════════════
        Serial.println("\n💾 Inicializando BiometricStorage...");
        
        if (bioStorage.begin()) {
            Serial.printf("✅ BiometricStorage inicializado: %d usuário(s)\n", bioStorage.count());
            
            // ═══ SINCRONIZAÇÃO: Migrar metadados do NVS para BiometricStorage ═══
            if (bioManager.getCount() > 0 && bioStorage.count() == 0) {
                Serial.println("⚠️  Detectado: Usuários no NVS mas não no BiometricStorage");
                Serial.println("🔄 Iniciando migração automática NVS → BiometricStorage...");
                
                int migrated = 0;
                for (int i = 0; i < bioManager.getCount(); i++) {
                    Fingerprint* fp = bioManager.getFingerprint(i);
                    if (fp) {
                        BiometricUser user;
                        user.slotId = fp->id;
                        user.userId = String(fp->id);
                        user.userName = String(fp->name);
                        user.registeredAt = fp->timestamp * 1000UL;  // Converter para millis
                        user.confidence = fp->confidence;
                        user.accessCount = fp->access_count;
                        user.lastAccess = fp->last_access * 1000UL;  // Converter para millis
                        user.active = fp->active;
                        
                        if (bioStorage.addUser(user)) {
                            Serial.printf("   ✅ Migrado: %s (ID=%d)\n", fp->name, fp->id);
                            migrated++;
                        } else {
                            Serial.printf("   ❌ Erro ao migrar: %s (ID=%d)\n", fp->name, fp->id);
                        }
                    }
                }
                
                Serial.printf("✅ Migração concluída: %d/%d usuário(s) migrados\n", 
                              migrated, bioManager.getCount());
            }
            
        } else {
            Serial.println("⚠️  BiometricStorage não disponível (LittleFS não montado)");
            Serial.println("   Sistema continuará com armazenamento apenas em NVS");
        }
        
    } else {
        Serial.println("\n⚠️ Biometria não disponível (continuando sem biometria)");
    }
    
    Serial.println("╚══════════════════════════════════════════════╝\n");
    
    esp_task_wdt_reset();
    
    // ⭐ NOVO v1.0.0: Inicializar controlador de relé
    #if RELAY_ENABLED
    Serial.println("🔌 Inicializando controlador de relé...");
    relayController.begin();
    Serial.println("✅ Relé configurado (Porta TRANCADA)");
    esp_task_wdt_reset();
    #endif
    
    // Criar interface DEPOIS de todos os sistemas
    Serial.println("🖼️  Criando interface LVGL...");
    
    // Limpar e reinicializar display (após SPI/UART dos sensores)
    tft.fillScreen(TFT_BLACK);
    lv_obj_invalidate(lv_scr_act());
    esp_task_wdt_reset();
    
    criar_header();
    mudar_tela(SCREEN_HOME);
    esp_task_wdt_reset();
    Serial.println("✅ Interface criada");
    
    Serial.println("\n========================================");
    Serial.println("  ✅ SISTEMA PRONTO!");
    Serial.println("========================================\n");
    
    // ════════════════════════════════════════════════════════════════
    // ⭐ AUTO-RECONEXÃO WIFI (v6.0.55)
    // ════════════════════════════════════════════════════════════════
    Serial.println("\n🔌 [WIFI] Verificando auto-reconexão...");
    
    Preferences prefs_wifi;
    prefs_wifi.begin("wifi_config", true);
    String saved_ssid = prefs_wifi.getString("ssid", "");
    String saved_password = prefs_wifi.getString("password", "");
    prefs_wifi.end();
    
    if (saved_ssid.length() > 0) {
        Serial.printf("[WIFI] ✅ Credenciais encontradas no NVS\n");
        Serial.printf("[WIFI] SSID: '%s'\n", saved_ssid.c_str());
        Serial.printf("[WIFI] Senha: %s\n", saved_password.length() > 0 ? "****** (oculta)" : "(vazio - rede aberta)");
        
        Serial.println("[WIFI] 🔄 Tentando reconexão automática...");
        
        // Configurar modo Station
        WiFi.mode(WIFI_STA);
        WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
        
        Serial.print("[WIFI] Conectando");
        
        // Aguardar conexão (timeout de 15 segundos)
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        Serial.println();
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n╔══════════════════════════════════════════════╗");
            Serial.println("║   ✅ WIFI AUTO-RECONECTADO!                  ║");
            Serial.println("╚══════════════════════════════════════════════╝");
            Serial.printf("[WIFI] SSID: %s\n", WiFi.SSID().c_str());
            Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WIFI] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
            Serial.printf("[WIFI] DNS: %s\n", WiFi.dnsIP().toString().c_str());
            Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());
            Serial.printf("[WIFI] Canal: %d\n", WiFi.channel());
            
            // Determinar qualidade do sinal
            int8_t rssi = WiFi.RSSI();
            const char* quality = "Desconhecido";
            if (rssi > -50) quality = "Excelente ▂▄▆█";
            else if (rssi > -60) quality = "Bom ▂▄▆░";
            else if (rssi > -70) quality = "Regular ▂▄░░";
            else if (rssi > -80) quality = "Fraco ▂░░░";
            else quality = "Muito Fraco ░░░░";
            
            Serial.printf("[WIFI] Qualidade: %s\n", quality);
            Serial.println("════════════════════════════════════════════════\n");
            
        } else {
            Serial.println("\n╔══════════════════════════════════════════════╗");
            Serial.println("║   ❌ FALHA NA AUTO-RECONEXÃO                 ║");
            Serial.println("╚══════════════════════════════════════════════╝");
            Serial.printf("[WIFI] Status code: %d\n", WiFi.status());
            
            // Mostrar erro específico
            switch (WiFi.status()) {
                case WL_NO_SSID_AVAIL:
                    Serial.println("[WIFI] ⚠️ SSID não encontrado (rede fora de alcance)");
                    break;
                case WL_CONNECT_FAILED:
                    Serial.println("[WIFI] ⚠️ Senha incorreta ou problema de autenticação");
                    Serial.println("[WIFI] 💡 Configure novamente em: CONFIG → WIFI");
                    break;
                case WL_CONNECTION_LOST:
                    Serial.println("[WIFI] ⚠️ Conexão perdida (sinal fraco)");
                    break;
                default:
                    Serial.printf("[WIFI] ⚠️ Erro desconhecido: %d\n", WiFi.status());
            }
            
            Serial.println("[WIFI] 📱 Configure manualmente em: CONFIG → WIFI");
            Serial.println("════════════════════════════════════════════════\n");
        }
    } else {
        Serial.println("[WIFI] ⚠️ Nenhuma credencial salva no NVS");
        Serial.println("[WIFI] 📱 Configure em: CONFIG → WIFI");
        Serial.println("════════════════════════════════════════════════\n");
    }
    // ════════════════════════════════════════════════════════════════
}

// ========================================
// LOOP PRINCIPAL
// ========================================

void loop() {
    // ⭐ CORREÇÃO: Inicialização adequada do last_tick
    static uint32_t last_tick = 0;
    if (last_tick == 0) {
        last_tick = millis();
    }
    
    // ⭐ NOVO: Verificar timeout de sessão admin
    #if ADMIN_AUTO_LOGOUT
    adminAuth.checkTimeout();
    #endif
    
    // ⭐ NOVO v1.0.0: Atualizar controlador de relé (gerencia timers)
    #if RELAY_ENABLED
    relayController.update();
    #endif
    
    // ⭐ NOVO v5.2.0: Verificar cadastro RFID em andamento
    if (rfid_enrolling) {
        if (rfidManager.detectCard()) {
            if (rfidManager.readCard(rfid_temp_uid, &rfid_temp_uid_length)) {
                Serial.printf("✅ Cartão detectado! UID: ");
                for (int i = 0; i < rfid_temp_uid_length; i++) {
                    Serial.printf("%02X", rfid_temp_uid[i]);
                }
                Serial.println();
                
                // Salvar no manager
                if (rfidManager.addCard(rfid_temp_uid, rfid_temp_uid_length, rfid_temp_name.c_str())) {
                    Serial.printf("✅ Cartão cadastrado: %s\n", rfid_temp_name.c_str());
                    if (rfid_status_label) {
                        lv_label_set_text(rfid_status_label, "Cadastro concluido!");
                        lv_obj_set_style_text_color(rfid_status_label, lv_color_hex(0x10b981), 0);
                    }
                    // Recriar lista
                    mudar_tela(SCREEN_SETTINGS); // Força recriação da tela CONFIG
                } else {
                    Serial.println("❌ Erro ao salvar cartão (duplicado ou memória cheia)");
                    if (rfid_status_label) {
                        lv_label_set_text(rfid_status_label, "Erro: Cartao duplicado!");
                        lv_obj_set_style_text_color(rfid_status_label, lv_color_hex(0xef4444), 0);
                    }
                }
                
                rfid_enrolling = false;
                rfid_temp_name = "";
            }
        }
    }
    
    // ⭐ NOVO v5.2.0: Processar cadastro BIOMÉTRICO em andamento
    processar_cadastro_biometrico();
    
    // ═══════════════════════════════════════════════════════════════════════
    // ⭐ NOVO v6.0.22: AUTENTICAÇÃO BIOMÉTRICA CONTÍNUA
    // ⭐ CORRIGIDO v6.0.23: Padrão do código funcional (SEM DEBOUNCE, SEM hasFingerOnSensor)
    // ⭐ CORRIGIDO v6.0.25: Respeitar modo de autenticação (não rodar bio quando aguardando RFID)
    // ═══════════════════════════════════════════════════════════════════════
    static bool bioProcessing = false;
    
    // ✅ PADRÃO DO CÓDIGO FUNCIONAL: POLLING CONTÍNUO SEM DEBOUNCE
    // Verificar apenas na tela HOME, se não está processando, e se modo permite biometria
    if (currentScreen == SCREEN_HOME &&                      // Apenas na HOME
        !bioProcessing &&                                     // Não processar se já está processando
        !bio_enrolling &&                                     // Não verificar durante cadastro
        (currentAuthMode == AUTH_AUTO_BIO || currentAuthMode == AUTH_BIO_MANUAL) && // ⭐ v6.0.25: Modo correto
        bioManager.isHardwareConnected()) {                   // Sensor conectado
        
        bioProcessing = true;
        
        // ═══ VERIFICAR DIGITAL (1:N) - MÉTODO SIMPLIFICADO ═══
        // ✅ verifyFinger() já faz getImage() internamente
        // ✅ NÃO chamar hasFingerOnSensor() antes (evita duplicação)
        if (bioManager.verifyFinger()) {
                // ✅ DIGITAL RECONHECIDA!
                uint16_t id = bioManager.getLastMatchedID();
                uint16_t confidence = bioManager.getLastConfidence();
                
                // ═══ BUSCAR INFORMAÇÕES DO USUÁRIO ═══
                int index = bioManager.findFingerprintIndex(id);
                
                if (index >= 0) {
                    Fingerprint* fp = bioManager.getFingerprint(index);
                    
                    if (fp && fp->active) {
                        // ═══════════════════════════════════════════
                        // 🔓 ACESSO CONCEDIDO!
                        // ═══════════════════════════════════════════
                        
                        Serial.println("╔════════════════════════════════════╗");
                        Serial.printf("║  🔓 ACESSO CONCEDIDO               ║\n");
                        Serial.printf("║  Usuário: %-24s║\n", fp->name);
                        Serial.printf("║  ID: %-3d  Confiança: %-3d         ║\n", id, confidence);
                        Serial.printf("║  Acessos: %-4d                     ║\n", fp->access_count);
                        Serial.println("╚════════════════════════════════════╝");
                        
                        // ⭐ v6.0.30: DEBUG - Atualizar bio_display_label
                        Serial.printf("🔍 [DEBUG] Atualizando bio_display_label: %p\n", bio_display_label);
                        // ⭐ v6.0.32: Atualizar label BIO com resultado
                        if (bio_display_label) {
                            char bio_msg[40];
                            snprintf(bio_msg, sizeof(bio_msg), "ACESSO\nCONCEDIDO\n%s", fp->name);
                            lv_label_set_text(bio_display_label, bio_msg);
                            lv_obj_set_style_text_color(bio_display_label, lv_color_hex(0x10b981), 0);
                            lv_obj_set_style_border_color(bio_box, lv_color_hex(0x10b981), 0);
                            Serial.printf("   ✓ BIO atualizado: '%s'\n", bio_msg);
                        } else {
                            Serial.println("   ❌ bio_display_label é NULL!");
                        }
                        
                        // ═══ ATUALIZAR CONTADOR NO STORAGE ═══
                        if (bioStorage.count() > 0) {
                            if (bioStorage.updateLastAccess(id, confidence)) {
                                Serial.printf("📊 [STORAGE] Acesso registrado no BiometricStorage\n");
                            }
                        }
                        
                        // ═══ ATIVAR RELÉ (DESTRANCAR PORTA) ═══
                        #if RELAY_ENABLED
                        Serial.println("🔓 Ativando relé (destrancando porta)...");
                        relayController.unlock();
                        Serial.println("✅ Porta destrancada por 3 segundos");
                        #else
                        Serial.println("💡 RELAY_ENABLED=false (relé não ativado)");
                        #endif
                        
                        // ⭐ v6.0.25: Resetar modo para bio automático após autenticação
                        currentAuthMode = AUTH_AUTO_BIO;
                        
                        // ⭐ v6.0.27: Restaurar display PIN
                        // ⭐ v6.0.38: NÃO usar flags - aguardar timeout para voltar ao PIN
                        
                    } else {
                        // ═══════════════════════════════════════════
                        // 🔒 USUÁRIO DESATIVADO
                        // ═══════════════════════════════════════════
                        
                        Serial.println("╔════════════════════════════════════╗");
                        Serial.printf("║  🔒 ACESSO BLOQUEADO               ║\n");
                        Serial.printf("║  Usuário: %-24s║\n", fp->name);
                        Serial.printf("║  ID: %-3d (DESATIVADO)             ║\n", id);
                        Serial.println("╚════════════════════════════════════╝");
                        
                        // ⭐ v6.0.38: Atualizar auth_display_label (box único)
                        if (auth_display_label) {
                            lv_label_set_text(auth_display_label, "ACESSO\nNEGADO\nDesativado");
                            lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0xef4444), 0);
                            lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0xef4444), 0);
                            lv_obj_invalidate(auth_display_box);
                        }
                        
                        // ⭐ v6.0.25: Resetar modo para bio automático após autenticação
                        currentAuthMode = AUTH_AUTO_BIO;
                    }
                    
                } else {
                    // ═══════════════════════════════════════════
                    // ⚠️ DIGITAL NO SENSOR MAS SEM METADADOS
                    // ═══════════════════════════════════════════
                    
                    Serial.println("⚠️  [BIOMETRIA] Digital reconhecida mas sem metadados no NVS");
                    Serial.printf("    ID=%d, Confiança=%d\n", id, confidence);
                    
                    // ⭐ v6.0.38: Atualizar auth_display_label (box único)
                    if (auth_display_label) {
                        lv_label_set_text(auth_display_label, "DIGITAL\nNAO\nCADASTRADA");
                        lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0xf59e0b), 0);
                        lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0xf59e0b), 0);
                        lv_obj_invalidate(auth_display_box);
                    }
                    
                    // ⭐ v6.0.25: Resetar modo para bio automático após autenticação
                    currentAuthMode = AUTH_AUTO_BIO;
                }
        }
        // ✅ Se verifyFinger() retornou false: sem dedo ou não reconhecido (silencioso)
        
        bioProcessing = false;
    }
    // ═══════════════════════════════════════════════════════════════════════
    // FIM: AUTENTICAÇÃO BIOMÉTRICA CONTÍNUA v6.0.22
    // ═══════════════════════════════════════════════════════════════════════
    
    // ═══════════════════════════════════════════════════════════════════════
    // ⭐ AUTENTICAÇÃO RFID v6.0.47 - REFATORADO COMPLETO
    // Baseado no modelo da BIOMETRIA (linhas 1204-1330)
    // ═══════════════════════════════════════════════════════════════════════
    #if PN532_ENABLED
    static bool rfidProcessing = false;
    
    // ✅ PADRÃO DO CÓDIGO FUNCIONAL: POLLING CONTÍNUO SEM DEBOUNCE (igual biometria)
    if (currentScreen == SCREEN_HOME &&                      // Apenas na HOME
        !rfidProcessing &&                                   // Não processar se já está processando
        !rfid_enrolling &&                                   // Não verificar durante cadastro
        currentAuthMode == AUTH_RFID &&                      // Modo RFID ativo
        rfidManager.isHardwareConnected()) {                 // Sensor conectado
        
        rfidProcessing = true;
        
        uint8_t uid[7];
        uint8_t uidLength = 0;
        
        // ═══ VERIFICAR CARTÃO RFID ═══
        if (rfidManager.detectCard() && rfidManager.readCard(uid, &uidLength)) {
            Serial.print("💳 [RFID] Cartão detectado! UID: ");
            for (int i = 0; i < uidLength; i++) {
                Serial.printf("%02X", uid[i]);
            }
            Serial.println();
            
            // ═══ BUSCAR INFORMAÇÕES DO CARTÃO ═══
            int index = rfidManager.findCardIndex(uid, uidLength);
            
            if (index >= 0) {
                RFIDCard* card = rfidManager.getCard(index);
                
                if (card && card->active && rfidManager.isCardAuthorized(uid, uidLength)) {
                        // ═══════════════════════════════════════════
                        // 🔓 ACESSO CONCEDIDO (RFID)!
                        // ═══════════════════════════════════════════
                        Serial.println("╔════════════════════════════════════╗");
                        Serial.printf("║  🔓 ACESSO CONCEDIDO (RFID)        ║\n");
                        Serial.printf("║  Cartão: %-26s║\n", card->name);
                        Serial.printf("║  Acessos: %-4d                     ║\n", card->access_count);
                        Serial.println("╚════════════════════════════════════╝");
                        
                        // ⭐ v6.0.43: Atualizar auth_display COM REDESENHO FORÇADO
                        Serial.printf("🔍 [DEBUG] Atualizando auth_display_label: %p\n", auth_display_label);
                        if (auth_display_label) {
                            char rfid_msg[40];
                            snprintf(rfid_msg, sizeof(rfid_msg), "ACESSO\nCONCEDIDO\n%s", card->name);
                            
                            // Debug: Verificar estado ANTES
                            bool was_hidden = lv_obj_has_flag(auth_display_box, LV_OBJ_FLAG_HIDDEN);
                            Serial.printf("   🔍 Box ANTES: Hidden=%d\n", was_hidden);
                            
                            lv_obj_clear_flag(auth_display_box, LV_OBJ_FLAG_HIDDEN);  // ⭐ MOSTRAR BOX!
                            
                            // Debug: Verificar estado DEPOIS
                            bool is_hidden = lv_obj_has_flag(auth_display_box, LV_OBJ_FLAG_HIDDEN);
                            Serial.printf("   🔍 Box DEPOIS: Hidden=%d\n", is_hidden);
                            
                            // ⭐ v6.0.51: ORDEM CORRETA - largura e wrap ANTES do texto!
                            lv_obj_set_width(auth_display_label, 180);  // 1º: Largura fixa
                            lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);  // 2º: Word wrap
                            lv_label_set_text(auth_display_label, rfid_msg);  // 3º: Texto (POR ÚLTIMO!)
                            
                            lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);  // Fundo escuro original
                            lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);  // Fonte grande
                            lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0x10b981), 0);  // Verde
                            lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0x10b981), 0);  // Borda verde
                            lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);  // Alinhamento central
                            lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);  // ⭐ align() ao invés de center()
                            lv_obj_move_foreground(auth_display_box);
                            lv_obj_update_layout(auth_display_box);
                            
                            // ⭐ v6.0.46: FORÇAR REDESENHO COMPLETO
                            lv_obj_invalidate(auth_display_box);
                            lv_refr_now(NULL);
                            lv_obj_invalidate(auth_display_box);
                            lv_task_handler();
                            Serial.printf("   ✓ RFID atualizado: '%s'\n", rfid_msg);
                        } else {
                            Serial.println("   ❌ auth_display_label é NULL!");
                        }
                        
                        // ═══ ATIVAR RELÉ (DESTRANCAR PORTA) ═══
                        #if RELAY_ENABLED
                        Serial.println("🔓 Ativando relé (destrancando porta)...");
                        relayController.unlock();
                        Serial.println("✅ Porta destrancada por 3 segundos");
                        #endif
                        
                        // ⭐ v6.0.41: NÃO resetar modo - aguardar timeout
                        // (removido: currentAuthMode = AUTH_AUTO_BIO)
                        
                    } else if (card && !card->active) {
                        // Cartão desativado
                        Serial.println("╔════════════════════════════════════╗");
                        Serial.printf("║  🔒 ACESSO BLOQUEADO (RFID)        ║\n");
                        Serial.printf("║  Cartão: %-26s║\n", card->name);
                        Serial.println("║  Status: DESATIVADO                ║");
                        Serial.println("╚══════════════════════��═════════════╝");
                        
                        // ⭐ v6.0.41: Atualizar auth_display (box único)
                        if (auth_display_label) {
                            lv_obj_set_width(auth_display_label, 180);
                            lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);
                            lv_label_set_text(auth_display_label, "ACESSO\nNEGADO\nDesativado");
                            lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);  // Fundo escuro original
                            lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);  // Fonte grande
                            lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0xef4444), 0);  // Vermelho
                            lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0xef4444), 0);  // Borda vermelha
                            lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
                            lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);
                            lv_obj_invalidate(auth_display_box);
                            lv_obj_invalidate(auth_display_label);
                        }
                        
                        // ⭐ v6.0.41: NÃO resetar modo - aguardar timeout
                } else {
                    // Cartão não cadastrado (não autorizado)
                    Serial.println("⚠️  [RFID] Cartão não cadastrado");
                    
                    if (auth_display_label) {
                        lv_obj_set_width(auth_display_label, 180);
                        lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);
                        lv_label_set_text(auth_display_label, "CARTAO\nNAO\nCADASTRADO");
                        lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);
                        lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);
                        lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0xf59e0b), 0);
                        lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0xf59e0b), 0);
                        lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
                        lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);
                    }
                }
            } else {
                // ═══ CARTÃO NÃO ENCONTRADO (index < 0) ═══
                Serial.println("⚠️  [RFID] Cartão não cadastrado");
                
                if (auth_display_label) {
                    lv_obj_set_width(auth_display_label, 180);
                    lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);
                    lv_label_set_text(auth_display_label, "CARTAO\nNAO\nCADASTRADO");
                    lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);
                    lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);
                    lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0xf59e0b), 0);
                    lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0xf59e0b), 0);
                    lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
                    lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);
                }
            }
        }
        
        rfidProcessing = false;
    }
    #endif
    // ═══════════════════════════════════════════════════════════════════════
    // FIM: AUTENTICAÇÃO RFID v6.0.47
    // ═══════════════════════════════════════════════════════════════════════
    
    // ⭐ v6.0.25: Timeout de modo de autenticação (resetar após 10s sem leitura)
    if (currentAuthMode != AUTH_AUTO_BIO && authModeStartTime > 0) {
        if (millis() - authModeStartTime > AUTH_TIMEOUT) {
            Serial.println("⏱️  [AUTH] Timeout - voltando para modo bio automático");
            currentAuthMode = AUTH_AUTO_BIO;
            authModeStartTime = 0;
            
            // ⭐ v6.0.52: Restaurar modo PIN (box único) COM FUNDO ESCURO ORIGINAL
            if (auth_display_box && auth_display_label) {
                lv_obj_set_size(auth_display_box, 200, 50);  // Tamanho PIN
                lv_obj_set_width(auth_display_label, 180);
                lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);
                lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);  // Fundo escuro original
                lv_obj_set_style_border_color(auth_display_box, lv_color_hex(COLOR_ACCENT), 0);  // Azul
                lv_label_set_text(auth_display_label, "----");
                lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);
                lv_obj_set_style_text_color(auth_display_label, lv_color_hex(COLOR_ACCENT), 0);
                lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);
                lv_obj_invalidate(auth_display_box);
                lv_obj_invalidate(auth_display_label);
            }
        }
    }
    
    uint32_t current_tick = millis();
    uint32_t elapsed = current_tick - last_tick;
    
    // Incrementar tick do LVGL (essencial para animações e timers)
    if (elapsed > 0) {
        lv_tick_inc(elapsed);
        last_tick = current_tick;
    }
    
    // Processar tarefas LVGL
    lv_timer_handler();
    
    // ⭐ NOVO v6.0.24: Gerenciar mensagens temporárias na HOME
    if (home_message_label && home_message_timer > 0) {
        if (millis() - home_message_timer > HOME_MESSAGE_DURATION) {
            lv_obj_add_flag(home_message_label, LV_OBJ_FLAG_HIDDEN);
            home_message_timer = 0;
        }
    }
    
    // Processar comandos serial para calibração
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();  // Remove \r, \n, espaços
        cmd.toUpperCase();  // Converte para maiúsculas
        
        if (cmd.startsWith("P") && cmd.length() == 2) {
            processar_preset(cmd);
        } else if (cmd.length() > 0) {
            processar_comando_calibracao(cmd);
        }
    }
    
    delay(5);
    esp_task_wdt_reset();
}

// ========================================
// GERENCIAMENTO DE TELAS
// ========================================

void mudar_tela(Screen nova_tela) {
    Serial.printf("🔄 Mudando tela: %d → %d\n", currentScreen, nova_tela);
    
    currentScreen = nova_tela;
    
    // Limpar conteúdo anterior
    if (content_container != NULL) {
        lv_obj_del(content_container);
        content_container = NULL;
    }
    
    // ⭐ LAYOUT ATUALIZADO: Header(20px) + Content(300px) = 320px (SEM FOOTER)
    content_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content_container, 480, 300);
    lv_obj_align(content_container, LV_ALIGN_TOP_LEFT, 0, 20);
    lv_obj_set_style_bg_color(content_container, lv_color_hex(COLOR_BG_DARK), 0);
    lv_obj_set_style_border_width(content_container, 0, 0);
    lv_obj_set_style_pad_all(content_container, 6, 0);
    lv_obj_clear_flag(content_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(content_container, LV_OBJ_FLAG_CLICKABLE); // ⭐ NÃO capturar eventos
    
    // Renderizar conteúdo da tela
    switch (nova_tela) {
        case SCREEN_HOME:
            criar_conteudo_home();
            break;
        case SCREEN_BIOMETRIC:
            criar_conteudo_biometric();
            break;
        case SCREEN_RFID:
            criar_conteudo_rfid();
            break;
        case SCREEN_MAINTENANCE:
            criar_conteudo_maintenance();
            break;
        case SCREEN_CONTROLS:
            criar_conteudo_controls();
            break;
        case SCREEN_SETTINGS:
            criar_conteudo_settings();
            break;
        case SCREEN_CALIBRATION:
            criar_conteudo_calibration();
            break;
        case SCREEN_ADMIN_AUTH:
            criar_conteudo_admin_auth();
            break;
    }
    
    // Atualizar navegação (apenas se os botões existirem - HOME screen)
    if (nova_tela == SCREEN_HOME) {
        atualizar_navegacao();
    }
    
    esp_task_wdt_reset();
}

// ========================================
// FUNÇÕES DE CALIBRAÇÃO VIA SERIAL
// ========================================

void processar_preset(String cmd) {
    Serial.println("\n🎯 ═══════════════════════════════════════");
    Serial.println("   PROCESSANDO PRESET DE CALIBRAÇÃO");
    Serial.println("═══════════════════════════════════════");
    
    // Comando no formato: P1, P2, P3, etc
    int preset = cmd.substring(1).toInt();
    
    switch (preset) {
        case 1:  // Preset 1: Valores do log (invertido)
            touch_min_x = 400;
            touch_max_x = 3950;
            touch_min_y = 330;
            touch_max_y = 3650;
            Serial.println("✅ PRESET 1: Valores do log (invertido)");
            break;
            
        case 2:  // Preset 2: Valores originais
            touch_min_x = TOUCH_MIN_X;
            touch_max_x = TOUCH_MAX_X;
            touch_min_y = TOUCH_MIN_Y;
            touch_max_y = TOUCH_MAX_Y;
            Serial.println("✅ PRESET 2: Valores padrão do config.h");
            break;
            
        case 3:  // Preset 3: Valores alternativos
            touch_min_x = 300;
            touch_max_x = 3900;
            touch_min_y = 250;
            touch_max_y = 3700;
            Serial.println("✅ PRESET 3: Valores alternativos");
            break;
            
        default:
            Serial.printf("❌ PRESET %d não encontrado!\n", preset);
            Serial.println("Presets disponíveis: P1, P2, P3");
            return;
    }
    
    imprimir_status_calibracao();
    Serial.println("💾 Para salvar: digite 'SAVE'");
    Serial.println("═══════════════════════════════════════\n");
}

void processar_comando_calibracao(String cmd) {
    // cmd já vem com trim() and toUpperCase() do loop()
    
    if (cmd == "HELP") {
        Serial.println("\n📖 ═══════════════════════════════════════");
        Serial.println("   COMANDOS DE CALIBRAÇÃO DISPONÍVEIS");
        Serial.println("═══════════════════════════════════════");
        Serial.println("P1         - Carregar preset 1 (valores do log)");
        Serial.println("P2         - Carregar preset 2 (valores padrão)");
        Serial.println("P3         - Carregar preset 3 (valores alternativos)");
        Serial.println("STATUS     - Mostrar calibração atual");
        Serial.println("SAVE       - Salvar calibração na Flash");
        Serial.println("LOAD       - Carregar calibração da Flash");
        Serial.println("RESET      - Resetar para valores padrão");
        Serial.println("TEST       - Entrar em modo de teste");
        Serial.println("HELP       - Mostrar esta ajuda");
        Serial.println("═══════════════════════════════════════\n");
    }
    else if (cmd == "STATUS") {
        imprimir_status_calibracao();
    }
    else if (cmd == "SAVE") {
        salvar_calibracao();
    }
    else if (cmd == "LOAD") {
        carregar_calibracao();
    }
    else if (cmd == "RESET") {
        touch_min_x = TOUCH_MIN_X;
        touch_max_x = TOUCH_MAX_X;
        touch_min_y = TOUCH_MIN_Y;
        touch_max_y = TOUCH_MAX_Y;
        Serial.println("✅ Calibração resetada para valores padrão");
        imprimir_status_calibracao();
    }
    else if (cmd == "TEST") {
        Serial.println("\n🧪 ═══════════════════════════════════════");
        Serial.println("   MODO DE TESTE ATIVADO");
        Serial.println("═══════════════════════════════════════");
        Serial.println("Toque na tela para ver coordenadas.");
        Serial.println("Digite 'HELP' para sair do modo de teste.");
        Serial.println("═══════════════════════════════════════\n");
    }
    else if (cmd.length() > 0) {
        Serial.printf("❌ Comando '%s' não reconhecido\n", cmd.c_str());
        Serial.println("💡 Digite 'HELP' para ver comandos disponíveis\n");
    }
}

// ========================================
// CABEÇALHO (Header)
// ========================================

void criar_header() {
    lv_obj_t * header = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header, 480, 20);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(COLOR_BG_DARK), 0);
    lv_obj_set_style_border_color(header, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(header, 3, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    // Título esquerdo
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "CONTROLE DE ACESSO");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 15, 0);
    
    // Status ESP32 (direita)
    header_status_dot = lv_obj_create(header);
    lv_obj_set_size(header_status_dot, 6, 6);
    lv_obj_align(header_status_dot, LV_ALIGN_RIGHT_MID, -40, 0);
    lv_obj_set_style_bg_color(header_status_dot, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_style_radius(header_status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(header_status_dot, 0, 0);
    
    header_signal = lv_label_create(header);
    lv_label_set_text(header_signal, "ESP32 95%");
    lv_obj_set_style_text_font(header_signal, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(header_signal, lv_color_hex(0x9CA3AF), 0);
    lv_obj_align(header_signal, LV_ALIGN_RIGHT_MID, -5, 0);
}

// ========================================
// RODAPÉ (Footer Navigation)
// ========================================

void btn_nav_clicked(lv_event_t * e) {
    int screen_index = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("🔘 Navegação clicada! Botão: %d\n", screen_index);
    mudar_tela((Screen)screen_index);
}

void criar_footer() {
    lv_obj_t * footer = lv_obj_create(lv_scr_act());
    lv_obj_set_size(footer, 480, 50);  // ⭐ AUMENTADO: 35px → 50px
    lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(footer, lv_color_hex(COLOR_BG_DARK), 0);
    lv_obj_set_style_border_color(footer, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(footer, 1, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(footer, 4, 0);  // ⭐ AUMENTADO: 3px → 4px
    lv_obj_set_style_pad_column(footer, 3, 0);  // ⭐ AUMENTADO: 2px → 3px
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 6 botões de navegação - MAIORES e mais fáceis de tocar
    const char * labels[] = {"ACESSO", "BIO", "RFID", "MANUT", "LOGS", "CONFIG"};
    
    for (int i = 0; i < 6; i++) {
        nav_buttons[i] = lv_btn_create(footer);
        lv_obj_set_size(nav_buttons[i], 75, 42);  // ⭐ ALTURA: 28px → 42px
        lv_obj_set_style_bg_color(nav_buttons[i], lv_color_hex(COLOR_BG_MEDIUM), 0);
        lv_obj_set_style_radius(nav_buttons[i], 4, 0);
        lv_obj_add_flag(nav_buttons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(nav_buttons[i], btn_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        lv_obj_t * label = lv_label_create(nav_buttons[i]);
        lv_label_set_text(label, labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);  // ⭐ FONTE: 8 → 10
        lv_obj_center(label);
    }
}

void atualizar_navegacao() {
    // ⭐ DESABILITADA - Os botões de navegação agora estão integrados na tela HOME
    // Não há mais nav_buttons[] globais
}

// ========================================
// TELA HOME (Acesso PIN)
// ========================================

void btn_pin_number_clicked(lv_event_t * e) {
    // 🛡️ VALIDAÇÃO 1: Rejeitar se toque começou em gap
    if (touch_in_gap) {
        Serial.println("🚫 Toque ignorado (começou em GAP)");
        return;
    }
    
    // 🛡️ VALIDAÇÃO 2: DEBOUNCE - Ignorar cliques muito rápidos (< 150ms)
    static unsigned long ultimo_clique = 0;
    if (millis() - ultimo_clique < 150) {
        Serial.println("⏭️ Clique ignorado (debounce)");
        return;
    }
    ultimo_clique = millis();
    
    Serial.println("🔢 [CALLBACK] btn_pin_number_clicked CHAMADO!");
    
    lv_obj_t * btn = lv_event_get_target(e);
    const char * numero = (const char *)lv_event_get_user_data(e);
    
    // 🔍 DEBUG: Coordenadas do botão clicado
    lv_coord_t btn_x = lv_obj_get_x(btn);
    lv_coord_t btn_y = lv_obj_get_y(btn);
    Serial.printf("🔍 Botão na posição: X=%d, Y=%d\n", btn_x, btn_y);
    
    if (numero == NULL) {
        Serial.println("❌ [ERRO] numero é NULL!");
        return;
    }
    
    Serial.printf("🔢 Botão numérico clicado: %s\n", numero);
    
    if (currentPin.length() < 6) {
        currentPin += numero;
        
        String masked = "";
        for (unsigned int i = 0; i < currentPin.length(); i++) {
            masked += "* ";
        }
        lv_label_set_text(pin_display_label, masked.c_str());
        
        Serial.printf("🔢 PIN atual: %s (length=%d)\n", currentPin.c_str(), currentPin.length());
    } else {
        Serial.println("⚠️  PIN já tem 6 dígitos!");
    }
}

void btn_pin_clear_clicked(lv_event_t * e) {
    Serial.println("🗑️ Botão CLR clicado!");
    currentPin = "";
    lv_label_set_text(pin_display_label, "----");
    lv_obj_set_style_text_color(pin_display_label, lv_color_hex(COLOR_ACCENT), 0);
    Serial.println("🗑️ PIN limpo");
}

void btn_pin_backspace_clicked(lv_event_t * e) {
    Serial.println("⬅️ Botão DEL clicado!");
    if (currentPin.length() > 0) {
        currentPin.remove(currentPin.length() - 1);
        
        String masked = "";
        if (currentPin.length() == 0) {
            masked = "----";
        } else {
            for (unsigned int i = 0; i < currentPin.length(); i++) {
                masked += "* ";
            }
        }
        lv_label_set_text(pin_display_label, masked.c_str());
        Serial.printf("⬅️ PIN após DEL: %s\n", currentPin.c_str());
    }
}

void btn_pin_confirm_clicked(lv_event_t * e) {
    Serial.printf("✅ Botão OK clicado! Validando PIN: %s\n", currentPin.c_str());
    
    if (currentPin == correctPin) {
        Serial.println("✅ PIN CORRETO! Liberando acesso...");
        lv_label_set_text(pin_display_label, "ACESSO OK!");
        lv_obj_set_style_text_color(pin_display_label, lv_color_hex(COLOR_SUCCESS), 0);
    } else {
        Serial.printf("❌ PIN INCORRETO! Esperado: %s, Recebido: %s\n", correctPin.c_str(), currentPin.c_str());
        lv_label_set_text(pin_display_label, "PIN ERRADO!");
        lv_obj_set_style_text_color(pin_display_label, lv_color_hex(COLOR_ERROR), 0);
    }
    
    currentPin = "";
}

/* ═══════════════════════════════════════════════════════════════════════
 * ⭐ v6.0.38: FUNÇÕES OBSOLETAS - REMOVIDAS (conflitavam com box único)
 * ═══════════════════════════════════════════════════════════════════════
 * btn_rfid_clicked() e btn_biometric_clicked() foram REMOVIDAS em v6.0.38
 * pois usavam flags HIDDEN que conflitavam com o auth_display_box único.
 * O controle de BIO/RFID é feito diretamente no handler de navegação.
 * ═══════════════════════════════════════════════════════════════════════ */

// ⭐ Array ESTÁTICO para preservar os ponteiros
static const char * keypad_numeros[] = {"1","2","3","4","5","6","7","8","9","*","0","#"};

void criar_conteudo_home() {
    Serial.println("🏠 Criando TELA HOME - Layout 2 Colunas Organizado");
    
    // ╔════════════════════════════════════════════════════════════════╗
    // ║  LAYOUT 2 COLUNAS (480x300px total)                            ║
    // ║  • Esquerda: 234px (Display + Navegação)                       ║
    // ║  • Gap: 6px                                                    ║
    // ║  • Direita: 240px (Teclado numérico)                          ║
    // ╚════════════════════════════════════════════════════════════════╝
    
    // ════════════════════════════════════════════════════════════════
    // 📱 COLUNA ESQUERDA (234px)
    // ════════════════════════════════════════════════════════════════
    
    // Container da coluna esquerda (sem padding, sem borda)
    lv_obj_t * left_column = lv_obj_create(content_container);
    lv_obj_set_size(left_column, 234, 300);
    lv_obj_set_pos(left_column, 0, 0);
    lv_obj_set_style_bg_color(left_column, lv_color_hex(COLOR_BG_DARK), 0);
    lv_obj_set_style_border_width(left_column, 0, 0);
    lv_obj_set_style_pad_all(left_column, 0, 0);
    lv_obj_clear_flag(left_column, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(left_column, LV_OBJ_FLAG_CLICKABLE);
    
    // ────────────────────────────────────────────────────────────────
    // 🖥️ ÁREA 1: DISPLAY DO PIN (130px altura)
    // ────────────────────────────────────────────────────────────────
    lv_obj_t * display_container = lv_obj_create(left_column);
    lv_obj_set_size(display_container, 234, 130);
    lv_obj_set_pos(display_container, 0, 0);
    lv_obj_set_style_bg_color(display_container, lv_color_hex(COLOR_BG_MEDIUM), 0);
    lv_obj_set_style_border_color(display_container, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(display_container, 1, 0);
    lv_obj_set_style_radius(display_container, 4, 0);
    lv_obj_set_style_pad_all(display_container, 6, 0);
    lv_obj_clear_flag(display_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(display_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(display_container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // ⭐ PERMITIR overflow!
    
    // ═══ v6.0.37: BOX ÚNICO (modelo PIN) - Troca conteúdo dinamicamente ═══
    
    auth_display_box = lv_obj_create(left_column);  // ⭐ DIRETO no left_column!
    lv_obj_set_size(auth_display_box, 200, 50);  // Tamanho inicial PIN (menor)
    lv_obj_set_pos(auth_display_box, 17, 10);  // Posição absoluta: X=17 (centralizado), Y=10
    lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);
    lv_obj_set_style_border_color(auth_display_box, lv_color_hex(COLOR_ACCENT), 0);  // Azul padrão (PIN)
    lv_obj_set_style_border_width(auth_display_box, 2, 0);
    lv_obj_set_style_radius(auth_display_box, 4, 0);
    lv_obj_clear_flag(auth_display_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(auth_display_box, LV_OBJ_FLAG_CLICKABLE);
    
    auth_display_label = lv_label_create(auth_display_box);
    lv_obj_set_width(auth_display_label, 180);  // ⭐ Largura fixa ANTES do texto
    lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);  // ⭐ Wrap ANTES do texto
    lv_label_set_text(auth_display_label, "----");  // Texto inicial (PIN)
    lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(auth_display_label, lv_color_hex(COLOR_ACCENT), 0);  // Azul padrão
    lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);  // ⭐ CORRIGIDO: align() ao invés de center()
    lv_obj_move_foreground(auth_display_box);  // ⭐ v6.0.46: Box sempre na frente!
    lv_obj_add_flag(left_column, LV_OBJ_FLAG_OVERFLOW_VISIBLE);  // ⭐ PERMITIR OVERFLOW!
    
    // ⭐ Compatibilidade: apontar variáveis antigas para o box/label únicos
    pin_box = auth_display_box;
    bio_box = auth_display_box;
    rfid_box = auth_display_box;
    pin_display_label = auth_display_label;
    bio_display_label = auth_display_label;
    rfid_display_label = auth_display_label;
    
    Serial.printf("✅ auth_display_box criado: %p (parent: left_column)\n", auth_display_box);
    Serial.printf("   📐 Tamanho inicial: 200x50 (PIN mode)\n");
    Serial.printf("   📍 Posição: X=17, Y=10\n");
    
    // ────────────────────────────────────────────────────────────────
    // 🎯 ÁREA 2: BOTÕES DE NAVEGAÇÃO (166px altura)
    // Grade 3×2 com 6 botões
    // ────────────────────────────────────────────────────────────────
    lv_obj_t * nav_area = lv_obj_create(left_column);
    lv_obj_set_size(nav_area, 234, 166);
    lv_obj_set_pos(nav_area, 0, 134); // 130 + 4px gap = 134
    lv_obj_set_style_bg_color(nav_area, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(nav_area, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(nav_area, 1, 0);
    lv_obj_set_style_radius(nav_area, 4, 0);
    lv_obj_set_style_pad_all(nav_area, 4, 0);
    lv_obj_clear_flag(nav_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(nav_area, LV_OBJ_FLAG_CLICKABLE);
    
    // Cálculo dos botões (grade 3×2)
    // Área útil: 234 - 8 = 226px largura, 166 - 8 = 158px altura
    // Gap entre botões: 6px
    // Largura botão: (226 - 6×2) / 3 ≈ 71px
    // Altura botão: (158 - 6×1) / 2 = 76px
    int nav_btn_w = 71;
    int nav_btn_h = 76;
    int nav_gap = 6;
    
    const char* nav_labels[] = {"ACESSO", "BIO", "RFID", "MANUT", "AJUDA", "CONFIG"};
    const uint32_t nav_colors[] = {COLOR_BLUE, 0x8B5CF6, COLOR_CYAN, COLOR_ORANGE, 0x22c55e, 0x6B7280};
    const Screen nav_screens[] = {SCREEN_HOME, SCREEN_BIOMETRIC, SCREEN_RFID, SCREEN_MAINTENANCE, SCREEN_CONTROLS, SCREEN_SETTINGS};
    
    Serial.println("🎯 Criando 6 botões de navegação (3×2)...");
    for (int i = 0; i < 6; i++) {
        int row = i / 3;
        int col = i % 3;
        
        lv_obj_t * nav_btn = lv_btn_create(nav_area);
        lv_obj_set_size(nav_btn, nav_btn_w, nav_btn_h);
        lv_obj_set_pos(nav_btn, col * (nav_btn_w + nav_gap), row * (nav_btn_h + nav_gap));
        
        // Destacar HOME como ativo
        if (i == 0) {
            lv_obj_set_style_bg_color(nav_btn, lv_color_hex(nav_colors[i]), 0);
            lv_obj_set_style_border_width(nav_btn, 0, 0);
        } else {
            lv_obj_set_style_bg_color(nav_btn, lv_color_hex(0x252540), 0);
            lv_obj_set_style_border_color(nav_btn, lv_color_hex(0x404060), 0);
            lv_obj_set_style_border_width(nav_btn, 1, 0);
        }
        
        lv_obj_set_style_radius(nav_btn, 3, 0);
        lv_obj_add_flag(nav_btn, LV_OBJ_FLAG_CLICKABLE);
        
        // ⭐ Salvar referência para debug
        nav_buttons[i] = nav_btn;
        
        lv_obj_add_event_cb(nav_btn, [](lv_event_t * e) {
            Screen target_screen = (Screen)(intptr_t)lv_event_get_user_data(e);
            lv_obj_t * btn = lv_event_get_target(e);
            lv_coord_t x = lv_obj_get_x(btn);
            lv_coord_t y = lv_obj_get_y(btn);
            lv_coord_t w = lv_obj_get_width(btn);
            lv_coord_t h = lv_obj_get_height(btn);
            
            Serial.printf("🔘 Nav [%d] clicado → tela %d\n", (int)(intptr_t)lv_event_get_user_data(e), target_screen);
            Serial.printf("   📍 Botão posição: X=%d, Y=%d, W=%d, H=%d\n", x, y, w, h);
            Serial.printf("   📍 Área absoluta: X[%d-%d], Y[%d-%d]\n", x, x+w, y, y+h);
            
            // ⭐ NOVA LÓGICA: Verificar autenticação para CONFIG
            if (target_screen == SCREEN_SETTINGS) {
                Serial.println("🔧 Botão CONFIG clicado");
#if ADMIN_AUTH_ENABLED
                if (adminAuth.isEnabled()) {
                    if (adminAuth.isAuthenticated()) {
                        Serial.println("[AdminAuth] ✓ Já autenticado, abrindo CONFIG");
                        mudar_tela(SCREEN_SETTINGS);
                    } else {
                        Serial.println("[AdminAuth] 🔐 Autenticação necessária");
                        mudar_tela(SCREEN_ADMIN_AUTH);
                    }
                } else {
                    Serial.println("[AdminAuth] ⚠ Sistema desabilitado, acesso direto");
                    mudar_tela(SCREEN_SETTINGS);
                }
#else
                mudar_tela(SCREEN_SETTINGS);
#endif
            }
            // ⭐ v6.0.30: ACESSO, BIO e RFID com controle de visibilidade
            else if (target_screen == SCREEN_HOME) {
                Serial.println("🔐 Botão ACESSO (PIN) clicado!");
                Serial.println("🔐 Modo PIN ativado");
                
                // ⭐ v6.0.52: Modo PIN - AZUL VIBRANTE COM FUNDO ESCURO ORIGINAL
                Serial.printf("   🔧 Mudando para modo PIN (box: %p, label: %p)\n", auth_display_box, auth_display_label);
                lv_obj_set_size(auth_display_box, 200, 50);  // Tamanho PIN (menor)
                lv_obj_set_width(auth_display_label, 180);
                lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);
                lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);  // ⭐ Fundo escuro original
                lv_obj_set_style_border_color(auth_display_box, lv_color_hex(COLOR_ACCENT), 0);  // Borda azul
                lv_label_set_text(auth_display_label, "----");
                lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);  // Fonte grande
                lv_obj_set_style_text_color(auth_display_label, lv_color_hex(COLOR_ACCENT), 0);  // Azul
                lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);
                lv_obj_move_foreground(auth_display_box);  // Trazer para frente
                lv_obj_invalidate(auth_display_box);  // Forçar redesenho
                lv_obj_invalidate(auth_display_label);  // Forçar redesenho do label
                Serial.printf("   ✅ Modo PIN ativado: Texto='%s', Cor=AZUL/BRANCO\n", lv_label_get_text(auth_display_label));
                Serial.printf("   📊 Box: %dx%d, Hidden=%d, Label: '%s'\n", 
                    lv_obj_get_width(auth_display_box), 
                    lv_obj_get_height(auth_display_box),
                    !lv_obj_has_flag(auth_display_box, LV_OBJ_FLAG_HIDDEN),
                    lv_label_get_text(auth_display_label));
                lv_obj_set_style_bg_color(nav_buttons[0], lv_color_hex(COLOR_ACCENT), 0);
                lv_obj_set_style_bg_color(nav_buttons[1], lv_color_hex(0x1E293B), 0);
                lv_obj_set_style_bg_color(nav_buttons[2], lv_color_hex(0x1E293B), 0);
                
                currentAuthMode = AUTH_NONE; // Modo normal (aguardando PIN manual)
                currentPin = "";  // Limpar PIN atual
            }
            else if (target_screen == SCREEN_BIOMETRIC) {
                Serial.println("👆 Botão BIO clicado!");
                Serial.println("👆 Leitura biométrica solicitada");
                
                // ⭐ v6.0.52: Modo BIO - ROXO VIBRANTE COM FUNDO ESCURO ORIGINAL
                Serial.printf("   🔧 Mudando para modo BIO (box: %p, label: %p)\n", auth_display_box, auth_display_label);
                lv_obj_set_size(auth_display_box, 200, 70);  // Tamanho BIO (maior)
                lv_obj_set_width(auth_display_label, 180);
                lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);
                lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);  // ⭐ Fundo escuro original
                lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0xa78bfa), 0);  // Borda roxa
                lv_label_set_text(auth_display_label, "Posicione\ndedo...");
                lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);  // ⭐ Fonte grande
                lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0xa78bfa), 0);  // Roxo
                lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);
                lv_obj_move_foreground(auth_display_box);  // Trazer para frente
                lv_obj_invalidate(auth_display_box);  // Forçar redesenho
                lv_obj_invalidate(auth_display_label);  // Forçar redesenho do label
                Serial.printf("   ✅ Modo BIO ativado: Texto='%s', Cor=ROXO/BRANCO\n", lv_label_get_text(auth_display_label));
                Serial.printf("   📊 Box: %dx%d, Hidden=%d, Label: '%s'\n", 
                    lv_obj_get_width(auth_display_box), 
                    lv_obj_get_height(auth_display_box),
                    !lv_obj_has_flag(auth_display_box, LV_OBJ_FLAG_HIDDEN),
                    lv_label_get_text(auth_display_label));
                lv_obj_set_style_bg_color(nav_buttons[0], lv_color_hex(0x1E293B), 0);
                lv_obj_set_style_bg_color(nav_buttons[1], lv_color_hex(0xa78bfa), 0);
                lv_obj_set_style_bg_color(nav_buttons[2], lv_color_hex(0x1E293B), 0);
                
                currentAuthMode = AUTH_BIO_MANUAL; // ⭐ v6.0.25: Ativar modo BIO manual
                authModeStartTime = millis(); // ⭐ v6.0.25: Iniciar timeout
            } else if (target_screen == SCREEN_RFID) {
                Serial.println("💳 Botão RFID clicado!");
                Serial.println("💳 Leitura RFID solicitada");
                
                // ⭐ v6.0.52: Modo RFID - COM REDESENHO FORÇADO
                Serial.printf("   🔧 Mudando para modo RFID (box: %p, label: %p)\n", auth_display_box, auth_display_label);
                lv_obj_clear_flag(auth_display_box, LV_OBJ_FLAG_HIDDEN);  // ⭐ GARANTIR visível ANTES!
                lv_obj_set_size(auth_display_box, 200, 70);  // Tamanho RFID (maior)
                lv_obj_set_width(auth_display_label, 180);
                lv_label_set_long_mode(auth_display_label, LV_LABEL_LONG_WRAP);
                lv_obj_set_style_bg_color(auth_display_box, lv_color_hex(0x0A0A1A), 0);  // Fundo escuro original
                lv_obj_set_style_border_color(auth_display_box, lv_color_hex(0x06b6d4), 0);  // Borda ciano
                lv_label_set_text(auth_display_label, "Aproxime\ncartao...");
                lv_obj_set_style_text_font(auth_display_label, &lv_font_montserrat_20, 0);  // Fonte grande
                lv_obj_set_style_text_color(auth_display_label, lv_color_hex(0x06b6d4), 0);  // Ciano
                lv_obj_set_style_text_align(auth_display_label, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_align(auth_display_label, LV_ALIGN_CENTER, 0, 0);
                lv_obj_move_foreground(auth_display_box);  // Trazer para frente
                lv_obj_invalidate(auth_display_box);  // Forçar redesenho
                lv_obj_invalidate(auth_display_label);  // Forçar redesenho do label
                lv_refr_now(NULL);  // ⭐ FORÇAR REDESENHO IMEDIATO!
                Serial.printf("   ✅ Modo RFID ativado: Texto='%s', Cor=CIANO/BRANCO\n", lv_label_get_text(auth_display_label));
                Serial.printf("   📊 Box APÓS lv_refr_now(): %dx%d, Label: '%s'\n", 
                    lv_obj_get_width(auth_display_box), 
                    lv_obj_get_height(auth_display_box),
                    lv_label_get_text(auth_display_label));
                lv_obj_set_style_bg_color(nav_buttons[0], lv_color_hex(0x1E293B), 0);
                lv_obj_set_style_bg_color(nav_buttons[1], lv_color_hex(0x1E293B), 0);
                lv_obj_set_style_bg_color(nav_buttons[2], lv_color_hex(0x06b6d4), 0);
                
                currentAuthMode = AUTH_RFID; // ⭐ v6.0.25: Desativar polling bio, aguardar RFID
                authModeStartTime = millis(); // ⭐ v6.0.25: Iniciar timeout
            } else {
                mudar_tela(target_screen);
            }
        }, LV_EVENT_CLICKED, (void*)(intptr_t)nav_screens[i]);
        
        lv_obj_t * nav_label = lv_label_create(nav_btn);
        lv_label_set_text(nav_label, nav_labels[i]);
        lv_obj_set_style_text_font(nav_label, &lv_font_montserrat_10, 0);
        lv_obj_center(nav_label);
        
        int abs_y_start = 134 + row * (nav_btn_h + nav_gap);
        int abs_y_end = abs_y_start + nav_btn_h;
        Serial.printf("  ✓ [%d] %s rel(%d,%d) → ABS_Y[%d-%d]\n", i, nav_labels[i], 
                     col * (nav_btn_w + nav_gap), row * (nav_btn_h + nav_gap),
                     abs_y_start, abs_y_end);
    }
    Serial.println("✅ Navegação criada!");
    
    // ⭐ DEBUG: Mostrar coordenadas REAIS dos botões após criação
    Serial.println("\n🔍 ═══ COORDENADAS REAIS DOS BOTÕES ═══");
    for (int i = 0; i < 6; i++) {
        if (nav_buttons[i] != NULL) {
            lv_coord_t x = lv_obj_get_x(nav_buttons[i]);
            lv_coord_t y = lv_obj_get_y(nav_buttons[i]);
            lv_coord_t w = lv_obj_get_width(nav_buttons[i]);
            lv_coord_t h = lv_obj_get_height(nav_buttons[i]);
            Serial.printf("  [%d] %s: X[%d-%d], Y[%d-%d]\n", i, nav_labels[i], x, x+w, y, y+h);
        }
    }
    Serial.println("═══════════════════════════════════════\n");
    
    // ════════════════════════════════════════════════════════════════
    // ⌨️ COLUNA DIREITA - TECLADO NUMÉRICO (240px largura)
    // ════════════════════════════════════════════════════════════════
    
    int base_x = 240;  // 234 (coluna esquerda) + 6 (gap)
    int base_y = 0;
    int btn_w = 75;    // 🎯 Botões: 3×75 + 2×4 = 233px
    int btn_h = 58;    // ⭐ AJUSTADO: Proporção 1.29:1 (75:58)
    int spacing_x = 4;
    int spacing_y = 4;
    
    Serial.println("⌨️  Criando teclado 4×3...");
    Serial.printf("📏 Layout: base_x=%d, btn: %dx%d, gap: %dx%d\n", 
                  base_x, btn_w, btn_h, spacing_x, spacing_y);
    
    // ═══ TECLADO NUMÉRICO: 4 linhas × 3 colunas ═══
    for (int i = 0; i < 12; i++) {
        int row = i / 3;
        int col = i % 3;
        
        int pos_x = base_x + col * (btn_w + spacing_x);
        int pos_y = base_y + row * (btn_h + spacing_y);
        
        lv_obj_t * btn = lv_btn_create(content_container);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, pos_x, pos_y);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x374151), 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(btn, btn_pin_number_clicked, LV_EVENT_CLICKED, (void*)keypad_numeros[i]);
        
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, keypad_numeros[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_center(label);
        
        Serial.printf("  Botão [%d] %s: X=%d, Y=%d\n", i, keypad_numeros[i], pos_x, pos_y);
    }
    
    Serial.println("✅ Teclado 4×3 criado!");
    
    // ═══ CONTROLES ABAIXO: CLR + DEL + OK ═══
    int ctrl_y = base_y + 4 * (btn_h + spacing_y); // Após 4 linhas
    int ctrl_h = 45;   // ⭐ AJUSTADO: Controles maiores e mais confortáveis
    
    Serial.printf("📏 Controles abaixo: Y=%d, H=%dpx\n", ctrl_y, ctrl_h);
    
    // CLR
    lv_obj_t * btn_clr = lv_btn_create(content_container);
    lv_obj_set_size(btn_clr, btn_w, ctrl_h);
    lv_obj_set_pos(btn_clr, base_x, ctrl_y);
    lv_obj_set_style_bg_color(btn_clr, lv_color_hex(COLOR_ERROR), 0);
    lv_obj_set_style_radius(btn_clr, 4, 0);
    lv_obj_add_flag(btn_clr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_clr, btn_pin_clear_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_clr = lv_label_create(btn_clr);
    lv_label_set_text(l_clr, "CLR");
    lv_obj_set_style_text_font(l_clr, &lv_font_montserrat_12, 0);
    lv_obj_center(l_clr);
    
    // DEL
    lv_obj_t * btn_del = lv_btn_create(content_container);
    lv_obj_set_size(btn_del, btn_w, ctrl_h);
    lv_obj_set_pos(btn_del, base_x + (btn_w + spacing_x), ctrl_y);
    lv_obj_set_style_bg_color(btn_del, lv_color_hex(COLOR_WARNING), 0);
    lv_obj_set_style_radius(btn_del, 4, 0);
    lv_obj_add_flag(btn_del, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_del, btn_pin_backspace_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_del = lv_label_create(btn_del);
    lv_label_set_text(l_del, "DEL");
    lv_obj_set_style_text_font(l_del, &lv_font_montserrat_12, 0);
    lv_obj_center(l_del);
    
    // OK
    lv_obj_t * btn_ok = lv_btn_create(content_container);
    lv_obj_set_size(btn_ok, btn_w, ctrl_h);
    lv_obj_set_pos(btn_ok, base_x + 2 * (btn_w + spacing_x), ctrl_y);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_style_radius(btn_ok, 4, 0);
    lv_obj_add_flag(btn_ok, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_ok, btn_pin_confirm_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_ok = lv_label_create(btn_ok);
    lv_label_set_text(l_ok, "OK");
    lv_obj_set_style_text_font(l_ok, &lv_font_montserrat_12, 0);
    lv_obj_center(l_ok);
    
    Serial.printf("✅ Layout final: Y máximo = %d (limite=300)\n", ctrl_y + ctrl_h);
    
    // ⭐ NOVO v6.0.24/v6.0.26: Label de mensagens temporárias
    // ═══ v6.0.26: POSICIONADA NA ÁREA DO DISPLAY PIN ═══
    // Display PIN: X=10, Y=20-120, W=214, H=113
    // Mensagem: Sobrepõe display PIN para feedback visual unificado (PIN, BIO, RFID)
    home_message_label = lv_label_create(lv_scr_act());
    lv_obj_set_size(home_message_label, 214, LV_SIZE_CONTENT);  // Mesma largura do display container
    lv_obj_set_pos(home_message_label, 10, 40); // Sobrepõe área do display PIN
    lv_obj_set_style_text_align(home_message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(home_message_label, &lv_font_montserrat_12, 0); // Fonte menor para caber
    lv_obj_set_style_bg_opa(home_message_label, LV_OPA_COVER, 0); // Opacidade total para cobrir PIN
    lv_obj_set_style_bg_color(home_message_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_color(home_message_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(home_message_label, 10, 0); // Padding menor
    lv_obj_set_style_radius(home_message_label, 6, 0);
    lv_obj_add_flag(home_message_label, LV_OBJ_FLAG_HIDDEN); // Inicialmente oculto
    Serial.printf("✅ [DEBUG] home_message_label criada: %p (posição: X=10, Y=40)\n", home_message_label);
    
    Serial.println("✅ Layout 2 colunas completo!");
}

// ⭐ NOVO v6.0.24/v6.0.26: Função para exibir mensagens temporárias na HOME
void show_home_message(const char* message, uint32_t color) {
    Serial.printf("🔍 [DEBUG] show_home_message() chamada\n");
    Serial.printf("   • message: \"%s\"\n", message);
    Serial.printf("   • color: 0x%06X\n", color);
    Serial.printf("   • home_message_label: %p\n", home_message_label);
    Serial.printf("   • currentScreen: %d (SCREEN_HOME=%d)\n", currentScreen, SCREEN_HOME);
    
    if (!home_message_label) {
        Serial.println("   ❌ home_message_label é NULL!");
        return;
    }
    
    if (currentScreen != SCREEN_HOME) {
        Serial.printf("   ❌ Tela errada! currentScreen=%d\n", currentScreen);
        return;
    }
    
    Serial.println("   ✅ Configurando label...");
    lv_label_set_text(home_message_label, message);
    lv_obj_set_style_bg_color(home_message_label, lv_color_hex(color), 0);
    lv_obj_clear_flag(home_message_label, LV_OBJ_FLAG_HIDDEN);
    home_message_timer = millis();
    
    Serial.println("   ✅ Label configurada e exibida!");
    Serial.printf("📢 [HOME_MSG] %s\n", message);
}

// ========================================
// TELA BIOMETRIC
// ========================================

void btn_voltar_home_clicked(lv_event_t * e) {
    Serial.println("⬅️ [CALLBACK] Botão VOLTAR clicado!");
    Serial.print("🔄 Voltando para HOME desde tela: ");
    Serial.println(currentScreen);
    mudar_tela(SCREEN_HOME);
}

void criar_conteudo_biometric() {
    Serial.println("🔐 Criando TELA BIOMETRIA - Layout Organizado");
    // ═══ LAYOUT ORGANIZADO: 300px altura útil ═══
    // Botão VOLTAR: 5-40px (altura 35, PROPORCIONAL)
    // Header: 45-80px (altura 35)
    // Content: 85-295px (altura 210)
    
    // ═══ BOTÃO VOLTAR (PROPORCIONAL) ═══
    lv_obj_t * btn_voltar = lv_btn_create(content_container);
    lv_obj_set_size(btn_voltar, 120, 35);  // PROPORCIONAL: 120x35 (adequado e clicável)
    lv_obj_set_pos(btn_voltar, 5, 5);      // Margem 5px
    lv_obj_set_style_bg_color(btn_voltar, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(btn_voltar, 6, 0);
    lv_obj_add_flag(btn_voltar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_voltar, btn_voltar_home_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_voltar = lv_label_create(btn_voltar);
    lv_label_set_text(l_voltar, LV_SYMBOL_LEFT " VOLTAR");
    lv_obj_set_style_text_font(l_voltar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_voltar);
    Serial.println("  ✓ Botão VOLTAR: 5,5 (120x35)");
    
    // ═══ CABEÇALHO ═══
    lv_obj_t * header = lv_obj_create(content_container);
    lv_obj_set_size(header, 470, 35);
    lv_obj_set_pos(header, 5, 45);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 5, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    Serial.println("  ✓ Header: 5,45 (470x35)");
    
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "BIOMETRIA AS608");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x9333EA), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    
    lv_obj_t * capacity = lv_label_create(header);
    lv_label_set_text(capacity, "0/162");
    lv_obj_set_style_text_font(capacity, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(capacity, lv_color_hex(0x9CA3AF), 0);
    lv_obj_align(capacity, LV_ALIGN_RIGHT_MID, -5, 0);
    
    // ═══ ÁREA DE CONTEÚDO ═══
    lv_obj_t * content = lv_obj_create(content_container);
    lv_obj_set_size(content, 470, 210);
    lv_obj_set_pos(content, 5, 85);
    Serial.println("  ✓ Content: 5,85 (470x210)");
    Serial.println("✅ BIO Layout: VOLTAR(5-40), HEADER(45-80), CONTENT(85-295)");
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 4, 0);
    lv_obj_set_style_pad_all(content, 8, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * msg1 = lv_label_create(content);
    lv_label_set_text(msg1, "Sensor AS608 - Capacidade: 162 templates");
    lv_obj_set_style_text_font(msg1, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(msg1, lv_color_white(), 0);
    lv_obj_set_pos(msg1, 10, 10);
    
    lv_obj_t * msg2 = lv_label_create(content);
    lv_label_set_text(msg2, "Digitais cadastradas: 0");
    lv_obj_set_style_text_font(msg2, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(msg2, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_pos(msg2, 10, 30);
    
    lv_obj_t * msg3 = lv_label_create(content);
    lv_label_set_text(msg3, "Status: Sensor conectado");
    lv_obj_set_style_text_font(msg3, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(msg3, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_pos(msg3, 10, 50);
    
    lv_obj_t * msg4 = lv_label_create(content);
    lv_label_set_text(msg4, "UART2 - 57600 baud");
    lv_obj_set_style_text_font(msg4, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(msg4, lv_color_hex(0x6B7280), 0);
    lv_obj_set_pos(msg4, 10, 70);
}

// ========================================
// TELA RFID
// ========================================

void criar_conteudo_rfid() {
    Serial.println("📡 Criando TELA RFID - Layout Organizado");
    // ═══ LAYOUT ORGANIZADO: 300px altura útil ═══
    // Botão VOLTAR: 5-40px (altura 35, PROPORCIONAL)
    // Header: 45-80px (altura 35)
    // Protocolos: 85-155px (altura 70)
    // Status: 160-195px (altura 35)
    
    // ═══ BOTÃO VOLTAR (PROPORCIONAL) ═══
    lv_obj_t * btn_voltar = lv_btn_create(content_container);
    lv_obj_set_size(btn_voltar, 120, 35);  // PROPORCIONAL: 120x35 (adequado e clicável)
    lv_obj_set_pos(btn_voltar, 5, 5);      // Margem 5px
    lv_obj_set_style_bg_color(btn_voltar, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(btn_voltar, 6, 0);
    lv_obj_add_flag(btn_voltar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_voltar, btn_voltar_home_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_voltar = lv_label_create(btn_voltar);
    lv_label_set_text(l_voltar, LV_SYMBOL_LEFT " VOLTAR");
    lv_obj_set_style_text_font(l_voltar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_voltar);
    Serial.println("  ✓ Botão VOLTAR: 5,5 (120x35)");
    
    // ═══ CABEÇALHO ═══
    lv_obj_t * header = lv_obj_create(content_container);
    lv_obj_set_size(header, 470, 35);
    lv_obj_set_pos(header, 5, 45);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 5, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    Serial.println("  ✓ Header: 5,45 (470x35)");
    
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "RFID CSH335 NXP");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x0891B2), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    
    lv_obj_t * count = lv_label_create(header);
    lv_label_set_text(count, "0 cartoes");
    lv_obj_set_style_text_font(count, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(count, lv_color_hex(0x9CA3AF), 0);
    lv_obj_align(count, LV_ALIGN_RIGHT_MID, -5, 0);
    
    // ═══ PROTOCOLOS ═══
    lv_obj_t * protocols = lv_obj_create(content_container);
    lv_obj_set_size(protocols, 470, 70);
    lv_obj_set_pos(protocols, 5, 85);
    Serial.println("  ✓ Protocolos: 5,85 (470x70)");
    lv_obj_set_style_bg_color(protocols, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(protocols, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(protocols, 1, 0);
    lv_obj_set_style_radius(protocols, 4, 0);
    lv_obj_set_style_pad_all(protocols, 5, 0);
    lv_obj_clear_flag(protocols, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * proto_title = lv_label_create(protocols);
    lv_label_set_text(proto_title, "PROTOCOLOS SUPORTADOS");
    lv_obj_set_style_text_font(proto_title, &lv_font_montserrat_8, 0);
    lv_obj_set_style_text_color(proto_title, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_pos(proto_title, 5, 5);
    
    lv_obj_t * proto_list = lv_label_create(protocols);
    lv_label_set_text(proto_list, 
        "ISO14443A (Mifare)\n"
        "ISO14443B (B-Type)\n"
        "NFC (NTAG)");
    lv_obj_set_style_text_font(proto_list, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(proto_list, lv_color_hex(0x0891B2), 0);
    lv_obj_set_pos(proto_list, 5, 20);
    
    // ═══ STATUS ═══
    lv_obj_t * status_box = lv_obj_create(content_container);
    lv_obj_set_size(status_box, 470, 35);
    lv_obj_set_pos(status_box, 5, 160);
    lv_obj_set_style_bg_color(status_box, lv_color_hex(0x064e3b), 0);
    lv_obj_set_style_border_color(status_box, lv_color_hex(0x059669), 0);
    lv_obj_set_style_border_width(status_box, 1, 0);
    lv_obj_set_style_radius(status_box, 4, 0);
    lv_obj_set_style_pad_all(status_box, 5, 0);
    lv_obj_clear_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
    Serial.println("  ✓ Status: 5,160 (470x35)");
    Serial.println("✅ RFID Layout: VOLTAR(5-40), HEADER(45-80), PROTO(85-155), STATUS(160-195)");
    
    lv_obj_t * status = lv_label_create(status_box);
    lv_label_set_text(status, "Leitor Conectado - SPI - 13.56 MHz");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(0x4CAF50), 0);
    lv_obj_center(status);
}

// ========================================
// TELA MAINTENANCE
// ========================================

void btn_maint_sub_nav_clicked(lv_event_t * e) {
    int sub = (int)(intptr_t)lv_event_get_user_data(e);
    maintenanceSubScreen = (MaintenanceSubScreen)sub;
    mudar_tela(SCREEN_MAINTENANCE);
}

void criar_conteudo_maintenance() {
    Serial.println("🔧 Criando TELA MANUTENÇÃO - FORMULÁRIO COMPLETO");
    
    // Inicializa requisição vazia
    inicializarRequisicao(&currentRequest);
    
    // ═══ BOTÃO VOLTAR ═══
    lv_obj_t * btn_voltar = lv_btn_create(content_container);
    lv_obj_set_size(btn_voltar, 120, 35);
    lv_obj_set_pos(btn_voltar, 5, 5);
    lv_obj_set_style_bg_color(btn_voltar, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(btn_voltar, 6, 0);
    lv_obj_add_event_cb(btn_voltar, btn_voltar_home_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_voltar = lv_label_create(btn_voltar);
    lv_label_set_text(l_voltar, "< VOLTAR");
    lv_obj_set_style_text_font(l_voltar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_voltar);
    
    // ═══ TÍTULO ═══
    lv_obj_t * titulo = lv_label_create(content_container);
    lv_label_set_text(titulo, "NOVA REQUISICAO");
    lv_obj_set_pos(titulo, 240, 15);
    lv_obj_set_style_text_font(titulo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(titulo, lv_color_hex(0xFBBF24), 0);
    
    // ═══ ÁREA DE CONTEÚDO (SCROLLABLE) ═══
    lv_obj_t * content = lv_obj_create(content_container);
    lv_obj_set_size(content, 470, 185);
    lv_obj_set_pos(content, 5, 50);
    lv_obj_set_style_bg_color(content, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(content, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(content, 1, 0);
    lv_obj_set_style_radius(content, 4, 0);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);  // ⭐ Scroll vertical
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);
    
    int y = 0;
    
    // ═══ CAMPO 1: PROBLEMA (OBRIGATÓRIO) ═══
    lv_obj_t * label_problema = lv_label_create(content);
    lv_label_set_text(label_problema, "Problema/Defeito: *");
    lv_obj_set_pos(label_problema, 0, y);
    lv_obj_set_style_text_font(label_problema, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(label_problema, lv_color_hex(0x9CA3AF), 0);
    y += 18;
    
    manut_textarea_problema = lv_textarea_create(content);
    lv_obj_set_size(manut_textarea_problema, 440, 55);
    lv_obj_set_pos(manut_textarea_problema, 0, y);
    lv_obj_set_style_bg_color(manut_textarea_problema, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(manut_textarea_problema, lv_color_hex(0x3a3a5e), 0);
    lv_obj_set_style_border_width(manut_textarea_problema, 1, 0);
    lv_obj_set_style_radius(manut_textarea_problema, 4, 0);
    lv_obj_set_style_text_color(manut_textarea_problema, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(manut_textarea_problema, &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_all(manut_textarea_problema, 6, 0);
    lv_textarea_set_placeholder_text(manut_textarea_problema, "Descreva o problema...");
    lv_textarea_set_max_length(manut_textarea_problema, 200);
    lv_textarea_set_one_line(manut_textarea_problema, false);
    lv_obj_add_event_cb(manut_textarea_problema, evento_foco_campo_manut, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(manut_textarea_problema, evento_defocus_campo_manut, LV_EVENT_DEFOCUSED, NULL);
    y += 65;
    
    // ═══ CAMPO 2: LOCAL (OBRIGATÓRIO) ═══
    lv_obj_t * label_local = lv_label_create(content);
    lv_label_set_text(label_local, "Local: *");
    lv_obj_set_pos(label_local, 0, y);
    lv_obj_set_style_text_font(label_local, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(label_local, lv_color_hex(0x9CA3AF), 0);
    y += 18;
    
    manut_dropdown_local = lv_dropdown_create(content);
    lv_obj_set_size(manut_dropdown_local, 440, 35);
    lv_obj_set_pos(manut_dropdown_local, 0, y);
    lv_dropdown_set_options(manut_dropdown_local,
        "Selecione...\n"
        "Sala - Eletronica Digital\n"
        "Sala - Eletronica Analogica\n"
        "Sala - Pneumatica\n"
        "Sala - Eletrica\n"
        "Outro");
    lv_obj_set_style_bg_color(manut_dropdown_local, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_text_color(manut_dropdown_local, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(manut_dropdown_local, &lv_font_montserrat_10, 0);
    y += 45;
    
    // ═══ CAMPO 3: PRIORIDADE (OBRIGATÓRIO) ═══
    lv_obj_t * label_prior = lv_label_create(content);
    lv_label_set_text(label_prior, "Prioridade: *");
    lv_obj_set_pos(label_prior, 0, y);
    lv_obj_set_style_text_font(label_prior, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(label_prior, lv_color_hex(0x9CA3AF), 0);
    y += 18;
    
    manut_dropdown_prioridade = lv_dropdown_create(content);
    lv_obj_set_size(manut_dropdown_prioridade, 440, 35);
    lv_obj_set_pos(manut_dropdown_prioridade, 0, y);
    lv_dropdown_set_options(manut_dropdown_prioridade,
        "Selecione...\n"
        "Baixa - Pode aguardar\n"
        "Media - Resolver em breve\n"
        "Alta - Urgente\n"
        "Critica - Emergencia");
    lv_obj_set_style_bg_color(manut_dropdown_prioridade, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_text_color(manut_dropdown_prioridade, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(manut_dropdown_prioridade, &lv_font_montserrat_10, 0);
    y += 45;
    
    // ═══ CAMPO 4: CONTATO (OPCIONAL) ═══
    lv_obj_t * label_contato = lv_label_create(content);
    lv_label_set_text(label_contato, "Contato (opcional):");
    lv_obj_set_pos(label_contato, 0, y);
    lv_obj_set_style_text_font(label_contato, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(label_contato, lv_color_hex(0x9CA3AF), 0);
    y += 18;
    
    manut_textarea_contato = lv_textarea_create(content);
    lv_obj_set_size(manut_textarea_contato, 440, 35);
    lv_obj_set_pos(manut_textarea_contato, 0, y);
    lv_obj_set_style_bg_color(manut_textarea_contato, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(manut_textarea_contato, lv_color_hex(0x3a3a5e), 0);
    lv_obj_set_style_border_width(manut_textarea_contato, 1, 0);
    lv_obj_set_style_radius(manut_textarea_contato, 4, 0);
    lv_obj_set_style_text_color(manut_textarea_contato, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(manut_textarea_contato, &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_all(manut_textarea_contato, 6, 0);
    lv_textarea_set_placeholder_text(manut_textarea_contato, "Nome ou ramal...");
    lv_textarea_set_max_length(manut_textarea_contato, 50);
    lv_textarea_set_one_line(manut_textarea_contato, true);
    lv_obj_add_event_cb(manut_textarea_contato, evento_foco_campo_manut, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(manut_textarea_contato, evento_defocus_campo_manut, LV_EVENT_DEFOCUSED, NULL);
    
    // ═══ FOOTER - BOTÕES ═══
    lv_obj_t * btn_cancelar = lv_btn_create(content_container);
    lv_obj_set_size(btn_cancelar, 220, 35);
    lv_obj_set_pos(btn_cancelar, 10, 240);
    lv_obj_set_style_bg_color(btn_cancelar, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_radius(btn_cancelar, 4, 0);
    lv_obj_add_event_cb(btn_cancelar, evento_cancelar_requisicao, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_cancelar = lv_label_create(btn_cancelar);
    lv_label_set_text(l_cancelar, "CANCELAR");
    lv_obj_set_style_text_font(l_cancelar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_cancelar);
    
    lv_obj_t * btn_enviar = lv_btn_create(content_container);
    lv_obj_set_size(btn_enviar, 220, 35);
    lv_obj_set_pos(btn_enviar, 240, 240);
    lv_obj_set_style_bg_color(btn_enviar, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_radius(btn_enviar, 4, 0);
    lv_obj_add_event_cb(btn_enviar, evento_enviar_requisicao, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_enviar = lv_label_create(btn_enviar);
    lv_label_set_text(l_enviar, "ENVIAR");
    lv_obj_set_style_text_font(l_enviar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_enviar);
    
    // ═══ LABEL DE STATUS ═══
    manut_label_status = lv_label_create(content_container);
    lv_obj_set_size(manut_label_status, 470, 20);
    lv_obj_set_pos(manut_label_status, 5, 280);
    lv_obj_set_style_text_align(manut_label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(manut_label_status, &lv_font_montserrat_10, 0);
    lv_obj_add_flag(manut_label_status, LV_OBJ_FLAG_HIDDEN);
    
    // ═══ TECLADO VIRTUAL ═══
    if (manut_keyboard == NULL) {
        manut_keyboard = lv_keyboard_create(lv_scr_act());
        lv_obj_set_size(manut_keyboard, 480, 140);
        lv_obj_add_flag(manut_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    
    Serial.println("✅ Formulário de manutenção criado");
}

// ========================================
// TELA CONTROLES/AJUDA
// ========================================

void criar_conteudo_controls() {
    Serial.println("❓ Criando TELA AJUDA/CONTROLES - Layout Organizado");
    // ═══ LAYOUT ORGANIZADO: 300px altura útil ═══
    // Botão VOLTAR: 5-40px (altura 35, PROPORCIONAL)
    // Header: 45-80px (altura 35)
    // Lista: 85-295px (altura 210)
    
    // ═══ BOTÃO VOLTAR (PROPORCIONAL) ═══
    lv_obj_t * btn_voltar = lv_btn_create(content_container);
    lv_obj_set_size(btn_voltar, 120, 35);  // PROPORCIONAL: 120x35 (adequado e clicável)
    lv_obj_set_pos(btn_voltar, 5, 5);      // Margem 5px
    lv_obj_set_style_bg_color(btn_voltar, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(btn_voltar, 6, 0);
    lv_obj_add_flag(btn_voltar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_voltar, btn_voltar_home_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_voltar = lv_label_create(btn_voltar);
    lv_label_set_text(l_voltar, LV_SYMBOL_LEFT " VOLTAR");
    lv_obj_set_style_text_font(l_voltar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_voltar);
    Serial.println("  ✓ Botão VOLTAR: 5,5 (120x35)");
    
    // ═══ CABEÇALHO ═══
    lv_obj_t * header = lv_obj_create(content_container);
    lv_obj_set_size(header, 470, 35);
    lv_obj_set_pos(header, 5, 45);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x16213e), 0);
    lv_obj_set_style_border_color(header, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 5, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    Serial.println("  ✓ Header: 5,45 (470x35)");
    
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "AJUDA E INFORMACOES");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x22c55e), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    
    // ═══ CONTEÚDO DE AJUDA ═══
    lv_obj_t * help_section = lv_obj_create(content_container);
    lv_obj_set_size(help_section, 470, 210);
    lv_obj_set_pos(help_section, 5, 85);
    Serial.println("  ✓ Conteudo: 5,85 (470x210)");
    Serial.println("✅ AJUDA Layout: VOLTAR(5-40), HEADER(45-80), CONTEUDO(85-295)");
    lv_obj_set_style_bg_color(help_section, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(help_section, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(help_section, 1, 0);
    lv_obj_set_style_radius(help_section, 4, 0);
    lv_obj_set_style_pad_all(help_section, 12, 0);
    lv_obj_clear_flag(help_section, LV_OBJ_FLAG_SCROLLABLE);
    
    // Texto de ajuda
    lv_obj_t * help_text = lv_label_create(help_section);
    lv_label_set_text(help_text, 
        "CONTROLES DO SISTEMA\n\n"
        "ACESSO: Digite PIN de 6 digitos\n"
        "  Use teclado numerico\n"
        "  Confirme com botao verde\n\n"
        "BIO: Posicione dedo no sensor\n"
        "  Aguarde leitura completa\n\n"
        "RFID: Aproxime cartao do leitor\n"
        "  Aguarde bip de confirmacao\n\n"
        "MANUT: Solicite servicos\n"
        "  Nova requisicao ou historico\n\n"
        "CONFIG: Ajustes do sistema\n"
        "  Calibracao touch e Wi-Fi"
    );
    lv_obj_set_style_text_font(help_text, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(help_text, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_line_space(help_text, 2, 0);
    lv_label_set_long_mode(help_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(help_text, 440);
    lv_obj_set_pos(help_text, 0, 0);
}

// ========================================
// TELA SETTINGS COM SUB-ABAS
// ========================================

void criar_conteudo_settings() {
    Serial.println("⚙️ Criando TELA CONFIGURAÇÕES com Sub-abas");
    // ═══ LAYOUT ORGANIZADO: 300px altura útil ═══
    // Botão VOLTAR: 5-40px (altura 35)
    // Sub-navegação: 45-80px (altura 35)
    // Conteúdo: 85-295px (altura 210)
    
    // ═══ BOTÃO VOLTAR ═══
    lv_obj_t * btn_voltar = lv_btn_create(content_container);
    lv_obj_set_size(btn_voltar, 120, 35);
    lv_obj_set_pos(btn_voltar, 5, 5);
    Serial.println("  ✓ Botão VOLTAR: 5,5 (120x35)");
    lv_obj_set_style_bg_color(btn_voltar, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(btn_voltar, 6, 0);
    lv_obj_add_flag(btn_voltar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_voltar, btn_voltar_home_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_voltar = lv_label_create(btn_voltar);
    lv_label_set_text(l_voltar, LV_SYMBOL_LEFT " VOLTAR");
    lv_obj_set_style_text_font(l_voltar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_voltar);
    
    // ═══ SUB-NAVEGAÇÃO (ABAS) ═══
    lv_obj_t * subnav = lv_obj_create(content_container);
    lv_obj_set_size(subnav, 470, 35);
    lv_obj_set_pos(subnav, 5, 45);
    Serial.println("  ✓ Sub-nav: 5,45 (470x35)");
    lv_obj_set_style_bg_color(subnav, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_width(subnav, 0, 0);
    lv_obj_set_style_pad_all(subnav, 0, 0);
    lv_obj_set_flex_flow(subnav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(subnav, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(subnav, LV_OBJ_FLAG_SCROLLABLE);
    
    // ⭐ ATUALIZADO v6.0.54: 5 botões de 90px cada (total: 450px + gaps)
    
    // Botão CALIBRAÇÃO
    lv_obj_t * btn_calibracao = lv_btn_create(subnav);
    lv_obj_set_size(btn_calibracao, 90, 30);
    lv_obj_set_style_radius(btn_calibracao, 4, 0);
    lv_obj_set_style_bg_color(btn_calibracao, 
        settingsSubScreen == SETTINGS_CALIBRATION ? lv_color_hex(0x6B7280) : lv_color_hex(0x1a1a2e), 0);
    lv_obj_add_event_cb(btn_calibracao, [](lv_event_t * e) {
        settingsSubScreen = SETTINGS_CALIBRATION;
        mudar_tela(SCREEN_SETTINGS);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * label_calibracao = lv_label_create(btn_calibracao);
    lv_label_set_text(label_calibracao, "CALIB");
    lv_obj_set_style_text_font(label_calibracao, &lv_font_montserrat_10, 0);
    lv_obj_center(label_calibracao);
    
    // Botão WI-FI
    lv_obj_t * btn_wifi = lv_btn_create(subnav);
    lv_obj_set_size(btn_wifi, 90, 30);
    lv_obj_set_style_radius(btn_wifi, 4, 0);
    lv_obj_set_style_bg_color(btn_wifi, 
        settingsSubScreen == SETTINGS_WIFI ? lv_color_hex(0x10B981) : lv_color_hex(0x1a1a2e), 0);
    lv_obj_add_event_cb(btn_wifi, [](lv_event_t * e) {
        settingsSubScreen = SETTINGS_WIFI;
        mudar_tela(SCREEN_SETTINGS);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * label_wifi = lv_label_create(btn_wifi);
    lv_label_set_text(label_wifi, "WI-FI");
    lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_10, 0);
    lv_obj_center(label_wifi);
    
    // ⭐ NOVO: Botão RFID
    lv_obj_t * btn_rfid = lv_btn_create(subnav);
    lv_obj_set_size(btn_rfid, 90, 30);
    lv_obj_set_style_radius(btn_rfid, 4, 0);
    lv_obj_set_style_bg_color(btn_rfid, 
        settingsSubScreen == SETTINGS_RFID ? lv_color_hex(0x3B82F6) : lv_color_hex(0x1a1a2e), 0);
    lv_obj_add_event_cb(btn_rfid, [](lv_event_t * e) {
        settingsSubScreen = SETTINGS_RFID;
        mudar_tela(SCREEN_SETTINGS);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * label_rfid = lv_label_create(btn_rfid);
    lv_label_set_text(label_rfid, "RFID");
    lv_obj_set_style_text_font(label_rfid, &lv_font_montserrat_10, 0);
    lv_obj_center(label_rfid);
    
    // ⭐ NOVO: Botão BIOMETRIA
    lv_obj_t * btn_bio = lv_btn_create(subnav);
    lv_obj_set_size(btn_bio, 90, 30);
    lv_obj_set_style_radius(btn_bio, 4, 0);
    lv_obj_set_style_bg_color(btn_bio, 
        settingsSubScreen == SETTINGS_BIOMETRIC ? lv_color_hex(0xF59E0B) : lv_color_hex(0x1a1a2e), 0);
    lv_obj_add_event_cb(btn_bio, [](lv_event_t * e) {
        settingsSubScreen = SETTINGS_BIOMETRIC;
        mudar_tela(SCREEN_SETTINGS);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * label_bio = lv_label_create(btn_bio);
    lv_label_set_text(label_bio, "BIO");
    lv_obj_set_style_text_font(label_bio, &lv_font_montserrat_10, 0);
    lv_obj_center(label_bio);
    
    // ⭐ NOVO v6.0.54: Botão E-MAIL
    lv_obj_t * btn_email = lv_btn_create(subnav);
    lv_obj_set_size(btn_email, 90, 30);
    lv_obj_set_style_radius(btn_email, 4, 0);
    lv_obj_set_style_bg_color(btn_email, 
        settingsSubScreen == SETTINGS_EMAIL ? lv_color_hex(0x3B82F6) : lv_color_hex(0x1a1a2e), 0);
    lv_obj_add_event_cb(btn_email, [](lv_event_t * e) {
        settingsSubScreen = SETTINGS_EMAIL;
        mudar_tela(SCREEN_SETTINGS);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * label_email = lv_label_create(btn_email);
    lv_label_set_text(label_email, "EMAIL");
    lv_obj_set_style_text_font(label_email, &lv_font_montserrat_10, 0);
    lv_obj_center(label_email);
    
    Serial.println("  ✓ Content: 5,85 (470x210)");
    Serial.println("✅ CONFIG Layout: VOLTAR(5-40), NAV(45-80), CONTENT(85-295)");
    
    // Renderizar conteúdo da aba selecionada
    if (settingsSubScreen == SETTINGS_CALIBRATION) {
        criar_settings_calibration();
    } else if (settingsSubScreen == SETTINGS_WIFI) {
        criar_settings_wifi();
    } else if (settingsSubScreen == SETTINGS_RFID) {
        criar_settings_rfid();
    } else if (settingsSubScreen == SETTINGS_BIOMETRIC) {
        criar_settings_biometric();
    } else if (settingsSubScreen == SETTINGS_EMAIL) {
        criar_settings_email();
    }
}

// ========================================
// CONFIG - ABA CALIBRAÇÃO
// ========================================

void criar_settings_calibration() {
    Serial.println("📐 Criando aba CALIBRAÇÃO");
    
    // ═══ SEÇÃO TOUCH (0-115px) ═══
    lv_obj_t * touch_section = lv_obj_create(content_container);
    lv_obj_set_size(touch_section, 470, 115);
    lv_obj_set_pos(touch_section, 5, 85);
    lv_obj_set_style_bg_color(touch_section, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(touch_section, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(touch_section, 1, 0);
    lv_obj_set_style_radius(touch_section, 4, 0);
    lv_obj_set_style_pad_all(touch_section, 8, 0);
    lv_obj_clear_flag(touch_section, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * touch_title = lv_label_create(touch_section);
    lv_label_set_text(touch_title, "TOUCH XPT2046");
    lv_obj_set_style_text_font(touch_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(touch_title, lv_color_white(), 0);
    lv_obj_set_pos(touch_title, 10, 5);
    
    lv_obj_t * touch_info = lv_label_create(touch_section);
    lv_label_set_text(touch_info, 
        "Calibracao: OK\n"
        "Sensibilidade: 5/10\n"
        "Filtro 8191: Ativo\n"
        "Debounce: 150ms");
    lv_obj_set_style_text_font(touch_info, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(touch_info, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_pos(touch_info, 10, 25);
    
    // ═══ SEÇÕES INFERIORES (120-210px = 90px altura) ═══
    
    // Seção Sistema (esquerda)
    lv_obj_t * system_section = lv_obj_create(content_container);
    lv_obj_set_size(system_section, 225, 85);
    lv_obj_set_pos(system_section, 5, 205);
    lv_obj_set_style_bg_color(system_section, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(system_section, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(system_section, 1, 0);
    lv_obj_set_style_radius(system_section, 4, 0);
    lv_obj_set_style_pad_all(system_section, 8, 0);
    lv_obj_clear_flag(system_section, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * system_title = lv_label_create(system_section);
    lv_label_set_text(system_title, "ESP32-S3");
    lv_obj_set_style_text_font(system_title, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(system_title, lv_color_white(), 0);
    lv_obj_set_pos(system_title, 10, 5);
    
    lv_obj_t * system_info = lv_label_create(system_section);
    lv_label_set_text(system_info, 
        "ILI9488 480x320\n"
        "LVGL 8.3.11\n"
        "8MB Flash + 8MB PSRAM");
    lv_obj_set_style_text_font(system_info, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(system_info, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_pos(system_info, 10, 25);
    
    // Botão CALIBRAR (direita)
    lv_obj_t * btn_calibrar = lv_btn_create(content_container);
    lv_obj_set_size(btn_calibrar, 235, 85);
    lv_obj_set_pos(btn_calibrar, 235, 205);
    lv_obj_set_style_bg_color(btn_calibrar, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_radius(btn_calibrar, 6, 0);
    lv_obj_add_event_cb(btn_calibrar, [](lv_event_t * e) {
        Serial.println("🎯 Iniciando calibração do touch...");
        Serial.println("💡 Use comandos Serial: P1-P9, SAVE, LOAD");
        mudar_tela(SCREEN_CALIBRATION);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * label_calibrar = lv_label_create(btn_calibrar);
    lv_label_set_text(label_calibrar, LV_SYMBOL_SETTINGS "\nCALIBRAR\nTOUCH");
    lv_obj_set_style_text_font(label_calibrar, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(label_calibrar, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label_calibrar);
}

// ════════════════════════════════════════════════════════════════
// CONFIG - ABA WI-FI (v6.0.55 - Scanner de Redes + Auto-reconexão)
// ════════════════════════════════════════════════════════════════

void criar_settings_wifi() {
    Serial.println("📡 Criando aba WI-FI com scanner de redes");
    
    // ═══ SEÇÃO WI-FI (210px altura) ═══
    lv_obj_t * wifi_section = lv_obj_create(content_container);
    lv_obj_set_size(wifi_section, 470, 210);
    lv_obj_set_pos(wifi_section, 5, 85);
    lv_obj_set_style_bg_color(wifi_section, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(wifi_section, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(wifi_section, 1, 0);
    lv_obj_set_style_radius(wifi_section, 4, 0);
    lv_obj_set_style_pad_all(wifi_section, 8, 0);
    
    // ═══ HEADER com status ═══
    bool is_connected = WiFi.status() == WL_CONNECTED;
    
    lv_obj_t * title = lv_label_create(wifi_section);
    if (is_connected) {
        char title_text[80];
        snprintf(title_text, sizeof(title_text), 
                 LV_SYMBOL_WIFI " CONECTADO: %s (%d dBm)", 
                 WiFi.SSID().c_str(),
                 WiFi.RSSI());
        lv_label_set_text(title, title_text);
        lv_obj_set_style_text_color(title, lv_color_hex(0x10b981), 0);
    } else {
        lv_label_set_text(title, LV_SYMBOL_WIFI " REDES DISPONIVEIS");
        lv_obj_set_style_text_color(title, lv_color_hex(0x3B82F6), 0);
    }
    lv_obj_set_style_text_font(title, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(title, 5, 5);
    
    // ═══ LISTA DE REDES (scrollable) ═══
    wifi_scan_list = lv_obj_create(wifi_section);
    lv_obj_set_size(wifi_scan_list, 454, 140);
    lv_obj_set_pos(wifi_scan_list, 5, 25);
    lv_obj_set_style_bg_color(wifi_scan_list, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(wifi_scan_list, 1, 0);
    lv_obj_set_style_border_color(wifi_scan_list, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_radius(wifi_scan_list, 4, 0);
    lv_obj_set_style_pad_all(wifi_scan_list, 4, 0);
    lv_obj_set_flex_flow(wifi_scan_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wifi_scan_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(wifi_scan_list, LV_SCROLLBAR_MODE_AUTO);
    
    // Scanner de redes WiFi
    Serial.println("[WIFI] Iniciando scan de redes...");
    int n = WiFi.scanNetworks();
    Serial.printf("[WIFI] %d redes encontradas\n", n);
    
    if (n == 0) {
        lv_obj_t * empty = lv_label_create(wifi_scan_list);
        lv_label_set_text(empty, "Nenhuma rede encontrada\nClique ATUALIZAR para tentar novamente");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x6b7280), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(empty);
    } else {
        // Listar redes encontradas (máximo 10)
        for (int i = 0; i < n && i < 10; i++) {
            String ssid = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            wifi_auth_mode_t encryption = WiFi.encryptionType(i);
            
            // Item da rede (clicável)
            lv_obj_t * net_item = lv_btn_create(wifi_scan_list);
            lv_obj_set_size(net_item, 440, 28);
            lv_obj_set_style_bg_color(net_item, lv_color_hex(0x0f172a), 0);
            lv_obj_set_style_radius(net_item, 3, 0);
            lv_obj_clear_flag(net_item, LV_OBJ_FLAG_SCROLLABLE);
            
            // Label com SSID
            lv_obj_t * name_label = lv_label_create(net_item);
            char name_buf[40];
            snprintf(name_buf, sizeof(name_buf), "%s", ssid.c_str());
            lv_label_set_text(name_label, name_buf);
            lv_obj_set_style_text_font(name_label, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(name_label, lv_color_white(), 0);
            lv_obj_set_pos(name_label, 4, 2);
            
            // Label com RSSI e segurança
            lv_obj_t * info_label = lv_label_create(net_item);
            char info_buf[30];
            const char* sec_icon = (encryption == WIFI_AUTH_OPEN) ? LV_SYMBOL_WARNING : LV_SYMBOL_CHARGE;
            snprintf(info_buf, sizeof(info_buf), "%s %d dBm", sec_icon, rssi);
            lv_label_set_text(info_label, info_buf);
            lv_obj_set_style_text_font(info_label, &lv_font_montserrat_10, 0);
            
            // Cor baseada no sinal
            uint32_t signal_color = 0x6b7280; // cinza
            if (rssi > -50) signal_color = 0x10b981; // verde (excelente)
            else if (rssi > -60) signal_color = 0x22c55e; // verde claro (bom)
            else if (rssi > -70) signal_color = 0xf59e0b; // laranja (regular)
            else signal_color = 0xef4444; // vermelho (fraco)
            
            lv_obj_set_style_text_color(info_label, lv_color_hex(signal_color), 0);
            lv_obj_set_pos(info_label, 320, 2);
            
            // Criar estrutura de dados para a rede
            NetworkData* data = new NetworkData{ssid, rssi, encryption};
            
            // Evento de clique na rede
            lv_obj_add_event_cb(net_item, [](lv_event_t * e) {
                NetworkData* data = (NetworkData*)lv_event_get_user_data(e);
                
                Serial.printf("[WIFI] Rede selecionada: '%s' (RSSI: %d dBm)\n", data->ssid.c_str(), data->rssi);
                
                selected_ssid = data->ssid;
                selected_rssi = data->rssi;
                
                // Se for rede aberta, conectar direto
                if (data->encryption == WIFI_AUTH_OPEN) {
                    Serial.println("[WIFI] Rede aberta, conectando sem senha...");
                    
                    WiFi.mode(WIFI_STA);
                    WiFi.begin(data->ssid.c_str());
                    
                    Serial.println("[WIFI] Conectando");
                    int attempts = 0;
                    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                        delay(500);
                        Serial.print(".");
                        attempts++;
                    }
                    Serial.println();
                    
                    if (WiFi.status() == WL_CONNECTED) {
                        Serial.println("[WIFI] ✅ CONECTADO!");
                        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
                        
                        // Salvar no NVS
                        Preferences prefs;
                        prefs.begin("wifi_config", false);
                        prefs.putString("ssid", data->ssid.c_str());
                        prefs.putString("password", "");
                        prefs.end();
                        
                        Serial.println("[WIFI] ✅ Credenciais salvas no NVS");
                        mudar_tela(SCREEN_SETTINGS);
                    } else {
                        Serial.println("[WIFI] ❌ FALHA NA CONEXÃO!");
                        mudar_tela(SCREEN_SETTINGS);
                    }
                } else {
                    // Rede protegida, abrir teclado para senha
                    open_virtual_keyboard(
                        "Senha WiFi:",
                        "",
                        [](const char* password) {
                            Serial.printf("[WIFI] Conectando a '%s' com senha\n", selected_ssid.c_str());
                            
                            WiFi.mode(WIFI_STA);
                            WiFi.begin(selected_ssid.c_str(), password);
                            
                            Serial.println("[WIFI] Conectando");
                            int attempts = 0;
                            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                                delay(500);
                                Serial.print(".");
                                attempts++;
                            }
                            Serial.println();
                            
                            if (WiFi.status() == WL_CONNECTED) {
                                Serial.println("[WIFI] ✅ CONECTADO!");
                                Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
                                Serial.printf("[WIFI] RSSI: %d dBm\n", WiFi.RSSI());
                                
                                // Salvar no NVS
                                Preferences prefs;
                                prefs.begin("wifi_config", false);
                                prefs.putString("ssid", selected_ssid.c_str());
                                prefs.putString("password", password);
                                prefs.end();
                                
                                Serial.println("[WIFI] ✅ Credenciais salvas no NVS");
                                mudar_tela(SCREEN_SETTINGS);
                            } else {
                                Serial.println("[WIFI] ❌ FALHA NA CONEXÃO!");
                                Serial.printf("[WIFI] Status code: %d\n", WiFi.status());
                                
                                // Mostrar erro específico
                                if (WiFi.status() == WL_CONNECT_FAILED) {
                                    Serial.println("[WIFI] ⚠️ Senha incorreta ou problema de autenticação");
                                }
                                
                                mudar_tela(SCREEN_SETTINGS);
                            }
                        }
                    );
                }
            }, LV_EVENT_CLICKED, data);
        }
    }
    
    // ═══ BOTÕES DE AÇÃO ═══
    lv_obj_t * btn_scan = lv_btn_create(wifi_section);
    lv_obj_set_size(btn_scan, 140, 28);
    lv_obj_set_pos(btn_scan, 5, 172);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_radius(btn_scan, 4, 0);
    
    lv_obj_add_event_cb(btn_scan, [](lv_event_t * e) {
        Serial.println("[WIFI] Atualizando lista de redes...");
        mudar_tela(SCREEN_SETTINGS); // Recria a tela com novo scan
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_scan_label = lv_label_create(btn_scan);
    lv_label_set_text(btn_scan_label, LV_SYMBOL_REFRESH " ATUALIZAR");
    lv_obj_set_style_text_font(btn_scan_label, &lv_font_montserrat_10, 0);
    lv_obj_center(btn_scan_label);
    
    // Botão DESCONECTAR (se conectado)
    if (is_connected) {
        lv_obj_t * btn_disconnect = lv_btn_create(wifi_section);
        lv_obj_set_size(btn_disconnect, 160, 28);
        lv_obj_set_pos(btn_disconnect, 294, 172);
        lv_obj_set_style_bg_color(btn_disconnect, lv_color_hex(0xef4444), 0);
        lv_obj_set_style_radius(btn_disconnect, 4, 0);
        
        lv_obj_add_event_cb(btn_disconnect, [](lv_event_t * e) {
            Serial.println("[WIFI] Desconectando...");
            WiFi.disconnect();
            delay(500);
            Serial.println("[WIFI] ✅ Desconectado");
            mudar_tela(SCREEN_SETTINGS);
        }, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t * btn_disc_label = lv_label_create(btn_disconnect);
        lv_label_set_text(btn_disc_label, LV_SYMBOL_CLOSE " DESCONECTAR");
        lv_obj_set_style_text_font(btn_disc_label, &lv_font_montserrat_10, 0);
        lv_obj_center(btn_disc_label);
    }
}

// ════════════════════════════════════════════════════════════════
// ⭐ v6.0.9: TECLADO VIRTUAL REMOVIDO! (212 linhas deletadas)
// ════════════════════════════════════════════════════════════════
// ANTES: Teclado custom de 400 linhas (40 botões manuais)
// AGORA: virtual_keyboard.h (lv_keyboard nativo - 150 linhas)
// 
// MELHORIAS:
// - 62% menos código (400 → 150 linhas)
// - 0 bugs (widget nativo testado)
// - Shift automático (maiúsculas/minúsculas)
// - Suporte a acentos
// - Cursor automático
// - Altura compacta (200px vs 280px)
// - Reutilizável (RFID, Biometria, Manutenção)
//
// USO:
// open_virtual_keyboard("Digite o nome:", "Nome...", [](const char* nome) {
//     Serial.printf("Nome: %s\n", nome);
// });
// ════════════════════════════════════════════════════════════════

// ════════════════════════════════════════════════════════════════
// CONFIG - ABA RFID (CADASTRO FUNCIONAL v5.2.0)
// ════════════════════════════════════════════════════════════════

void criar_settings_rfid() {
    bool hardware_ok = rfidManager.isHardwareConnected();
    Serial.printf("📇 Criando aba RFID (Hardware: %s)\n", hardware_ok ? "CONECTADO" : "NÃO CONECTADO");
    
    // ═══ SEÇÃO PRINCIPAL ═══
    lv_obj_t * rfid_section = lv_obj_create(content_container);
    lv_obj_set_size(rfid_section, 470, 210);
    lv_obj_set_pos(rfid_section, 5, 85);
    lv_obj_set_style_bg_color(rfid_section, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(rfid_section, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(rfid_section, 1, 0);
    lv_obj_set_style_radius(rfid_section, 4, 0);
    lv_obj_set_style_pad_all(rfid_section, 8, 0);
    lv_obj_clear_flag(rfid_section, LV_OBJ_FLAG_SCROLLABLE);
    
    // ═══ CABEÇALHO ═══
    lv_obj_t * header = lv_obj_create(rfid_section);
    lv_obj_set_size(header, 454, 30);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(hardware_ok ? 0x10b981 : 0xef4444), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * title = lv_label_create(header);
    char title_buf[64];
    int card_count = rfidManager.getCardCount();
    snprintf(title_buf, sizeof(title_buf), LV_SYMBOL_CHARGE " RFID PN532 (%d cartoes)", card_count);
    lv_label_set_text(title, title_buf);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 4, 0);
    
    if (!hardware_ok) {
        // Hardware não conectado - mostrar erro
        lv_obj_t * error = lv_label_create(rfid_section);
        lv_label_set_text(error, 
            "PN532 NAO DETECTADO!\n\n"
            "Verifique:\n"
            "- Conexao SPI (GPIO11/12/13/21)\n"
            "- DIP Switch: CH1=OFF, CH2=ON\n"
            "- Alimentacao 3.3V/5V");
        lv_obj_set_style_text_color(error, lv_color_hex(0xff9800), 0);
        lv_obj_set_style_text_font(error, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(error, 0, 36);
        lv_label_set_long_mode(error, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(error, 440);
        return;
    }
    
    // ═══ LISTA DE CARTÕES (SCROLLABLE) ═══
    rfid_list_container = lv_obj_create(rfid_section);
    lv_obj_set_size(rfid_list_container, 454, 120);
    lv_obj_set_pos(rfid_list_container, 0, 36);
    lv_obj_set_style_bg_color(rfid_list_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(rfid_list_container, 1, 0);
    lv_obj_set_style_border_color(rfid_list_container, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_radius(rfid_list_container, 4, 0);
    lv_obj_set_style_pad_all(rfid_list_container, 4, 0);
    lv_obj_set_flex_flow(rfid_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(rfid_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(rfid_list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Listar cartões cadastrados
    for (int i = 0; i < card_count && i < 10; i++) {
        RFIDCard* card = rfidManager.getCard(i);
        if (!card) continue;
        
        lv_obj_t * card_item = lv_obj_create(rfid_list_container);
        lv_obj_set_size(card_item, 440, 30);
        lv_obj_set_style_bg_color(card_item, lv_color_hex(0x0f172a), 0);
        lv_obj_set_style_border_width(card_item, 0, 0);
        lv_obj_set_style_radius(card_item, 3, 0);
        lv_obj_clear_flag(card_item, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t * name = lv_label_create(card_item);
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "%d. %s", i+1, card->name);
        lv_label_set_text(name, name_buf);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_set_pos(name, 4, 2);
        
        lv_obj_t * uid_label = lv_label_create(card_item);
        char uid_buf[32];
        snprintf(uid_buf, sizeof(uid_buf), "UID:%02X%02X%02X%02X", 
                 card->uid[0], card->uid[1], card->uid[2], card->uid[3]);
        lv_label_set_text(uid_label, uid_buf);
        lv_obj_set_style_text_font(uid_label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(uid_label, lv_color_hex(0x9ca3af), 0);
        lv_obj_set_pos(uid_label, 250, 2);
    }
    
    if (card_count == 0) {
        lv_obj_t * empty = lv_label_create(rfid_list_container);
        lv_label_set_text(empty, "Nenhum cartao cadastrado");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x6b7280), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_10, 0);
        lv_obj_center(empty);
    }
    
    // ═══ STATUS E BOTÃO CADASTRAR ═══
    rfid_status_label = lv_label_create(rfid_section);
    lv_label_set_text(rfid_status_label, "Pronto para cadastro");
    lv_obj_set_style_text_font(rfid_status_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(rfid_status_label, lv_color_hex(0x10b981), 0);
    lv_obj_set_pos(rfid_status_label, 0, 162);
    
    lv_obj_t * btn_cadastrar = lv_btn_create(rfid_section);
    lv_obj_set_size(btn_cadastrar, 160, 30);
    lv_obj_set_pos(btn_cadastrar, 294, 160);
    lv_obj_set_style_bg_color(btn_cadastrar, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_radius(btn_cadastrar, 4, 0);
    lv_obj_add_event_cb(btn_cadastrar, [](lv_event_t * e) {
        Serial.println("📇 [RFID] Iniciando fluxo de cadastro...");
        
        // ⭐ v6.0.18: Abrir teclado SEM recriar tela (manter labels válidos)
        open_virtual_keyboard(
            "Digite o nome do usuario:", 
            "Nome...", 
            [](const char* nome) {
                // Callback de texto confirmado
                if (strlen(nome) == 0) {
                    Serial.println("⚠️ Nome vazio, cadastro cancelado");
                    // ⭐ CORREÇÃO: Recriar tela SETTINGS após cancelamento
                    mudar_tela(SCREEN_SETTINGS);
                    return;
                }
                
                rfid_temp_name = nome;
                rfid_enrolling = true;
                
                Serial.printf("✅ Nome salvo: '%s'\n", nome);
                Serial.println("📇 Aguardando cartão RFID...");
                
                // ⭐ CORREÇÃO: Recriar tela SETTINGS com label válido
                mudar_tela(SCREEN_SETTINGS);
                
                // ⭐ IMPORTANTE: Agora rfid_status_label aponta para o label DA NOVA TELA!
                if (rfid_status_label) {
                    lv_label_set_text(rfid_status_label, "Aproxime o cartao/TAG...");
                    lv_obj_set_style_text_color(rfid_status_label, lv_color_hex(0xf59e0b), 0);
                }
            }
            // ⭐ REMOVIDO: on_close_callback (não precisa mais!)
        );
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_label = lv_label_create(btn_cadastrar);
    lv_label_set_text(btn_label, LV_SYMBOL_PLUS " CADASTRAR");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_10, 0);
    lv_obj_center(btn_label);
}

// ════════════════════════════════════════════════════════════════
// CONFIG - ABA BIOMETRIA (CADASTRO FUNCIONAL v5.2.0)
// ════════════════════════════════════════════════════════════════

void criar_settings_biometric() {
    bool hardware_ok = bioManager.isHardwareConnected();
    Serial.printf("👆 Criando aba BIOMETRIA (Hardware: %s)\n", hardware_ok ? "CONECTADO" : "NÃO CONECTADO");
    
    lv_obj_t * bio_section = lv_obj_create(content_container);
    lv_obj_set_size(bio_section, 470, 210);
    lv_obj_set_pos(bio_section, 5, 85);
    lv_obj_set_style_bg_color(bio_section, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(bio_section, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(bio_section, 1, 0);
    lv_obj_set_style_radius(bio_section, 4, 0);
    lv_obj_set_style_pad_all(bio_section, 8, 0);
    lv_obj_clear_flag(bio_section, LV_OBJ_FLAG_SCROLLABLE);
    
    // ═══ CABEÇALHO ═══
    lv_obj_t * header = lv_obj_create(bio_section);
    lv_obj_set_size(header, 454, 30);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(hardware_ok ? 0x10b981 : 0xef4444), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 4, 0);
    lv_obj_set_style_pad_all(header, 4, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * title = lv_label_create(header);
    char title_buf[64];
    int finger_count = bioManager.getCount();
    snprintf(title_buf, sizeof(title_buf), LV_SYMBOL_EYE_CLOSE " AS608 (%d digitais)", finger_count);
    lv_label_set_text(title, title_buf);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 4, 0);
    
    if (!hardware_ok) {
        // Hardware não conectado - mostrar erro
        lv_obj_t * error = lv_label_create(bio_section);
        lv_label_set_text(error, 
            "AS608 NAO DETECTADO!\n\n"
            "Verifique:\n"
            "- Conexao UART2 (GPIO1/2)\n"
            "- TX/RX cruzados (TX->RX, RX->TX)\n"
            "- Alimentacao 3.3V/5V\n"
            "- Baudrate: 57600");
        lv_obj_set_style_text_color(error, lv_color_hex(0xff9800), 0);
        lv_obj_set_style_text_font(error, &lv_font_montserrat_10, 0);
        lv_obj_set_pos(error, 0, 36);
        lv_label_set_long_mode(error, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(error, 440);
        return;
    }
    
    // ═══ LISTA DE DIGITAIS (SCROLLABLE) ═══
    bio_list_container = lv_obj_create(bio_section);
    lv_obj_set_size(bio_list_container, 454, 120);
    lv_obj_set_pos(bio_list_container, 0, 36);
    lv_obj_set_style_bg_color(bio_list_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(bio_list_container, 1, 0);
    lv_obj_set_style_border_color(bio_list_container, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_radius(bio_list_container, 4, 0);
    lv_obj_set_style_pad_all(bio_list_container, 4, 0);
    lv_obj_set_flex_flow(bio_list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bio_list_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(bio_list_container, LV_SCROLLBAR_MODE_AUTO);
    
    // Listar digitais cadastradas
    for (int i = 0; i < finger_count && i < 10; i++) {
        Fingerprint* finger = bioManager.getFingerprint(i);
        if (!finger) continue;
        
        lv_obj_t * finger_item = lv_obj_create(bio_list_container);
        lv_obj_set_size(finger_item, 440, 30);
        lv_obj_set_style_bg_color(finger_item, lv_color_hex(0x0f172a), 0);
        lv_obj_set_style_border_width(finger_item, 0, 0);
        lv_obj_set_style_radius(finger_item, 3, 0);
        lv_obj_clear_flag(finger_item, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t * name = lv_label_create(finger_item);
        char name_buf[64];
        snprintf(name_buf, sizeof(name_buf), "%d. %s", i+1, finger->name);
        lv_label_set_text(name, name_buf);
        lv_obj_set_style_text_font(name, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(name, lv_color_white(), 0);
        lv_obj_set_pos(name, 4, 2);
        
        lv_obj_t * id_label = lv_label_create(finger_item);
        char id_buf[32];
        snprintf(id_buf, sizeof(id_buf), "ID:%d", finger->id);
        lv_label_set_text(id_label, id_buf);
        lv_obj_set_style_text_font(id_label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(id_label, lv_color_hex(0x9ca3af), 0);
        lv_obj_set_pos(id_label, 350, 2);
    }
    
    if (finger_count == 0) {
        lv_obj_t * empty = lv_label_create(bio_list_container);
        lv_label_set_text(empty, "Nenhuma digital cadastrada");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x6b7280), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_10, 0);
        lv_obj_center(empty);
    }
    
    // ═══ STATUS E BOTÃO CADASTRAR ═══
    bio_status_label = lv_label_create(bio_section);
    lv_label_set_text(bio_status_label, "Pronto para cadastro");
    lv_obj_set_style_text_font(bio_status_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0x10b981), 0);
    lv_obj_set_pos(bio_status_label, 0, 162);
    
    lv_obj_t * btn_cadastrar = lv_btn_create(bio_section);
    lv_obj_set_size(btn_cadastrar, 160, 30);
    lv_obj_set_pos(btn_cadastrar, 294, 160);
    lv_obj_set_style_bg_color(btn_cadastrar, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_radius(btn_cadastrar, 4, 0);
    lv_obj_add_event_cb(btn_cadastrar, [](lv_event_t * e) {
        Serial.println("👆 [BIO] Iniciando fluxo de cadastro...");
        
        // ⭐ v6.0.18: Abrir teclado SEM recriar tela (manter labels válidos)
        open_virtual_keyboard(
            "Digite o nome do usuario:", 
            "Nome...", 
            [](const char* nome) {
                // Callback de texto confirmado
                if (strlen(nome) == 0) {
                    Serial.println("⚠️ Nome vazio, cadastro cancelado");
                    // ⭐ CORREÇÃO: Recriar tela SETTINGS após cancelamento
                    mudar_tela(SCREEN_SETTINGS);
                    return;
                }
                
                bio_temp_name = nome;
                bio_enrolling = true;
                
                Serial.printf("✅ Nome salvo: '%s'\n", nome);
                Serial.println("👆 Iniciando processo de cadastro...");
                
                // ⭐ INICIAR MÁQUINA DE ESTADOS DO BIOMETRIC MANAGER
                bioManager.startEnrollment();
                
                // ⭐ CORREÇÃO: Recriar tela SETTINGS com label válido
                mudar_tela(SCREEN_SETTINGS);
                
                // ⭐ IMPORTANTE: Agora bio_status_label aponta para o label DA NOVA TELA!
                if (bio_status_label) {
                    lv_label_set_text(bio_status_label, "Coloque o dedo (1/2)...");
                    lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xf59e0b), 0);
                }
            }
            // ⭐ REMOVIDO: on_close_callback (não precisa mais!)
        );
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_label = lv_label_create(btn_cadastrar);
    lv_label_set_text(btn_label, LV_SYMBOL_PLUS " CADASTRAR");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_10, 0);
    lv_obj_center(btn_label);
}

// ════════════════════════════════════════════════════════════════
// CONFIG - ABA E-MAIL (v6.0.54)
// ════════════════════════════════════════════════════════════════

void criar_settings_email() {
    Serial.println("📧 Criando aba E-MAIL");
    
    // ═══ SEÇÃO E-MAIL (210px altura) ═══
    lv_obj_t * email_section = lv_obj_create(content_container);
    lv_obj_set_size(email_section, 470, 210);
    lv_obj_set_pos(email_section, 5, 85);
    lv_obj_set_style_bg_color(email_section, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(email_section, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(email_section, 1, 0);
    lv_obj_set_style_radius(email_section, 4, 0);
    lv_obj_set_style_pad_all(email_section, 8, 0);
    
    // ═══ HEADER ═══
    lv_obj_t * title = lv_label_create(email_section);
    lv_label_set_text(title, LV_SYMBOL_ENVELOPE " CONFIGURACAO DE E-MAIL");
    lv_obj_set_style_text_color(title, lv_color_hex(0x3B82F6), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
    lv_obj_set_pos(title, 5, 5);
    
    // ═══ CAMPO 1: DESTINATÁRIO ═══
    lv_obj_t * label_dest = lv_label_create(email_section);
    lv_label_set_text(label_dest, "DESTINATARIO (Manutencao):");
    lv_obj_set_style_text_color(label_dest, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_font(label_dest, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(label_dest, 5, 30);
    
    lv_obj_t * ta_dest = lv_textarea_create(email_section);
    lv_obj_set_size(ta_dest, 450, 30);
    lv_obj_set_pos(ta_dest, 5, 48);
    lv_textarea_set_placeholder_text(ta_dest, "manutencao@empresa.com");
    lv_textarea_set_one_line(ta_dest, true);
    lv_textarea_set_max_length(ta_dest, 64);
    lv_obj_set_style_bg_color(ta_dest, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(ta_dest, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_text_font(ta_dest, &lv_font_montserrat_10, 0);
    
    // Carregar valor salvo do NVS
    Preferences prefs;
    prefs.begin("email_config", true);
    String saved_dest = prefs.getString("recipient", "");
    prefs.end();
    
    Serial.printf("[E-MAIL] Destinatário carregado: '%s'\n", saved_dest.c_str());
    
    if (saved_dest.length() > 0) {
        lv_textarea_set_text(ta_dest, saved_dest.c_str());
        Serial.println("[E-MAIL] ✅ Campo DESTINATÁRIO preenchido");
    } else {
        Serial.println("[E-MAIL] ⚠️ Campo DESTINATÁRIO vazio (usar placeholder)");
    }
    
    // ⭐ Evento de clique para abrir teclado
    lv_obj_add_event_cb(ta_dest, [](lv_event_t * e) {
        lv_obj_t * ta = lv_event_get_target(e);
        const char* current_text = lv_textarea_get_text(ta);
        
        open_virtual_keyboard(
            "E-mail Destinatario:", 
            current_text,
            [](const char* new_text) {
                // Salvar no NVS
                Serial.printf("[E-MAIL] Salvando DESTINATÁRIO: '%s'\n", new_text);
                
                Preferences prefs;
                prefs.begin("email_config", false);
                prefs.putString("recipient", new_text);
                prefs.end();
                
                Serial.println("[E-MAIL] ✅ DESTINATÁRIO salvo no NVS");
                
                // Voltar para a tela CONFIG (recria a tela com novo valor)
                mudar_tela(SCREEN_SETTINGS);
            }
        );
    }, LV_EVENT_CLICKED, NULL);
    
    // ═══ CAMPO 2: REMETENTE ═══
    lv_obj_t * label_smtp = lv_label_create(email_section);
    lv_label_set_text(label_smtp, "REMETENTE (SMTP Login):");
    lv_obj_set_style_text_color(label_smtp, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_font(label_smtp, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(label_smtp, 5, 85);
    
    lv_obj_t * ta_smtp = lv_textarea_create(email_section);
    lv_obj_set_size(ta_smtp, 450, 30);
    lv_obj_set_pos(ta_smtp, 5, 103);
    lv_textarea_set_placeholder_text(ta_smtp, "sistema@gmail.com");
    lv_textarea_set_one_line(ta_smtp, true);
    lv_textarea_set_max_length(ta_smtp, 64);
    lv_obj_set_style_bg_color(ta_smtp, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(ta_smtp, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_text_font(ta_smtp, &lv_font_montserrat_10, 0);
    
    // Carregar valor salvo do NVS
    prefs.begin("email_config", true);
    String saved_smtp = prefs.getString("smtp_email", "");
    prefs.end();
    
    Serial.printf("[E-MAIL] Remetente carregado: '%s'\n", saved_smtp.c_str());
    
    if (saved_smtp.length() > 0) {
        lv_textarea_set_text(ta_smtp, saved_smtp.c_str());
        Serial.println("[E-MAIL] ✅ Campo REMETENTE preenchido");
    } else {
        Serial.println("[E-MAIL] ⚠️ Campo REMETENTE vazio (usar placeholder)");
    }
    
    // ⭐ Evento de clique para abrir teclado
    lv_obj_add_event_cb(ta_smtp, [](lv_event_t * e) {
        lv_obj_t * ta = lv_event_get_target(e);
        const char* current_text = lv_textarea_get_text(ta);
        
        open_virtual_keyboard(
            "E-mail Remetente (SMTP):", 
            current_text,
            [](const char* new_text) {
                // Salvar no NVS
                Serial.printf("[E-MAIL] Salvando REMETENTE: '%s'\n", new_text);
                
                Preferences prefs;
                prefs.begin("email_config", false);
                prefs.putString("smtp_email", new_text);
                prefs.end();
                
                Serial.println("[E-MAIL] ✅ REMETENTE salvo no NVS");
                
                // Voltar para a tela CONFIG (recria a tela com novo valor)
                mudar_tela(SCREEN_SETTINGS);
            }
        );
    }, LV_EVENT_CLICKED, NULL);
    
    // ═══ CAMPO 3: SENHA ===
    lv_obj_t * label_pass = lv_label_create(email_section);
    lv_label_set_text(label_pass, "SENHA (App Password):");
    lv_obj_set_style_text_color(label_pass, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_font(label_pass, &lv_font_montserrat_10, 0);
    lv_obj_set_pos(label_pass, 5, 140);
    
    lv_obj_t * ta_pass = lv_textarea_create(email_section);
    lv_obj_set_size(ta_pass, 450, 30);
    lv_obj_set_pos(ta_pass, 5, 158);
    lv_textarea_set_placeholder_text(ta_pass, "****************");
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_max_length(ta_pass, 32);
    lv_textarea_set_password_mode(ta_pass, true);
    lv_obj_set_style_bg_color(ta_pass, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(ta_pass, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_text_font(ta_pass, &lv_font_montserrat_10, 0);
    
    // Carregar valor salvo do NVS
    prefs.begin("email_config", true);
    String saved_pass = prefs.getString("smtp_password", "");
    prefs.end();
    
    Serial.printf("[E-MAIL] Senha carregada: %s\n", saved_pass.length() > 0 ? "****** (oculta)" : "(vazio)");
    
    if (saved_pass.length() > 0) {
        lv_textarea_set_text(ta_pass, saved_pass.c_str());
        Serial.println("[E-MAIL] ✅ Campo SENHA preenchido");
    } else {
        Serial.println("[E-MAIL] ⚠️ Campo SENHA vazio (usar placeholder)");
    }
    
    // ⭐ Evento de clique para abrir teclado
    lv_obj_add_event_cb(ta_pass, [](lv_event_t * e) {
        lv_obj_t * ta = lv_event_get_target(e);
        const char* current_text = lv_textarea_get_text(ta);
        
        open_virtual_keyboard(
            "Senha App Password:", 
            current_text,
            [](const char* new_text) {
                // Salvar no NVS
                Serial.printf("[E-MAIL] Salvando SENHA: %s\n", strlen(new_text) > 0 ? "****** (oculta)" : "(vazio)");
                
                Preferences prefs;
                prefs.begin("email_config", false);
                prefs.putString("smtp_password", new_text);
                prefs.end();
                
                Serial.println("[E-MAIL] ✅ SENHA salva no NVS");
                
                // Voltar para a tela CONFIG (recria a tela com novo valor)
                mudar_tela(SCREEN_SETTINGS);
            }
        );
    }, LV_EVENT_CLICKED, NULL);
    
    // ═══ BOTÃO SALVAR ===
    lv_obj_t * btn_save = lv_btn_create(email_section);
    lv_obj_set_size(btn_save, 450, 24);
    lv_obj_set_pos(btn_save, 5, 192);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x10B981), 0);
    lv_obj_set_style_radius(btn_save, 4, 0);
    
    lv_obj_t * label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, LV_SYMBOL_SAVE " SALVAR CONFIGURACAO");
    lv_obj_set_style_text_font(label_save, &lv_font_montserrat_10, 0);
    lv_obj_center(label_save);
    
    // ⭐ Callback do botão SALVAR
    lv_obj_add_event_cb(btn_save, [](lv_event_t * e) {
        // ⚠️ ATENÇÃO: Não podemos usar referências locais aqui!
        // Precisamos buscar os text areas pela hierarquia ou guardar em variáveis globais
        Serial.println("📧 [EMAIL] Salvando configuração...");
        
        lv_obj_t * email_section = lv_obj_get_parent(lv_event_get_target(e));
        lv_obj_t * ta_dest = lv_obj_get_child(email_section, 2);
        lv_obj_t * ta_smtp = lv_obj_get_child(email_section, 4);
        lv_obj_t * ta_pass = lv_obj_get_child(email_section, 6);
        
        String recipient = lv_textarea_get_text(ta_dest);
        String smtp_email = lv_textarea_get_text(ta_smtp);
        String smtp_pass = lv_textarea_get_text(ta_pass);
        
        // Validação básica
        if (recipient.length() < 5 || smtp_email.length() < 5 || smtp_pass.length() < 8) {
            Serial.println("❌ Campos inválidos ou vazios!");
            Serial.printf("   Destinatário: %d chars\n", recipient.length());
            Serial.printf("   Remetente: %d chars\n", smtp_email.length());
            Serial.printf("   Senha: %d chars\n", smtp_pass.length());
            return;
        }
        
        // Salvar no NVS
        Preferences prefs;
        prefs.begin("email_config", false);
        prefs.putString("recipient", recipient);
        prefs.putString("smtp_email", smtp_email);
        prefs.putString("smtp_password", smtp_pass);
        prefs.putBool("configured", true);
        prefs.end();
        
        Serial.println("✅ Configuração de e-mail salva com sucesso!");
        Serial.printf("   📧 Destinatário: %s\n", recipient.c_str());
        Serial.printf("   📧 Remetente: %s\n", smtp_email.c_str());
        Serial.println("   🔒 Senha: ******** (oculta por segurança)");
        
    }, LV_EVENT_CLICKED, NULL);
    
    Serial.println("✅ Aba E-MAIL criada");
}

// ════════════════════════════════════════════════════════════════
// CADASTRO BIOMÉTRICO - MÁQUINA DE ESTADOS (v5.2.0)
// ════════════════════════════════════════════════════════════════

void processar_cadastro_biometrico() {
    if (!bio_enrolling) return;
    
    // ⭐ USA A MÁQUINA DE ESTADOS DO BIOMETRIC MANAGER
    bioManager.processEnrollment();
    
    // ═══ ATUALIZAR UI COM BASE NO ESTADO ═══
    BiometricEnrollState state = bioManager.enrollState;
    String stateStr = bioManager.getEnrollStateString();
    
    // Atualizar label de status
    if (bio_status_label) {
        switch (state) {
            case BIO_WAITING_FINGER_1:
                lv_label_set_text(bio_status_label, "Coloque o dedo (1/2)...");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0x3b82f6), 0);
                break;
                
            case BIO_READING_1:
                lv_label_set_text(bio_status_label, "Lendo digital...");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xf59e0b), 0);
                break;
                
            case BIO_REMOVE_FINGER:
                lv_label_set_text(bio_status_label, "Remova o dedo!");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0x10b981), 0);
                break;
                
            case BIO_WAITING_FINGER_2:
                lv_label_set_text(bio_status_label, "Coloque o dedo novamente (2/2)...");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0x3b82f6), 0);
                break;
                
            case BIO_READING_2:
                lv_label_set_text(bio_status_label, "Lendo digital novamente...");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xf59e0b), 0);
                break;
                
            case BIO_COMPARING:
            case BIO_CREATING_MODEL:
                lv_label_set_text(bio_status_label, "Criando modelo...");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xf59e0b), 0);
                break;
                
            case BIO_STORING:
                lv_label_set_text(bio_status_label, "Salvando...");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xf59e0b), 0);
                break;
                
            case BIO_AWAITING_NAME:
                // ⭐ Quando sensor salvou, adicionar metadados e marcar como sucesso
                lv_label_set_text(bio_status_label, "Salvando metadados...");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xf59e0b), 0);
                
                // Salvar metadados com o nome
                if (bio_temp_name.length() > 0) {
                    // ═══ Salvar no NVS (BiometricManager) ═══
                    bioManager.addFingerprint(bioManager.tempID, bio_temp_name.c_str());
                    Serial.printf("✅ Metadados salvos no NVS: ID=%d, Nome='%s'\n", 
                                  bioManager.tempID, bio_temp_name.c_str());
                    
                    // ═══ NOVO v6.0.22: Salvar no BiometricStorage também ═══
                    if (bioStorage.count() >= 0) {  // Se storage inicializou
                        BiometricUser user;
                        user.slotId = bioManager.tempID;
                        user.userId = String(bioManager.tempID);
                        user.userName = bio_temp_name;
                        user.registeredAt = millis();
                        user.confidence = 95;  // Confiança padrão inicial
                        user.accessCount = 0;
                        user.lastAccess = 0;
                        user.active = true;
                        
                        if (bioStorage.addUser(user)) {
                            Serial.printf("✅ Usuário adicionado ao BiometricStorage (ID=%d)\n", 
                                          bioManager.tempID);
                        } else {
                            Serial.println("⚠️  Erro ao adicionar no BiometricStorage (continuando...)");
                        }
                    }
                    
                    bioManager.enrollState = BIO_SUCCESS; // Marcar como concluído
                }
                break;
                
            case BIO_SUCCESS:
                lv_label_set_text(bio_status_label, "Cadastrado com sucesso!");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0x10b981), 0);
                
                // Resetar estado
                bio_enrolling = false;
                bio_temp_name = "";
                bioManager.enrollState = BIO_IDLE; // Resetar máquina de estados
                break;
                
            case BIO_ERROR_TIMEOUT:
                lv_label_set_text(bio_status_label, "Timeout - tente novamente");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xef4444), 0);
                bio_enrolling = false;
                break;
                
            case BIO_ERROR_NO_MATCH:
                lv_label_set_text(bio_status_label, "Digitais diferentes - reinicie");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xef4444), 0);
                bio_enrolling = false;
                break;
                
            case BIO_ERROR_DUPLICATE:
                lv_label_set_text(bio_status_label, "Digital já cadastrada!");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xef4444), 0);
                bio_enrolling = false;
                break;
                
            case BIO_ERROR_FULL:
                lv_label_set_text(bio_status_label, "Memória cheia (127 digitais)");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xef4444), 0);
                bio_enrolling = false;
                break;
                
            case BIO_ERROR_SENSOR:
            case BIO_ERROR_HARDWARE:
                lv_label_set_text(bio_status_label, "Erro no sensor AS608");
                lv_obj_set_style_text_color(bio_status_label, lv_color_hex(0xef4444), 0);
                bio_enrolling = false;
                break;
                
            default:
                break;
        }
    }
}

// ========================================
// ⭐ TELA CALIBRAÇÃO TOUCH
// ========================================

void criar_conteudo_calibration() {
    Serial.println("🎯 Criando TELA CALIBRAÇÃO TOUCH");
    
    // ═══ BOTÃO VOLTAR ═══
    lv_obj_t * btn_voltar = lv_btn_create(content_container);
    lv_obj_set_size(btn_voltar, 120, 35);
    lv_obj_set_pos(btn_voltar, 5, 5);
    lv_obj_set_style_bg_color(btn_voltar, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(btn_voltar, 6, 0);
    lv_obj_add_flag(btn_voltar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_voltar, btn_voltar_home_clicked, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_voltar = lv_label_create(btn_voltar);
    lv_label_set_text(l_voltar, LV_SYMBOL_LEFT " VOLTAR");
    lv_obj_set_style_text_font(l_voltar, &lv_font_montserrat_12, 0);
    lv_obj_center(l_voltar);
    
    // ═══ CABEÇALHO ═══
    lv_obj_t * header = lv_obj_create(content_container);
    lv_obj_set_size(header, 470, 40);
    lv_obj_set_pos(header, 5, 45);
    lv_obj_set_style_bg_color(header, lv_color_hex(COLOR_ORANGE), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 6, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " CALIBRACAO TOUCHSCREEN");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_center(title);
    
    // ═══ INSTRUÇÕES ═══
    lv_obj_t * instrucoes = lv_label_create(content_container);
    lv_label_set_text(instrucoes, 
        "TOQUE nos alvos vermelhos que aparecerao na tela.\n"
        "Sao 5 pontos: cantos e centro.\n\n"
        "Pressione INICIAR para comecar.");
    lv_obj_set_style_text_font(instrucoes, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(instrucoes, lv_color_white(), 0);
    lv_obj_set_style_text_align(instrucoes, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_size(instrucoes, 460, 80);
    lv_obj_set_pos(instrucoes, 10, 95);
    
    // ═══ BOTÃO INICIAR ═══
    lv_obj_t * btn_iniciar = lv_btn_create(content_container);
    lv_obj_set_size(btn_iniciar, 200, 50);
    lv_obj_set_pos(btn_iniciar, 140, 190);
    lv_obj_set_style_bg_color(btn_iniciar, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_style_radius(btn_iniciar, 8, 0);
    lv_obj_add_flag(btn_iniciar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_iniciar, [](lv_event_t * e) {
        Serial.println("🎯 [CALIBRAÇÃO] Iniciando calibração...");
        // Aqui virá a lógica de calibração
        lv_obj_t * label = lv_obj_get_child(lv_event_get_target(e), 0);
        lv_label_set_text(label, "EM DESENVOLVIMENTO");
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * l_iniciar = lv_label_create(btn_iniciar);
    lv_label_set_text(l_iniciar, LV_SYMBOL_PLAY " INICIAR");
    lv_obj_set_style_text_font(l_iniciar, &lv_font_montserrat_14, 0);
    lv_obj_center(l_iniciar);
    
    // ═══ STATUS ═══
    lv_obj_t * status = lv_label_create(content_container);
    lv_label_set_text(status, "Status: Aguardando inicio...");
    lv_obj_set_style_text_font(status, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_pos(status, 10, 260);
    
    Serial.println("✅ Tela CALIBRAÇÃO criada!");
}

// ════════════════════════════════════════════════════════════════
// 🔐 TELAS DE AUTENTICAÇÃO ADMIN (PIN 4 DÍGITOS)
// ════════════════════════════════════════════════════════════════

void criar_conteudo_admin_auth() {
    Serial.println("🔐 Criando TELA AUTENTICAÇÃO ADMIN");
    
    adminPinInput = "";
    adminAuthInProgress = false;
    
    if (adminAuth.isLocked()) {
        uint32_t remaining = adminAuth.getLockoutTimeRemaining();
        criar_admin_locked_screen(remaining);
        return;
    }
    
    // 📏 LAYOUT OTIMIZADO PARA 480x320px
    // Header: 30px, Central: 282px = 312px total (margem 8px)
    
    lv_obj_t * header = lv_obj_create(content_container);
    lv_obj_set_size(header, 470, 30);  // REDUZIDO: 40→30
    lv_obj_set_pos(header, 5, 4);      // AJUSTADO: 5→4
    lv_obj_set_style_bg_color(header, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 4, 0);  // REDUZIDO: 8→4
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "\xF0\x9F\x94\x92 ACESSO ADMIN"); // TEXTO ENCURTADO
    lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);  // REDUZIDO: 14→12
    lv_obj_set_style_text_color(title, lv_color_hex(0xFBBF24), 0);
    lv_obj_center(title);
    
    lv_obj_t * central_area = lv_obj_create(content_container);
    lv_obj_set_size(central_area, 470, 282);  // AJUSTADO: 245→282 (320-30-8=282)
    lv_obj_set_pos(central_area, 5, 38);      // AJUSTADO: 50→38 (4+30+4=38)
    lv_obj_set_style_bg_color(central_area, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(central_area, lv_color_hex(0x2a2a4e), 0);
    lv_obj_set_style_border_width(central_area, 1, 0);
    lv_obj_set_style_radius(central_area, 6, 0);
    lv_obj_set_style_pad_all(central_area, 8, 0);  // REDUZIDO: 12→8
    lv_obj_clear_flag(central_area, LV_OBJ_FLAG_SCROLLABLE);
    
    adminMessageLabel = lv_label_create(central_area);
    uint8_t remaining = adminAuth.getRemainingAttempts();
    char msg[64];
    snprintf(msg, sizeof(msg), "Digite PIN (%d tentativas)", remaining);  // TEXTO COMPACTO
    lv_label_set_text(adminMessageLabel, msg);
    lv_obj_set_style_text_font(adminMessageLabel, &lv_font_montserrat_10, 0);  // REDUZIDO: 12→10
    lv_obj_set_style_text_color(adminMessageLabel, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_align(adminMessageLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(adminMessageLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(adminMessageLabel, 450);  // AJUSTADO: 440→450
    lv_obj_set_pos(adminMessageLabel, 10, 5);  // REDUZIDO: 15,10 → 10,5
    
    adminPinDisplay = lv_label_create(central_area);
    lv_label_set_text(adminPinDisplay, "- - - -");
    lv_obj_set_style_text_font(adminPinDisplay, &lv_font_montserrat_16, 0);  // REDUZIDO: 20→16
    lv_obj_set_style_text_color(adminPinDisplay, lv_color_white(), 0);
    lv_obj_set_style_text_align(adminPinDisplay, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(adminPinDisplay, 450);  // AJUSTADO: 440→450
    lv_obj_set_pos(adminPinDisplay, 10, 30);  // REDUZIDO: 15,50 → 10,30
    
    criar_teclado_admin(central_area);
    
    Serial.println("✅ Tela de autenticação admin criada");
}

void criar_admin_locked_screen(uint32_t remaining_seconds) {
    Serial.printf("🔒 Criando TELA BLOQUEIO (%u segundos)\n", remaining_seconds);
    
    lv_obj_t * lock_screen = lv_obj_create(content_container);
    lv_obj_set_size(lock_screen, 470, 290);
    lv_obj_set_pos(lock_screen, 5, 5);
    lv_obj_set_style_bg_color(lock_screen, lv_color_hex(0x1a0a0a), 0);
    lv_obj_set_style_border_color(lock_screen, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_border_width(lock_screen, 2, 0);
    lv_obj_set_style_radius(lock_screen, 8, 0);
    lv_obj_clear_flag(lock_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t * icon = lv_label_create(lock_screen);
    lv_label_set_text(icon, "\xF0\x9F\x94\x92"); // 🔒 emoji UTF-8
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0xFF0000), 0);
    lv_obj_set_pos(icon, 200, 40);
    
    lv_obj_t * msg = lv_label_create(lock_screen);
    char text[128];
    uint32_t minutes = remaining_seconds / 60;
    uint32_t seconds = remaining_seconds % 60;
    snprintf(text, sizeof(text), 
             "ACESSO BLOQUEADO\n\n"
             "Muitas tentativas falhadas!\n\n"
             "Aguarde %02u:%02u para tentar novamente", 
             minutes, seconds);
    lv_label_set_text(msg, text);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(msg, lv_color_white(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg, 440);
    lv_obj_set_pos(msg, 15, 120);
    
    lv_obj_t * btn_back = lv_btn_create(lock_screen);
    lv_obj_set_size(btn_back, 150, 40);
    lv_obj_set_pos(btn_back, 160, 230);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x374151), 0);
    lv_obj_add_event_cb(btn_back, [](lv_event_t * e) {
        mudar_tela(SCREEN_HOME);
    }, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_label = lv_label_create(btn_back);
    lv_label_set_text(btn_label, "VOLTAR");
    lv_obj_center(btn_label);
}

void criar_teclado_admin(lv_obj_t * parent) {
    const char* keys[12] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "C", "0", "OK"};
    const int btn_w = 70;
    const int btn_h = 38;   // REDUZIDO: 40→38
    const int spacing = 8;   // REDUZIDO: 10→8
    const int start_x = 85;
    const int start_y = 60;  // REDUZIDO: 100→60 (após PIN display em Y=30+20=50)
    
    for (int i = 0; i < 12; i++) {
        int row = i / 3;
        int col = i % 3;
        
        lv_obj_t * btn = lv_btn_create(parent);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_set_pos(btn, start_x + (col * (btn_w + spacing)), 
                            start_y + (row * (btn_h + spacing)));
        
        if (strcmp(keys[i], "OK") == 0) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x10B981), 0);
        } else if (strcmp(keys[i], "C") == 0) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xEF4444), 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a2e), 0);
        }
        
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        
        char* key_copy = (char*)malloc(strlen(keys[i]) + 1);
        strcpy(key_copy, keys[i]);
        lv_obj_add_event_cb(btn, admin_keypad_clicked, LV_EVENT_CLICKED, key_copy);
        
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, keys[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_center(label);
    }
}

void admin_keypad_clicked(lv_event_t * e) {
    if (adminAuthInProgress) return;
    
    const char* key = (const char*)lv_event_get_user_data(e);
    Serial.printf("[AdminAuth] Tecla pressionada: %s\n", key);
    
    if (strcmp(key, "OK") == 0) {
        if (adminPinInput.length() == ADMIN_PIN_LENGTH) {
            admin_validate_pin();
        } else {
            lv_label_set_text(adminMessageLabel, "PIN incompleto!\nDigite 4 digitos");
            lv_obj_set_style_text_color(adminMessageLabel, lv_color_hex(0xEF4444), 0);
        }
    } else if (strcmp(key, "C") == 0) {
        if (adminPinInput.length() > 0) {
            adminPinInput = "";
            atualizar_admin_pin_display();
            lv_label_set_text(adminMessageLabel, "PIN limpo");
        } else {
            mudar_tela(SCREEN_HOME);
        }
    } else {
        if (adminPinInput.length() < ADMIN_PIN_LENGTH) {
            adminPinInput += key;
            atualizar_admin_pin_display();
        }
    }
}

void atualizar_admin_pin_display() {
    String display = "";
    int len = adminPinInput.length();
    
    for (int i = 0; i < ADMIN_PIN_LENGTH; i++) {
        if (i < len) {
            display += "*";
        } else {
            display += "-";
        }
        if (i < ADMIN_PIN_LENGTH - 1) display += " ";
    }
    
    lv_label_set_text(adminPinDisplay, display.c_str());
}

void admin_validate_pin() {
    adminAuthInProgress = true;
    
    Serial.printf("[AdminAuth] Validando PIN: %s\n", adminPinInput.c_str());
    lv_label_set_text(adminMessageLabel, "Validando...");
    lv_obj_set_style_text_color(adminMessageLabel, lv_color_hex(0xFBBF24), 0);
    
    lv_timer_handler();
    delay(300);
    
    bool valid = adminAuth.validate(adminPinInput.c_str());
    
    if (valid) {
        lv_label_set_text(adminMessageLabel, "ACESSO CONCEDIDO!");
        lv_obj_set_style_text_color(adminMessageLabel, lv_color_hex(0x10B981), 0);
        lv_timer_handler();
        delay(500);
        
        mudar_tela(SCREEN_SETTINGS);
    } else {
        uint8_t remaining = adminAuth.getRemainingAttempts();
        
        if (remaining > 0) {
            char msg[64];
            snprintf(msg, sizeof(msg), "PIN INCORRETO!\n%d tentativas restantes", remaining);
            lv_label_set_text(adminMessageLabel, msg);
            lv_obj_set_style_text_color(adminMessageLabel, lv_color_hex(0xEF4444), 0);
            
            adminPinInput = "";
            atualizar_admin_pin_display();
        } else {
            lv_label_set_text(adminMessageLabel, "ACESSO BLOQUEADO!\nMuitas tentativas falhadas");
            lv_obj_set_style_text_color(adminMessageLabel, lv_color_hex(0xFF0000), 0);
            lv_timer_handler();
            delay(1500);
            
            mudar_tela(SCREEN_ADMIN_AUTH);
        }
    }
    
    adminAuthInProgress = false;
}