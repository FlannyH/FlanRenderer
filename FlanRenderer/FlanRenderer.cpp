#include <iostream>

#include "Renderer.h"

int main()
{
    // Initialize renderer
    Flan::RendererDX12 renderer;
    renderer.init();

    // Create a window
    if (!renderer.create_window(1280, 720, "FlanRenderer (DirectX 12)")) {
        throw std::exception("Could not create window!");
    }

    puts("All is good!");
}
