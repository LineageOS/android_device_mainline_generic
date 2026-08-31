# TODO

Contributions are welcome.

- Add Android Hardware Buffer support in every Mesa drivers
- Add RGB pixel format support in Codec2, as some/most of graphics cards does not support YUV
- Bring in GloDroid's mm-radio stack, or we implement a better alternative of it
- Bring in a Codec2 HAL that uses generic APIs such as VA-API as backend, and does not require per-hardware configuration, or we implement a better alternative of it
- Implement a AIDL Audio HAL that uses configuration from `alsa-ucm-conf`
- Keep improving the quality of the documentations
- Make it possible to display YUV content on graphics cards that does not support it
- Make the IIO sensors HAL work
