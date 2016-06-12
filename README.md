#Elf loader

Runs ps4sdk elf files in-process on your PS4.

## Prerequisites
* Basic build-tools (clang due to ps4 support)
* ps4dev/ps4sdk
* node.js to run the server
* socat, netcat, etc.

You will also need a current version of clang or alter the Makefile (remove -target specifically additionally ld/objectcopy may produce undesired (large) binaries).

You can of course use other tools for the server, upload or debug output (socat, netcat or a C implementation for example).

The elf loader does not support dynamically linked executables. All libraries need to be statically linked into the executable. You can try musl for an easy to build and use statically linkable libc on your x86-64 linux system (e.g. to test elfs
locally). Libps4 provides a statically linkable libc for the PS4.

##Example
```
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

##Credit
- Rop/Exec stuff goes to CTurt, flatz, SKFU, droogie, Xerpi, Hunger, Takezo, nas, Proxima
	- No idea who did what - contact me to clarify (open an issue)
	- Yifan (?)
