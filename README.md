## Adventure Engine (working title, we can choose one later)

### Architecture Design So Far
Settling on a modular approach like this:

NOTE: modules e.g. /core/ have their api visible in /core/, while internal implementation (internal headers and source) for these modules that aren't part of an exported API should go in e.g. /core/impl/.
```
ARCHITECTURE NOTES:
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
