#pragma once
#include <inttypes.h>

extern uint8_t cpuCount;

void savePersistenceFile(uint32_t id, uint64_t hash);
void loadPersistenceFile();
void socketMain(int descriptor);
void socketServer(const char *listenAddr, int listenPort);
void forkDaemon();
void startSystem();
