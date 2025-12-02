/**
 * @file touch_driver.h
 * @brief Driver XPT2046 para LVGL - Touch Resistivo
 *
 * Integra XPT2046 com LVGL para entrada touch
 */

#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

// Pinagem Touch XPT2046
#define TOUCH_CS 13  // ⚠️ EXCLUSIVO TOUCH!
#define TOUCH_IRQ 21

// Calibração Touch (ajustar conforme necessário)
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 200
#define TOUCH_Y_MAX 3800

// Resolução do display (landscape)
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

class TouchDriver {
   private:
    XPT2046_Touchscreen* touch;
    lv_indev_drv_t indev_drv;
    lv_indev_t* indev;
    bool calibrated;

    // Dados de calibração
    int16_t cal_x_min, cal_x_max;
    int16_t cal_y_min, cal_y_max;

    // Callback de leitura para LVGL
    static void lvgl_read_cb(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
        TouchDriver* driver = (TouchDriver*)indev_drv->user_data;

        if (driver->touch->touched()) {
            TS_Point p = driver->touch->getPoint();

            // Mapear coordenadas
            int16_t x = map(p.x, driver->cal_x_min, driver->cal_x_max, 0, SCREEN_WIDTH);
            int16_t y = map(p.y, driver->cal_y_min, driver->cal_y_max, 0, SCREEN_HEIGHT);

            // Limitar aos bounds da tela
            x = constrain(x, 0, SCREEN_WIDTH - 1);
            y = constrain(y, 0, SCREEN_HEIGHT - 1);

            data->point.x = x;
            data->point.y = y;
            data->state = LV_INDEV_STATE_PR;
        } else {
            data->state = LV_INDEV_STATE_REL;
        }
    }

   public:
    TouchDriver() : touch(nullptr), indev(nullptr), calibrated(false) {
        // Valores padrão de calibração
        cal_x_min = TOUCH_X_MIN;
        cal_x_max = TOUCH_X_MAX;
        cal_y_min = TOUCH_Y_MIN;
        cal_y_max = TOUCH_Y_MAX;
    }

    bool init() {
        Serial.println("[Touch] Inicializando XPT2046...");

        // Criar instância XPT2046
        touch = new XPT2046_Touchscreen(TOUCH_CS, TOUCH_IRQ);
        if (!touch) {
            Serial.println("[Touch] ERRO: Falha ao criar XPT2046");
            return false;
        }

        // Inicializar touchscreen
        if (!touch->begin()) {
            Serial.println("[Touch] ERRO: XPT2046 não respondeu");
            return false;
        }

        Serial.println("[Touch] XPT2046 inicializado!");
        Serial.printf("[Touch] Pinagem: CS=%d, IRQ=%d\n", TOUCH_CS, TOUCH_IRQ);

        // Configurar rotação (ajustar conforme orientação)
        touch->setRotation(1);  // Landscape

        // Registrar driver no LVGL
        lv_indev_drv_init(&indev_drv);
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = lvgl_read_cb;
        indev_drv.user_data = this;

        indev = lv_indev_drv_register(&indev_drv);

        if (!indev) {
            Serial.println("[Touch] ERRO: Falha ao registrar driver LVGL");
            return false;
        }

        Serial.println("[Touch] Driver LVGL registrado!");

        return true;
    }

    // Calibração manual do touchscreen
    void setCalibration(int16_t x_min, int16_t x_max, int16_t y_min, int16_t y_max) {
        cal_x_min = x_min;
        cal_x_max = x_max;
        cal_y_min = y_min;
        cal_y_max = y_max;
        calibrated = true;

        Serial.println("[Touch] Calibração atualizada:");
        Serial.printf("  X: %d - %d\n", cal_x_min, cal_x_max);
        Serial.printf("  Y: %d - %d\n", cal_y_min, cal_y_max);
    }

    // Teste de toque (retorna true se tocado)
    bool isTouched() { return touch ? touch->touched() : false; }

    // Obter ponto de toque bruto
    TS_Point getRawPoint() {
        if (touch && touch->touched()) {
            return touch->getPoint();
        }
        return TS_Point();
    }

    // Mostrar calibração no serial
    void printCalibration() {
        Serial.println("\n[Touch] === CALIBRAÇÃO ATUAL ===");
        Serial.printf("X min: %d, max: %d\n", cal_x_min, cal_x_max);
        Serial.printf("Y min: %d, max: %d\n", cal_y_min, cal_y_max);
        Serial.println("================================\n");
    }

    // Calibração interativa (toque nos cantos)
    void calibrate() {
        Serial.println("\n[Touch] === CALIBRAÇÃO INTERATIVA ===");
        Serial.println("Toque no canto SUPERIOR ESQUERDO...");

        // Aguardar toque
        while (!touch->touched()) {
            delay(100);
        }

        TS_Point p1 = touch->getPoint();
        while (touch->touched()) delay(50);  // Aguardar soltar

        Serial.printf("Ponto 1: X=%d, Y=%d\n", p1.x, p1.y);
        delay(1000);

        Serial.println("Toque no canto INFERIOR DIREITO...");

        while (!touch->touched()) {
            delay(100);
        }

        TS_Point p2 = touch->getPoint();
        while (touch->touched()) delay(50);

        Serial.printf("Ponto 2: X=%d, Y=%d\n", p2.x, p2.y);

        // Calcular calibração
        cal_x_min = min(p1.x, p2.x);
        cal_x_max = max(p1.x, p2.x);
        cal_y_min = min(p1.y, p2.y);
        cal_y_max = max(p1.y, p2.y);

        calibrated = true;

        Serial.println("\n[Touch] Calibração concluída!");
        printCalibration();
    }

    bool isCalibrated() { return calibrated; }

    lv_indev_t* getInputDevice() { return indev; }
};

#endif  // TOUCH_DRIVER_H
