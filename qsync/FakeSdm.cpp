#include "FakeSdm.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>

using namespace std::chrono_literals;
namespace {
struct Fake {
    std::recursive_mutex outer, inner;
    int state = 0, requested = 0, reject = -1, remaining = 0;
    bool backlight = false, destroyed = false;
    bool primary = true;
    std::mutex pauseMutex;
    std::condition_variable pauseCv;
    bool pauseNext = false, paused = false, resume = false;
};
}

void* fakeCreate() { return new Fake; }
void fakeDelete(void* p) { delete static_cast<Fake*>(p); }
int sdmInit(void*) { return 0; }

__attribute__((noinline)) int sdmLocked(void* p, int mode) {
    auto& f = *static_cast<Fake*>(p);
    if (f.destroyed) std::abort();
    if (mode == f.reject && f.remaining > 0) { --f.remaining; return 2; }
    f.requested = mode;
    f.backlight = mode == 4;
    return 0;
}

__attribute__((noinline)) int sdmMode(void* p, int mode) {
    auto& f = *static_cast<Fake*>(p);
    {
        std::unique_lock lock(f.pauseMutex);
        if (f.pauseNext) {
            f.pauseNext = false;
            f.paused = true;
            f.pauseCv.notify_all();
            f.pauseCv.wait(lock, [&] { return f.resume; });
            f.paused = false;
        }
    }
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    return sdmLocked(p, mode);
}

__attribute__((noinline)) int sdmBasePower(void* p, int state, bool, void*) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    f.state = state;
    return 0;
}

__attribute__((noinline)) int sdmPower(void* p, int state, bool teardown, void* fence) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    return sdmBasePower(p, state, teardown, fence);
}

int sdmGetPower(void* p, int* state) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    *state = f.state;
    return 0;
}
int sdmGetMode(void* p, int* mode) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    *mode = f.backlight ? 0 : f.requested;
    return 0;
}
int sdmGetId(void*, int* id) { *id = 67; return 0; }
bool sdmIsPrimary(void* p) { return static_cast<Fake*>(p)->primary; }
void fakeSetPrimary(void* p, bool primary) { static_cast<Fake*>(p)->primary = primary; }

__attribute__((noinline)) int sdmDeinit(void* p) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    f.destroyed = true;
    return 0;
}
int sdmDestroy(void*, void* p) { return sdmDeinit(p); }

int fakeRequested(void* p) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    return f.requested;
}
bool fakeBacklight(void* p) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    return f.backlight;
}
void fakeReject(void* p, int mode, int count) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    f.reject = mode; f.remaining = count;
}
void fakePauseNext(void* p) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard lock(f.pauseMutex);
    f.pauseNext = true; f.resume = false; f.paused = false;
}
bool fakeWaitPaused(void* p) {
    auto& f = *static_cast<Fake*>(p);
    std::unique_lock lock(f.pauseMutex);
    return f.pauseCv.wait_for(lock, 2s, [&] { return f.paused; });
}
void fakeResume(void* p) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard lock(f.pauseMutex);
    f.resume = true; f.pauseCv.notify_all();
}
bool fakeDestroyed(void* p) {
    auto& f = *static_cast<Fake*>(p);
    std::lock_guard outer(f.outer);
    std::lock_guard inner(f.inner);
    return f.destroyed;
}
