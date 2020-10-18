# Rootless

Rootless Rootless creates a "sandbox" environment of the filesystem. Any access to the file system will be first redirected to a user-provided fake root path: `RL_PATH`. Since the `RL_PATH` can be a path that the user have full control, the user can use rootless to "change" root-privileged files without being root - Rootless will redirect the write to the `RL_PATH`.

## Related works

There are other products that does similar but different work of Rootless:
 * Docker - In theory, kernel isolation (like docker) is the best solution to change the root. However, docker requires the root privilege by default. The rootless docker is still unstable by now (Oct. 2020), as far as I know. Also, some system maintainers may prefer to disable unprivileged kernel isolation. So if you don't have root, you cannot easily use docker.
 * chroot - is a tool created years ago to change the root in the subprocess. But it still requires being root to use chroot.
 * fakeroot - does similar things of Rootless and does not require the root privilege. It even implements its functionalities in a similar way of Rootless: replacing `libc` functions to redirect the filesystem calls. However, it uses `LD_PRELOAD` trick to replace the file-related functions. This trick cannot patch the filesystem calls within the `libc`. e.g., `mktemp` creates a temp file, which uses `open` under the hood. If we just replace `open` using `LD_PRELOAD` trick, it cannot patch the call to `mktemp`. So fakeroot does not patch `open` function as well as many other file-related functions. We Rootless use another way of patching the system calls and AIM to patch all of the related functions. 

## Usage

Change the following command with the path in your local file system

```bash
RL_ROOT=/home/menooker/fish/ LD_PRELOAD=/home/menooker/Rootless/build/libRootless.so  bash
```

Then you can create a file where you like in the new bash environemnt:

```bash
echo "Hello" > /hello.txt
cat /hello.txt
```

This will create a file "hello.txt" in the fake root: `/home/menooker/fish/`

## Build

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

## How it works

TBD

