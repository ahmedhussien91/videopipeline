# Cross-Compiling with Yocto SDK (Raspberry Pi)

This project can be built against the Yocto SDK and Raspberry Pi kernel sources provided at:
- SDK sysroot: `/opt/yocto/poky/5.0.8/sysroots/cortexa72-poky-linux`
- Kernel sources: `/opt/yocto/ycoto-excersise/rpi-build-sysv/workspace/sources/linux-raspberrypi`

## Quick Start
```bash
# 1) Load the Yocto environment (sets CC/CXX/sysroot/pkg-config paths)
source /opt/yocto/poky/5.0.8/environment-setup-cortexa72-poky-linux

# 2) Configure using the provided toolchain file
cmake -S . -B build/rpi \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/rpi-yocto.cmake

# 3) Build
cmake --build build/rpi -j$(nproc)
```

## Notes
- The toolchain file auto-picks up `SDKTARGETSYSROOT`/`OECORE_TARGET_SYSROOT` from the environment setup script and constrains package lookups to the SDK sysroot.
- Kernel headers default to `KERNEL_HEADERS_DIR=/opt/yocto/ycoto-excersise/rpi-build-sysv/workspace/sources/linux-raspberrypi/include`. Override if needed:
  ```bash
  cmake -S . -B build/rpi \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/rpi-yocto.cmake \
    -DKERNEL_HEADERS_DIR=/path/to/linux/include
  ```
- Ensure any extra platform libraries (e.g., V4L2 helpers) are present in the SDK sysroot or added via `PKG_CONFIG_PATH` before configuring.
