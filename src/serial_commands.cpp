/**
 * @file serial_commands.cpp
 * @brief Comandos Serial para debug e testes dos sistemas RFID/BIO/RELÉ
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * Adicione ao main.cpp:
 * 
 * #include "serial_commands.h"
 * 
 * void loop() {
 *     processSerialCommands();
 *     ...
 * }
 */

#include <Arduino.h>
#include <vector>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "serial_commands.h"
#include "relay_controller.h"
#include "config.h"

// ═══════════════════════════════════════════════════════════════════════
// IMPORTANTE: Resolução de conflitos de estruturas
// ═══════════════════════════════════════════════════════════════════════
// 
// PROBLEMA: rfid_storage.h e rfid_manager.h definem structs diferentes com o mesmo nome
// SOLUÇÃO: Usar interface separada (manager_interface.h) que fornece acesso aos
//          métodos dos managers sem incluir as definições conflitantes

// Incluir bibliotecas de storage (usam String-based structs)
#include "rfid_storage.h"
#include "biometric_storage.h"

// Incluir interface dos managers (sem conflitos de estruturas)
#include "manager_interface.h"

// Handlers RFID simples
#include "rfid_handlers_simple.h"

// Instâncias externas (definidas no main.cpp)
extern RelayController relayController;
extern RFIDStorage rfidStorage;
extern BiometricStorage bioStorage;

// ═══════════════════════════════════════════════════════════════════════
// PROCESSAMENTO DE COMANDOS
// ═══════════════════════════════════════════════════════════════════════

void processSerialCommands() {
    if (!Serial.available()) return;
    
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    Serial.println("\n═══════════════════════════════════");
    Serial.printf("Comando recebido: %s\n", cmd.c_str());
    Serial.println("═══════════════════════════════════");
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDOS DE AJUDA
    // ═══════════════════════════════════════════════════════════════
    
    if (cmd == "HELP" || cmd == "?") {
        Serial.println("\n📚 COMANDOS DISPONÍVEIS:");
        Serial.println("\n=== GERAL ===");
        Serial.println("HELP, ?          - Esta mensagem");
        Serial.println("STATUS           - Status de todos os sistemas");
        Serial.println("STATS            - Estatísticas gerais");
        Serial.println("VERSION          - Versão do firmware");
        
        Serial.println("\n=== RELÉ ===");
        Serial.println("ABRIR            - Destranca porta (5s)");
        Serial.println("ABRIR <ms>       - Destranca porta (tempo custom)");
        Serial.println("FECHAR           - Tranca porta");
        Serial.println("RELE_STATUS      - Status do relé");
        
        Serial.println("\n=== RFID ===");
        Serial.println("LISTAR_RFID      - Lista cartões cadastrados");
        Serial.println("ADD_RFID_TEST    - Adiciona cartão de teste");
        Serial.println("REMOVE_RFID <uid> - Remove cartão");
        Serial.println("CLEAR_RFID       - Remove TODOS os cartões");
        Serial.println("EXPORT_RFID      - Exporta dados em JSON");
        
        Serial.println("\n=== BIOMETRIA ===");
        Serial.println("LISTAR_BIO       - Lista usuários cadastrados");
        Serial.println("ADD_BIO_TEST     - Adiciona usuário de teste");
        Serial.println("REMOVE_BIO <slot> - Remove usuário");
        Serial.println("CLEAR_BIO        - Remove TODOS os usuários");
        Serial.println("EXPORT_BIO       - Exporta dados em JSON");
        
        Serial.println("\n=== BACKUP ===");
        Serial.println("BACKUP           - Faz backup completo");
        Serial.println("RESTORE          - Restaura backup");
        
        Serial.println("\n=== DEBUG ===");
        Serial.println("TEST_PN532       - Testa comunicação PN532");
        Serial.println("TEST_AS608       - Testa comunicação AS608");
        Serial.println("FORMAT_LITTLEFS  - Formata LittleFS (CUIDADO!)");
        Serial.println("REBOOT           - Reinicia ESP32");
        
        Serial.println("═══════════════════════════════════\n");
    }
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDOS DE STATUS
    // ═══════════════════════════════════════════════════════════════
    
    else if (cmd == "STATUS") {
        Serial.println("\n📊 STATUS DO SISTEMA:");
        Serial.println("\n🔌 RELÉ:");
        Serial.printf("  Estado: %s\n", 
            relayController.isUnlocked() ? "DESTRANCADO" : "TRANCADO");
        
        Serial.println("\n📇 RFID:");
        Serial.printf("  Hardware: %s\n", 
            rfidHardwareConnected() ? "CONECTADO" : "DESCONECTADO");
        Serial.printf("  Cartões cadastrados: %d / %d\n", 
            rfidStorage.count(), MAX_RFID_CARDS);
        
        Serial.println("\n👆 BIOMETRIA:");
        Serial.printf("  Hardware: %s\n", 
            bioHardwareConnected() ? "CONECTADO" : "DESCONECTADO");
        Serial.printf("  Usuários cadastrados: %d / %d\n", 
            bioStorage.count(), MAX_FINGERPRINTS);
        Serial.printf("  Templates no sensor: %d\n", 
            bioSensorTemplateCount());
        
        Serial.println("\n💾 LITTLEFS:");
        Serial.printf("  Total: %d bytes\n", LittleFS.totalBytes());
        Serial.printf("  Usado: %d bytes\n", LittleFS.usedBytes());
        Serial.printf("  Livre: %d bytes\n", 
            LittleFS.totalBytes() - LittleFS.usedBytes());
        
        Serial.println("\n⚡ SISTEMA:");
        Serial.printf("  Uptime: %lu ms\n", millis());
        Serial.printf("  Free Heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("  PSRAM Free: %d bytes\n", ESP.getFreePsram());
        
        Serial.println("═══════════════════════════════════\n");
    }
    
    else if (cmd == "STATS") {
        Serial.printf("RFID: %d cartões\n", rfidStorage.count());
        Serial.printf("BIO: %d usuários\n", bioStorage.count());
    }
    
    else if (cmd == "VERSION") {
        Serial.println(PROJECT_NAME);
        Serial.println(FIRMWARE_VERSION);
        Serial.println(HARDWARE_MODEL);
    }
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDOS DE RELÉ
    // ═══════════════════════════════════════════════════════════════
    
    else if (cmd == "ABRIR") {
        relayController.unlock(5000);
        Serial.println("✅ Porta destrancada por 5 segundos");
    }
    
    else if (cmd.startsWith("ABRIR ")) {
        uint32_t time = cmd.substring(6).toInt();
        if (time > 0 && time <= 60000) {
            relayController.unlock(time);
            Serial.printf("✅ Porta destrancada por %lu ms\n", time);
        } else {
            Serial.println("❌ Tempo inválido (1-60000 ms)");
        }
    }
    
    else if (cmd == "FECHAR") {
        relayController.lock();
        Serial.println("✅ Porta trancada");
    }
    
    else if (cmd == "RELE_STATUS") {
        if (relayController.isUnlocked()) {
            Serial.println("🔓 Relé DESTRANCADO");
        } else {
            Serial.println("🔒 Relé TRANCADO");
        }
    }
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDOS RFID
    // ═══════════════════════════════════════════════════════════════
    
    else if (cmd == "LISTAR_RFID") {
        Serial.println("\n📇 CARTÕES RFID CADASTRADOS:");
        Serial.println("═══════════════════════════════════");
        
        std::vector<RFIDCard> cards = rfidStorage.getAllCards();
        
        if (cards.empty()) {
            Serial.println("Nenhum cartão cadastrado.");
        } else {
            for (size_t i = 0; i < cards.size(); i++) {
                const RFIDCard& card = cards[i];
                
                Serial.printf("\n[%d] %s\n", i+1, card.userName.c_str());
                Serial.printf("    UID: %s\n", card.uid.c_str());
                Serial.printf("    Acessos: %d\n", card.accessCount);
                Serial.printf("    Status: %s\n", 
                    card.active ? "ATIVO" : "INATIVO");
                
                if (card.lastAccess > 0) {
                    Serial.printf("    Último acesso: %lu\n", card.lastAccess);
                }
            }
        }
        Serial.println("═══════════════════════════════════\n");
    }
    
    else if (cmd == "ADD_RFID_TEST") {
        String testUID = "AA:BB:CC:DD";
        String testName = "Usuário Teste RFID";
        
        if (rfidStorage.addCard(testUID, testName)) {
            Serial.printf("✅ Cartão teste adicionado\n");
            Serial.printf("   UID: %s\n", testUID.c_str());
            Serial.printf("   Nome: %s\n", testName.c_str());
        } else {
            Serial.println("❌ Erro ao adicionar cartão teste");
        }
    }
    
    else if (cmd.startsWith("REMOVE_RFID ")) {
        String uid = cmd.substring(12);
        if (rfidStorage.removeCard(uid)) {
            Serial.printf("✅ Cartão %s removido\n", uid.c_str());
        } else {
            Serial.printf("❌ Cartão %s não encontrado\n", uid.c_str());
        }
    }
    
    else if (cmd == "CLEAR_RFID") {
        Serial.println("⚠️  TEM CERTEZA? Digite 'SIM' para confirmar:");
        delay(5000);
        
        if (Serial.available()) {
            String confirm = Serial.readStringUntil('\n');
            confirm.trim();
            confirm.toUpperCase();
            
            if (confirm == "SIM") {
                rfidStorage.clearAll();
                Serial.println("✅ Todos os cartões removidos");
            } else {
                Serial.println("❌ Operação cancelada");
            }
        } else {
            Serial.println("❌ Timeout - operação cancelada");
        }
    }
    
    else if (cmd == "EXPORT_RFID") {
        String json = rfidStorage.exportJSON();
        Serial.println("\n📤 EXPORT JSON - RFID:");
        Serial.println("═══════════════════════════════════");
        Serial.println(json);
        Serial.println("═══════════════════════════════════\n");
    }
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDOS BIOMETRIA
    // ═══════════════════════════════════════════════════════════════
    
    else if (cmd == "LISTAR_BIO") {
        Serial.println("\n👆 USUÁRIOS BIOMÉTRICOS CADASTRADOS:");
        Serial.println("═══════════════════════════════════");
        
        std::vector<BiometricUser> users = bioStorage.getAllUsers();
        
        if (users.empty()) {
            Serial.println("Nenhum usuário cadastrado.");
        } else {
            for (size_t i = 0; i < users.size(); i++) {
                const BiometricUser& user = users[i];
                
                Serial.printf("\n[%d] Slot %d - %s\n", 
                    i+1, user.slotId, user.userName.c_str());
                Serial.printf("    ID: %s\n", user.userId.c_str());
                Serial.printf("    Acessos: %d\n", user.accessCount);
                Serial.printf("    Confiança: %d/255\n", user.confidence);
                Serial.printf("    Status: %s\n", 
                    user.active ? "ATIVO" : "INATIVO");
                
                if (user.lastAccess > 0) {
                    Serial.printf("    Último acesso: %lu\n", user.lastAccess);
                }
            }
        }
        Serial.println("═══════════════════════════════════\n");
    }
    
    else if (cmd == "ADD_BIO_TEST") {
        uint16_t nextSlot = bioStorage.getNextFreeSlot();
        
        if (nextSlot > MAX_FINGERPRINTS) {
            Serial.println("❌ Memória cheia (127 slots)");
        } else {
            BiometricUser user;
            user.slotId = nextSlot;
            user.userId = "TEST" + String(nextSlot);
            user.userName = "Usuário Teste BIO";
            user.registeredAt = millis();
            user.lastAccess = 0;
            user.accessCount = 0;
            user.confidence = 0;
            user.active = true;
            
            if (bioStorage.addUser(user)) {
                Serial.printf("✅ Usuário teste adicionado\n");
                Serial.printf("   Slot: %d\n", user.slotId);
                Serial.printf("   Nome: %s\n", user.userName.c_str());
            } else {
                Serial.println("❌ Erro ao adicionar usuário teste");
            }
        }
    }
    
    else if (cmd.startsWith("REMOVE_BIO ")) {
        uint16_t slotId = cmd.substring(11).toInt();
        if (bioStorage.removeUser(slotId)) {
            Serial.printf("✅ Usuário do slot %d removido\n", slotId);
        } else {
            Serial.printf("❌ Slot %d não encontrado\n", slotId);
        }
    }
    
    else if (cmd == "CLEAR_BIO") {
        Serial.println("⚠️  TEM CERTEZA? Digite 'SIM' para confirmar:");
        delay(5000);
        
        if (Serial.available()) {
            String confirm = Serial.readStringUntil('\n');
            confirm.trim();
            confirm.toUpperCase();
            
            if (confirm == "SIM") {
                bioStorage.clearAll();
                Serial.println("✅ Todos os usuários removidos");
            } else {
                Serial.println("❌ Operação cancelada");
            }
        } else {
            Serial.println("❌ Timeout - operação cancelada");
        }
    }
    
    else if (cmd == "EXPORT_BIO") {
        String json = bioStorage.exportJSON();
        Serial.println("\n📤 EXPORT JSON - BIOMETRIA:");
        Serial.println("═══════════════════════════════════");
        Serial.println(json);
        Serial.println("═══════════════════════════════════\n");
    }
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDOS DE BACKUP
    // ═══════════════════════════════════════════════════════════════
    
    else if (cmd == "BACKUP") {
        Serial.println("🔄 Fazendo backup...");
        
        // Criar JSON consolidado
        DynamicJsonDocument doc(8192);
        doc["timestamp"] = millis();
        doc["version"] = FIRMWARE_VERSION;
        
        // RFID
        String rfidJSON = rfidStorage.exportJSON();
        doc["rfid"] = rfidJSON;
        
        // Biometria
        String bioJSON = bioStorage.exportJSON();
        doc["biometric"] = bioJSON;
        
        // Salvar arquivo
        File file = LittleFS.open("/backup.json", "w");
        if (file) {
            serializeJson(doc, file);
            file.close();
            Serial.println("✅ Backup salvo em /backup.json");
        } else {
            Serial.println("❌ Erro ao salvar backup");
        }
    }
    
    else if (cmd == "RESTORE") {
        Serial.println("🔄 Restaurando backup...");
        
        File file = LittleFS.open("/backup.json", "r");
        if (!file) {
            Serial.println("❌ Arquivo de backup não encontrado");
            return;
        }
        
        String content = file.readString();
        file.close();
        
        DynamicJsonDocument doc(8192);
        deserializeJson(doc, content);
        
        // Restaurar RFID
        String rfidJSON = doc["rfid"];
        if (rfidStorage.importJSON(rfidJSON)) {
            Serial.println("✅ RFID restaurado");
        }
        
        // Restaurar Biometria
        String bioJSON = doc["biometric"];
        if (bioStorage.importJSON(bioJSON)) {
            Serial.println("✅ Biometria restaurada");
        }
        
        Serial.println("✅ Backup restaurado!");
    }
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDOS DE DEBUG
    // ═══════════════════════════════════════════════════════════════
    
    else if (cmd == "TEST_PN532") {
        Serial.println("🧪 Testando PN532...");
        if (rfidHardwareConnected()) {
            Serial.println("✅ PN532 respondendo");
        } else {
            Serial.println("❌ PN532 não responde");
            Serial.println("   Verificar:");
            Serial.println("   - Pinagem (GPIO21/47)");
            Serial.println("   - DIP Switches (CH1=OFF, CH2=ON)");
            Serial.println("   - Alimentação 3.3V");
        }
    }
    
    else if (cmd == "TEST_AS608") {
        Serial.println("🧪 Testando AS608...");
        if (bioHardwareConnected()) {
            Serial.println("✅ AS608 respondendo");
            Serial.printf("   Templates: %d\n", 
                bioSensorTemplateCount());
        } else {
            Serial.println("❌ AS608 não responde");
            Serial.println("   Verificar:");
            Serial.println("   - Pinagem (GPIO16/15)");
            Serial.println("   - Baudrate (57600)");
            Serial.println("   - Alimentação 3.3V");
        }
    }
    
    else if (cmd == "FORMAT_LITTLEFS") {
        Serial.println("⚠️  FORMATAR LITTLEFS? Digite 'SIM' para confirmar:");
        delay(5000);
        
        if (Serial.available()) {
            String confirm = Serial.readStringUntil('\n');
            confirm.trim();
            confirm.toUpperCase();
            
            if (confirm == "SIM") {
                Serial.println("🔄 Formatando...");
                LittleFS.format();
                Serial.println("✅ LittleFS formatado");
                Serial.println("⚠️  TODOS OS DADOS FORAM APAGADOS!");
            } else {
                Serial.println("❌ Operação cancelada");
            }
        } else {
            Serial.println("❌ Timeout - operação cancelada");
        }
    }
    
    else if (cmd == "REBOOT") {
        Serial.println("🔄 Reiniciando ESP32 em 3 segundos...");
        delay(3000);
        ESP.restart();
    }
    
    // ═══════════════════════════════════════════════════════════════
    // COMANDO INVÁLIDO
    // ═══════════════════════════════════════════════════════════════
    
    else {
        Serial.printf("❌ Comando '%s' não reconhecido\n", cmd.c_str());
        Serial.println("   Digite 'HELP' para ver comandos disponíveis\n");
    }
}
