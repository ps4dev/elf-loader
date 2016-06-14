# Elf loader

Runs ps4sdk elf files in-process on your PS4.

## Prerequisites
* Basic build-tools (clang due to ps4 support)
* ps4dev/ps4sdk
* node.js to run the server
* socat, netcat, etc.

You will also need a current version of clang or alter the Makefile (remove -target specifically additionally ld/objectcopy may produce undesired (large) binaries).

You can of course use other tools for the server, upload or debug output (socat, netcat or a C implementation for example).

The elf loader does not support dynamically linked executables. All libraries need to be statically linked into the executable. [ps4sdk](https://github.com/ps4dev/ps4sdk) provides a variaty of position independant, statically linkable libraries, such as a libc, for the PS4.

## Example
```bash
# build for the ps4 / we need to set all prefered defaults at build time (no args to main)
make clean && make

# Start server
cd local
node server.js

# Browse ps4 browser to server (<local>:5350)
# Wait until the browser hangs in step 5

# Connect debug/stdio channel
socat - TCP:<ps4>:5052

# Send elf file
socat -u FILE:../../ps4sdk-examples/libless/stress/bin/stress TCP:<ps4>:5053

# Send kernel elf file (mode for long-running code, or module-like code)
socat -u FILE:../../ps4sdk-examples/kernel/function-hook/bin/function-hook TCP:<ps4>:5055

# Send kernel elf file (runs in webbrowser process)
socat -u FILE:../../libps4-examples/libless/cache/bin/cache TCP:<ps4>:5054
```

## Docker images
A stand alone elf-loader container is also available (but currently a bit large):

```bash
# Make sure newest container is used
docker pull ps4dev/elf-loader
# Run the elf loader (listens on port 5350)
docker run -p 5350:5350 -rm ps4dev/elf-loader&
# Stop elf loader
docker kill $(docker ps -q -f ancestor=ps4dev/elf-loader)
```
