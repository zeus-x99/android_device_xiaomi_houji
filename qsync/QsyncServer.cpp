#include "QsyncWire.h"

#include <android/binder_manager.h>
#include <android/binder_auto_utils.h>
#include <android/binder_process.h>
#include <android/log.h>
#include <dlfcn.h>

#include <cerrno>
#include <cstdlib>
#include <mutex>

extern "C" int houji_qsync_notify(unsigned);
extern "C" int houji_qsync_dump(int);

namespace houji::qsync {
namespace {
struct ServiceState {
    std::mutex mutex;
    bool displayReady = false;
    bool poolReady = false;
    ndk::SpAIBinder service;
};
ServiceState& serviceState() {
    static auto* state = new ServiceState;
    return *state;
}
void* create(void* data) { return data; }
void destroy(void*) {}
binder_status_t transact(AIBinder*, transaction_code_t code, const AParcel* in, AParcel* out) {
    const uid_t uid = AIBinder_getCallingUid();
    // DisplayFeature runs as system. Root is for bounded device diagnostics;
    // SELinux service lookup/binder rules independently restrict callers.
    if (uid != 1000 && uid != 0) return STATUS_PERMISSION_DENIED;
    if (code != kNotifyBacklight) return STATUS_UNKNOWN_TRANSACTION;
    int32_t display = -1;
    const auto status = AParcel_readInt32(in, &display);
    if (status != STATUS_OK) return status;
    const int result = display == 0 ? houji_qsync_notify(0) : -EINVAL;
    return AParcel_writeInt32(out, result);
}
binder_status_t dump(AIBinder*, int fd, const char**, uint32_t) {
    const uid_t uid = AIBinder_getCallingUid();
    if (uid != 0 && uid != 1000 && uid != 2000) return STATUS_PERMISSION_DENIED;
    return houji_qsync_dump(fd) == 0 ? STATUS_OK : STATUS_NAME_NOT_FOUND;
}
void registerIfReady(ServiceState& state) {
    if (!state.displayReady || !state.poolReady || state.service.get()) return;
    static AIBinder_Class* clazz = [] {
        auto* value = AIBinder_Class_define(kTimerDescriptor, create, destroy, transact);
        AIBinder_Class_setOnDump(value, dump);
        return value;
    }();
    ndk::SpAIBinder candidate(AIBinder_new(clazz, nullptr));
    const auto status = AServiceManager_addService(candidate.get(), kTimerService);
    if (status == STATUS_OK) state.service = std::move(candidate);
    __android_log_print(status == STATUS_OK ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR,
                        "HoujiQsync", "timer service registration status=%d", status);
}
}

void startQsyncService() {
    auto& state = serviceState();
    std::lock_guard lock(state.mutex);
    state.displayReady = true;
    registerIfReady(state);
}

void qsyncThreadPoolStarted() {
    auto& state = serviceState();
    std::lock_guard lock(state.mutex);
    state.poolReady = true;
    registerIfReady(state);
}
}  // namespace houji::qsync

// Observe the original service's startup, without changing its thread count
// or starting an additional pool before the original main is ready.
void ABinderProcess_startThreadPool() {
    static const auto original = reinterpret_cast<void (*)()>(
        dlsym(RTLD_NEXT, "ABinderProcess_startThreadPool"));
    if (!original) std::abort();
    original();
    houji::qsync::qsyncThreadPoolStarted();
}
