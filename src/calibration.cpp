/**
 * @file calibration.cpp
 * @brief Implementação simples de calibração de touchscreen
 * @date 21/11/2025
 */

#include "calibration.h"
#include "config.h"
#include <Preferences.h>

// ════════════════════════════════════════════════════════════════
// 📊 VARIÁVEIS GLOBAIS
// ════════════════════════════════════════════════════════════════

uint16_t touch_min_x = TOUCH_MIN_X;
uint16_t touch_max_x = TOUCH_MAX_X;
uint16_t touch_min_y = TOUCH_MIN_Y;
uint16_t touch_max_y = TOUCH_MAX_Y;

static Preferences prefs;

// ════════════════════════════════════════════════════════════════
// 🔧 IMPLEMENTAÇÃO DAS FUNÇÕES
// ════════════════════════════════════════════════════════════════

void carregar_calibracao() {
    Serial.println("\n📐 ═══════════════════════════════════════");
    Serial.println("   CARREGANDO CALIBRAÇÃO DO TOUCHSCREEN");
    Serial.println("═══════════════════════════════════════");
    
    // Abre Preferences em modo leitura
    if (prefs.begin("touch_cal", true)) {  // true = read-only
        if (prefs.isKey("cal_valid")) {
            touch_min_x = prefs.getUShort("cal_min_x", TOUCH_MIN_X);
            touch_max_x = prefs.getUShort("cal_max_x", TOUCH_MAX_X);
            touch_min_y = prefs.getUShort("cal_min_y", TOUCH_MIN_Y);
            touch_max_y = prefs.getUShort("cal_max_y", TOUCH_MAX_Y);
            
            Serial.println("✅ Calibração carregada da memória Flash");
        } else {
            Serial.println("⚠️ Nenhuma calibração salva, usando valores padrão");
        }
        prefs.end();
    } else {
        Serial.println("⚠️ Erro ao acessar Preferences, usando valores padrão");
    }
    
    Serial.println("═══════════════════════════════════════\n");
}

void salvar_calibracao() {
    Serial.println("\n💾 ═══════════════════════════════════════");
    Serial.println("   SALVANDO CALIBRAÇÃO DO TOUCHSCREEN");
    Serial.println("═══════════════════════════════════════");
    
    // Abre Preferences em modo escrita
    if (prefs.begin("touch_cal", false)) {  // false = read-write
        prefs.putUShort("cal_min_x", touch_min_x);
        prefs.putUShort("cal_max_x", touch_max_x);
        prefs.putUShort("cal_min_y", touch_min_y);
        prefs.putUShort("cal_max_y", touch_max_y);
        prefs.putBool("cal_valid", true);
        
        Serial.println("✅ Calibração salva com sucesso na Flash!");
        prefs.end();
    } else {
        Serial.println("❌ Erro ao salvar calibração!");
    }
    
    Serial.println("═══════════════════════════════════════\n");
}

void imprimir_status_calibracao() {
    Serial.println("\n📊 ═══ STATUS DA CALIBRAÇÃO ═══");
    Serial.printf("  MIN_X: %d (padrão: %d)\n", touch_min_x, TOUCH_MIN_X);
    Serial.printf("  MAX_X: %d (padrão: %d)\n", touch_max_x, TOUCH_MAX_X);
    Serial.printf("  MIN_Y: %d (padrão: %d)\n", touch_min_y, TOUCH_MIN_Y);
    Serial.printf("  MAX_Y: %d (padrão: %d)\n", touch_max_y, TOUCH_MAX_Y);
    
    // Validação básica
    bool valid = true;
    if (touch_min_x >= touch_max_x || touch_min_y >= touch_max_y) {
        Serial.println("  ❌ ERRO: MIN >= MAX");
        valid = false;
    }
    if (touch_max_x > 4095 || touch_max_y > 4095) {
        Serial.println("  ⚠️ AVISO: Valores acima de 4095");
        valid = false;
    }
    
    Serial.printf("  STATUS: %s\n", valid ? "✅ OK" : "❌ INVÁLIDO");
    Serial.println("══════════════════════════════════\n");
}

void calibrar_coordenadas(int16_t raw_x, int16_t raw_y, int16_t &x, int16_t &y) {
    // Mapeamento invertido conforme config.h
    // x = map(raw_x, MAX_X, MIN_X, 0, 480)  ← Invertido
    // y = map(raw_y, MAX_Y, MIN_Y, 0, 320)  ← Invertido
    
    x = map(raw_x, touch_max_x, touch_min_x, 0, 480);
    y = map(raw_y, touch_max_y, touch_min_y, 0, 320);
    
    // Aplicar limites (clamp)
    if (x < 0) x = 0;
    if (x > 479) x = 479;
    if (y < 0) y = 0;
    if (y > 319) y = 319;
}
