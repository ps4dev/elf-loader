# Elf loader

Runs ps4sdk elf files in-process on your PS4.

## Prerequisites
* clang (not the OSX version though)
* make
* [ps4sdk](https://github.com/ps4dev/ps4sdk)
* node.js to run server.js (or any alternative to serve /local)
* socat, netcat, etc. to send files and to communicate with the standard IO

## Important

The elf loader does not support dynamically linked executables. All libraries need to be statically linked into the executable. [ps4sdk](https://github.com/ps4dev/ps4sdk) provides a variaty of position independant, statically linkable libraries, such as a libc, for the PS4. Depending on their build system and requirenments, you can compile third party libraries using the ps4-lib target of the sdk. Alternatively you will have to alter their build system to compile them as PIC statically linked libraries.

## Example
```bash
# Build as raw binary to bin/ and then convert to ldr.js in /local (you can 'make keepelf=1' to debug)
make clean && make

# Start server
cd local
node server.js

# Browse ps4 browser to local server (<local>:5350)
# Wait until the browser hangs in 'step 5'

# Connect debug/stdio channel
socat - TCP:<ps4>:5052

# Send elf file to the user space process for execution
socat -u FILE:ps4sdk-examples/libless/stress/bin/stress TCP:<ps4>:5053
# OR Send kernel elf file (mode for long-running code, or module-like code)
socat -u FILE:ps4sdk-examples/kernel/function-hook/bin/function-hook TCP:<ps4>:5055
# OR Send kernel elf file (runs in the browsers process, but is loaded and executed into the kernel)
socat -u FILE:ps4sdk-examples/kernel/cache/bin/cache TCP:<ps4>:5054

# Some examples (esp. kernel) use a second socket for their comminication. The default choice is 5088
# Connect to it after the upload to trigger the execution of the code - please see the examples sources for more
socat - TCP:<ps4>:5088
```

## Docker images
A stand alone elf-loader container is available (but currently a bit large):

```bash
# Make sure newest container is used (only do this as needed)
docker pull ps4dev/elf-loader
# Run the elf loader (listens on port 5350)
docker run -p 5350:5350 --rm ps4dev/elf-loader&
# Stop elf loader
docker kill $(docker ps -q -f ancestor=ps4dev/elf-loader)
```
