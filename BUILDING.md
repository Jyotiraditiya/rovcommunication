# Building `rov_sim`

The simulator uses the header-only MAVLink C library. CMake downloads the
pinned MAVLink `c_library_v2` release into the build directory, so no separate
system installation is required.

```sh
cmake -S . -B build
cmake --build build --target rov_sim
```

Run it with an optional ground-control-station address and port:

```sh
./build/rov_sim --gcs-ip 127.0.0.1 --gcs-port 14550
```
