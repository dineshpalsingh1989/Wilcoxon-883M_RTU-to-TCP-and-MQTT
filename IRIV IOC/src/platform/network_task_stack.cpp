#include "platform/platform.h"

#if GATEWAY_ETHERNET
// Arduino-Pico 6.1 creates CORE0 with only 4 KB of heap-backed stack. The
// gateway needs more space, and RP2350 PSPLIM must allow BearSSL's heap-backed
// alternate stack. Keep the actual Arduino setup/loop task, but allocate its
// stack in static RAM below the heap. All other tasks keep their original API.
// Linked with --wrap=xTaskCreate; no SDK sources are modified.
namespace {
StackType_t networkStack[32768 / sizeof(StackType_t)];
StaticTask_t networkTask;
bool allocated = false;
}
extern "C" BaseType_t __real_xTaskCreate(TaskFunction_t, const char*,
    configSTACK_DEPTH_TYPE, void*, UBaseType_t, TaskHandle_t*);
extern "C" BaseType_t __wrap_xTaskCreate(TaskFunction_t fn, const char* name,
    configSTACK_DEPTH_TYPE depth, void* arg, UBaseType_t priority, TaskHandle_t* handle) {
    if (name && strcmp(name, "CORE0") == 0) {
        if (allocated || depth > sizeof(networkStack) / sizeof(StackType_t)) return errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
        allocated = true;
        TaskHandle_t created = xTaskCreateStatic(fn, name,
            sizeof(networkStack) / sizeof(StackType_t), arg, priority, networkStack, &networkTask);
        if (handle) *handle = created;
        return created ? pdPASS : errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY;
    }
    return __real_xTaskCreate(fn, name, depth, arg, priority, handle);
}
#endif
