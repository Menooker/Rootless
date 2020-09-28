# Rootless

Rootless helps you to "access" privileged paths without "root". Work in progress.

# Build

Build dependency: cmake

Code dependency: [PFishHook](https://github.com/Menooker/PFishHook)

```
git clone https://github.com/Menooker/Rootless
cd Rootless
git submodule update --init --recursive
mkdir build
cd build
cmake ..
make
```