#pragma once

#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>

namespace houji::qsync {
inline constexpr char kTimerDescriptor[] = "vendor.lineage.houji.IQsyncTimer";
inline constexpr char kTimerService[] = "vendor.lineage.houji.IQsyncTimer/default";
inline constexpr transaction_code_t kNotifyBacklight = FIRST_CALL_TRANSACTION;
void startQsyncService();
}
