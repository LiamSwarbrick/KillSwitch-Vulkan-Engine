#include "core/core.h"
#include "renderer/renderer.h"
#include "foundations/scene.h"
#include "core/components.h"
#include "core/animation.h"

// TODO: Implementation is exposed?
#include "renderer/impl/debug_ui/debug_ui.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

static DebugUI::DebugUIState debug_ui_state;

void OnImGuiBuild(void* user_data)
{
    Scene* scene = (Scene*)user_data;
    DebugUI::Draw(debug_ui_state, scene->GetECS());
}

glm::mat4 temp_camera_view_matrix()
{
    static glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f);

    // Rotation state
    static float yaw   = -90.0f;  // Looking down -Z initially
    static float pitch =  0.0f;

    const bool* state = SDL_GetKeyboardState(NULL);

    float move_speed = 0.05f;
    float rot_speed  = 1.5f;  // Degrees per frame

    if (state[SDL_SCANCODE_LCTRL]) move_speed *= 20.0f;

    // --- ROTATION (arrow keys) ---
    if (state[SDL_SCANCODE_LEFT])  yaw   -= rot_speed;
    if (state[SDL_SCANCODE_RIGHT]) yaw   += rot_speed;
    if (state[SDL_SCANCODE_UP])    pitch += rot_speed;
    if (state[SDL_SCANCODE_DOWN])  pitch -= rot_speed;

    // Clamp pitch to avoid flipping
    if (pitch > 89.0f)  pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    // --- DIRECTION VECTOR ---
    glm::vec3 forward;
    forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward.y = sin(glm::radians(pitch));
    forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(forward);

    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // --- MOVEMENT (WASD relative to camera) ---
    if (state[SDL_SCANCODE_W]) pos += forward * move_speed;
    if (state[SDL_SCANCODE_S]) pos -= forward * move_speed;
    if (state[SDL_SCANCODE_A]) pos -= right   * move_speed;
    if (state[SDL_SCANCODE_D]) pos += right   * move_speed;
    if (state[SDL_SCANCODE_E]) pos += up  * move_speed;
    if (state[SDL_SCANCODE_Q]) pos -= up  * move_speed;

    // --- VIEW MATRIX ---
    glm::mat4 view = glm::lookAt(pos, pos + forward, up);

    return view;
}

int main(int argc, char *argv[])
{
    bool is_debugging = true;
#ifdef NDEBUG
    is_debugging = false;
#endif

    SDL_Window* window = Core_Init((Core_InitInfo){
        "Close-quarters Adventure Game",
        1280, 720
    });

    // NOTE: Currently checking validation in release mode, but on realse you would normally disable it
    Renderer_InitInfo renderer_info = {
        .window = window,
        .enable_validation = 1,//is_debugging
        .preferred_initial_settings = {  // Will fallback if these aren't possible
            .uncapped_fps = 0,
            .msaa_sample_count = 1,
            .fov_y = 90.0f
        }
    };
    Renderer_Init(&renderer_info);

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
    Renderer_SetImGuiCallback(OnImGuiBuild, &scene);
    scene.StartUp();

    Asset* catPrefab = scene.LoadPrefab("assets/animations/cat.gltf");
    Asset* animationPrefab = scene.LoadPrefab("assets/animations/sceneglb.glb");

    scene.InstantiatePrefab(catPrefab, glm::vec3(0, 0, 0));
    scene.InstantiatePrefab(animationPrefab, glm::vec3(5, 20, 0));
    // render a second cat
    AdvEng::EntityID playerEntity = scene.InstantiatePrefab(catPrefab, glm::vec3(10, 0, 10));

    scene.BuildRendererScene();


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


            // Animation Testing, can be removed whenever
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                auto view = scene.GetECS().GetView<C_AnimatedMesh>();

                view.ForEach([&](AdvEng::EntityID player, C_AnimatedMesh& anim)
                {
                    switch (event.key.scancode)
                    {
                    case SDL_SCANCODE_1:
                        SDL_Log("Test: Blending to Idle");
                        PlayAnim(anim, "Idle", 0.5f);
                        break;

                    case SDL_SCANCODE_2:
                        SDL_Log("Test: Blending to Walk (loop on)");
                        PlayAnim(anim, "Walk", 0.5f);
                        SetLooping(anim, anim.lowerBodyLayer, true);
                        break;

                    case SDL_SCANCODE_3:
                        SDL_Log("Test: Triggering Upper Body Reload (loop on)");
                        PlayUpperBodyAnim(anim, "Reload", 0.3f);
                        SetLooping(anim, anim.upperBodyLayer, true);
                        break;

                    case SDL_SCANCODE_4:
                        SDL_Log("Test: Stopping Lower Body Action");
                        StopAnim(anim, 0.5f);
                        break;

                    case SDL_SCANCODE_5:
                        SDL_Log("Test: Stopping Upper Body Action");
                        StopUpperBodyAnim(anim, 0.5f);
                        break;

                    case SDL_SCANCODE_6:
                        SDL_Log("Test: Full Body Hit Reaction");
                        PlayFullBodyAnim(anim, "Idle", 0.1f);
                        break;

                    case SDL_SCANCODE_7:
                        SDL_Log("Test: Lower Looping Off");
                        SetLooping(anim, anim.lowerBodyLayer, false);
                        break;

                    case SDL_SCANCODE_8:
                        SDL_Log("Test: Upper Looping Off");
                        SetLooping(anim, anim.upperBodyLayer, false);
                        break;

                    default: break;
                    }
                });

                // Toggle Aiming with T
                if (event.key.scancode == SDL_SCANCODE_T) {
                    auto view = scene.GetECS().GetView<C_AnimatedMesh>();
                    view.ForEach([&](AdvEng::EntityID p, C_AnimatedMesh& anim) {
                        anim.isAiming = !anim.isAiming;
                        SDL_Log("Aiming: %s", anim.isAiming ? "ON" : "OFF");
                    });
                }
            }

            // keep this
            Renderer_ListenToWindowEvent(event);
        }

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
                    tf->position += movement;
                    tf->matrix = glm::translate(glm::mat4(1.0f), tf->position) * glm::mat4_cast(tf->rotation);
                }
            }
        }

        // more aiming testing logic, can be removed whenever
        const bool* keyboard = SDL_GetKeyboardState(NULL);
        auto view = scene.GetECS().GetView<C_AnimatedMesh>();
        view.ForEach([&](AdvEng::EntityID p, C_AnimatedMesh& anim) {
            if (anim.isAiming) {
                float speed = 60.0f; // Degrees per second
                if (keyboard[SDL_SCANCODE_V])    anim.aimPitch += dt * speed;
                if (keyboard[SDL_SCANCODE_B])    anim.aimPitch -= dt * speed;
                if (keyboard[SDL_SCANCODE_N])    anim.aimYaw -= dt * speed;
                if (keyboard[SDL_SCANCODE_M])    anim.aimYaw += dt * speed;

                // Log it occasionally so you can see the values
                static float logTimer = 0;
                logTimer += dt;
                if (logTimer > 0.5f) {
                    SDL_Log("Aim - Pitch: %.2f, Yaw: %.2f", anim.aimPitch, anim.aimYaw);
                    logTimer = 0;
                }
            }
            });


        // Game ticks
        scene.Update(dt);

        // Rendering
        uint32_t flags = SDL_GetWindowFlags(window);
        if (!(flags & SDL_WINDOW_MINIMIZED))
        {
            // Do this buddo:
            // Renderer_PushRenderable(renderable);

            scene.Render();
            
            
            Renderer_DrawFrame(temp_camera_view_matrix());
        }
    }

    scene.Shutdown();
    Renderer_Shutdown();
    Core_Shutdown(window);

    return 0;
}
