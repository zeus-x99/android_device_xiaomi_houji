#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <shared_mutex>

namespace houji::qsync {

// Internal SDM modes, not the AIDL enumeration.
enum class Mode : int { Off = 0, Continuous = 1, OneShot = 2, OneShotContinuous = 3,
                        Backlight = 4 };

struct Deadline {
    uint64_t generation;
    std::chrono::steady_clock::time_point at;
};

class Backend {
public:
    virtual ~Backend() = default;
    // The real adapter calls the original SetQSyncModeLocked here, under SDM's
    // locks. Zero means committed; errors must be propagated, not treated as OK.
    virtual int apply(int mode) = 0;
};

// All methods (including queries) must be serialized by the adapter. In the
// real service, mutations must run inside the original SDM locked entry. Do
// not hold a separate coordinator lock while entering the public SDM method.
class Coordinator {
public:
    using Clock = std::chrono::steady_clock;
    explicit Coordinator(Backend& backend, std::chrono::milliseconds delay);

    int externalMode(int mode);
    int backlight(Clock::time_point now);
    int expire(uint64_t generation, Clock::time_point now);
    // Only at successful Init, before any frame/mode request can race it.
    // The adapter must verify both initial modes are zero, not infer pending
    // mode from GetQSyncMode (which reports the committed mode).
    void initializedOff();
    // Clear our backlight override after a failed restore or before power off.
    // This is error recovery, not a successful restore-to-continuous result.
    int clearBacklightOverride();
    // Called for an authoritative power transition. Cancels immediately;
    // a known accepted external request remains known, but an uncleared
    // temporary backlight override requires reconciliation.
    void powerChanged(bool on);
    void disconnected();

    std::optional<Deadline> deadline() const { return deadline_; }
    uint64_t generation() const { return generation_; }
    bool isOn() const { return on_; }
    bool needsReconciliation() const { return !known_; }
    bool ownsBacklightMode() const { return ownsBacklight_; }
    std::optional<int> requestedMode() const {
        return known_ ? std::optional<int>(applied_) : std::nullopt;
    }

private:
    void cancel();
    Backend& backend_;
    const std::chrono::milliseconds delay_;
    uint64_t generation_ = 0;
    std::optional<Deadline> deadline_;
    int applied_ = 0;
    bool on_ = false;
    bool known_ = false;
    bool ownsBacklight_ = false;
};

// Guards a captured display pointer against asynchronous use after teardown.
// DestroyDisplay must close() BEFORE calling the original destroy routine.
// close() must be called before taking any SDM locks needed by leased work.
class DisplayLifetime {
public:
    class Lease {
    public:
        Lease(Lease&&) = default;
        Lease& operator=(Lease&&) = default;
        explicit operator bool() const { return object_ != nullptr; }
        void* object() const { return object_; }
    private:
        friend class DisplayLifetime;
        explicit Lease(DisplayLifetime& owner);
        std::shared_lock<std::shared_mutex> lock_;
        void* object_;
    };

    explicit DisplayLifetime(void* object) : object_(object) {}
    Lease acquire() { return Lease(*this); }
    void close();
    bool closing() const { return closing_.load(std::memory_order_acquire); }

private:
    std::shared_mutex mutex_;
    std::atomic<bool> closing_{false};
    void* object_;
};
}  // namespace houji::qsync
