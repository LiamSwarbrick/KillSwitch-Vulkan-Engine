## Adventure Engine (working title, we can choose one later)

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
