/**
 * @file calibration.h
 * @brief Sistema simples de calibração de touchscreen
 * @date 21/11/2025
 * 
 * Sistema simplificado sem dependências complexas
 */

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

// ════════════════════════════════════════════════════════════════
// 📝 FUNÇÕES DE CALIBRAÇÃO
// ════════════════════════════════════════════════════════════════

// Variáveis globais para calibração (usadas pelo main.cpp)
extern uint16_t touch_min_x;
extern uint16_t touch_max_x;
extern uint16_t touch_min_y;
extern uint16_t touch_max_y;

// Funções de calibração
void carregar_calibracao();
void salvar_calibracao();
void imprimir_status_calibracao();
void calibrar_coordenadas(int16_t raw_x, int16_t raw_y, int16_t &x, int16_t &y);

#endif // CALIBRATION_H