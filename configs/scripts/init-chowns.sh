#!/vendor/bin/sh

# Backlight
chown -R system:graphics /sys/class/backlight/ /sys/class/backlight/*/

# IIO
chown -R system:system /dev/iio* /sys/bus/iio/devices/ /sys/bus/iio/devices/*/

exit 0
