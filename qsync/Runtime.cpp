// Houji product adapter. ABI pinned to the verified SDM implementation.
#include "QsyncCoordinator.h"
#ifndef HOUJI_QSYNC_FAKE_SDM_TEST
#include "QsyncWire.h"
#endif

#include <android/log.h>
#include <dlfcn.h>
#include <link.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

using namespace houji::qsync;
using namespace std::chrono_literals;

extern "C" int runtimeMode(void*, int) asm("_ZN3sdm14DisplayBuiltIn18SetQSyncModeLockedENS_9QSyncModeE");
extern "C" int runtimePower(void*, int, bool, void*) asm("_ZN3sdm14DisplayBuiltIn15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE");
extern "C" int runtimeBasePower(void*, int, bool, void*) asm("_ZN3sdm11DisplayBase15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE");
extern "C" int runtimeInit(void*) asm("_ZN3sdm14DisplayBuiltIn4InitEv");
extern "C" int runtimeDeinit(void*) asm("_ZN3sdm14DisplayBuiltIn6DeinitEv");
extern "C" int houji_qsync_abi_supported();

namespace {
constexpr char kLocked[] = "_ZN3sdm14DisplayBuiltIn18SetQSyncModeLockedENS_9QSyncModeE";
constexpr char kPublic[] = "_ZN3sdm14DisplayBuiltIn12SetQSyncModeENS_9QSyncModeE";
constexpr char kPower[] = "_ZN3sdm14DisplayBuiltIn15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE";
constexpr char kBasePower[] = "_ZN3sdm11DisplayBase15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE";
constexpr char kGetPower[] = "_ZN3sdm11DisplayBase15GetDisplayStateEPNS_12DisplayStateE";
constexpr char kGetMode[] = "_ZN3sdm14DisplayBuiltIn12GetQSyncModeEPNS_9QSyncModeE";
constexpr char kGetId[] = "_ZN3sdm11DisplayBase12GetDisplayIdEPi";
constexpr char kIsPrimary[] = "_ZN3sdm11DisplayBase16IsPrimaryDisplayEv";
constexpr char kInit[] = "_ZN3sdm14DisplayBuiltIn4InitEv";
constexpr char kDeinit[] = "_ZN3sdm14DisplayBuiltIn6DeinitEv";
constexpr char kDestroy[] = "_ZN3sdm8CoreImpl14DestroyDisplayEPNS_16DisplayInterfaceE";

template <typename F> F original(const char* name) {
    auto fn = reinterpret_cast<F>(dlsym(RTLD_NEXT, name));
    if (!fn) {
        __android_log_print(ANDROID_LOG_FATAL, "HoujiQsync", "missing original: %s", name);
        std::abort();
    }
    return fn;
}

bool supportedAbi() {
#ifdef HOUJI_QSYNC_FAKE_SDM_TEST
    return true;
#else
    static const bool supported = [] {
        struct Search { void* base; bool matches = false; };
        Dl_info info{};
        if (!dladdr(reinterpret_cast<void*>(original<int (*)(void*, int)>(kLocked)), &info))
            return false;
        Search search{info.dli_fbase};
        dl_iterate_phdr([](dl_phdr_info* loaded, size_t, void* opaque) {
            auto& s = *static_cast<Search*>(opaque);
            if (loaded->dlpi_addr != reinterpret_cast<uintptr_t>(s.base)) return 0;
            constexpr unsigned char expected[] = {
                0x30, 0x71, 0x3b, 0x1f, 0xf3, 0xe1, 0xc1, 0x3e,
                0x86, 0x0a, 0x4d, 0x7c, 0x38, 0x28, 0x28, 0x20};
            for (unsigned i = 0; i < loaded->dlpi_phnum; ++i) {
                const auto& ph = loaded->dlpi_phdr[i];
                if (ph.p_type != PT_NOTE) continue;
                auto* p = reinterpret_cast<const unsigned char*>(loaded->dlpi_addr + ph.p_vaddr);
                size_t left = ph.p_memsz;
                while (left >= sizeof(ElfW(Nhdr))) {
                    ElfW(Nhdr) n;
                    std::memcpy(&n, p, sizeof(n));
                    p += sizeof(n); left -= sizeof(n);
                    const size_t names = (size_t(n.n_namesz) + 3) & ~size_t(3);
                    const size_t desc = (size_t(n.n_descsz) + 3) & ~size_t(3);
                    if (names > left || desc > left - names) break;
                    if (n.n_type == NT_GNU_BUILD_ID && n.n_namesz == 4 &&
                        std::memcmp(p, "GNU", 4) == 0 && n.n_descsz == sizeof(expected) &&
                        std::memcmp(p + names, expected, sizeof(expected)) == 0) s.matches = true;
                    p += names + desc; left -= names + desc;
                }
            }
            return 1;
        }, &search);
        if (!search.matches) return false;
        Dl_info self{}, replacement{};
        if (!dladdr(reinterpret_cast<void*>(houji_qsync_abi_supported), &self) ||
            !dladdr(reinterpret_cast<void*>(runtimeMode), &replacement) ||
            self.dli_fbase != replacement.dli_fbase || self.dli_fbase == info.dli_fbase)
            return false;
        struct Binding { size_t offset; void* target; };
        const Binding bindings[] = {
            {0xab8d0, reinterpret_cast<void*>(runtimeMode)},
            {0xab3c0, reinterpret_cast<void*>(runtimeBasePower)},
            {0xa8790, reinterpret_cast<void*>(runtimePower)},
            {0xa8aa0, reinterpret_cast<void*>(runtimeInit)},
            {0xa8aa8, reinterpret_cast<void*>(runtimeDeinit)},
        };
        for (const auto& binding : bindings) {
            void* actual;
            std::memcpy(&actual, static_cast<char*>(info.dli_fbase) + binding.offset, sizeof(actual));
            if (actual != binding.target) return false;
        }
        return true;
    }();
    return supported;
#endif
}

struct SdmBackend : Backend {
    explicit SdmBackend(void* p) : object(p) {}
    int apply(int mode) override {
        static const auto call = original<int (*)(void*, int)>(kLocked);
        const int status = call(object, mode);
        __android_log_print(status ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, "HoujiQsync",
                            "apply mode=%d status=%d", mode, status);
        return status;
    }
    void* object;
};

struct Entry {
    explicit Entry(void* p) : lifetime(p), backend(p), coordinator(backend, 5s) {}
    DisplayLifetime lifetime;
    SdmBackend backend;
    Coordinator coordinator;  // SDM locks only; never access from the timer mutex.
    std::atomic<uint64_t> requests{0}, expirations{0};
    std::atomic<int> lastStatus{0}, cleanupStatus{0}, mode{-1};
    std::atomic<unsigned> recoveryAttempts{0};
};

enum class Operation { Backlight, Expire, Recover };
struct Command { Entry* entry; Operation operation; uint64_t generation; bool handled = false; };
thread_local Command* command = nullptr;
thread_local void* powerObject = nullptr;
template <typename T> class ScopedValue {
public:
    ScopedValue(T& slot, T value) : slot_(slot), previous_(slot) { slot_ = value; }
    ~ScopedValue() { slot_ = previous_; }
private:
    T& slot_;
    T previous_;
};

int execute(const std::shared_ptr<Entry>& entry, Operation operation, uint64_t generation = 0) {
    auto lease = entry->lifetime.acquire();
    if (!lease) return -ENODEV;
    Command request{entry.get(), operation, generation};
    const ScopedValue scope(command, &request);
    static const auto call = original<int (*)(void*, int)>(kPublic);
    // This argument is intercepted, never used as a mode request by the adapter.
    // Public SDM enters its own locks and waits for commit before reaching Locked.
    const int status = call(lease.object(), 0);
    if (!request.handled) {
        __android_log_print(ANDROID_LOG_FATAL, "HoujiQsync", "locked entry was not intercepted");
        std::abort();  // Deployment must verify the PLT binding before enabling IPC.
    }
    return status;
}

class Registry {
public:
    Registry() { std::thread([this] { timerLoop(); }).detach(); }
    std::shared_ptr<Entry> find(void* object) {
        std::lock_guard lock(mutex_);
        const auto it = entries_.find(object);
        return it == entries_.end() ? nullptr : it->second;
    }
    std::shared_ptr<Entry> primary() {
        std::lock_guard lock(mutex_);
        return entries_.size() == 1 ? entries_.begin()->second : nullptr;
    }
    void add(void* object, const std::shared_ptr<Entry>& entry) {
        std::lock_guard lock(mutex_);
        entries_.emplace(object, entry);
    }
    void remove(void* object) {
        std::shared_ptr<Entry> entry;
        {
            std::lock_guard lock(mutex_);
            const auto it = entries_.find(object);
            if (it == entries_.end()) return;
            entry = it->second;
        }
        // Never wait for a lease under registry/SDM locks.
        // Keep the entry visible until in-flight public calls reach Locked.
        // Erasing it first would forward their placeholder mode to the backend.
        entry->lifetime.close();
        std::lock_guard lock(mutex_);
        const auto it = entries_.find(object);
        if (it != entries_.end() && it->second == entry) entries_.erase(it);
        deadlines_.erase(entry.get());
        cv_.notify_one();
    }
    void publish(const std::shared_ptr<Entry>& entry) {
        // Caller holds SDM locks. Worker releases mutex_ before entering SDM.
        const auto next = entry->coordinator.deadline();
        entry->mode.store(entry->coordinator.requestedMode().value_or(-1));
        std::lock_guard lock(mutex_);
        if (entry->lifetime.closing()) return;
        if (next) {
            deadlines_[entry.get()] = Scheduled{entry, *next, Operation::Expire};
        } else if (entry->coordinator.ownsBacklightMode() && entry->cleanupStatus.load() &&
                   entry->recoveryAttempts.load() < 8) {
            const auto generation = entry->coordinator.generation();
            const auto it = deadlines_.find(entry.get());
            // Repeated brightness notifications must not keep pushing recovery
            // into the future. Only a new generation starts a new retry delay.
            if (it == deadlines_.end() || it->second.operation != Operation::Recover ||
                it->second.deadline.generation != generation) {
                deadlines_[entry.get()] = Scheduled{entry,
                    Deadline{generation, Coordinator::Clock::now() + 250ms}, Operation::Recover};
            }
        } else {
            deadlines_.erase(entry.get());
        }
        cv_.notify_one();
    }
private:
    struct Scheduled { std::weak_ptr<Entry> entry; Deadline deadline; Operation operation; };
    void timerLoop() {
        std::unique_lock lock(mutex_);
        for (;;) {
            if (deadlines_.empty()) { cv_.wait(lock); continue; }
            const auto first = std::min_element(deadlines_.begin(), deadlines_.end(),
                [](const auto& a, const auto& b) { return a.second.deadline.at < b.second.deadline.at; });
            const auto when = first->second.deadline.at;
            if (Coordinator::Clock::now() < when) { cv_.wait_until(lock, when); continue; }
            const auto scheduled = first->second;
            deadlines_.erase(first);
            auto entry = scheduled.entry.lock();
            lock.unlock();
            if (entry) execute(entry, scheduled.operation, scheduled.deadline.generation);
            lock.lock();
        }
    }
    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<void*, std::shared_ptr<Entry>> entries_;
    std::map<Entry*, Scheduled> deadlines_;
};
Registry& registry() {
    // Process lifetime; avoids joining a display worker during static destruction.
    static Registry* instance = new Registry;
    return *instance;
}

void recover(const std::shared_ptr<Entry>& entry) {
    const int status = entry->coordinator.clearBacklightOverride();
    entry->cleanupStatus.store(status);
    if (status) entry->recoveryAttempts.fetch_add(1);
    else entry->recoveryAttempts.store(0);
    if (status) __android_log_print(ANDROID_LOG_ERROR, "HoujiQsync", "override cleanup failed: %d", status);
}
}  // namespace

extern "C" int houji_qsync_abi_supported() { return supportedAbi() ? 1 : 0; }

extern "C" int houji_qsync_notify(unsigned displayId) {
    if (displayId != 0) return -EINVAL;
    auto entry = registry().primary();
    if (!entry) return -ENODEV;
    entry->requests.fetch_add(1);
    return execute(entry, Operation::Backlight);
}

extern "C" int houji_qsync_dump(int fd) {
    auto entry = registry().primary();
    if (!entry) return -ENODEV;
    return dprintf(fd, "requests=%llu expirations=%llu mode=%d status=%d cleanup=%d recovery_attempts=%u\n",
                   static_cast<unsigned long long>(entry->requests.load()),
                   static_cast<unsigned long long>(entry->expirations.load()), entry->mode.load(),
                   entry->lastStatus.load(), entry->cleanupStatus.load(),
                   entry->recoveryAttempts.load()) < 0 ? -errno : 0;
}

int runtimeMode(void* object, int mode) {
    static const auto call = original<int (*)(void*, int)>(kLocked);
    auto entry = registry().find(object);
    if (!entry) return call(object, mode);
    int status;
    if (command && command->entry == entry.get()) {
        command->handled = true;
        if (command->operation == Operation::Recover &&
            command->generation != entry->coordinator.generation()) return 0;
        static const auto getPower = original<int (*)(void*, int*)>(kGetPower);
        int state = -1;
        status = getPower(object, &state);
        if (status || state != 1) {
            if (entry->coordinator.isOn()) entry->coordinator.powerChanged(false);
            if (command->operation == Operation::Recover || !entry->cleanupStatus.load())
                recover(entry);
        } else if (command->operation == Operation::Recover) {
            recover(entry);
            status = entry->cleanupStatus.load();
        } else if (command->operation == Operation::Backlight) {
            status = entry->coordinator.backlight(Coordinator::Clock::now());
            if (status && !entry->cleanupStatus.load()) recover(entry);
        } else {
            entry->expirations.fetch_add(1);
            status = entry->coordinator.expire(command->generation, Coordinator::Clock::now());
            if (status) recover(entry);
        }
    } else {
        entry->recoveryAttempts.store(0);
        status = entry->coordinator.externalMode(mode);
        if (status) recover(entry);
        else entry->cleanupStatus.store(0);
    }
    entry->lastStatus.store(status);
    registry().publish(entry);
    return status;
}

int runtimePower(void* object, int state, bool teardown, void* fence) {
    static const auto call = original<int (*)(void*, int, bool, void*)>(kPower);
    const ScopedValue scope(powerObject, object);
    return call(object, state, teardown, fence);
}

int runtimeBasePower(void* object, int state, bool teardown, void* fence) {
    static const auto call = original<int (*)(void*, int, bool, void*)>(kBasePower);
    auto entry = powerObject == object ? registry().find(object) : nullptr;
    if (!entry) return call(object, state, teardown, fence);
    entry->recoveryAttempts.store(0);
    entry->coordinator.powerChanged(false);  // Invalidate old tickets under SDM locks.
    recover(entry);
    const auto requested = entry->coordinator.requestedMode();
    if (state != 1 && requested && *requested != 0) {
        const int clear = entry->coordinator.externalMode(0);
        entry->cleanupStatus.store(clear);
        if (clear) __android_log_print(ANDROID_LOG_ERROR, "HoujiQsync", "power mode cleanup failed: %d", clear);
    }
    const int status = call(object, state, teardown, fence);
    if (!status) entry->coordinator.powerChanged(state == 1);
    // On failure leave timer disarmed, but do not hide the real power error.
    entry->lastStatus.store(status);
    registry().publish(entry);
    return status;
}

int runtimeInit(void* object) {
    static const auto call = original<int (*)(void*)>(kInit);
    const int status = call(object);
    if (status) return status;
    if (!supportedAbi()) {
        __android_log_print(ANDROID_LOG_ERROR, "HoujiQsync", "incompatible SDM ABI or startup binding; not attached");
        return status;
    }
    static const auto getId = original<int (*)(void*, int*)>(kGetId);
    static const auto isPrimary = original<bool (*)(void*)>(kIsPrimary);
    int id = -1;
    // SDM uses the DRM connector ID (67 on this panel), not Android display 0.
    // Route the sole primary built-in display through the logical IPC ID 0.
    if (getId(object, &id) || !isPrimary(object)) return status;
    auto entry = std::make_shared<Entry>(object);
    // No client can submit a frame to this new object before Init returns.
    // Verify the pending mode in this exact ELF as well as the committed getter.
    int pending = -1, committed = -1, power = -1;
#ifdef HOUJI_QSYNC_FAKE_SDM_TEST
    pending = 0;
#else
    std::memcpy(&pending, static_cast<char*>(object) + 0x41eac, sizeof(pending));
#endif
    static const auto getMode = original<int (*)(void*, int*)>(kGetMode);
    static const auto getPower = original<int (*)(void*, int*)>(kGetPower);
    if (pending == 0 && !getMode(object, &committed) && committed == 0 &&
        !getPower(object, &power) && power == 0) entry->coordinator.initializedOff();
    registry().add(object, entry);
#ifndef HOUJI_QSYNC_FAKE_SDM_TEST
    startQsyncService();
#endif
    __android_log_print(ANDROID_LOG_INFO, "HoujiQsync", "attached display=%d initial_pending=%d committed=%d power=%d", id, pending, committed, power);
    return status;
}

int runtimeDeinit(void* object) {
    static const auto call = original<int (*)(void*)>(kDeinit);
    registry().remove(object);
    return call(object);
}

extern "C" int runtimeDestroy(void*, void*) asm("_ZN3sdm8CoreImpl14DestroyDisplayEPNS_16DisplayInterfaceE");
int runtimeDestroy(void* core, void* object) {
    static const auto call = original<int (*)(void*, void*)>(kDestroy);
    registry().remove(object);
    return call(core, object);
}
