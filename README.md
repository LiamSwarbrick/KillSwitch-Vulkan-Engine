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

Here's a simple way to generate intellisense if using clangd on vscode:
```
# Install bear, which listens to compile commands and generates the clangd 'compile_commands.json'
# Then build with
$ bear -- make -j
```