# uxnpi

Uxn for the Raspberry Pi, on bare metal. Inspired by
[uxnrpi](https://git.badd10de.dev/uxnrpi), though at this point it contains very
little code from that project. Powered by
[Circle](https://github.com/rsta2/circle). Should support all of the Varvara
devices except audio.

Currently very much a work in progress. If you want to compile it on your own,
good luck. It's set up specifically for the GPi Case 2W (a Game Boy-style Pi
case with a 640x480 LCD and a USB gamepad), and hasn't been tested on anything
else yet.

Requires an aarch64 cross-compiler build of GCC and a custom build of QEMU to
test; see Circle's instructions.

## License

This project contains two subprojects under different licenses.

My C++ Uxn framework, in the `uxn-cpp` subdirectory, is distributed under the
MIT license. This will likely be split off into a separate repository in the
future.

The full uxnpi project (all source files outside the `uxn-cpp` subdirectory) is
distributed under the GPL 3.0, because Circle is also GPL.

