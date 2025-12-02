/**
 * @file storage_init.h
 * @brief Interface para inicialização dos sistemas de armazenamento
 * @version 1.0.0
 * @date 2025-11-27
 * 
 * Fornece funções de inicialização dos storages sem incluir suas definições,
 * evitando conflitos de estruturas com os managers.
 */

#ifndef STORAGE_INIT_H
#define STORAGE_INIT_H

/**
 * @brief Inicializa o storage RFID
 * @return true se inicializado com sucesso
 */
bool initRfidStorage();

/**
 * @brief Inicializa o storage biométrico
 * @return true se inicializado com sucesso
 */
bool initBioStorage();

#endif // STORAGE_INIT_H
