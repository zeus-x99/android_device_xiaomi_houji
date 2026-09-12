#include "FakeSdm.h"
#ifdef HOUJI_QSYNC_ANDROID_IPC_TEST
#include "QsyncWire.h"
#include <android/binder_process.h>
#include <android/binder_manager.h>
#endif
#include <dlfcn.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <thread>

using namespace std::chrono_literals;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::abort(); } } while (false)

#ifdef HOUJI_QSYNC_ANDROID_IPC_TEST
extern "C" int houji_qsync_client_notify(unsigned);
#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
#ifdef HOUJI_QSYNC_ANDROID_IPC_TEST
    if (argc == 2 && std::strcmp(argv[1], "--client") == 0) {
        CHECK(houji_qsync_client_notify(1) == -EINVAL);
        const int result = houji_qsync_client_notify(0);
        std::printf("Binder client result=%d\n", result);
        return result == 0 ? 0 : 1;
    }
#endif
    const auto notify = reinterpret_cast<int (*)(unsigned)>(dlsym(RTLD_DEFAULT, "houji_qsync_notify"));
    const auto dump = reinterpret_cast<int (*)(int)>(dlsym(RTLD_DEFAULT, "houji_qsync_dump"));
    CHECK(notify && dump);
    CHECK(notify(0) == -ENODEV && notify(1) == -EINVAL);
    void* secondary = fakeCreate();
    fakeSetPrimary(secondary, false);
    CHECK(sdmInit(secondary) == 0);
    CHECK(notify(0) == -ENODEV);
    CHECK(sdmDestroy(nullptr, secondary) == 0);
    fakeDelete(secondary);
    void* display = fakeCreate();
    CHECK(sdmInit(display) == 0);
    CHECK(sdmPower(display, 1, false, nullptr) == 0);
    if (argc == 2 && std::strcmp(argv[1], "--identity") == 0) {
        CHECK(notify(0) == 0);
        CHECK(sdmDestroy(nullptr, display) == 0);
        fakeDelete(display);
        CHECK(notify(0) == -ENODEV);
        std::puts("PASS: SDM primary ID 67 routes to logical display 0; secondary ignored");
        return 0;
    }
#ifdef HOUJI_QSYNC_ANDROID_IPC_TEST
    if (argc == 2 && std::strcmp(argv[1], "--server") == 0) {
        CHECK(sdmMode(display, 1) == 0);
        CHECK(ABinderProcess_setThreadPoolMaxThreadCount(2));
        houji::qsync::startQsyncService();
        CHECK(AServiceManager_checkService(houji::qsync::kTimerService) == nullptr);
        ABinderProcess_startThreadPool();
        AIBinder* registered = AServiceManager_checkService(houji::qsync::kTimerService);
        CHECK(registered != nullptr);
        AIBinder_decStrong(registered);
        std::puts("PASS: service withheld until original Binder pool starts");
        std::puts("Fake-SDM IPC server started for 45 seconds; no hardware display object");
        std::fflush(stdout);
        std::this_thread::sleep_for(45s);
        CHECK(dump(STDOUT_FILENO) == 0);
        CHECK(sdmDestroy(nullptr, display) == 0);
        fakeDelete(display);
        return 0;
    }
#endif
    CHECK(notify(0) == 0 && fakeRequested(display) == 0);
    CHECK(sdmMode(display, 1) == 0);
    CHECK(notify(0) == 0 && fakeRequested(display) == 4 && fakeBacklight(display));
    std::this_thread::sleep_for(3s);
    CHECK(notify(0) == 0);
    std::this_thread::sleep_for(2500ms);
    CHECK(fakeRequested(display) == 4);  // First deadline must not restore.
    const auto deadline = std::chrono::steady_clock::now() + 4s;
    while (fakeRequested(display) == 4 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(10ms);
    CHECK(fakeRequested(display) == 1 && !fakeBacklight(display));

    CHECK(notify(0) == 0);
    CHECK(sdmMode(display, 0) == 0);
    std::this_thread::sleep_for(5200ms);
    CHECK(fakeRequested(display) == 0 && !fakeBacklight(display));

    CHECK(sdmMode(display, 1) == 0);
    CHECK(notify(0) == 0);
    CHECK(sdmPower(display, 0, false, nullptr) == 0);
    CHECK(!fakeBacklight(display) && fakeRequested(display) == 0);
    CHECK(sdmPower(display, 1, false, nullptr) == 0);
    std::this_thread::sleep_for(5200ms);
    CHECK(fakeRequested(display) == 0);

    CHECK(sdmMode(display, 1) == 0);
    fakeReject(display, 4, 1);
    CHECK(notify(0) == 2);  // Error propagated even though cleanup succeeds.
    CHECK(fakeRequested(display) == 0 && !fakeBacklight(display));
    CHECK(sdmMode(display, 1) == 0);
    CHECK(notify(0) == 0);
    fakeReject(display, 1, 1);
    std::this_thread::sleep_for(5200ms);
    CHECK(fakeRequested(display) == 0 && !fakeBacklight(display));
    CHECK(dump(STDOUT_FILENO) == 0);

    CHECK(sdmMode(display, 1) == 0);
    CHECK(notify(0) == 0);
    fakeReject(display, 0, 3);  // External close, cleanup and first retry fail.
    CHECK(sdmMode(display, 0) == 2);
    CHECK(fakeBacklight(display));
    CHECK(dump(STDOUT_FILENO) == 0);  // Both failures must remain observable.
    const auto recoveryLimit = std::chrono::steady_clock::now() + 2s;
    while (fakeBacklight(display) && std::chrono::steady_clock::now() < recoveryLimit) {
        const int result = notify(0);  // A busy client must not postpone recovery.
        CHECK(result == -EAGAIN || result == 0);
        if (result == 0) CHECK(!fakeBacklight(display));
        std::this_thread::sleep_for(10ms);
    }
    CHECK(!fakeBacklight(display) && fakeRequested(display) == 0);
    CHECK(sdmPower(display, 0, false, nullptr) == 0);
    CHECK(!fakeBacklight(display) && fakeRequested(display) == 0);
    CHECK(sdmPower(display, 1, false, nullptr) == 0);

    CHECK(sdmMode(display, 1) == 0);
    fakePauseNext(display);
    auto inFlight = std::async(std::launch::async, [&] { return notify(0); });
    CHECK(fakeWaitPaused(display));
    auto destroy = std::async(std::launch::async, [&] { return sdmDestroy(nullptr, display); });
    CHECK(destroy.wait_for(50ms) == std::future_status::timeout);
    CHECK(!fakeDestroyed(display));
    fakeResume(display);
    CHECK(inFlight.wait_for(2s) == std::future_status::ready && inFlight.get() == 0);
    CHECK(destroy.wait_for(2s) == std::future_status::ready && destroy.get() == 0);
    CHECK(fakeDestroyed(display) && notify(0) == -ENODEV);
    fakeDelete(display);
    std::puts("PASS: runtime reset/expiry/override/power/error/teardown using fake SDM; real HAL not tested");
    return 0;
}
