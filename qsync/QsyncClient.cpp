#include "QsyncWire.h"

#include <android/binder_auto_utils.h>
#include <android/binder_manager.h>
#include <android/log.h>

#include <cerrno>
#include <chrono>
#include <mutex>

using namespace houji::qsync;
namespace {
void* create(void* data) { return data; }
void destroy(void*) {}
binder_status_t unused(AIBinder*, transaction_code_t, const AParcel*, AParcel*) {
    return STATUS_UNKNOWN_TRANSACTION;
}
class Client {
public:
    int notify(unsigned display) {
        if (display != 0) return -EINVAL;
        ndk::SpAIBinder target;
        {
            std::lock_guard lock(mutex_);
            if (!service_.get()) {
                const auto now = std::chrono::steady_clock::now();
                if (now < nextLookup_) return -EAGAIN;
                nextLookup_ = now + std::chrono::milliseconds(500);
                service_.set(AServiceManager_checkService(kTimerService));
                static AIBinder_Class* clazz =
                    AIBinder_Class_define(kTimerDescriptor, create, destroy, unused);
                if (service_.get() && !AIBinder_associateClass(service_.get(), clazz))
                    service_.set(nullptr);
            }
            target = service_;
        }
        if (!target.get()) return -ENODEV;
        ndk::ScopedAParcel in, out;
        auto status = AIBinder_prepareTransaction(target.get(), in.getR());
        if (status == STATUS_OK) status = AParcel_writeInt32(in.get(), 0);
        if (status == STATUS_OK)
            status = AIBinder_transact(target.get(), kNotifyBacklight, in.getR(), out.getR(), 0);
        int32_t result = -EIO;
        if (status == STATUS_OK) status = AParcel_readInt32(out.get(), &result);
        if (status != STATUS_OK) {
            std::lock_guard lock(mutex_);
            if (service_.get() == target.get()) service_.set(nullptr);
            // Do not replay a possibly executed transaction; the next real
            // brightness request reconnects after the lookup backoff.
            return status;
        }
        return result;
    }
private:
    std::mutex mutex_;
    ndk::SpAIBinder service_;
    std::chrono::steady_clock::time_point nextLookup_{};
};
}

extern "C" int houji_qsync_client_notify(unsigned display) {
    static Client client;
    return client.notify(display);
}

extern "C" int qsyncTimerRequest(void*, unsigned)
    asm("_ZN7android13DisplayEffect13SetQsyncTimerEj");
int qsyncTimerRequest(void*, unsigned display) {
    const int result = houji_qsync_client_notify(display);
    if (result) {
        // Limit diagnostics during startup/service recovery, never turn an
        // unsuccessful operation into a successful return value.
        static std::mutex mutex;
        static auto next = std::chrono::steady_clock::time_point{};
        std::lock_guard lock(mutex);
        const auto now = std::chrono::steady_clock::now();
        if (now >= next) {
            __android_log_print(ANDROID_LOG_ERROR, "HoujiQsync", "timer request failed=%d", result);
            next = now + std::chrono::seconds(5);
        }
    }
    return result;
}
