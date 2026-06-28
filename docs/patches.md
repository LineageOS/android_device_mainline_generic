# Patches

Based on repositories from LineageOS org, the lists below might be not applicable to any other upstream.

## external/mesa

All the entries [here](https://review.lineageos.org/q/project:LineageOS/android_external_mesa+owner:me.cafebabe@gmail.com+status:open).
You can simply check out the repository to the HEAD of the top entry on the chain.

## kernel/mainline/android-mainline

- `drm/vmwgfx: add ABGR8888 to vmw_primary_plane_formats[]`: https://review.lineageos.org/c/LineageOS/android_kernel_virt_virtio/+/423579
- `HACK: selinux: Force permissive when androidboot.selinux=permissive`: https://review.lineageos.org/c/LineageOS/android_kernel_virt_virtio/+/464426

## system/core

For `lineage-23.2` branch:

| Change id | Change number | Commit message |
|---------------|-----------|----------------|
| `Ic3faaddb2097c5091d3a7dbd32f830679270d516` | 442536 | `fs_mgr: Don't fail on unable to open ZRAM max_comp_streams` |
| `Ie62f6ce7783f4c1b19464a44b8cd58e09fcb2e7b` | 471113 | `init: Make first stage init call the real SetFatalRebootTarget()` |
| `Ie86329f1a03169d08c2dbb9269705e56c5f9ff1f` | 471112 | `init: reboot_utils: Add option to pause on init fatal error` |
| `Id51f200ca2c5123cf16212c363d770f503744581` | 471111 | `Add console boot mode` |

You can run `repopick [change number ...]` to pick these patches.
