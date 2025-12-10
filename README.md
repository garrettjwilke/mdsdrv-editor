## MDSDRV - Editor

### Building

#### Prerequisites


#### Native Build Instructions

`git clone https://github.com/garrettjwilke/mdsdrv-editor && cd mdsdrv-editor`

`git submodule update --init --recursive`

`mkdir build && cd build`

`cmake .. && cmake --build .`

#### WebAssembly Build Instructions

`git clone https://github.com/garrettjwilke/mdsdrv-editor && cd mdsdrv-editor`

`git submodule update --init --recursive`

`mkdir wasm-build && cd wasm-build`

`emcmake cmake .. && emmake make`

`python3 -m http.server`
