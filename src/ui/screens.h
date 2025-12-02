/**
 * @file screens.h
 * @brief Telas LVGL do Sistema de Controle de Acesso
 *
 * Define todas as telas da interface
 */

#ifndef SCREENS_H
#define SCREENS_H

#include <lvgl.h>

// Forward declarations
class UIManager;

// ==========================================
// TELA DE BOAS-VINDAS
// ==========================================
class WelcomeScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* title_label;
    lv_obj_t* subtitle_label;
    UIManager* ui_manager;

   public:
    WelcomeScreen(UIManager* manager);
    void show();
    void hide();
    lv_obj_t* getScreen() { return screen; }
};

// ==========================================
// MENU PRINCIPAL
// ==========================================
class MainMenuScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* title_label;
    lv_obj_t* btn_pin;
    lv_obj_t* btn_fingerprint;
    lv_obj_t* btn_rfid;
    lv_obj_t* btn_settings;
    UIManager* ui_manager;

    static void btn_pin_clicked(lv_event_t* e);
    static void btn_fingerprint_clicked(lv_event_t* e);
    static void btn_rfid_clicked(lv_event_t* e);
    static void btn_settings_clicked(lv_event_t* e);

   public:
    MainMenuScreen(UIManager* manager);
    void show();
    void hide();
    lv_obj_t* getScreen() { return screen; }
};

// ==========================================
// TELA DE PIN
// ==========================================
class PINScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* title_label;
    lv_obj_t* pin_display;
    lv_obj_t* keyboard;
    lv_obj_t* btn_back;
    UIManager* ui_manager;

    String current_pin;
    static const int MAX_PIN_LENGTH = 6;

    static void keyboard_event(lv_event_t* e);
    static void btn_back_clicked(lv_event_t* e);
    void updatePINDisplay();
    void validatePIN();

   public:
    PINScreen(UIManager* manager);
    void show();
    void hide();
    void reset();
    lv_obj_t* getScreen() { return screen; }
};

// ==========================================
// TELA DE BIOMETRIA
// ==========================================
class FingerprintScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* title_label;
    lv_obj_t* status_label;
    lv_obj_t* finger_icon;
    lv_obj_t* spinner;
    lv_obj_t* btn_back;
    lv_obj_t* btn_enroll;
    UIManager* ui_manager;

    static void btn_back_clicked(lv_event_t* e);
    static void btn_enroll_clicked(lv_event_t* e);

   public:
    FingerprintScreen(UIManager* manager);
    void show();
    void hide();
    void setStatus(const char* message, lv_color_t color);
    void setWaiting();
    void setReading();
    void setSuccess(uint16_t id, uint16_t confidence);
    void setError(const char* message);
    lv_obj_t* getScreen() { return screen; }
};

// ==========================================
// TELA DE RFID
// ==========================================
class RFIDScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* title_label;
    lv_obj_t* status_label;
    lv_obj_t* card_icon;
    lv_obj_t* btn_back;
    UIManager* ui_manager;

    static void btn_back_clicked(lv_event_t* e);

   public:
    RFIDScreen(UIManager* manager);
    void show();
    void hide();
    void setStatus(const char* message, lv_color_t color);
    void setWaiting();
    void setCardDetected(const char* uid);
    void setError(const char* message);
    lv_obj_t* getScreen() { return screen; }
};

// ==========================================
// TELA DE SUCESSO
// ==========================================
class SuccessScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* icon;
    lv_obj_t* message_label;
    lv_obj_t* name_label;
    UIManager* ui_manager;

    lv_timer_t* auto_close_timer;
    static void auto_close_timer_cb(lv_timer_t* timer);

   public:
    SuccessScreen(UIManager* manager);
    void show(const char* message, const char* name = nullptr);
    void hide();
    lv_obj_t* getScreen() { return screen; }
};

// ==========================================
// TELA DE ERRO
// ==========================================
class ErrorScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* icon;
    lv_obj_t* message_label;
    lv_obj_t* btn_retry;
    UIManager* ui_manager;

    static void btn_retry_clicked(lv_event_t* e);

   public:
    ErrorScreen(UIManager* manager);
    void show(const char* message);
    void hide();
    lv_obj_t* getScreen() { return screen; }
};

// ==========================================
// TELA DE CONFIGURAÇÕES
// ==========================================
class SettingsScreen {
   private:
    lv_obj_t* screen;
    lv_obj_t* title_label;
    lv_obj_t* brightness_slider;
    lv_obj_t* btn_calibrate;
    lv_obj_t* btn_enroll_finger;
    lv_obj_t* btn_clear_fingers;
    lv_obj_t* btn_back;
    UIManager* ui_manager;

    static void brightness_changed(lv_event_t* e);
    static void btn_calibrate_clicked(lv_event_t* e);
    static void btn_enroll_clicked(lv_event_t* e);
    static void btn_clear_clicked(lv_event_t* e);
    static void btn_back_clicked(lv_event_t* e);

   public:
    SettingsScreen(UIManager* manager);
    void show();
    void hide();
    lv_obj_t* getScreen() { return screen; }
};

#endif  // SCREENS_H
