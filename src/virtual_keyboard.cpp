/**
 * @file virtual_keyboard.cpp
 * @brief Sistema Unificado de Teclado Virtual - v6.0.20
 * @version 6.0.20
 * @date 2025-11-29
 * 
 * CORREÇÃO CRÍTICA v6.0.20:
 * - NÃO deletar tela do teclado manualmente após chamar callback de texto
 * - Callback chama mudar_tela() → lv_scr_load() → LVGL deleta tela anterior automaticamente
 * - Deletar manualmente apenas se CANCELADO (sem chamar callback)
 * - Elimina double-free e erro "lv_obj_get_screen: obj != NULL"
 * 
 * v6.0.19: Ordem de execução corrigida (callback antes de deletar)
 * v6.0.18: on_close_callback removido de fluxos de cadastro
 */

#include "virtual_keyboard.h"
#include <Arduino.h>

// ════════════════════════════════════════════════════════════════
// VARIÁVEIS GLOBAIS (PRIVADAS)
// ════════════════════════════════════════════════════════════════

static lv_obj_t * keyboard_screen = nullptr;          // ⭐ TELA DEDICADA!
static lv_obj_t * keyboard_widget = nullptr;          // lv_keyboard nativo
static lv_obj_t * keyboard_textarea = nullptr;        // Input field
static lv_obj_t * btn_cancel_obj = nullptr;           // Botão cancelar
static lv_obj_t * btn_ok_obj = nullptr;               // Botão confirmar
static std::function<void(const char*)> keyboard_callback = nullptr;
static std::function<void()> keyboard_on_close_callback = nullptr;  // ⭐ NOVO!
static bool event_ready_fired = false;                // Flag para evitar eventos duplicados

// ════════════════════════════════════════════════════════════════
// CALLBACKS INTERNOS
// ════════════════════════════════════════════════════════════════

/**
 * @brief Evento do botão CANCELAR
 */
static void btn_cancel_event(lv_event_t * e) {
    Serial.println("❌ [VirtualKeyboard] Usuário cancelou");
    event_ready_fired = false;
    close_virtual_keyboard(false);
}

/**
 * @brief Evento do botão CONFIRMAR
 */
static void btn_ok_event(lv_event_t * e) {
    if (event_ready_fired) {
        Serial.println("⚠️  [VirtualKeyboard] Evento duplicado ignorado");
        return;
    }
    
    if (keyboard_textarea) {
        const char* text = lv_textarea_get_text(keyboard_textarea);
        Serial.printf("✅ [VirtualKeyboard] Usuário confirmou: '%s'\n", text);
        event_ready_fired = true;
        close_virtual_keyboard(true);
    }
}

/**
 * @brief Evento do teclado (detecta READY/CANCEL para fechar)
 */
static void keyboard_event(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY) {
        if (event_ready_fired) {
            Serial.println("⚠️  [VirtualKeyboard] Evento READY duplicado ignorado");
            return;
        }
        
        Serial.println("⌨️ [VirtualKeyboard] Evento READY detectado");
        btn_ok_event(e);
    } 
    else if (code == LV_EVENT_CANCEL) {
        Serial.println("⌨️ [VirtualKeyboard] Evento CANCEL detectado");
        close_virtual_keyboard(false);
    }
}

// ════════════════════════════════════════════════════════════════
// FUNÇÃO PRINCIPAL: ABRIR TECLADO
// ════════════════════════════════════════════════════════════════

void open_virtual_keyboard(const char* title, const char* placeholder, 
                           std::function<void(const char*)> callback,
                           std::function<void()> on_close_callback) {
    
    Serial.println("\n╔══════════════════════════════════════════════╗");
    Serial.println("║   ABRINDO TELA DEDICADA DE TECLADO v6.0.17   ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.printf("📝 Título: %s\n", title);
    Serial.printf("💬 Placeholder: %s\n", placeholder);
    
    // ⭐ Reset flag
    event_ready_fired = false;
    
    // Salvar callbacks
    keyboard_callback = callback;
    keyboard_on_close_callback = on_close_callback;  // ⭐ NOVO!
    
    // ═══════════════════════════════════════════════════════════
    // CRIAR TELA DEDICADA (480×320px) - TELA COMPLETA!
    // ═══════════════════════════════════════════════════════════
    keyboard_screen = lv_obj_create(NULL);
    lv_obj_set_size(keyboard_screen, 480, 320);
    lv_obj_set_style_bg_color(keyboard_screen, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_bg_opa(keyboard_screen, LV_OPA_100, 0);
    lv_obj_clear_flag(keyboard_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // ⭐ CARREGAR A TELA (substitui a tela anterior)
    lv_scr_load(keyboard_screen);
    
    Serial.println("🖥️  Tela dedicada criada (480×320px, 100% opaca)");
    
    // ═══════════════════════════════════════════════════════════
    // TÍTULO (topo centralizado)
    // ═══════════════════════════════════════════════════════════
    lv_obj_t * lbl_title = lv_label_create(keyboard_screen);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // ═══════════════════════════════════════════════════════════
    // TEXTAREA (INPUT) - Grande e centralizado
    // ═══════════════════════════════════════════════════════════
    keyboard_textarea = lv_textarea_create(keyboard_screen);
    lv_obj_set_size(keyboard_textarea, 460, 50);
    lv_obj_align(keyboard_textarea, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(keyboard_textarea, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_color(keyboard_textarea, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_border_width(keyboard_textarea, 3, 0);
    lv_obj_set_style_radius(keyboard_textarea, 8, 0);
    lv_obj_set_style_text_color(keyboard_textarea, lv_color_white(), 0);
    lv_obj_set_style_text_font(keyboard_textarea, &lv_font_montserrat_20, 0);
    lv_obj_set_style_pad_all(keyboard_textarea, 12, 0);
    lv_textarea_set_placeholder_text(keyboard_textarea, placeholder);
    lv_textarea_set_one_line(keyboard_textarea, true);
    lv_textarea_set_max_length(keyboard_textarea, 50);
    
    // ═══════════════════════════════════════════════════════════
    // TECLADO LVGL NATIVO (480×165px) - GRANDE!
    // ═══════════════════════════════════════════════════════════
    keyboard_widget = lv_keyboard_create(keyboard_screen);
    lv_obj_set_size(keyboard_widget, 480, 165);
    lv_obj_align(keyboard_widget, LV_ALIGN_TOP_MID, 0, 100);
    lv_keyboard_set_textarea(keyboard_widget, keyboard_textarea);
    lv_keyboard_set_mode(keyboard_widget, LV_KEYBOARD_MODE_TEXT_LOWER);
    
    // ⭐ v6.0.54: REMOVER eventos automáticos - usar APENAS botões manuais
    // Eventos LV_EVENT_READY/CANCEL do teclado LVGL são disparados indevidamente
    // por teclas especiais (DEL, Backspace etc), causando fechamento acidental
    // lv_obj_add_event_cb(keyboard_widget, keyboard_event, LV_EVENT_READY, NULL);
    // lv_obj_add_event_cb(keyboard_widget, keyboard_event, LV_EVENT_CANCEL, NULL);
    
    // ═══════════════════════════════════════════════════════════
    // BOTÕES (EMBAIXO DO TECLADO) - Y=275
    // ═══════════════════════════════════════════════════════════
    
    // Botão CANCELAR (esquerda)
    btn_cancel_obj = lv_btn_create(keyboard_screen);
    lv_obj_set_size(btn_cancel_obj, 230, 40);
    lv_obj_set_pos(btn_cancel_obj, 5, 275);
    lv_obj_set_style_bg_color(btn_cancel_obj, lv_color_hex(0xef4444), 0);
    lv_obj_set_style_radius(btn_cancel_obj, 8, 0);
    lv_obj_add_event_cb(btn_cancel_obj, btn_cancel_event, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_cancel = lv_label_create(btn_cancel_obj);
    lv_label_set_text(lbl_cancel, LV_SYMBOL_CLOSE " CANCELAR");
    lv_obj_set_style_text_font(lbl_cancel, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_cancel);
    
    // Botão CONFIRMAR (direita)
    btn_ok_obj = lv_btn_create(keyboard_screen);
    lv_obj_set_size(btn_ok_obj, 230, 40);
    lv_obj_set_pos(btn_ok_obj, 245, 275);
    lv_obj_set_style_bg_color(btn_ok_obj, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_radius(btn_ok_obj, 8, 0);
    lv_obj_add_event_cb(btn_ok_obj, btn_ok_event, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_ok = lv_label_create(btn_ok_obj);
    lv_label_set_text(lbl_ok, LV_SYMBOL_OK " CONFIRMAR");
    lv_obj_set_style_text_font(lbl_ok, &lv_font_montserrat_18, 0);
    lv_obj_center(lbl_ok);
    
    Serial.println("🎹 Teclado nativo criado");
    Serial.println("✅ Tela de teclado aberta");
    Serial.println("   📐 Tela: 480×320px (COMPLETA!)");
    Serial.println("   🎨 Fundo: Sólido escuro (100% opaco)");
    Serial.println("   📐 Título: Y=10");
    Serial.println("   📐 Textarea: Y=40 (460×50px)");
    Serial.println("   📐 Teclado: Y=100 (480×165px)");
    Serial.println("   📐 Botões: Y=275 (embaixo!)");
    Serial.println("   ✅ Layout sem sobreposição!");
    Serial.println("╚══════════════════════════════════════════════╝\n");
}

// ════════════════════════════════════════════════════════════════
// FUNÇÃO FECHAR TECLADO
// ════════════════════════════════════════════════════════════════

void close_virtual_keyboard(bool confirmed) {
    Serial.println("\n╔══════════════════════════════════════════════╗");
    Serial.println("║   FECHANDO TELA DE TECLADO v6.0.17           ║");
    Serial.println("╚══════════════════════════════════════════════╝");
    Serial.printf("   Confirmado: %s\n", confirmed ? "SIM" : "NÃO");
    
    // ═══ ETAPA 1: Copiar texto (se confirmado) ═══
    char copied_text[128] = "";
    if (confirmed && keyboard_textarea) {
        const char* text = lv_textarea_get_text(keyboard_textarea);
        strncpy(copied_text, text, sizeof(copied_text) - 1);
        copied_text[sizeof(copied_text) - 1] = '\0';
        Serial.printf("   📝 Texto copiado: '%s'\n", copied_text);
    }
    
    // ═══ ETAPA 2: Desvincular teclado ═══
    if (keyboard_widget) {
        lv_keyboard_set_textarea(keyboard_widget, nullptr);
        Serial.println("   🔗 Teclado desvinculado do textarea");
    }
    
    // ═══ ETAPA 3: ⭐ CHAMAR CALLBACK DE TEXTO PRIMEIRO! ═══
    if (confirmed && keyboard_callback) {
        Serial.printf("   📞 Chamando callback de texto com: '%s'\n", copied_text);
        keyboard_callback(copied_text);  // ⭐ Recria tela SETTINGS → LVGL deleta tela do teclado automaticamente!
        
        // ⭐ IMPORTANTE: mudar_tela() chama lv_scr_load() que DELETA automaticamente a tela anterior!
        // Então NÃO deletamos manualmente para evitar double-free!
        Serial.println("   ✅ Tela do teclado substituída (deletada automaticamente pelo LVGL)");
        keyboard_screen = nullptr;  // Apenas limpar referência
    } else if (!confirmed) {
        Serial.println("   ❌ Cancelado pelo usuário");
        
        // ⭐ Se cancelado, PRECISAMOS deletar manualmente porque mudar_tela() não foi chamado
        if (keyboard_screen) {
            lv_obj_del(keyboard_screen);
            keyboard_screen = nullptr;
            Serial.println("   🗑️  Tela de teclado deletada manualmente");
        }
    }
    
    // Limpar referências
    keyboard_widget = nullptr;
    keyboard_textarea = nullptr;
    btn_cancel_obj = nullptr;
    btn_ok_obj = nullptr;
    event_ready_fired = false;
    
    Serial.println("✅ Tela de teclado fechada");
    Serial.println("╚══════════════════════════════════════════════╝\n");
    
    // Limpar callbacks
    keyboard_callback = nullptr;
    keyboard_on_close_callback = nullptr;
}

// ════════════════════════════════════════════════════════════════
// FUNÇÃO IS_KEYBOARD_OPEN
// ════════════════════════════════════════════════════════════════

bool is_keyboard_open() {
    return keyboard_screen != nullptr;
}