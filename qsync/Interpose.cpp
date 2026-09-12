// ABI feasibility probe only. Does not change modes or create a timer.
// Signatures are pinned to the investigated houji libsdmcore.so.
#include <android/log.h>
#include <dlfcn.h>

#include <atomic>
#include <cerrno>
#include <cstdlib>

namespace {
constexpr char kSetMode[] =
        "_ZN3sdm14DisplayBuiltIn18SetQSyncModeLockedENS_9QSyncModeE";
constexpr char kSetPower[] =
        "_ZN3sdm14DisplayBuiltIn15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE";
constexpr char kDestroy[] = "_ZN3sdm8CoreImpl14DestroyDisplayEPNS_16DisplayInterfaceE";
constexpr char kBasePower[] =
        "_ZN3sdm11DisplayBase15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE";

// Only a Base call nested in the matching BuiltIn entry is known to have
// acquired BuiltIn's two locks. Do not assume other Base callers hold them.
thread_local void* powerObject = nullptr;
class PowerScope {
public:
    explicit PowerScope(void* object) : previous_(powerObject) { powerObject = object; }
    ~PowerScope() { powerObject = previous_; }
private:
    void* previous_;
};

template <typename F>
F original(const char* name) {
    void* symbol = dlsym(RTLD_NEXT, name);
    if (!symbol) {
        __android_log_print(ANDROID_LOG_FATAL, "HoujiQsyncProbe",
                            "Missing original symbol %s: %s", name, dlerror());
        std::abort();
    }
    return reinterpret_cast<F>(symbol);
}

void report(const char* operation, int value, int result, std::atomic<unsigned>& count) {
    const int savedErrno = errno;
    if (count.fetch_add(1, std::memory_order_relaxed) < 8) {
        __android_log_print(ANDROID_LOG_INFO, "HoujiQsyncProbe", "%s value=%d result=%d",
                            operation, value, result);
    }
    errno = savedErrno;
}
}  // namespace

extern "C" int forwardMode(void*, int)
        asm("_ZN3sdm14DisplayBuiltIn18SetQSyncModeLockedENS_9QSyncModeE");
extern "C" int forwardPower(void*, int, bool, void*)
        asm("_ZN3sdm14DisplayBuiltIn15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE");
extern "C" int forwardDestroy(void*, void*)
        asm("_ZN3sdm8CoreImpl14DestroyDisplayEPNS_16DisplayInterfaceE");
extern "C" int forwardBasePower(void*, int, bool, void*)
        asm("_ZN3sdm11DisplayBase15SetDisplayStateENS_12DisplayStateEbPNSt3__110shared_ptrINS_5FenceEEE");

int forwardMode(void* object, int mode) {
    static const auto call = original<int (*)(void*, int)>(kSetMode);
    static std::atomic<unsigned> count{0};
    const int result = call(object, mode);
    report("mode", mode, result, count);
    return result;
}

int forwardPower(void* object, int state, bool teardown, void* fence) {
    static const auto call = original<int (*)(void*, int, bool, void*)>(kSetPower);
    static std::atomic<unsigned> count{0};
    const PowerScope scope(object);
    const int result = call(object, state, teardown, fence);
    report("power", state, result, count);
    return result;
}

int forwardBasePower(void* object, int state, bool teardown, void* fence) {
    static const auto call = original<int (*)(void*, int, bool, void*)>(kBasePower);
    static std::atomic<unsigned> nestedCount{0}, otherCount{0};
    const bool nested = powerObject == object;
    const int result = call(object, state, teardown, fence);
    report(nested ? "power-under-builtin-locks" : "power-other-caller", state, result,
           nested ? nestedCount : otherCount);
    return result;
}

int forwardDestroy(void* core, void* display) {
    static const auto call = original<int (*)(void*, void*)>(kDestroy);
    static std::atomic<unsigned> count{0};
    const int result = call(core, display);
    report("destroy", 0, result, count);
    return result;
}
