/**
 * @file touch_calibration.h
 * @brief Sistema de calibração de touchscreen para ESP32 + LVGL
 * @date 21/11/2025
 * 
 * Sistema completo de calibração com:
 * - Calibração de 4 pontos (cantos da tela)
 * - Armazenamento persistente em Preferences
 * - Modo de teste visual
 * - Fallback para valores padrão
 */

#ifndef TOUCH_CALIBRATION_H
#define TOUCH_CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>

// ════════════════════════════════════════════════════════════════
// 📊 ESTRUTURA DE DADOS DE CALIBRAÇÃO
// ════════════════════════════════════════════════════════════════

struct TouchCalibrationData {
    uint16_t min_x;        // Valor RAW mínimo de X
    uint16_t max_x;        // Valor RAW máximo de X
    uint16_t min_y;        // Valor RAW mínimo de Y
    uint16_t max_y;        // Valor RAW máximo de Y
    bool is_valid;         // Flag de validação
    uint32_t checksum;     // Checksum para validar integridade
};

// ════════════════════════════════════════════════════════════════
// 🎯 PONTOS DE CALIBRAÇÃO (4 cantos + 1 centro)
// ════════════════════════════════════════════════════════════════

struct CalibrationPoint {
    uint16_t screen_x;     // Coordenada X na tela
    uint16_t screen_y;     // Coordenada Y na tela
    uint16_t raw_x;        // Valor RAW lido
    uint16_t raw_y;        // Valor RAW lido
};

// ════════════════════════════════════════════════════════════════
// 🔧 CLASSE DE CALIBRAÇÃO
// ════════════════════════════════════════════════════════════════

class TouchCalibration {
public:
    TouchCalibration();
    
    // Inicialização
    bool begin();
    
    // Carregar/Salvar calibração
    bool loadCalibration();
    bool saveCalibration();
    bool hasValidCalibration();
    
    // Calibração
    void startCalibration();
    bool addCalibrationPoint(uint16_t raw_x, uint16_t raw_y);
    bool finishCalibration();
    void resetToDefaults();
    
    // Getters
    TouchCalibrationData getCalibrationData() const;
    void printCalibrationData() const;
    
    // Modo de teste
    void enterTestMode();
    void exitTestMode();
    bool isInTestMode() const;
    
private:
    Preferences preferences;
    TouchCalibrationData calibData;
    CalibrationPoint calibPoints[5];  // 4 cantos + 1 centro
    uint8_t currentPointIndex;
    bool testMode;
    
    // Helpers
    uint32_t calculateChecksum(const TouchCalibrationData& data);
    bool validateCalibrationData(const TouchCalibrationData& data);
    void calculateCalibrationFromPoints();
};

// ════════════════════════════════════════════════════════════════
// 📝 VALORES PADRÃO (FALLBACK)
// ════════════════════════════════════════════════════════════════

const TouchCalibrationData DEFAULT_CALIBRATION = {
    .min_x = 400,
    .max_x = 3950,
    .min_y = 330,
    .max_y = 3650,
    .is_valid = true,
    .checksum = 0
};

// Pontos de calibração (posições na tela 480x320)
const CalibrationPoint CALIBRATION_TARGETS[5] = {
    {40, 40, 0, 0},          // Canto superior esquerdo
    {440, 40, 0, 0},         // Canto superior direito
    {440, 280, 0, 0},        // Canto inferior direito
    {40, 280, 0, 0},         // Canto inferior esquerdo
    {240, 160, 0, 0}         // Centro
};

#endif // TOUCH_CALIBRATION_H
