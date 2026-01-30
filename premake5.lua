
-- include_paths = {
--     SDL3 = "extern/SDL/include",
--     Vulkan = os.getenv("VULKAN_SDK") .. "/Include",
--     Core = "core/"
-- }

local SRC = "src/"

workspace "AdventureEngine"  -- Working name, we can decided on a different one
    architecture "x64"
    configurations { "debug", "release" }
    startproject "game"

    targetdir ("bin")
    objdir ("build-artefacts/%{cfg.buildcfg}")

    filter "configurations:Debug"
        symbols "On"
        targetprefix "debug-"
    filter "configurations:Release"
        optimize "On"
        defines { "NDEBUG" }
        targetprefix "release-"
    filter "*"

    -- VULKAN_SDK = os.getenv("VULKAN_SDK")
    SDL3_PATH = "extern/SDL/"


project "game"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++23"
    location (SRC .. "game")
    
    files {
        SRC .. "game/**.h",
        SRC .. "game/**.cpp"
    }

    -- TODO: Game uses each module
    includedirs {
        SRC,  -- <- All the submodules e.g. renderer are visible through their names e.g. renderer/renderer.h
        -- SDL3_PATH .. "include"
    }

    -- Dependency enforcement: Game links against these modules
    links {
        -- "Core",
        -- "Renderer"
        -- "AssetSys"
    }

    -- Link the actual Third Party binaries
    libdirs {
        -- "%{VULKAN_SDK}/Lib",
        -- "%{SDL3_PATH}/lib"
    }
    links {
        -- "vulkan",
        -- "SDL3"
    }
