# xbanan

An X11 compatibility layer for [banan-os's](https://git.bananymous.com/Bananymous/banan-os) native GUI windowing system.

## Running on linux

### Building
```sh
git clone --recursive https://git.bananymous.com/Bananymous/xbanan.git
cd xbanan

cd banan-os
git apply ../0001-linux-window-server-sdl2.patch
cd ..

cmake -B build -S . -G Ninja
cmake --build build
```

### Running

To start the WindowServer, run the following command in project root
```sh
./build/banan-os/userspace/programs/WindowServer/WindowServer
```
To run X11 apps specify `DISPLAY=:69` environment variable. For example
```sh
DISPLAY=:69 xeyes
```
