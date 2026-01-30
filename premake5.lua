
local EXTERNAL = "extern/"
local SRC = "src/"

local SDL_DIR = EXTERNAL .. "SDL"
local SDL_BUILD_DIR = SDL_DIR .. "/build"

local VULKAN_SDK = os.getenv("VULKAN_SDK") or ""
-- TODO: Check for good enough Vulkan SDK version. e.g. 1.4

include_paths = {}
include_paths.SDL3 = SDL_DIR .. "/include"
include_paths.Vulkan = VULKAN_SDK .. "/include",
filter "system:windows"
    include_paths.Vulkan = VULKAN_SDK .. "/Include"
filter "*"


workspace "AdventureEngine"
    architecture "x64"
    configurations { "debug", "release" }
    startproject "game"

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
    staticruntime "off"

    files { SRC .. "core/**.h", SRC .. "core/**.cpp" }
    
    includedirs { 
        SRC, 
        SDL_DIR .. "/include" 
    }

-- --------------------------------------------------------------------
-- Renderer Module (Vulkan implementation)
-- --------------------------------------------------------------------
project "renderer"
    kind "StaticLib"
    language "C++"
    cppdialect "C++23"
    staticruntime "off"

    files { SRC .. "renderer/**.h", SRC .. "renderer/**.cpp" }

    includedirs { 
        SRC, 
        SRC .. "renderer/include", -- Internal includes
        SDL_DIR .. "/include",
        VULKAN_INCLUDE
    }


-- --------------------------------------------------------------------
-- Game:
-- --------------------------------------------------------------------
project "game"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"    

    files { SRC .. "game/**.h", SRC .. "game/**.cpp" }

    prebuildcommands {
        -- Build SDL with their cmake build system
        "mkdir -p " .. SDL_BUILD_DIR,
        "{ cd " .. SDL_BUILD_DIR .. " && cmake .. -DCMAKE_BUILD_TYPE=%{cfg.buildcfg == 'debug' and 'Debug' or 'Release'} && make -j; }"
    }

    includedirs {
        SRC,
        include_paths.SDL3,
        include_paths.Vulkan
    }

    libdirs {
        SDL_BUILD_DIR
        -- "%{VULKAN_SDK}/Lib",
    }

    links {
        "SDL3",   -- The lib we just built via cmake in prebuildcommands
        "vulkan"  -- System vulkan folder

        -- Dependency enforcement: Game links against these modules:
        -- "core",
        -- "renderer"
        -- "assetsys"
    }
