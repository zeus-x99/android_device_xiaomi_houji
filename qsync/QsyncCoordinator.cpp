#include "QsyncCoordinator.h"

#include <cerrno>
#include <mutex>

namespace houji::qsync {

Coordinator::Coordinator(Backend& backend, std::chrono::milliseconds delay)
    : backend_(backend), delay_(delay) {}

void Coordinator::cancel() {
    ++generation_;
    deadline_.reset();
}

int Coordinator::externalMode(int mode) {
    cancel();  // Even a failed newer request forbids restoring an older intent.
    const int status = backend_.apply(mode);
    if (status != 0) {
        known_ = false;
        return status;
    }
    applied_ = mode;
    ownsBacklight_ = false;
    known_ = mode >= 0 && mode <= 4;
    return 0;
}

int Coordinator::backlight(Clock::time_point now) {
    if (!on_) return 0;
    if (!known_) return -EAGAIN;
    if (delay_.count() <= 0) return -EINVAL;
    if (applied_ == 0 || (applied_ == 4 && !ownsBacklight_)) return 0;
    if (applied_ != 4) {
        const int status = backend_.apply(static_cast<int>(Mode::Backlight));
        if (status != 0) {
            // Even an error before suspension requires reconciliation: the
            // adapter must not assume an undocumented backend is transactional.
            known_ = false;
            ownsBacklight_ = true;  // The failed call might have partially applied.
            return status;
        }
        applied_ = 4;
        ownsBacklight_ = true;
    }
    cancel();
    deadline_ = Deadline{generation_, now + delay_};
    return 0;
}

int Coordinator::expire(uint64_t generation, Clock::time_point now) {
    if (!deadline_ || deadline_->generation != generation || now < deadline_->at) return 0;
    cancel();
    if (!on_ || !known_ || !ownsBacklight_ || applied_ != 4) return 0;
    // This matches the observed reference restore-to-continuous behavior.
    const int status = backend_.apply(static_cast<int>(Mode::Continuous));
    if (status != 0) {
        known_ = false;
        return status;
    }
    applied_ = 1;
    ownsBacklight_ = false;
    return 0;
}

void Coordinator::initializedOff() {
    cancel();
    applied_ = 0;
    known_ = true;
    on_ = false;
    ownsBacklight_ = false;
}

int Coordinator::clearBacklightOverride() {
    cancel();
    if (!ownsBacklight_) return 0;
    const int status = backend_.apply(static_cast<int>(Mode::Off));
    if (status != 0) {
        known_ = false;
        return status;
    }
    applied_ = 0;
    known_ = true;
    ownsBacklight_ = false;
    return 0;
}

void Coordinator::powerChanged(bool on) {
    cancel();
    on_ = on;
    // An accepted external request is still known across a power transition.
    // Our temporary override must be cleared before the adapter calls here.
    if (ownsBacklight_) known_ = false;
    // Do not forget ownership: the adapter must reconcile/clear a still-set
    // backlight flag before allowing future configuration changes.
}

void Coordinator::disconnected() {
    cancel();
    on_ = false;
    known_ = false;
    ownsBacklight_ = false;  // The old object must no longer be used.
}

DisplayLifetime::Lease::Lease(DisplayLifetime& owner)
    : lock_(owner.mutex_, std::defer_lock), object_(nullptr) {
    if (owner.closing()) return;
    lock_.lock();
    if (owner.closing()) {
        lock_.unlock();
        return;
    }
    object_ = owner.object_;
}

void DisplayLifetime::close() {
    closing_.store(true, std::memory_order_release);
    std::unique_lock lock(mutex_);
    object_ = nullptr;
}
}  // namespace houji::qsync
