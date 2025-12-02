/**
 * @file pins.h
 * @brief Mapeamento de pinos GPIO para Freenove ESP32-S3-WROOM-N8R8
 * @version 5.1.0
 * @date 2025-11-28
 * 
 * Hardware: Freenove ESP32-S3-WROOM-N8R8
 * - 8MB Flash (N8) + 8MB PSRAM (R8) - OPI Mode
 * - Display: ILI9488 3.5" 480x320 SPI
 * - Touch: XPT2046 SPI (compartilhado com display)
 * - Biometria: AS608 UART2 (GPIO1/2)
 * - NFC/RFID: PN532 SPI (barramento compartilhado)
 * 
 * ⚠️ IMPORTANTE: Pinagem baseada em TABELA OFICIAL Freenove ESP32-S3 N8R8
 * 
 * CHANGELOG v5.1.0 (2025-11-28):
 * ✅ PN532 NFC/RFID implementado (SPI)
 * ✅ Suporte completo a NFC/Mifare/FeliCa/NTAG
 * ✅ Display ILI9488: GPIO11(MOSI), GPIO12(SCLK), GPIO10(CS), GPIO17(DC), GPIO18(RST), GPIO5(BL)
 * ✅ Touch XPT2046: GPIO11(MOSI), GPIO13(MISO), GPIO12(SCLK), GPIO9(CS), GPIO4(IRQ)
 * ✅ CRÍTICO: GPIO13 conectado APENAS ao DOUT do XPT2046 (NÃO ao MISO do ILI9488)
 * ✅ Relé em GPIO19, Buzzer em GPIO20
 * ✅ Biometria AS608 em GPIO1/2 (UART2)
 * ✅ GPIO35, 36, 37 RESERVADOS para PSRAM (NÃO USAR)
 */

#ifndef PINS_H
#define PINS_H

#include <Arduino.h>

/* ============================================================================
 * CONFIGURAÇÕES GLOBAIS
 * ========================================================================== */
#define ESP32_S3_FREENOVE_N8R8      1       // Hardware identifier
#define FLASH_SIZE_MB               8       // 8MB Flash
#define PSRAM_SIZE_MB               8       // 8MB PSRAM OPI
#define HARDWARE_VERSION            "5.1.0" // Version

/* ============================================================================
 * DISPLAY TFT 3.5" ILI9488 (SPI)
 * Resolution: 480x320 pixels (landscape)
 * Interface: 4-line Serial SPI
 * Speed: Up to 40MHz
 * 
 * ⚠️ BASEADO EM: Tabela Oficial de Pinagem Display ILI9488 para ESP32-S3 N8R8
 * 
 * IMPORTANTE: O pino SDO(MISO) do ILI9488 NÃO deve ser conectado ao GPIO13
 * devido a conflito de hardware com o XPT2046. O ILI9488 funciona sem MISO.
 * ========================================================================== */
#ifndef TFT_MOSI
#define TFT_MOSI        11      // GPIO11 → SDA/MOSI (Pin 6) - Master Out Slave In
#endif
#ifndef TFT_MISO
#define TFT_MISO        -1      // NÃO CONECTADO (conflito de hardware com touch)
#endif
#ifndef TFT_SCLK
#define TFT_SCLK        12      // GPIO12 → SCL/DCLK (Pin 7) - Serial Clock
#endif
#ifndef TFT_CS
#define TFT_CS          10      // GPIO10 → CS (Pin 3) - Chip Select (Ativo Baixo)
#endif
#ifndef TFT_DC
#define TFT_DC          17      // GPIO17 → DC/RS (Pin 5) - Data/Command (HIGH=Data, LOW=Command)
#endif
#ifndef TFT_RST
#define TFT_RST         18      // GPIO18 → RESET (Pin 4) - Reset (Ativo Baixo)
#endif
#ifndef TFT_BL
#define TFT_BL          5       // GPIO5 → BL_EN (Pin 8) - Backlight Enable (HIGH=ON)
#endif

// Display Settings
#define TFT_WIDTH               320     // Physical width (landscape)
#define TFT_HEIGHT              480     // Physical height (landscape)
#define TFT_ROTATION            1       // 0=portrait, 1=landscape
#define TFT_BRIGHTNESS_DEFAULT  255     // 0-255 (PWM)
#define TFT_BRIGHTNESS_MIN      50      // Minimum brightness
#define TFT_BRIGHTNESS_MAX      255     // Maximum brightness

/* ============================================================================
 * TOUCH SCREEN XPT2046 (SPI)
 * Resolution: 4096x4096 (12-bit ADC)
 * Interface: SPI (barramento compartilhado com display)
 * 
 * ⚠️ BASEADO EM: Tabela Oficial de Pinagem Display ILI9488 para ESP32-S3 N8R8
 * 
 * CRÍTICO: GPIO13 é conectado EXCLUSIVAMENTE ao T_DOUT (Pin 13) do XPT2046.
 * NÃO conectar ao SDO(MISO) do ILI9488 devido a conflito de hardware!
 * ========================================================================== */
#define TOUCH_MOSI      11      // GPIO11 → DIN (Pin 12) - Compartilhado com TFT_MOSI
#define TOUCH_MISO      13      // GPIO13 → DOUT (Pin 13) - EXCLUSIVO para Touch (conflito de HW)
#define TOUCH_SCLK      12      // GPIO12 → DCLK (Pin 10) - Compartilhado com TFT_SCLK
#define TOUCH_CS        9       // GPIO9 → T_CS (Pin 11) - Touch Chip Select
#define TOUCH_IRQ       4       // GPIO4 → PENIRQ (Pin 14) - Touch Interrupt (Ativo Baixo)

// Touch Settings
#define TOUCH_ROTATION          1       // Match TFT rotation
#define TOUCH_SWAP_XY           true    // Swap X/Y coordinates
#define TOUCH_INVERT_X          false   // Invert X axis
#define TOUCH_INVERT_Y          false   // Invert Y axis
#define TOUCH_X_MIN             200     // Calibration X min
#define TOUCH_X_MAX             3800    // Calibration X max
#define TOUCH_Y_MIN             200     // Calibration Y min
#define TOUCH_Y_MAX             3800    // Calibration Y max
#define TOUCH_THRESHOLD         400     // Touch pressure threshold

/* ============================================================================
 * SENSOR BIOMÉTRICO AS608 (UART2) ✅ ATUALIZADO v5.1.0
 * Baud Rate: 57600 bps (default)
 * Protocol: Proprietary packet-based
 * 
 * ⚠️ PINAGEM ATUALIZADA: GPIO1/2 (UART2)
 * ========================================================================== */
#define BIO_RX_PIN      1       // GPIO1 → ESP32 RX ← AS608 TX
#define BIO_TX_PIN      2       // GPIO2 → ESP32 TX → AS608 RX
#define BIO_UART_NUM    2       // UART2 (0=USB, 1=conflito, 2=livre)
#define BIO_BAUDRATE    57600   // 9600, 19200, 38400, 57600, 115200
#define BIO_TIMEOUT     1000    // Communication timeout (ms)
#define BIO_PASSWORD    0x00000000  // Default password
#define BIO_SECURITY    3       // Security level (1-5, 3=medium)

// AS608 Settings
#define BIO_ADDR        0xFFFFFFFF  // Default address
#define BIO_MAX_FINGERS 256         // Maximum fingerprint capacity

/* ============================================================================
 * PN532 NFC/RFID - SPI Interface (v5.1.0)
 * 
 * Protocolo: Mifare Classic, Mifare Ultralight, NTAG, FeliCa
 * Frequency: 13.56 MHz (NFC/RFID)
 * SPI Speed: até 5 MHz
 * 
 * ⚠️ DIP SWITCH: Configure para modo SPI: CH1=ON, CH2=OFF
 * 
 * ⚠️ RST DESABILITADO: GPIO47 pode não estar conectado fisicamente
 *    Se precisar habilitar, use GPIO48 ou GPIO45 como alternativa
 * 
 * ⚠️ BARRAMENTO SPI COMPARTILHADO:
 *    - GPIO11 (MOSI): Display + Touch + PN532 ✅
 *    - GPIO12 (SCK):  Display + Touch + PN532 ✅
 *    - GPIO13 (MISO): Touch + PN532 APENAS ✅
 *      (Display ILI9488 NÃO usa MISO - funciona somente com escrita)
 * ========================================================================== */
#define RFID_SCK_PIN    12      // GPIO12 → SPI Clock (compartilhado com TFT/Touch)
#define RFID_MOSI_PIN   11      // GPIO11 → SPI MOSI (compartilhado com TFT/Touch)
#define RFID_MISO_PIN   13      // GPIO13 → SPI MISO (compartilhado APENAS com Touch, NÃO com Display)
#define RFID_SS_PIN     21      // GPIO21 → PN532 Chip Select (exclusivo)
#define RFID_RST_PIN    -1      // ⚠️ DESABILITADO (GPIO47 não conectado)
#define RFID_IRQ_PIN    -1      // Não usado (polling mode)

// PN532 Settings
#define PN532_SPI_SPEED     5000000  // 5 MHz SPI speed (máx recomendado)

// ⚠️ NOTA: PN532_SS_PIN, PN532_RST_PIN e PN532_TIMEOUT estão definidos em config.h
// Para evitar redefinições, não declare aqui. Use config.h como fonte única.

/* ============================================================================
 * CONTROLE DE ACESSO - Atuadores ✅ ATUALIZADO v5.1.0
 * 
 * ATUALIZADO v5.1.0:
 * - RELAY_PIN: GPIO20 → GPIO19
 * - BUZZER_PIN: GPIO19 → GPIO20
 * ========================================================================== */
#define RELAY_PIN       19      // GPIO19 → Relé/Trava (HIGH=destravar) - ATUALIZADO v5.1.0
#define BUZZER_PIN      20      // GPIO20 → Buzzer piezoelétrico (PWM) - ATUALIZADO v5.1.0
#define LED_GREEN       39      // GPIO39 → LED Verde (acesso permitido)
#define LED_RED         38      // GPIO38 → LED Vermelho (acesso negado)

// Relay Settings
#define RELAY_ACTIVE_HIGH   true    // true=HIGH abre, false=LOW abre
#define RELAY_PULSE_MS      2000    // Tempo de ativação (ms)

// Buzzer Settings
#define BUZZER_FREQUENCY    2000    // Frequência padrão (Hz)
#define BUZZER_DURATION     100     // Duração padrão (ms)
#define BUZZER_PWM_CHANNEL  0       // Canal PWM (0-15)
#define BUZZER_PWM_RESOLUTION 8     // Resolução PWM (bits)

/* ============================================================================
 * BOTÕES FÍSICOS (⚠️ NÃO USAR - RESERVADOS PARA PSRAM)
 * 
 * CRÍTICO: GPIO35, GPIO36, GPIO37 pertencem à interface PSRAM OPI.
 * NÃO conectar dispositivos externos a estes pinos!
 * ========================================================================== */
// Botões alternativos (se necessário no futuro)
#define BTN_1_PIN       6       // GPIO6 → Botão 1 (disponível)
#define BTN_2_PIN       7       // GPIO7 → Botão 2 (disponível)
#define BTN_3_PIN       8       // GPIO8 → Botão 3 (disponível)

// Button Settings
#define BTN_ACTIVE_LOW      true    // true=pressiona=LOW
#define BTN_DEBOUNCE_MS     50      // Debounce time (ms)

/* ============================================================================
 * COMUNICAÇÃO - WiFi, UART, I2C (Opcional)
 * ========================================================================== */
// UART0 - USB CDC (built-in)
#define USB_CDC_RX      44      // USB CDC RX (interno)
#define USB_CDC_TX      43      // USB CDC TX (interno)

// I2C - Para expansões (ex: RTC, sensors)
#define I2C_SDA         8       // GPIO8 → I2C Data
#define I2C_SCL         18      // GPIO18 → I2C Clock
#define I2C_FREQUENCY   100000  // 100kHz (standard mode)

/* ============================================================================
 * LEDS INDICADORES ADICIONAIS (Opcional)
 * ========================================================================== */
#define LED_BLUE        40      // GPIO40 → LED Azul (processando)

/* ============================================================================
 * OUTROS RECURSOS
 * ========================================================================== */
#ifndef LED_BUILTIN
#define LED_BUILTIN     2       // LED interno RGB do ESP32-S3
#endif

// RGB LED interno (WS2812 - opcional)
#define RGB_LED_PIN     48      // GPIO48 → RGB LED (se disponível)
#define RGB_LED_COUNT   1       // Quantidade de LEDs

/* ============================================================================
 * PINOS DISPONÍVEIS PARA EXPANSÃO
 * ========================================================================== */
/*
 * GPIOs Livres (testados e seguros para uso):
 * - GPIO 3, 6, 7, 8, 14, 15, 16, 33, 34, 41, 42
 * 
 * Total: 11 GPIOs disponíveis para expansão
 */

/* ============================================================================
 * PINOS RESERVADOS / EVITAR
 * ========================================================================== */
/*
 * ⚠️ NÃO USAR estes GPIOs (strapping pins ou conflitos):
 * 
 * Strapping Pins (afetam boot):
 * - GPIO 0  : Boot mode select (LOW=download mode)
 * - GPIO 45 : VDD_SPI voltage select
 * 
 * Flash/PSRAM (OPI Mode):
 * - GPIO 26-32 : PSRAM/Flash interface (NÃO USAR!)
 * - GPIO 35-37 : PSRAM OPI data lines (NÃO USAR!)
 * 
 * USB CDC (interno):
 * - GPIO 19 : USB D- (LIVRE - usado pelo RELAY)
 * - GPIO 20 : USB D+ (LIVRE - usado pelo BUZZER)
 */

/* ============================================================================
 * MAPEAMENTO COMPLETO - RESUMO
 * ========================================================================== */
/*
 * ┌────────────────────────────────────────────────────────────┐
 * │ DISPLAY ILI9488 (SPI)                                      │
 * ├──────────┬────────┬──────────────────────────────────────┤
 * │ MOSI     │ GPIO11 │ Compartilhado com Touch                │
 * │ MISO     │ -1     │ NÃO CONECTADO (conflito de hardware)   │
 * │ SCK      │ GPIO12 │ Compartilhado com Touch                │
 * │ CS       │ GPIO10 │ Exclusivo Display                    │
 * │ DC       │ GPIO17 │ Data/Command                         │
 * │ RST      │ GPIO18 │ Reset                                │
 * │ BL       │ GPIO5  │ Backlight PWM                        │
 * └──────────┴────────┴──────────────────────────────────────┘
 * 
 * ┌────────────────────────────────────────────────────────────┐
 * │ TOUCH XPT2046 (SPI)                                        │
 * ├──────────┬────────┬──────────────────────────────────────┤
 * │ MOSI     │ GPIO11 │ Compartilhado com Display              │
 * │ MISO     │ GPIO13 │ Exclusivo Touch                      │
 * │ SCK      │ GPIO12 │ Compartilhado com Display              │
 * │ CS       │ GPIO9  │ Exclusivo Touch                      │
 * │ IRQ      │ GPIO4  │ Interrupt                            │
 * └──────────┴────────┴──────────────────────────────────────┘
 * 
 * ┌────────────────────────────────────────────────────────────┐
 * │ BIOMÉTRICO AS608 (UART2) ✅ ATUALIZADO v5.1.0             │
 * ├──────────┬────────┬──────────────────────────────────────┤
 * │ RX       │ GPIO1  │ ESP32 RX ← AS608 TX                  │
 * │ TX       │ GPIO2  │ ESP32 TX → AS608 RX                  │
 * │ UART     │ UART2  │ Sem conflito                         │
 * └──────────┴────────┴──────────────────────────────────────┘
 * 
 * ┌────────────────────────────────────────────────────────────┐
 * │ NFC/RFID PN532 (SPI - Compartilhado)                       │
 * ├──────────┬────────┬──────────────────────────────────────┤
 * │ MOSI     │ GPIO11 │ Compartilhado com Display e Touch      │
 * │ MISO     │ GPIO13 │ Compartilhado com Touch                │
 * │ SCK      │ GPIO12 │ Compartilhado com Display e Touch      │
 * │ SS       │ GPIO21 │ Exclusivo RFID                       │
 * │ RST      │ -1     │ Desabilitado                         │
 * └──────────┴────────┴──────────────────────────────────────┘
 * 
 * ┌────────────────────────────────────────────────────────────┐
 * │ CONTROLE DE ACESSO ✅ ATUALIZADO v5.1.0                   │
 * ├──────────┬────────┬──────────────────────────────────────┤
 * │ Relé     │ GPIO19 │ Trava elétrica                       │
 * │ Buzzer   │ GPIO20 │ Buzzer PWM                           │
 * │ LED Red  │ GPIO38 │ Acesso negado                        │
 * │ LED Green│ GPIO39 │ Acesso permitido                     │
 * └──────────┴────────┴──────────────────────────────────────┘
 */

/* ============================================================================
 * MACROS AUXILIARES
 * ========================================================================== */
// Set pin mode with validation
#define SET_PIN_OUTPUT(pin) do { \
    if ((pin) >= 0 && (pin) <= 48) pinMode((pin), OUTPUT); \
} while(0)

#define SET_PIN_INPUT(pin) do { \
    if ((pin) >= 0 && (pin) <= 48) pinMode((pin), INPUT); \
} while(0)

#define SET_PIN_INPUT_PULLUP(pin) do { \
    if ((pin) >= 0 && (pin) <= 48) pinMode((pin), INPUT_PULLUP); \
} while(0)

// Read/Write with validation
#define DIGITAL_WRITE(pin, val) do { \
    if ((pin) >= 0 && (pin) <= 48) digitalWrite((pin), (val)); \
} while(0)

#define DIGITAL_READ(pin) (((pin) >= 0 && (pin) <= 48) ? digitalRead(pin) : LOW)

/* ============================================================================
 * INFORMAÇÕES DE VERSÃO
 * ========================================================================== */
#define PINS_VERSION_MAJOR  5
#define PINS_VERSION_MINOR  1
#define PINS_VERSION_PATCH  0
#define PINS_VERSION_STRING "5.1.0"
#define PINS_BUILD_DATE     "2025-11-28"
#define PINS_AUTHOR         "ESP32 Access Control System"

#endif // PINS_H
