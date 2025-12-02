/**
 * @file display_driver.h
 * @brief Driver LovyanGFX para LVGL - ESP32-S3 + ILI9488
 *
 * Integra LovyanGFX com LVGL para interface gráfica
 */

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <lvgl.h>

#include <LovyanGFX.hpp>

// Configuração de buffer LVGL
#define LVGL_BUFFER_SIZE (LV_HOR_RES_MAX * 40)  // 40 linhas de buffer

// Classe LovyanGFX customizada para ILI9488
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9488 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;

   public:
    LGFX(void) {
        // ==========================================
        // Configuração SPI Bus
        // ==========================================
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 40000000;  // 40MHz write
            cfg.freq_read = 16000000;   // 16MHz read
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            // Pinagem SPI
            cfg.pin_sclk = 14;  // TFT_CLK
            cfg.pin_mosi = 11;  // TFT_SDO
            cfg.pin_miso = 37;  // TFT_SDI (CORRIGIDO!)
            cfg.pin_dc = 8;     // TFT_DC

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        // ==========================================
        // Configuração Painel ILI9488
        // ==========================================
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 10;  // TFT_CS
            cfg.pin_rst = 9;  // TFT_RST
            cfg.pin_busy = -1;

            // Resolução
            cfg.memory_width = 320;
            cfg.memory_height = 480;
            cfg.panel_width = 320;
            cfg.panel_height = 480;

            // Offset
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;

            // Configurações de leitura
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = true;

            // Configurações de cor
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;

            _panel_instance.config(cfg);
        }

        // ==========================================
        // Configuração Backlight PWM
        // ==========================================
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = 5;       // GPIO5
            cfg.invert = false;   // false = HIGH liga
            cfg.freq = 44100;     // PWM 44.1kHz
            cfg.pwm_channel = 1;  // Canal PWM

            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }

        setPanel(&_panel_instance);
    }
};

// Classe Display Driver
class DisplayDriver {
   private:
    LGFX* lcd;
    lv_disp_draw_buf_t draw_buf;
    lv_color_t* buf1;
    lv_color_t* buf2;
    lv_disp_drv_t disp_drv;
    lv_disp_t* disp;

    // Callback flush para LVGL
    static void lvgl_flush_cb(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
        LGFX* lcd = (LGFX*)disp_drv->user_data;

        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);

        lcd->startWrite();
        lcd->setAddrWindow(area->x1, area->y1, w, h);
        lcd->pushPixels((uint16_t*)&color_p->full, w * h, true);
        lcd->endWrite();

        lv_disp_flush_ready(disp_drv);
    }

   public:
    DisplayDriver() : lcd(nullptr), buf1(nullptr), buf2(nullptr), disp(nullptr) {}

    bool init() {
        Serial.println("[Display] Inicializando LovyanGFX...");

        // Criar instância LovyanGFX
        lcd = new LGFX();
        if (!lcd) {
            Serial.println("[Display] ERRO: Falha ao criar LGFX");
            return false;
        }

        // Inicializar display
        lcd->init();
        lcd->setRotation(1);      // Landscape (480x320)
        lcd->setBrightness(255);  // Backlight máximo
        lcd->fillScreen(TFT_BLACK);

        Serial.println("[Display] LovyanGFX inicializado!");

        // Inicializar LVGL
        Serial.println("[Display] Inicializando LVGL...");
        lv_init();

        // Alocar buffers
        buf1 = (lv_color_t*)heap_caps_malloc(LVGL_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
        buf2 = (lv_color_t*)heap_caps_malloc(LVGL_BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);

        if (!buf1 || !buf2) {
            Serial.println("[Display] ERRO: Falha ao alocar buffers LVGL");
            return false;
        }

        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LVGL_BUFFER_SIZE);

        // Configurar driver LVGL
        lv_disp_drv_init(&disp_drv);
        disp_drv.hor_res = 480;
        disp_drv.ver_res = 320;
        disp_drv.flush_cb = lvgl_flush_cb;
        disp_drv.draw_buf = &draw_buf;
        disp_drv.user_data = lcd;

        disp = lv_disp_drv_register(&disp_drv);

        if (!disp) {
            Serial.println("[Display] ERRO: Falha ao registrar driver LVGL");
            return false;
        }

        Serial.println("[Display] LVGL inicializado!");
        Serial.printf("[Display] Resolução: %dx%d\n", disp_drv.hor_res, disp_drv.ver_res);
        Serial.printf("[Display] Buffer: %d pixels\n", LVGL_BUFFER_SIZE);

        return true;
    }

    void setBrightness(uint8_t brightness) {
        if (lcd) {
            lcd->setBrightness(brightness);
        }
    }

    LGFX* getLCD() { return lcd; }

    lv_disp_t* getDisplay() { return disp; }
};

#endif  // DISPLAY_DRIVER_H
