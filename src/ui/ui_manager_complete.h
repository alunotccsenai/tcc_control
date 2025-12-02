/**
 * @file ui_manager_complete.h
 * @brief Gerenciador completo de UI com LVGL
 *
 * Gerencia todas as telas e navegação do sistema
 */

#ifndef UI_MANAGER_COMPLETE_H
#define UI_MANAGER_COMPLETE_H

#include <Arduino.h>
#include <lvgl.h>

#include "../drivers/biometric_driver.h"
#include "../drivers/display_driver.h"
#include "../drivers/touch_driver.h"

// Forward declaration
class WelcomeScreen;
class MainMenuScreen;
class PINScreen;
class FingerprintScreen;
class RFIDScreen;
class SuccessScreen;
class ErrorScreen;
class SettingsScreen;

// Enum de telas
enum ScreenType {
    SCREEN_WELCOME,
    SCREEN_MAIN_MENU,
    SCREEN_PIN,
    SCREEN_FINGERPRINT,
    SCREEN_RFID,
    SCREEN_SUCCESS,
    SCREEN_ERROR,
    SCREEN_SETTINGS
};

class UIManager {
   private:
    DisplayDriver* display_driver;
    TouchDriver* touch_driver;
    BiometricDriver* bio_driver;

    // Telas
    WelcomeScreen* welcome_screen;
    MainMenuScreen* main_menu_screen;
    PINScreen* pin_screen;
    FingerprintScreen* fingerprint_screen;
    RFIDScreen* rfid_screen;
    SuccessScreen* success_screen;
    ErrorScreen* error_screen;
    SettingsScreen* settings_screen;

    ScreenType current_screen;
    ScreenType previous_screen;

    // Timer LVGL
    lv_timer_t* lvgl_timer;
    static void lvgl_timer_handler(lv_timer_t* timer);

    // Tema customizado
    void applyTheme();

   public:
    UIManager(DisplayDriver* display, TouchDriver* touch, BiometricDriver* bio);
    ~UIManager();

    bool init();
    void update();

    // Navegação entre telas
    void showScreen(ScreenType screen);
    void goBack();
    ScreenType getCurrentScreen() { return current_screen; }
    ScreenType getPreviousScreen() { return previous_screen; }

    // Acessar telas
    WelcomeScreen* getWelcomeScreen() { return welcome_screen; }
    MainMenuScreen* getMainMenuScreen() { return main_menu_screen; }
    PINScreen* getPINScreen() { return pin_screen; }
    FingerprintScreen* getFingerprintScreen() { return fingerprint_screen; }
    RFIDScreen* getRFIDScreen() { return rfid_screen; }
    SuccessScreen* getSuccessScreen() { return success_screen; }
    ErrorScreen* getErrorScreen() { return error_screen; }
    SettingsScreen* getSettingsScreen() { return settings_screen; }

    // Acessar drivers
    DisplayDriver* getDisplayDriver() { return display_driver; }
    TouchDriver* getTouchDriver() { return touch_driver; }
    BiometricDriver* getBiometricDriver() { return bio_driver; }

    // Helpers para feedback
    void showSuccess(const char* message, const char* name = nullptr);
    void showError(const char* message);
    void setBrightness(uint8_t brightness);
};

#endif  // UI_MANAGER_COMPLETE_H
