
#
# Copyright (C) 2023 The Android Open Source Project
#
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from sm8650-common
$(call inherit-product, device/xiaomi/sm8650-common/common.mk)

# Get non-open-source specific aspects
$(call inherit-product, vendor/xiaomi/houji/houji-vendor.mk)

# Euicc
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/permissions/privapp-permissions-euiccgoogle.xml:$(TARGET_COPY_OUT_PRODUCT)/etc/permissions/privapp-permissions-euiccgoogle.xml

PRODUCT_PACKAGES += \
    XiaomiEuicc \
    XiaomiEsimSwitcher

PRODUCT_BROKEN_VERIFY_USES_LIBRARIES := true

# init
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/init/init.houji.rc:$(TARGET_COPY_OUT_VENDOR)/etc/init/init.houji.rc \

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(LOCAL_PATH)

# Radio
PRODUCT_VENDOR_PROPERTIES += ro.vendor.radio.hangup_pending_mo=true

# Add an on-demand FOD wake sensor through the supplementary ODM HAL list.
PRODUCT_PACKAGES += sensors.xiaomi.v2

PRODUCT_VENDOR_PROPERTIES += ro.vendor.fingerprint.lhbm_ready_event=true
PRODUCT_VENDOR_PROPERTIES += \
    ro.vendor.sensors.xiaomi.udfps=true \
    ro.vendor.sensors.xiaomi.udfps.touchfeature=true \
    ro.vendor.sensors.xiaomi.udfps.location_x=600 \
    ro.vendor.sensors.xiaomi.udfps.location_y=2390

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/configs/sensors/hals.conf:$(TARGET_COPY_OUT_ODM)/etc/sensors/hals.conf

# Overlays
PRODUCT_PACKAGE_OVERLAYS += $(LOCAL_PATH)/overlay

PRODUCT_PACKAGES += \
    FrameworksResHouji \
    HoujiEuiccOverlay \
    SettingsOverlayHouji \
    SystemUIResHouji
