#include <iostream>

#include "Renderer.h"
#include "Resources.h"
#include "FlanRenderer.h"

int main()
{
    // Initialize resource manager
    Flan::ResourceManager resources;

    // Initialize renderer
    Flan::RendererDX12 renderer(&resources);
    renderer.init(1280, 720);

    Flan::ResourceHandle quad_handle = resources.load_mesh("Assets/Models/quad.gltf");
    resources.upload_mesh_to_gpu(quad_handle, renderer.get_device());

    // Debug memory
    resources.get_allocator_instance()->debug_memory();

    // Define where the quad should be
    Flan::Transform quad_transform {
        {0, 0, 0},
        {1, 0, 0, 0},
        {1, 1, 1},
    };

    // Main loop
    while (!renderer.should_close()) {
        printf("frame\n");
        renderer.begin_frame();
        renderer.draw_model({ quad_handle, quad_transform });
        renderer.end_frame();
    }

    puts("All is good!");
}
