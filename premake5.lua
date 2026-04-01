-- University lab machines are super locked down, so build slightly differently
-- E.g. Use gcc instead of clang, and use the included ./glslc instead of the one in the user's path.
newoption {
    trigger     = "uni",
    description = "Build for university lab environment"
}
print "NOTE FOR UNIVERSITY LAB MACHINES: use ./premake5 gmake --uni"

local EXTERNAL = "extern/"
local SRC = "src/"

local SDL_DIR = EXTERNAL .. "SDL"
local SDL_BUILD_DIR = SDL_DIR .. "/build"
local SDL_BUILD_FLAGS = "" -- initialized later in the SDL section

local VULKAN_SDK = os.getenv("VULKAN_SDK") or ""
-- TODO: Check for good enough Vulkan SDK version. e.g. 1.4

include_paths = {}
include_paths.SDL3 = SDL_DIR .. "/include"
include_paths.Vulkan = VULKAN_SDK .. "/include"
include_paths.volk = EXTERNAL .. "volk"
include_paths.VMA = EXTERNAL .. "VMA"
include_paths.glm = EXTERNAL .. "glm"
include_paths.cgltf = EXTERNAL .. "cgltf"
include_paths.stb = EXTERNAL .. "stb"
include_paths.rapidjson = EXTERNAL .. "rapidjson"
include_paths.imgui = EXTERNAL .. "imgui"
include_paths.imgui_backends = EXTERNAL .. "imgui/backends"

lib_dirs = {}

-- SDL build flags
local sdl_build_type = "Release" -- default (no need to debug SDL right?)
if _ACTION == "vs2022" then
    SDL_BUILD_FLAGS = "-G \"Visual Studio 17 2022\""
end

-- VULKAN_SDK
if os.host() == "windows" then
    include_paths.Vulkan = VULKAN_SDK .. "/Include"
    lib_dirs.Vulkan      = VULKAN_SDK .. "/Lib"
    lib_dirs.SDL3        = SDL_BUILD_DIR .. "/" .. sdl_build_type
else
    include_paths.Vulkan = VULKAN_SDK .. "/include"
    lib_dirs.Vulkan      = VULKAN_SDK .. "/lib"
    lib_dirs.SDL3        = SDL_BUILD_DIR
end

-- Google's glsl compiler (by default we expect it installed)
glslc_cmd = "glslc"
if _OPTIONS["uni"] then
    -- But on lab machines we use the included executable
    glslc_cmd = "./glslc"
end

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
            "cmake " .. SDL_BUILD_FLAGS .. " .. -DSDL_TESTS=OFF",
            "cmake --build . --config " .. sdl_build_type,
        }, " && ")
    else
        cmd = table.concat({
            "cd " .. SDL_BUILD_DIR,
            "cmake .. -DCMAKE_BUILD_TYPE=" .. sdl_build_type .. " -DSDL_TESTS=OFF -DSDL_X11_XSCRNSAVER=OFF",
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
    filter "options:uni"
        toolset "gcc"
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
        cppdialect "C++latest"

        files {
            SRC .. "core/**.h",
            SRC .. "core/**.cpp",
            SRC .. "core/**.c"
        }

        includedirs { 
            SRC,  -- Exported API headers
            SRC .. "core",
            SRC .. "core/**",  -- Internal include headers
            include_paths.SDL3,
            include_paths.glm,
            include_paths.cgltf,
            include_paths.stb,
            include_paths.rapidjson,
            include_paths.imgui,
            include_paths.imgui_backends
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
        cppdialect "C++latest"

        files {
            SRC .. "renderer/**.h",
            SRC .. "renderer/impl/**.cpp",
            EXTERNAL .. "volk/volk.c",

            -- ImGui
            EXTERNAL .. "imgui/imgui.cpp",
            EXTERNAL .. "imgui/imgui_demo.cpp",
            EXTERNAL .. "imgui/imgui_draw.cpp",
            EXTERNAL .. "imgui/imgui_tables.cpp",
            EXTERNAL .. "imgui/imgui_widgets.cpp",
            EXTERNAL .. "imgui/backends/imgui_impl_sdl3.cpp",
            EXTERNAL .. "imgui/backends/imgui_impl_vulkan.cpp",


            -- Shader src
            SRC .. "renderer/shadersrc/**.vert",
            SRC .. "renderer/shadersrc/**.frag",
            SRC .. "renderer/shadersrc/**.comp",
            SRC .. "renderer/shadersrc/**.glsl"
        }

        defines {
            "VK_NO_PROTOTYPES",
            "IMGUI_IMPL_VULKAN_USE_VOLK"
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
            include_paths.stb,
            include_paths.cgltf,
            include_paths.imgui,
            include_paths.imgui_backends
        }

        libdirs {
            lib_dirs.Vulkan,
            lib_dirs.SDL3
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
                "%{glslc_cmd} %{file.relpath} -o shaderspv/%{file.name}.spv"
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
        cppdialect "C++latest"

        files {
            SRC .. "game/**.h",
            SRC .. "game/**.cpp"
        }

        includedirs {
            SRC,
            SRC .. "game",
            include_paths.SDL3,
            include_paths.glm,
            include_paths.cgltf,
            include_paths.rapidjson,
            include_paths.imgui,
            include_paths.imgui_backends
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