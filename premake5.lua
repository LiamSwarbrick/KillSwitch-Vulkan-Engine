local EXTERNAL = "extern/"
local SRC = "src/"

local SDL_DIR = EXTERNAL .. "SDL"
local SDL_BUILD_DIR = SDL_DIR .. "/build"

local VULKAN_SDK = os.getenv("VULKAN_SDK") or ""
-- TODO: Check for good enough Vulkan SDK version. e.g. 1.4

include_paths = {}
include_paths.SDL3 = SDL_DIR .. "/include"
include_paths.Vulkan = VULKAN_SDK .. "/include"
include_paths.volk = EXTERNAL .. "volk"
include_paths.VMA = EXTERNAL .. "VMA"
include_paths.glm = EXTERNAL .. "glm"
include_paths.stb = EXTERNAL .. "stb"

lib_dirs = {}
lib_dirs.SDL3 = SDL_BUILD_DIR
lib_dirs.Vulkan = VULKAN_SDK .. "/lib"

filter "system:windows"
    include_paths.Vulkan = VULKAN_SDK .. "/Include"
    lib_dirs.Vulkan = VULKAN_SDK .. "/Lib"
filter "*"

local function ensure_sdl_built()
    if os.isdir(SDL_BUILD_DIR) then
        print("SDL already built — skipping")
        return
    end

    print("SDL build directory not found, building SDL...")

    os.mkdir(SDL_BUILD_DIR)

    local build_type = "Release" -- default (no need to debug SDL right?)

    -- Premake knows nothing about cfg here, so pick one or build both
    local cmd = table.concat({
        "cd " .. SDL_BUILD_DIR,
        "cmake .. -DCMAKE_BUILD_TYPE=" .. build_type,
        "cmake --build . -j"
    }, " && ")

    local result = os.execute(cmd)
    if result ~= 0 then
        error("SDL build failed")
    end
end


workspace "AdventureEngine"
    architecture "x64"
    configurations { "debug", "release" }

    targetdir ("bin")
    objdir ("build-artefacts/%{cfg.buildcfg}")

    -- Shared config for all projects:
    filter "configurations:Debug"
            symbols "On"
            targetprefix "debug-"
        filter "configurations:Release"
            optimize "On"
            defines { "NDEBUG" }
            targetprefix "release-"
        filter "*"


    -- --------------------------------------------------------------------
    -- Core Module (Windowing, Input)
    -- --------------------------------------------------------------------
    project "core"
        kind "StaticLib"
        language "C++"
        cppdialect "C++23"

        ensure_sdl_built()

        files {
            SRC .. "core/**.h",
            SRC .. "core/impl/**.cpp"
        }

        includedirs { 
            SRC,  -- Exported API headers
            SRC .. "core",
            SRC .. "core/impl",  -- Internal include headers
            include_paths.SDL3,
            include_paths.glm
        }

        libdirs {
            lib_dirs.SDL3
        }

        links {
            "SDL3"   -- The lib we just built via cmake in prebuildcommands
        }


    -- --------------------------------------------------------------------
    -- Renderer Module (Vulkan implementation)
    -- --------------------------------------------------------------------
    project "renderer"
        kind "StaticLib"
        language "C++"
        cppdialect "C++23"

        files {
            SRC .. "renderer/**.h",
            SRC .. "renderer/impl/**.cpp",  -- NOTE: Removed **.cpp for now, but put it back when due rework folder is gone
            EXTERNAL .. "volk/volk.c"
        }

        defines {
            "VK_NO_PROTOTYPES"
        }

        includedirs {
            SRC,
            SRC .. "renderer",
            SRC .. "renderer/impl",
            include_paths.SDL3,
            include_paths.volk,
            include_paths.Vulkan,
            include_paths.VMA,
            include_paths.glm,
            include_paths.stb
        }

        libdirs {
            lib_dirs.Vulkan
        }

        links {
            "vulkan",  -- System vulkan folder
            "core",
            "SDL3"
        }


    -- --------------------------------------------------------------------
    -- Game:
    -- --------------------------------------------------------------------
    project "game"
        kind "ConsoleApp"
        language "C++"
        cppdialect "C++23"

        files {
            SRC .. "game/**.h",
            SRC .. "game/**.cpp"
        }

        includedirs {
            SRC,
            SRC .. "game/include",
            include_paths.SDL3,
            include_paths.glm
        }

        libdirs {
            lib_dirs.SDL3
        }

        links {  -- NOTE: Must link from highest level dependency to lowest level.   
            "renderer",
            "core",
            "SDL3"
        }
