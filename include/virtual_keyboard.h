/**
 * @file virtual_keyboard.h
 * @brief Sistema Unificado de Teclado Virtual - v6.0.20
 * @version 6.0.20
 * @date 2025-11-29
 * 
 * CORREÇÃO CRÍTICA v6.0.20:
 * - NÃO deletar tela manualmente após callback de texto
 * - LVGL deleta tela anterior automaticamente ao chamar lv_scr_load()
 * - Deletar manualmente apenas se usuário cancelar
 * - Elimina double-free e erro de objetos NULL
 * 
 * v6.0.19: Ordem de execução corrigida
 * v6.0.18: on_close_callback removido de fluxos de cadastro
 */

#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include <lvgl.h>
#include <functional>

// ════════════════════════════════════════════════════════════════
// API PÚBLICA
// ════════════════════════════════════════════════════════════════

/**
 * @brief Abre teclado virtual com modal
 * @param title Título do modal (ex: "Digite o nome do usuário:")
 * @param placeholder Texto placeholder (ex: "Nome...")
 * @param callback Função chamada quando usuário confirma (recebe texto)
 * @param on_close_callback Função chamada ANTES de fechar tela para recriar tela anterior
 * 
 * EXEMPLO:
 * open_virtual_keyboard("Digite o nome:", "Nome...", 
 *   [](const char* nome) {
 *     Serial.printf("Nome: %s\n", nome);
 *   },
 *   []() {
 *     mudar_tela(SCREEN_SETTINGS);
 *   }
 * );
 */
void open_virtual_keyboard(
    const char* title, 
    const char* placeholder, 
    std::function<void(const char*)> callback,
    std::function<void()> on_close_callback = nullptr
);

/**
 * @brief Fecha teclado virtual (uso interno ou emergencial)
 * @param confirmado true = chama callback, false = cancela
 */
void close_virtual_keyboard(bool confirmado);

/**
 * @brief Verifica se teclado está aberto
 * @return true se modal está visível
 */
bool is_keyboard_open();

#endif // VIRTUAL_KEYBOARD_H
