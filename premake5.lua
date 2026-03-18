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

-- SDL
local sdl_build_type = "Release" -- default (no need to debug SDL right?)
filter "system:windows"
    libdirs { SDL_BUILD_DIR .. "/" .. sdl_build_type }
filter "not system:windows"
    libdirs { SDL_BUILD_DIR }

-- VULKAN_SDK
filter "system:windows"
    include_paths.Vulkan = VULKAN_SDK .. "/Include"
    lib_dirs.Vulkan      = VULKAN_SDK .. "/Lib"
filter "not system:windows"
    include_paths.Vulkan = VULKAN_SDK .. "/include"
    lib_dirs.Vulkan      = VULKAN_SDK .. "/lib"

filter {}

-- Clean action: cleanall
newaction {
    trigger = "cleanall",
    description = "Clean all generated build files",
    execute = function ()

        -- Clean SDL
        local cmd
        if os.host() == "windows" then
            cmd = string.format(
                "cd %s && cmake --build . --config %s --target clean",
                SDL_BUILD_DIR, sdl_build_type
            )
        else
            cmd = string.format(
                "cd %s && cmake --build . --target clean",
                SDL_BUILD_DIR
            )
        end
        os.execute(cmd)
        os.rmdir(SDL_BUILD_DIR)

        -- Clean other dirs
        local dirs = {
            "bin",
            "build-artefacts",
            "shaderspv"
        }

        for _, dir in ipairs(dirs) do
            if os.isdir(dir) then
                print("Removing " .. dir)
                os.rmdir(dir .. "/**")
            end
        end
    end
}


local function ensure_sdl_built()
    if os.isdir(SDL_BUILD_DIR) then
        print("SDL already built — skipping")
        return
    end

    print("SDL build directory not found, building SDL...")

    os.mkdir(SDL_BUILD_DIR)

    if os.host() == "windows" then
        cmd = table.concat({
            "cd " .. SDL_BUILD_DIR,
            "cmake .. -DSDL_TESTS=OFF",
            "cmake --build . --config " .. sdl_build_type,
        }, " && ")
    else
        cmd = table.concat({
            "cd " .. SDL_BUILD_DIR,
            "cmake .. -DCMAKE_BUILD_TYPE=" .. sdl_build_type .. " -DSDL_TESTS=OFF",
            "cmake --build . -j"
        }, " && ")
    end

    local result = os.execute(cmd)
    if result ~= true and result ~= 0 then
        error("SDL build failed")
    end
end
if _ACTION ~= "cleanall" then
    ensure_sdl_built()
end

workspace "AdventureEngine"
    architecture "x64"
    configurations { "debug", "release" }
    startproject "game"

    targetdir ("bin")
    objdir ("build-artefacts/%{cfg.buildcfg}")

    -- Using clang
    filter "system:windows"
        toolset "clang"
    filter "system:linux"
        toolset "clang"
    filter "system:macosx"
        toolset "clang"
    filter {}

    filter "toolset:clang"
        -- VMA spits out a billion Nullability warnings with clang
        buildoptions { "-Wno-nullability-completeness" }
    filter {}

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
    -- ECS Module
    -- --------------------------------------------------------------------
    project "ecs"
        kind "StaticLib"
        language "C++"
        cppdialect "C++23"

        files {
            SRC .. "ecs/**.hpp",
            SRC .. "ecs/impl/**.hpp"
        }

        includedirs { 
            SRC,  -- Exported API headers
            SRC .. "ecs",
            SRC .. "ecs/impl",  -- Internal include headers
            include_paths.SDL3
        }

        libdirs {
            lib_dirs.SDL3
        }

        links {
            "core",
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
            SRC .. "renderer/impl/**.cpp",
            EXTERNAL .. "volk/volk.c",

            -- Shader src
            SRC .. "renderer/shadersrc/**.vert",
            SRC .. "renderer/shadersrc/**.frag",
            SRC .. "renderer/shadersrc/**.comp"
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

        filter "system:windows"
            links { "vulkan-1" }
        filter "not system:windows"
            links { "vulkan" }
        filter "*"

        links {
            "core",
            "SDL3"
        }

        -- Shader compilation
        prebuildcommands {
            "{MKDIR} shaderspv"
        }
        filter "files:**.vert or files:**.frag or files:**.comp"
            buildmessage "Compiling shader %{file.relpath}"
            buildcommands {
                "glslc %{file.relpath} -o shaderspv/%{file.name}.spv"
            }
            buildoutputs {
                "shaderspv/%{file.name}.spv"
            }
        filter {}


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

        filter "system:windows"
            postbuildcommands {
                "{COPYFILE} " .. path.getabsolute(SDL_BUILD_DIR .. "/" .. sdl_build_type .. "/SDL3.dll") .. " %{cfg.targetdir}"
            }
        filter "*"