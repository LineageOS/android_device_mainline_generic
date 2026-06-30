# Status

## Audio

Basic audio works, for most of the non-fancy sound cards, like the ones embedded in x86 desktop motherboards, and the PCI/USB ones.

Only the first sink/source on the first sound card is selected.

## Backlight

The first backlight device appeared during boot is selected.

## Battery

Works if it has driver in the kernel and exposes `/sys/class/power_supply` interface.

If no battery devices appears in `/sys/class/power_supply`, then a fake battery with 85% state-of-change will appear in android system.

## Bluetooth

Should work for bluetooth cards that supports recent bluetooth versions.

Old bluetooth cards might end up with some issues.

## Booting

Should boot as long as the device can boot any other Linux OS, and have matching/enough specs for Android OS.

## Camera

To be done.

## Codecs

Software codecs is available from AOSP Codec2.
Hardware codecs is not available yet.

## Display

Display may work via DRM subsystem, should always work via framebuffer subsystem.

## Ethernet

Works.

Though, we have no option to configure static IP address for Ethernet yet.

## GNSS

No any chance yet.

## Graphics

3D accelerated graphics is not guaranteed to work on every graphics cards.

CPU rendered graphics is always available as fallback, but might need to be selected manually (Framebuffer display always does this).

Hybrid GPU setup (i.e. one GPU for rendering and another GPU for display) is currently not supported.

## Input

Most of the common types of input devices should work as long as it's recognized by Linux kernel.

For tablet type of input devices, Android doesn't support it natively, we have a service that attempts to translate its input events to touchscreen input events,
however the service only function for the few tablet input devices listed in it for now.

## Power management

To be done.

## Radio

To be done.

## Sensors

IIO sensors HAL exists, and a library that reads sensors information from hwdb exists too, but has not been verified to work on this target yet.

Currently, no sensor devices is supported.

## USB

Host mode should work like how every normal Android devices does.

Device mode requires the controller supporting it and the controller driver exposing USB ConfigFS.

## Vibration

Should work for vibrator devices that exposes `/dev/input` interface.

## Wi-Fi

Should work with most of Wi-Fi cards, though there might be some issues.

VirtWifi is available if the device have no Wi-Fi card but have a Ethernet card.
