# Patches

Based on repositories from LineageOS org, the lists below might be not applicable to any other upstream.

## Topics

Run these:

```
repopick -t c2-rgb
repopick -t lineage-light-disable-backlight
repopick -t mainline-kernel
```

## external/mesa

All the entries [here](https://review.lineageos.org/q/project:LineageOS/android_external_mesa+owner:me.cafebabe@gmail.com+branch:lineage-24.0).
You can simply check out the repository to the HEAD of the top entry on the chain.

## external/zstd

For `lineage-24.0` branch:

| Change id | Change number | Commit message |
|---------------|-----------|----------------|
| `I12cbe2c8b47a0ceccf248b088202fefc74525b3a` | 496520 | `zstd: Android.bp: Add `//vendor:__subpackages__` to visibility` |

You can run `repopick [change number ...]` to pick these patches.

## hardware/interfaces

For `lineage-24.0` branch:

| Change id | Change number | Commit message |
|---------------|-----------|----------------|
| `Ife4de11c0b0646ad5577d0d37a4d28bc25de1cfa` | 501200 | `bluetooth/aidl/default: Fix 100% CPU spin while waiting for the HCI interface` |

You can run `repopick [change number ...]` to pick these patches.

## kernel/mainline/android-mainline

- `drm/vmwgfx: add ABGR8888 to vmw_primary_plane_formats[]`: https://review.lineageos.org/c/LineageOS/android_kernel_virt_virtio/+/423579
- `HACK: selinux: Force permissive when androidboot.selinux=permissive`: https://review.lineageos.org/c/LineageOS/android_kernel_virt_virtio/+/464426
- `Revert "drm/virtio: Don't create a context with default param if context_init is supported"` : https://review.lineageos.org/c/LineageOS/android_kernel_virt_virtio/+/496668

## system/core

For `lineage-24.0` branch:

| Change id | Change number | Commit message |
|---------------|-----------|----------------|
| `Ie62f6ce7783f4c1b19464a44b8cd58e09fcb2e7b` | 501438 | `init: Make first stage init call the real SetFatalRebootTarget()` |
| `Ie86329f1a03169d08c2dbb9269705e56c5f9ff1f` | 501439 | `init: reboot_utils: Add option to pause on init fatal error` |
| `Id51f200ca2c5123cf16212c363d770f503744581` | 501440 | `Add console boot mode` |

You can run `repopick [change number ...]` to pick these patches.

## system/memory/libmeminfo

Apply from [here](../.patches/system/memory/libmeminfo).

## vendor/lineage

For `lineage-24.0` branch:

| Change id | Change number | Commit message |
|---------------|-----------|----------------|
| `I2b9f0524d89e03779fde5a36516c3e8cc4c40c11` | 501163 | `kernel: Probe libclang before using it for rust bindgen` |

You can run `repopick [change number ...]` to pick these patches.
