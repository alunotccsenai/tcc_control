/**
 * @file serial_commands.h
 * @brief Sistema de comandos Serial para debug e testes
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * Permite controlar e testar o sistema via Serial Monitor
 */

#ifndef SERIAL_COMMANDS_H
#define SERIAL_COMMANDS_H

/**
 * @brief Processa comandos recebidos via Serial
 * 
 * Deve ser chamado no loop() principal.
 * 
 * Comandos disponíveis:
 * - HELP               - Lista todos os comandos
 * - STATUS             - Status de todos os sistemas
 * - STATS              - Estatísticas resumidas
 * - ABRIR              - Destranca porta (5s)
 * - FECHAR             - Tranca porta
 * - LISTAR_RFID        - Lista cartões cadastrados
 * - LISTAR_BIO         - Lista usuários biométricos
 * - ADD_RFID_TEST      - Adiciona cartão de teste
 * - ADD_BIO_TEST       - Adiciona usuário de teste
 * - BACKUP             - Faz backup completo
 * - RESTORE            - Restaura backup
 * - TEST_PN532         - Testa PN532
 * - TEST_AS608         - Testa AS608
 * - FORMAT_LITTLEFS    - Formata LittleFS (CUIDADO!)
 * - REBOOT             - Reinicia ESP32
 * 
 * Exemplo de uso:
 * 
 * ```cpp
 * #include "serial_commands.h"
 * 
 * void loop() {
 *     processSerialCommands();
 *     // ... resto do código
 * }
 * ```
 */
void processSerialCommands();

#endif // SERIAL_COMMANDS_H
