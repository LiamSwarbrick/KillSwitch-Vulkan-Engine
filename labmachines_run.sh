# On Lab machines, it's pretty locked down, so there are some differences.
#   NOTE: Slow the first time, because it builds SDL
#   But after that, all builds are cached, so you can run this script again and see the program immediately

# Latest GCC otherwise it won't build (some people using C++23 features or something)
module load gcc

# Uni options means it uses the included glslc compiler
# On our other build targets, we just have glslc installed on our machines.
./premake5 gmake --uni

make -j

# To run the program, the SDL build won't be found unless we set the runtime linker path like this...
export LD_LIBRARY_PATH=./extern/SDL/build:$LD_LIBRARY_PATH

echo Running debug build
./bin/debug-game
