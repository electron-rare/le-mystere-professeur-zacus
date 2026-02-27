// RTOSManager.h
// Gestion des tâches FreeRTOS

#ifndef RTOSMANAGER_H
#define RTOSMANAGER_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>

class RTOSManager {
public:
    RTOSManager();
    bool begin();
    bool createTask(const char* name, void (*taskFunc)(void*), uint16_t stackSize, void* params, UBaseType_t priority);
    void startScheduler();
    void auditTasks();
    void logStatus();
    void enableWatchdog(uint32_t timeoutMs);
    void feedWatchdog();
private:
    bool initialized = false;
    bool watchdogEnabled = false;
    uint32_t watchdogTimeout = 0;
};

#endif // RTOSMANAGER_H
