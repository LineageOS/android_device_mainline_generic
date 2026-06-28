# Build dependencies

## Repositories inside of LineageOS org

These should be automatically handled by `roomservice.py` program during [Cloning this device tree and its dependencies in LineageOS org](build.md#cloning-this-device-tree-and-its-dependencies-in-lineageos-org).

## Kernel source related repositories

These would likely never get into LineageOS org. You'll need to clone these manually.

Here is table of required kernel source repositories for the default configuration of target devices in this device tree:

| Source | Destination | Branch |
|--------|-------------|--------|
| https://salsa.debian.org/kernel-team/linux/ | `external/debian-linux` | `debian/latest` |
| https://android.googlesource.com/kernel/common | `kernel/mainline/android-mainline` | `android-mainline` |

If you will use a different kernel configuration, then these repositories might not be needed.
