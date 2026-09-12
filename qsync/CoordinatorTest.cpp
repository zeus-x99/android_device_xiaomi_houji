#include "QsyncCoordinator.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace houji::qsync;

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "FAILED line %d: %s\n", __LINE__, #condition); std::abort(); \
} } while (false)

struct RecordingBackend : Backend {
    std::vector<int> writes;
    int reject = -1;
    int apply(int mode) override {
        writes.push_back(mode);
        return mode == reject ? -EIO : 0;
    }
};

int main() {
    const auto zero = Coordinator::Clock::time_point{};
    unsigned cases = 0;
    for (int initial : {1, 2, 3}) {
        RecordingBackend backend;
        Coordinator c(backend, 5s);
        c.powerChanged(true);
        CHECK(c.externalMode(initial) == 0);
        CHECK(c.backlight(zero) == 0);
        const auto first = *c.deadline();
        CHECK(c.backlight(zero + 4s) == 0);
        const auto last = *c.deadline();
        CHECK(c.expire(first.generation, zero + 5s) == 0);
        CHECK(c.expire(last.generation, zero + 8999ms) == 0);
        CHECK(backend.writes == (std::vector<int>{initial, 4}));
        CHECK(c.expire(last.generation, zero + 9s) == 0);
        CHECK(c.expire(last.generation, zero + 20s) == 0);
        CHECK(backend.writes == (std::vector<int>{initial, 4, 1}));
        ++cases;
    }
    for (bool timeoutFirst : {false, true}) {
        for (int replacement : {0, 1, 2, 3, 4}) {
            RecordingBackend backend;
            Coordinator c(backend, 5s);
            c.powerChanged(true);
            CHECK(c.externalMode(1) == 0);
            CHECK(c.backlight(zero) == 0);
            const auto ticket = *c.deadline();
            if (timeoutFirst) CHECK(c.expire(ticket.generation, zero + 5s) == 0);
            CHECK(c.externalMode(replacement) == 0);
            const size_t n = backend.writes.size();
            CHECK(c.expire(ticket.generation, zero + 5s) == 0);
            CHECK(backend.writes.size() == n);
            if (replacement == 0 || replacement == 4) {
                CHECK(c.backlight(zero + 6s) == 0);
                CHECK(backend.writes.size() == n && !c.deadline());
            }
            ++cases;
        }
    }
    for (int failureStage : {0, 1, 2}) {
        RecordingBackend backend;
        Coordinator c(backend, 5s);
        c.powerChanged(true);
        CHECK(c.backlight(zero) == -EAGAIN);
        CHECK(c.externalMode(1) == 0);
        if (failureStage == 0) {
            backend.reject = 4;
            CHECK(c.backlight(zero) == -EIO);
        } else {
            CHECK(c.backlight(zero) == 0);
            const auto ticket = *c.deadline();
            if (failureStage == 1) {
                backend.reject = 1;
                CHECK(c.expire(ticket.generation, zero + 5s) == -EIO);
            } else {
                backend.reject = 0;
                CHECK(c.externalMode(0) == -EIO);
            }
            const size_t n = backend.writes.size();
            CHECK(c.expire(ticket.generation, zero + 10s) == 0);
            CHECK(backend.writes.size() == n && c.ownsBacklightMode());
        }
        CHECK(c.needsReconciliation() && !c.deadline());
        CHECK(c.backlight(zero + 20s) == -EAGAIN);
        backend.reject = -1;
        CHECK(c.externalMode(0) == 0);
        CHECK(!c.ownsBacklightMode() && !c.needsReconciliation());
        ++cases;
    }
    {
        RecordingBackend backend;
        Coordinator c(backend, 5s);
        c.powerChanged(true);
        CHECK(c.externalMode(1) == 0);
        CHECK(c.backlight(zero) == 0);
        const auto ticket = *c.deadline();
        c.powerChanged(false);
        CHECK(c.expire(ticket.generation, zero + 10s) == 0);
        CHECK(c.backlight(zero + 10s) == 0);
        c.powerChanged(true);
        CHECK(c.backlight(zero + 11s) == -EAGAIN);
        CHECK(c.ownsBacklightMode());
        CHECK(c.externalMode(0) == 0);
        c.disconnected();
        CHECK(!c.ownsBacklightMode() && !c.deadline());
        CHECK(backend.writes == (std::vector<int>{1, 4, 0}));
        ++cases;
    }
    {
        int object = 42;
        DisplayLifetime life(&object);
        std::promise<void> acquired, release;
        auto released = release.get_future();
        std::atomic<bool> destroyed{false};
        std::thread worker([&] {
            auto lease = life.acquire();
            CHECK(lease && lease.object() == &object);
            acquired.set_value();
            released.wait();
            CHECK(!destroyed.load());
        });
        acquired.get_future().wait();
        auto teardown = std::async(std::launch::async, [&] {
            life.close();
            destroyed.store(true);
        });
        const auto limit = std::chrono::steady_clock::now() + 2s;
        while (!life.closing() && std::chrono::steady_clock::now() < limit) {
            std::this_thread::yield();
        }
        CHECK(life.closing());
        CHECK(!life.acquire());  // Must reject rather than queue behind teardown.
        CHECK(teardown.wait_for(20ms) == std::future_status::timeout);
        release.set_value();
        worker.join();
        CHECK(teardown.wait_for(2s) == std::future_status::ready);
        teardown.get();
        CHECK(!life.acquire());
        life.close();
        ++cases;
    }
    std::printf("PASS: %u controller/lifetime scenarios; adapter and HAL not exercised\n", cases);
}
