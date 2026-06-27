# Boot parameters

## generic_init

## Init rc

### androidboot.lcd_density

Passes the set value to property `ro.sf.lcd_density`.
If this is not set, the property `ro.sf.lcd_density` will be set by the `get_display_ppi` service.

### androidboot.save_early_logs

When this is set to `1`, The logs (only logcat for now) before booting completes will be saved to `/metadata`.

### androidboot.seriallogging

If you want logcat getting printed to a serial device, set this to the name of the serial device.

### androidboot.seriallogging.logcat_buffer

Sets the logcat buffer(s) to be printed to the serial device.

The set value will be passed to the `-b` option of `logcat`.

### androidboot.wifi_impl

When this is set to `virt_wifi`, `setup_wifi` service will be started and it will create a wlan interface that is bridged to a existing ethernet interface.

## libinit

### androidboot.insecure_adb

When this is set to `true`, adb authentication is disabled, adb runs as root by default, and adb service will be available earlier during boot.
