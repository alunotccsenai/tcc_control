/**
 * @file biometric_driver.h
 * @brief Driver AS608 para Sensor Biométrico
 *
 * Integração do sensor de impressão digital AS608
 */

#ifndef BIOMETRIC_DRIVER_H
#define BIOMETRIC_DRIVER_H

#include <Adafruit_Fingerprint.h>
#include <Arduino.h>

// Pinagem UART para AS608
#define FINGERPRINT_RX 17  // ESP32 RX <- AS608 TX
#define FINGERPRINT_TX 18  // ESP32 TX -> AS608 RX

// Estados do sensor
enum BiometricState {
    BIO_IDLE,
    BIO_WAITING_FINGER,
    BIO_READING,
    BIO_MATCHED,
    BIO_NOT_MATCHED,
    BIO_ERROR,
    BIO_ENROLLING,
    BIO_ENROLL_COMPLETE
};

class BiometricDriver {
   private:
    HardwareSerial* fingerSerial;
    Adafruit_Fingerprint* finger;
    BiometricState state;
    uint16_t lastMatchedID;
    uint16_t lastConfidence;

    // Callback de status
    void (*statusCallback)(BiometricState state, const char* message);

   public:
    BiometricDriver()
        : fingerSerial(nullptr),
          finger(nullptr),
          state(BIO_IDLE),
          lastMatchedID(0),
          lastConfidence(0),
          statusCallback(nullptr) {}

    bool init() {
        Serial.println("[Biometria] Inicializando AS608...");

        // Criar serial UART1
        fingerSerial = new HardwareSerial(1);
        fingerSerial->begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);

        // Criar instância do sensor
        finger = new Adafruit_Fingerprint(fingerSerial);

        if (!finger) {
            Serial.println("[Biometria] ERRO: Falha ao criar Adafruit_Fingerprint");
            return false;
        }

        // Verificar conexão
        if (finger->verifyPassword()) {
            Serial.println("[Biometria] AS608 detectado!");
        } else {
            Serial.println("[Biometria] ERRO: AS608 não encontrado");
            return false;
        }

        // Obter parâmetros do sensor
        finger->getParameters();
        Serial.printf("[Biometria] Status: %d templates armazenados\n", finger->templateCount);
        Serial.printf("[Biometria] Capacidade: %d templates\n", finger->capacity);

        state = BIO_IDLE;
        return true;
    }

    // Definir callback de status
    void setStatusCallback(void (*callback)(BiometricState, const char*)) {
        statusCallback = callback;
    }

    // Verificar digital (modo 1:N)
    bool verify() {
        state = BIO_WAITING_FINGER;
        notifyStatus("Coloque o dedo no sensor");

        // Aguardar dedo
        int p = finger->getImage();
        if (p != FINGERPRINT_OK) {
            if (p == FINGERPRINT_NOFINGER) {
                return false;  // Sem dedo, tentar novamente
            }
            state = BIO_ERROR;
            notifyStatus("Erro ao ler imagem");
            return false;
        }

        state = BIO_READING;
        notifyStatus("Lendo impressão digital...");

        // Converter imagem
        p = finger->image2Tz();
        if (p != FINGERPRINT_OK) {
            state = BIO_ERROR;
            notifyStatus("Erro ao processar imagem");
            return false;
        }

        // Buscar no banco de dados
        p = finger->fingerSearch();
        if (p == FINGERPRINT_OK) {
            lastMatchedID = finger->fingerID;
            lastConfidence = finger->confidence;
            state = BIO_MATCHED;

            char msg[64];
            snprintf(msg, sizeof(msg), "Digital reconhecida! ID: %d (Conf: %d)", lastMatchedID,
                     lastConfidence);
            notifyStatus(msg);

            Serial.printf("[Biometria] Match! ID=%d, Conf=%d\n", lastMatchedID, lastConfidence);
            return true;
        } else {
            state = BIO_NOT_MATCHED;
            notifyStatus("Digital não reconhecida");
            Serial.println("[Biometria] Não reconhecido");
            return false;
        }
    }

    // Cadastrar nova digital
    bool enroll(uint16_t id) {
        state = BIO_ENROLLING;
        Serial.printf("[Biometria] Iniciando cadastro ID %d\n", id);

        // Etapa 1: Primeira leitura
        notifyStatus("Coloque o dedo no sensor");
        while (finger->getImage() != FINGERPRINT_OK) {
            delay(50);
        }

        if (finger->image2Tz(1) != FINGERPRINT_OK) {
            state = BIO_ERROR;
            notifyStatus("Erro ao processar imagem 1");
            return false;
        }

        notifyStatus("Retire o dedo");
        delay(2000);
        while (finger->getImage() != FINGERPRINT_NOFINGER) {
            delay(50);
        }

        // Etapa 2: Segunda leitura
        notifyStatus("Coloque o mesmo dedo novamente");
        while (finger->getImage() != FINGERPRINT_OK) {
            delay(50);
        }

        if (finger->image2Tz(2) != FINGERPRINT_OK) {
            state = BIO_ERROR;
            notifyStatus("Erro ao processar imagem 2");
            return false;
        }

        // Criar modelo
        int p = finger->createModel();
        if (p != FINGERPRINT_OK) {
            state = BIO_ERROR;
            notifyStatus("Erro ao criar modelo");
            return false;
        }

        // Armazenar modelo
        p = finger->storeModel(id);
        if (p != FINGERPRINT_OK) {
            state = BIO_ERROR;
            notifyStatus("Erro ao armazenar digital");
            return false;
        }

        state = BIO_ENROLL_COMPLETE;
        char msg[64];
        snprintf(msg, sizeof(msg), "Digital cadastrada! ID: %d", id);
        notifyStatus(msg);

        Serial.printf("[Biometria] Digital cadastrada com sucesso! ID=%d\n", id);
        return true;
    }

    // Deletar digital
    bool deleteFingerprint(uint16_t id) {
        int p = finger->deleteModel(id);
        if (p == FINGERPRINT_OK) {
            Serial.printf("[Biometria] Digital ID %d deletada\n", id);
            return true;
        }
        Serial.printf("[Biometria] Erro ao deletar ID %d\n", id);
        return false;
    }

    // Limpar todas as digitais
    bool emptyDatabase() {
        int p = finger->emptyDatabase();
        if (p == FINGERPRINT_OK) {
            Serial.println("[Biometria] Banco de dados limpo");
            return true;
        }
        Serial.println("[Biometria] Erro ao limpar banco");
        return false;
    }

    // Obter número de templates cadastrados
    uint16_t getTemplateCount() {
        finger->getParameters();
        return finger->templateCount;
    }

    // Obter capacidade do sensor
    uint16_t getCapacity() {
        finger->getParameters();
        return finger->capacity;
    }

    // Obter estado atual
    BiometricState getState() { return state; }

    // Obter último ID reconhecido
    uint16_t getLastMatchedID() { return lastMatchedID; }

    // Obter última confiança
    uint16_t getLastConfidence() { return lastConfidence; }

    // Reset estado
    void reset() {
        state = BIO_IDLE;
        lastMatchedID = 0;
        lastConfidence = 0;
    }

   private:
    void notifyStatus(const char* message) {
        if (statusCallback) {
            statusCallback(state, message);
        }
    }
};

#endif  // BIOMETRIC_DRIVER_H
