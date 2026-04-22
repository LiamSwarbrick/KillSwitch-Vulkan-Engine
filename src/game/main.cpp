#include "core/core.h"
#include "core/input.h"
#include "renderer/renderer.h"
#include "renderer/debug_ui_api.h"
#include "foundations/scene.h"
#include "core/components.h"
#include "core/animation.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

CameraInfo temp_camera(float dt)
{
    static glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);

    // Rotation state
    static float yaw   = -90.0f;  // Looking down -Z initially
    static float pitch =  0.0f;

    static constexpr float MOUSE_SENSITIVITY  = 0.10f;   // degrees per pixel
    static constexpr float GAMEPAD_LOOK_SPEED = 120.0f;  // degrees per second at full deflection
    static constexpr float MOVE_SPEED         = 5.0f;    // units per second
    static constexpr float SPRINT_MULTIPLIER  = 4.0f;

    // --- ROTATION: mouse delta + right stick ---
    float mouse_dx, mouse_dy;
    Input_GetMouseDelta(&mouse_dx, &mouse_dy);

    yaw   +=  mouse_dx * MOUSE_SENSITIVITY;
    pitch -=  mouse_dy * MOUSE_SENSITIVITY;  // inverted: mouse up = look up

    yaw   += (Input_GetActionValue(ACTION_CAMERA_RIGHT) - Input_GetActionValue(ACTION_CAMERA_LEFT)) * GAMEPAD_LOOK_SPEED * dt;
    pitch += (Input_GetActionValue(ACTION_CAMERA_UP)    - Input_GetActionValue(ACTION_CAMERA_DOWN)) * GAMEPAD_LOOK_SPEED * dt;

    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // --- DIRECTION VECTOR ---
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // --- MOVEMENT: free-cam, WASD + E/Q, full forward including pitch ---
    float speed = MOVE_SPEED * dt;
    if (Input_IsActionPressed(ACTION_SPRINT)) speed *= SPRINT_MULTIPLIER;

    pos += forward * (Input_GetActionValue(ACTION_MOVE_FORWARD)  * speed);
    pos -= forward * (Input_GetActionValue(ACTION_MOVE_BACKWARD) * speed);
    pos -= right   * (Input_GetActionValue(ACTION_MOVE_LEFT)     * speed);
    pos += right   * (Input_GetActionValue(ACTION_MOVE_RIGHT)    * speed);
    pos += up      * (Input_GetActionValue(ACTION_MOVE_UP)       * speed);
    pos -= up      * (Input_GetActionValue(ACTION_MOVE_DOWN)     * speed);

    // --- VIEW MATRIX ---
    glm::mat4 view = glm::lookAt(pos, pos + forward, up);

    return {
        .view = view,
        .position = pos,
        .lense_distortion = 0.0f
    };
}

int main(int argc, char *argv[])
{
    bool enabled_validation_layers = true;
    // NOTE: For non production builds, we still want Vulkan validation layers on in release mode, because release mode can have different bugs
    // To make sure it's obvious when validation layers are used, we'll put it in the window title.
    // Note that validation layers degrade performance significantly, so should be disabled in performance metrics and absolutely in production builds
    char title[256] = {};
    snprintf(title, sizeof(title), "Close-quarters Adventure Game [%s]", enabled_validation_layers ? "VULKAN VALIDATION LAYERS ENABLED" : "VULKAN VALIDATION LAYERS DISABLED");
    SDL_Window* window = Core_Init((Core_InitInfo){
        title,
        1280, 720
    });

    // NOTE: Currently checking validation in release mode, but on realse you would normally disable it
    Renderer_InitInfo renderer_info = {
        .window = window,
        .enable_validation = enabled_validation_layers,
        .preferred_initial_settings = {  // Will fallback if these aren't possible
            .uncapped_fps = 0,
            .msaa_sample_count = 4,
            .fov_y = 70.0f
        }
    };
    Renderer_Init(&renderer_info);

    Input_Init("assets/keybindings.json");

    // Dunno whether this resource manager will end up in the final build, if no one is integrating it due to more important tasks

    // ResourceManager resource_manager = ResourceManager_Create((ResourceManagerCreateInfo){
    //     .debug_name = "GameAssets",
    //     .initial_capacity = 32
    // });

    // ResourceHandle boot_vert = ResourceManager_RequestBinary(
    //     &resource_manager,
    //     RESOURCE_TYPE_SHADER_BYTECODE,
    //     RESOURCE_RESIDENCY_BOOT,
    //     "shader.unlit.vert",
    //     "shaderspv/unlit.vert.spv"
    // );

    // ResourceHandle boot_frag = ResourceManager_RequestBinary(
    //     &resource_manager,
    //     RESOURCE_TYPE_SHADER_BYTECODE,
    //     RESOURCE_RESIDENCY_BOOT,
    //     "shader.unlit.frag",
    //     "shaderspv/unlit.frag.spv"
    // );

    // (void)boot_vert;
    // (void)boot_frag;

    // Set 4xMSAA to test settings API
    if (Renderer_GetSettingsCapabilities().max_msaa_samples >= 4)
    {
        Renderer_Settings settings = Renderer_GetSettings();
        settings.msaa_sample_count = 4;
        Renderer_ChangeSettings(settings);
    }

    
    /* LOADING NOTES
       Realistically, the splash screen assets would load first, then the main menu assets
       And while the user is on the main menu, we are loading the prefabs.
       That way, we can hide ALL of the latency and it will seem like there are no loading screens at all.
    */

    // Testing Scene and ECS
    Scene scene{};
    scene.StartUp();

    Asset* room_prefab = scene.LoadPrefab("assets/levels/testroom_new.gltf");
    Asset* cube_prefab = scene.LoadPrefab("assets/props/simple_cube.gltf");
    Asset* sphere_prefab = scene.LoadPrefab("assets/props/simple_sphere.gltf");
    Asset* capsule_prefab = scene.LoadPrefab("assets/props/simple_capsule.gltf");
    // TODO: Change the following 2 prefabs so they can be imported (add the boolean "Is ECS Entity" with the new script where it is needed)
    Asset* catPrefab = scene.LoadPrefab("assets/animations/scene.gltf");
    Asset* animationPrefab = scene.LoadPrefab("assets/animations/sceneglb.glb");

    scene.InstantiatePrefab(room_prefab, glm::vec3(0, 0, 0));
    scene.InstantiatePrefab(cube_prefab, glm::vec3(0, 5.1, 0));
    scene.InstantiatePrefab(cube_prefab, glm::vec3(3, 4.9, 0));
    scene.InstantiatePrefab(capsule_prefab, glm::vec3(0, 5, 2));
    scene.InstantiatePrefab(sphere_prefab, glm::vec3(4.7, 7, 0.1));
    scene.InstantiatePrefab(sphere_prefab, glm::vec3(-4.7, 7, -0.1));
    scene.InstantiatePrefab(sphere_prefab, glm::vec3(0.1, 7, -4.7));
    scene.InstantiatePrefab(sphere_prefab, glm::vec3(-0.1, 7, 4.7));
    scene.InstantiatePrefab(catPrefab, glm::vec3(0, 0, 0));
    scene.InstantiatePrefab(animationPrefab, glm::vec3(5, 20, 0));
    // render a second cat
    EntityID playerEntity = scene.InstantiatePrefab(catPrefab, glm::vec3(10, 0, 10));

    scene.BuildRendererScene();

    // TODO: Debug UI is built around the idea of 1 asset at the moment.
    //       This must change with the new scene system that can load many asset prefabs.
    DebugUI_SetECS(&scene.GetECS());
    DebugUI_SetAsset(animationPrefab);

    bool running = true;

    // Set up the time tracker
    uint64_t last_time = SDL_GetTicksNS();

    while (running)
    {
		// delta time calculation for testing
        uint64_t current_time = SDL_GetTicksNS();
        float dt = (float)(current_time - last_time) / 1000000000.0f;
        last_time = current_time;
        if (dt > 0.1f) dt = 0.1f;   

        // Event Loop
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT) running = false;
            Renderer_ListenToWindowEvent(event);
            Input_ProcessEvent(event);
        }
        Input_Update();

        // Capture mouse for free-camera rotation
        SDL_SetWindowRelativeMouseMode(window, true);

        // controller test not ideal at all
        const bool* state = SDL_GetKeyboardState(NULL);
        float speed = 5.0f * dt;
        glm::vec3 movement(0.0f);

        if (state[SDL_SCANCODE_I]) movement.z -= speed;
        if (state[SDL_SCANCODE_K]) movement.z += speed;

        if (state[SDL_SCANCODE_J]) movement.x -= speed;
        if (state[SDL_SCANCODE_L]) movement.x += speed;

        if (glm::length(movement) > 0.0f)
        {
            for (uint32_t i = 0; i < catPrefab->node_count; i++)
            {
                C_Transform* tf = scene.GetECS().GetComponentPtr<C_Transform>(playerEntity + i);
                if (tf)
                {
                    // Apply movement directly to the world translation (column 3 of the matrix)
                    tf->matrix[3][0] += movement.x;
                    tf->matrix[3][1] += movement.y;
                    tf->matrix[3][2] += movement.z;
                }
            }
        }

        // Game ticks
        scene.Update(dt);

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            scene.Render();

            Renderer_DrawFrame(temp_camera(dt));
        }
    }

    scene.Shutdown();
    Input_Shutdown();
    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
