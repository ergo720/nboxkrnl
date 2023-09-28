# Nboxkrnl

"nboxkrnl" is a work in progress, open source reimplementation of the kernel of the original xbox, with some input from the kernel code found in the xbox emulator
[Cxbx-Reloaded](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded). Currently, it can be tested with the test app provided by [lib86cpu](https://github.com/ergo720/lib86cpu),
a cpu emulation library. Note that it's not a goal of this project to be able to run on xbox emulators such as x(q)emu or on real hardware.

## Building

Cmake version 3.4.3 or higher is required.\
Visual Studio 2022.\
Only Windows builds are supported.

**On Windows:**

1. `git clone --recurse-submodules https://github.com/ergo720/nboxkrnl`
2. `mkdir build && cd build`  
3. `cmake .. -G "Visual Studio 17 2022" -A Win32`  
4. Build the resulting solution file nboxkrnl.sln with Visual Studio
