/**
 * @file ui_manager.h
 * @brief Gerenciador de interface LVGL
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <lvgl.h>

/**
 * @brief Inicializa a interface LVGL
 */
void ui_init();

/**
 * @brief Mostra tela de boot/splash
 */
void ui_show_boot_screen();

/**
 * @brief Mostra tela principal de acesso
 */
void ui_show_main_screen();

/**
 * @brief Atualiza status dos sensores
 */
void ui_update_status(bool wifi_ok, bool bio_ok, bool rfid_ok);

/**
 * @brief Mostra mensagem de acesso concedido
 */
void ui_show_access_granted(const char* method, const char* user);

/**
 * @brief Mostra mensagem de acesso negado
 */
void ui_show_access_denied(const char* reason);

#endif  // UI_MANAGER_H
