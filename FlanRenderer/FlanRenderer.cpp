#include <iostream>

#include "Renderer.h"
#include "Resources.h"

int main()
{
    // Initialize renderer
    Flan::RendererDX12 renderer;
    renderer.init();

    // Initialize resource manager
    Flan::ResourceManager resources;
    Flan::ResourceHandle quad_handle = resources.load_mesh("Assets/Models/quad.gltf");
    resources.upload_mesh_to_gpu(quad_handle, renderer.get_device());

    // Debug memory
    resources.get_allocator_instance()->debug_memory();

    // Create a window
    if (!renderer.create_window(1280, 720, "FlanRenderer (DirectX 12)")) {
        throw std::exception("Could not create window!");
    }

    puts("All is good!");
}
