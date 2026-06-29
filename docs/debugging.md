# Debugging

## Gathering shell access on specified serial console

Specify the serial device on `androidboot.console=` parameter, like the Linux `console=` parameter.

## Gathering ADB access via ethernet or VirtIO VSOCK when system services encounters crash loop

Boot with the parameter `androidboot.insecure_adb=true`.

If you want to connect via ethernet, get shell access, and run the following commands:

(Assuming the ethernet interface is `eth0`, and IPv4 address to be set is `192.168.1.100/24`)

```
stop
ip link set eth0 up
ip address add 192.168.1.100/24 dev eth0
ip rule add from all lookup main
```

Finally, connect to it.

To connect via ethernet: `adb connect <IPv4 address>` (for example: `192.168.1.100`)

To connect via VirtIO VSOCK: `adb connect vsock:<Guest CID>:5555` (`<Guest CID>` is usually visible on the VM configuration)

## Pausing at shell during early boot

Please check out the `androidboot.mode=console` parameter on [Boot parameters](boot-parameters.md).

## Printing kernel log to serial console

Just use the standard Linux `console=` parameter.

## Printing logcat to serial console

Please check out the `androidboot.seriallogging` parameters on [Boot parameters](boot-parameters.md).

## Saving early logs to metadata partition

Please check out the `androidboot.save_early_logs` parameter on [Boot parameters](boot-parameters.md).
