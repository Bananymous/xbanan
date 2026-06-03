# xbanan

A portable X11 compatibility layer. There are 2 officially supported platforms: [banan-os's](https://git.bananymous.com/Bananymous/banan-os) native GUI windowing system and SDL3.

## Running on linux

### Prerequisites

You need to have SDL3 development library and X11 headers installed on your system.

### Building
```sh
git clone --recursive https://git.bananymous.com/Bananymous/xbanan.git
cd xbanan

cmake -B build -S . -DPLATFORM=SDL3
cmake --build build --parallel $(nproc)
```

### Running

To start xbanan, run the following command in project root
```sh
./build/xbanan/xbanan
```
To run X11 apps, specify `DISPLAY=:69` environment variable. For example
```sh
DISPLAY=:69 xeyes
```

## Porting to a new platform

Add your platform to the `VALID_PLATFORMS` list in the root `CMakeLists.txt` file. Create a directory for your platform in `xbanan/<your platform>` and define the needed source files and libraries in `xbanan/CMakeLists.txt` based on your platform.

### Barebones

For the simplest port, you need to implement 3 functions: `initialize`, `create_window` and `invalidate`. Define the variable `g_platform_ops` declared in `xbanan/Platform.h` with function pointers to the your functions. You can look at the existing platforms and the `xbanan/Platform.h` header for help.

### Events

To handle events, you have to hook an event receiver fd for your platform using `register_event_fd` declared in `xbanan/Events.h` and define the `poll_events` function in `g_platform_ops`. When the registered fd becomes readable, xbanan will call your platform's `poll_events` function. This function should drain all of the pending events and call appropriate `on_*_event` function declared in the events header. If your platform does not expose fd(s) that can be polled for events, you can do what the SDL3 port does and create an eventfd/pipe that gets signaled periodically.

### Installation

To install xbanan to sysroot, just copy the `build/xbanan/xbanan` to the destination. xbanan is statically linked against the libraries from this repository so you don't have to care about those. **DO NOT** use the cmake's install target, as that installs banan-os libraries to the root of the sysroot. You also need to have the x11 misc fonts at `FONT_PATH/misc` in your sysroot if you want server side font support. The fonts are shipped (public domain) with xbanan, so you can just copy `fonts/misc` into `FONT_PATH`. You can specify the `FONT_PATH` with cmake using `-DFONT_PATH=...` when configuring.
