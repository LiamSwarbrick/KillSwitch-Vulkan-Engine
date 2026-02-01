
local EXTERNAL = "extern/"
local SRC = "src/"

local SDL_DIR = EXTERNAL .. "SDL"
local SDL_BUILD_DIR = SDL_DIR .. "/build"

local VULKAN_SDK = os.getenv("VULKAN_SDK") or ""
-- TODO: Check for good enough Vulkan SDK version. e.g. 1.4

include_paths = {}
include_paths.SDL3 = SDL_DIR .. "/include"
include_paths.Vulkan = VULKAN_SDK .. "/include"
include_paths.volk = "volk"

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

    local build_type = "Release" -- default

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

    -- prebuildcommands {
    --     -- Build SDL with their cmake build system
    --     "mkdir -p " .. SDL_BUILD_DIR,
    --     "{ cd " .. SDL_BUILD_DIR .. " && cmake .. -DCMAKE_BUILD_TYPE=%{cfg.buildcfg == 'debug' and 'Debug' or 'Release'} && make -j; }"
    -- }
    ensure_sdl_built()

    files { SRC .. "core/**.h", SRC .. "core/**.cpp" }

    includedirs { 
        SRC,
        include_paths.SDL3
    }

    -- This makes any project linking 'core' also search this directory
    usageincludedirs {
        SRC .. "core/include"  -- Internal include headers
    }

    libdirs {
        lib_dirs.SDL3
    }

    links {
        "SDL3",   -- The lib we just built via cmake in prebuildcommands
    }

-- --------------------------------------------------------------------
-- Renderer Module (Vulkan implementation)
-- --------------------------------------------------------------------
project "renderer"
    kind "StaticLib"
    language "C++"
    cppdialect "C++23"
    staticruntime "off"

    files {
        SRC .. "renderer/**.h",
        SRC .. "renderer/**.cpp",
        EXTERNAL .. "volk/volk.c"
    }

    includedirs { 
        SRC,                       -- Exported API headers
        include_paths.volk,
        include_paths.Vulkan,
    }

    usageincludedirs {
        SRC .. "renderer/include",
    }

    libdirs {
        lib_dirs.Vulkan
    }

    links {
        "core", -- TODO: Finish syntax here
        "vulkan"  -- System vulkan folder
    }


-- --------------------------------------------------------------------
-- Game:
-- --------------------------------------------------------------------
project "game"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"    

    files { SRC .. "game/**.h", SRC .. "game/**.cpp" }

    includedirs {
        SRC,
        SRC .. "game/include"
    }

    libdirs {
        
    }

    links {
        -- Dependency enforcement: Game links against these modules:
        "core",
        "renderer"
        -- "assetsys"
    }
