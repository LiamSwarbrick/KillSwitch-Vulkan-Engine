## Adventure Engine (working title, we can choose one later)

### Architecture Design So Far
Settling on a modular approach like this:

NOTE: modules e.g. /core/ have their api visible in /core/, while internal implementation (internal headers and source) for these modules that aren't part of an exported API should go in e.g. /core/impl/.
```
ARCHITECTURE NOTES:
Internal headers MUST NOT include the public API headers, but the .cpp implementation source code can include the public headers.

src/core/
|- core.h             <-- PUBLIC: No SDL includes here. Pure C/C++ types.
|- impl/
|  |- core_internal.h <-- PRIVATE: SDL includes, platform-specifics.
|  |- core.cpp        <-- IMPLEMENTATION.

src/renderer/
|- renderer.h              <-- PUBLIC: No Vulkan includes.
|- impl/
|  |- renderer_internal.h <-- PRIVATE: volk.h, vk_mem_alloc.h, etc.
|  |- renderer.cpp        <-- IMPLEMENTATION: Uses vulkan to render,..
NOTE: No Vulkan types in exposed API, only opaque handles and transform data.

src/game/
|- include/
|  |- ...
|- main.cpp  <-- Console app that the other modules as static libs to glue together the assetsystem, simulation, and renderer into a game. 

Also TODO similarly for: Asset system (assetsys/) and simulation system (simulation/).
```

```
MEMORY LEAK DETECTION:
Make sure each subsystem uses a ThreadAllocTracker (defined in core/my_c_runtime.h).
And use L_calloc and L_free instead of malloc and free
Example: See ThreadAllocTracker in renderer/impl/internal_state.h.
For runtime stuff try to use larger allocations and bulk data structures.
Contained one off allocations during init, shutdown and data uploads are fine,
but if allocs and frees are happening each frame at runtime, especially on the main
thread (not background asset loader threads), then that can be bad.

On shutdown of a module or subsystem of a module,
check for memory leaks with check_tracker_for_memory_leaks() e.g.:
    check_tracker_for_memory_leaks(&renderstate.main.tt);

This will output the exact file and line of code where an allocation occured that did was not freed.
And will output a green success message if all allocations were freed.
(In release mode all the overhead (which isn't much) of the alloc tracking is gone).
```

### Build
```
The premake will build SDL from source, but you will likely need to install SDL's dependencies:
- https://wiki.libsdl.org/SDL3/README-linux#build-dependencies

On linux: do
$ ./premake5 gmake
$ make -j
$ ./bin/debug-game.exe
$
$ make -j config=release
$ ./bin/release-game.exe
```

Here's a simple way to generate intellisense if using clangd on vscode:
```
# Install bear, which listens to compile commands and generates the clangd 'compile_commands.json'
# Then build with
$ bear -- make -j
```

```
# On a fresh linux machine with bear installed, you can just do:
./premake5 gmake && bear -- make -j

# Or if you don't have bear and have some other way of getting intellisense:
./premake5 gmake && make -j
```

```
# I like doing this to build after added / changing file names and locations:
make clean && ./premake5 gmake && bear -- make -j
```
