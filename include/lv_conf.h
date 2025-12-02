/**
 * @file lv_conf.h
 * @brief Configuração LVGL 8.3.11 para ESP32S3 + TFT 3.5" ILI9488
 * @version 1.0
 * @date 2025-10-21
 * 
 * CONFIGURAÇÃO VALIDADA E FUNCIONAL
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ============================================================================
 * CONFIGURAÇÕES DE CORES E DISPLAY
 * ========================================================================== */
#define LV_COLOR_DEPTH 16                   // 16-bit (RGB565)
#define LV_COLOR_16_SWAP 0                  // Swap bytes
#define LV_COLOR_SCREEN_TRANSP 0
#define LV_COLOR_MIX_ROUND_OFS 128

/* ============================================================================
 * CONFIGURAÇÕES DE MEMÓRIA
 * ========================================================================== */
#define LV_MEM_CUSTOM 0                     // Usar malloc/free padrão
#define LV_MEM_SIZE (64U * 1024U)          // 64KB para LVGL
#define LV_MEM_ATTR                         // Nenhum atributo especial

/* ============================================================================
 * CONFIGURAÇÕES DE DISPLAY
 * ========================================================================== */
#define LV_HOR_RES_MAX 480                  // Horizontal resolution
#define LV_VER_RES_MAX 320                  // Vertical resolution
#define LV_DPI_DEF 130                      // DPI padrão

/* Buffering */
#define LV_VDB_SIZE (LV_HOR_RES_MAX * 40)  // Virtual Display Buffer (40 linhas)
#define LV_VDB_DOUBLE 0                     // Double buffering desabilitado
#define LV_VDB_ADR 0                        // Endereço automático

/* Refresh */
#define LV_REFR_PERIOD 30                   // Período de refresh (ms)
#define LV_INV_FIFO_SIZE 32                 // Tamanho da fila de invalidação

/* Input device */
#define LV_INDEV_DEF_READ_PERIOD 30         // Input device read period (ms)
#define LV_INDEV_DEF_DRAG_LIMIT 10          // Drag threshold
#define LV_INDEV_DEF_DRAG_THROW 10          // Drag throw slow-down
#define LV_INDEV_DEF_LONG_PRESS_TIME 400    // Long press time (ms)
#define LV_INDEV_DEF_LONG_PRESS_REP_TIME 100 // Long press repeat time (ms)

/* ============================================================================
 * FEATURES
 * ========================================================================== */
/* Enable shadows */
#define LV_USE_SHADOW 1
#if LV_USE_SHADOW
#define LV_SHADOW_CACHE_SIZE 0
#endif

/* Enable blend modes */
#define LV_USE_BLEND_MODES 1

/* Enable outline */
#define LV_USE_OUTLINE 1

/* Enable pattern */
#define LV_USE_PATTERN 1

/* Enable value string */
#define LV_USE_VALUE_STR 1

/* Enable animations */
#define LV_USE_ANIMATION 1
#if LV_USE_ANIMATION
#define LV_ANIM_DEF_TIME 200
#endif

/* Enable anti-aliasing */
#define LV_USE_ANTIALIAS 1

/* Enable groups (for keyboard/encoder navigation) */
#define LV_USE_GROUP 1
#if LV_USE_GROUP
typedef void * lv_group_user_data_t;
#endif

/* Enable GPU (não disponível no ESP32) */
#define LV_USE_GPU 0

/* Enable file system */
#define LV_USE_FILESYSTEM 1
#if LV_USE_FILESYSTEM
typedef void * lv_fs_drv_user_data_t;
#endif

/* ============================================================================
 * LOGGING
 * ========================================================================== */
#define LV_USE_LOG 1
#if LV_USE_LOG
#ifndef LV_LOG_LEVEL
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO      // TRACE, INFO, WARN, ERROR, USER
#endif
#define LV_LOG_PRINTF 1                     // Usar printf para logs
#endif

/* ============================================================================
 * THEMES
 * ========================================================================== */
#define LV_USE_THEME_EMPTY 0
#define LV_USE_THEME_TEMPLATE 0
#define LV_USE_THEME_MATERIAL 1
#define LV_USE_THEME_MONO 0

#define LV_THEME_DEFAULT_INIT lv_theme_material_init

#if LV_USE_THEME_MATERIAL
#define LV_THEME_DEFAULT_COLOR_PRIMARY lv_color_hex(0x01579b)
#define LV_THEME_DEFAULT_COLOR_SECONDARY lv_color_hex(0x37474f)
#define LV_THEME_DEFAULT_FLAG LV_THEME_MATERIAL_FLAG_DARK
#define LV_THEME_DEFAULT_FONT_SMALL &lv_font_montserrat_12
#define LV_THEME_DEFAULT_FONT_NORMAL &lv_font_montserrat_14
#define LV_THEME_DEFAULT_FONT_SUBTITLE &lv_font_montserrat_16
#define LV_THEME_DEFAULT_FONT_TITLE &lv_font_montserrat_20
#endif

/* ============================================================================
 * FONTES (Fonts)
 * ========================================================================== */
#define LV_FONT_MONTSERRAT_8  1             // Habilitado para labels pequenos
#define LV_FONT_MONTSERRAT_10 1             // Habilitado para labels pequenos
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 1             // Habilitado para PIN display
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1             // Habilitado para ícones grandes

/* Outras fontes */
#define LV_FONT_UNSCII_8 0

/* Font customizado (se necessário) */
#define LV_FONT_CUSTOM_DECLARE

/* Default font */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* ============================================================================
 * WIDGETS (Objetos)
 * ========================================================================== */
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CALENDAR 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 1
#define LV_USE_CHART 1
#define LV_USE_CONT 1
#define LV_USE_CPICKER 1
#define LV_USE_DROPDOWN 1
#define LV_USE_GAUGE 1
#define LV_USE_IMG 1
#define LV_USE_IMGBTN 1
#define LV_USE_KEYBOARD 1
#define LV_USE_LABEL 1
#define LV_USE_LED 1
#define LV_USE_LINE 1
#define LV_USE_LIST 1
#define LV_USE_LINEMETER 1
#define LV_USE_MSGBOX 1
#define LV_USE_OBJMASK 1
#define LV_USE_PAGE 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SPINBOX 1
#define LV_USE_SPINNER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1
#define LV_USE_TABVIEW 1
#define LV_USE_TILEVIEW 1
#define LV_USE_WIN 1

/* ============================================================================
 * CONFIGURAÇÕES AVANÇADAS
 * ========================================================================== */
#define LV_TICK_CUSTOM 0                    // Usar lv_tick_inc() manual
#define LV_DISP_DEF_REFR_PERIOD 30         // Refresh period (ms)
#define LV_INDEV_DEF_GESTURE_LIMIT 50      // Gesture threshold
#define LV_INDEV_DEF_GESTURE_MIN_VELOCITY 3 // Minimum velocity for gesture

/* Enable performance monitor */
#ifndef LV_USE_PERF_MONITOR
#define LV_USE_PERF_MONITOR 0              // Desabilitado para produção
#endif

/* Enable memory monitor */
#define LV_USE_MEM_MONITOR 0               // Desabilitado para produção

/* Enable printf */
#define LV_SPRINTF_CUSTOM 0

/* Enable assert */
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MEM 1
#define LV_USE_ASSERT_STR 0
#define LV_USE_ASSERT_OBJ 1
#define LV_USE_ASSERT_STYLE 0

/* ============================================================================
 * STRING & TEXT
 * ========================================================================== */
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 12
#define LV_TXT_LINE_BREAK_LONG_PRE_MIN_LEN 3
#define LV_TXT_LINE_BREAK_LONG_POST_MIN_LEN 3
#define LV_TXT_ENC LV_TXT_ENC_UTF8

/* ============================================================================
 * COMPILER SETTINGS
 * ========================================================================== */
#define LV_ATTRIBUTE_TICK_INC
#define LV_ATTRIBUTE_TASK_HANDLER
#define LV_ATTRIBUTE_FLUSH_READY
#define LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_COMPILER_VLA_SUPPORTED 1
#define LV_COMPILER_NON_CONST_INIT_SUPPORTED 1

#endif /* LV_CONF_H */