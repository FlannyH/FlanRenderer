#include <iostream>

#include "Renderer.h"
#include "Resources.h"
#include "FlanRenderer.h"

static float calculate_delta_time()
{
    static std::chrono::time_point<std::chrono::steady_clock> start;
    static std::chrono::time_point<std::chrono::steady_clock> end;

    end = std::chrono::steady_clock::now();
    const std::chrono::duration<float> delta = end - start;
    start = std::chrono::steady_clock::now();
    return delta.count();
}

int main()
{
    // Initialize resource manager
    Flan::ResourceManager resources;

    // Initialize renderer
    Flan::RendererDX12 renderer(&resources);
    renderer.init(1280, 720);

    Flan::ResourceHandle quad_handle = resources.load_mesh("Assets/Models/helmet.gltf");
    renderer.upload_mesh(quad_handle, resources);

    // Debug memory
    resources.get_allocator_instance()->debug_memory();

    // Define where the quad should be
    Flan::Transform quad_transform{
        {1, 0, -3},
        {1, 0, 0, 0},
        {1, 1, 1},
    };
    // Define where the quad should be
    Flan::Transform quad_transform2{
        {-1, 0, -3},
        {1, 0, 0, 0},
        {1, 1, 1},
    };

    // Main loop
    while (!renderer.should_close()) {
        //Calculate deltatime
        float delta_time = calculate_delta_time();
        if (delta_time > 0.1f)
            delta_time = 0.1f;

        // Render
        renderer.begin_frame();
        renderer.draw_model({ quad_handle, quad_transform });
        renderer.draw_model({ quad_handle, quad_transform2 });
        quad_transform.rotation *= glm::quat(glm::vec3{ 0, 2.0f * delta_time, 0 });
        quad_transform2.rotation *= glm::quat(glm::vec3{ 0, -2.0f * delta_time, 0 });
        renderer.end_frame();
    }

    puts("All is good!");
}
