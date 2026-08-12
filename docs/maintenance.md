# Maintenance

This page describes the maintenance of this device tree.

The core principle is: Keep this really generic and actually useful!

This entire device tree was initially made based on `android-16.0.0_r4` AOSP tag.

Here are the most common maintenance tasks:
- Ensure everything still works in intended way on upcoming Android version
- Keep up with changes from the upstream for components that has its upstream
- Keep extending support for more hardwares, and improving support for supported hardwares
- Keep making this device tree convenient to use, for both developers and end users

## Table of components with upstream

| Component path | Upstream |
|----------------|----------|
| `configs/audio/primary_audio_policy_configuration.xml` | `frameworks/av/services/audiopolicy/config/primary_audio_policy_configuration_7_0.xml` |
| `configs/init/console_override.rc` | `console` service on `system/core/rootdir/init.rc` |
| `configs/init/hal_services.rc` | `hardware/interfaces/graphics/allocator/2.0/default/android.hardware.graphics.allocator@2.0-service.rc` and `external/drm_hwcomposer-upstream/hwc3/hwc3-drm-upstream.rc` |
| `configs/init/init_dev_config_override.rc` | `init_dev_config` service on `system/core/rootdir/init.rc` |
| `configs/input/Generic.kl` | `frameworks/base/data/keyboards/Generic.kl` |
| `configs/misc/pci.ids` | https://pci-ids.ucw.cz/ |
| `configs/misc/usb.ids` | http://www.linux-usb.org/usb-ids.html |
| `configs/properties/vendor_bluetooth_profiles.prop` | `device/linaro/dragonboard/product.prop` |
| `hals/health` | `hardware/interfaces/health/aidl/default` and `device/google/cuttlefish/guest/hals/health/health-aidl.cpp` |
| `overlays/rro_overlays/MainlineGenericWifiOverlay` | `device/google/cuttlefish/shared/phone/overlays/CuttlefishWifiOverlay` |
| `services/modprobe_kernel/modprobe.cpp` | `system/core/toolbox/modprobe.cpp` |
| `services/overlay_remounter_override/overlay_remounter.cpp` | `system/core/overlay_remounter/overlay_remounter.cpp` |
