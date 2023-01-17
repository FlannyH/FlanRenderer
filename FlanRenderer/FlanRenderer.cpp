#include <iostream>

#include "Renderer.h"
#include "Resources.h"
#include "FlanRenderer.h"

#include "Input.h"

void update_camera(Flan::RendererHigh renderer, Input input, float move_speed, float delta_time, float mouse_sensitivity, Flan::Transform& transform)
{

    if (Input::keys_held[GLFW_KEY_D]) { transform.add_position(move_speed * delta_time * +transform.get_right_vector()); }
    if (Input::keys_held[GLFW_KEY_A]) { transform.add_position(move_speed * delta_time * -transform.get_right_vector()); }
    if (Input::keys_held[GLFW_KEY_W]) { transform.add_position(move_speed * delta_time * +transform.get_forward_vector()); }
    if (Input::keys_held[GLFW_KEY_S]) { transform.add_position(move_speed * delta_time * -transform.get_forward_vector()); }
    if (Input::keys_held[GLFW_KEY_SPACE]) { transform.add_position(move_speed * delta_time * -glm::vec3{ 0, 1, 0 }); }
    if (Input::keys_held[GLFW_KEY_LEFT_SHIFT]) { transform.add_position(move_speed * delta_time * +glm::vec3{ 0, 1, 0 }); }


    //TODO: make this platform independent
    if (input.mouse_held[GLFW_MOUSE_BUTTON_RIGHT])
    {
        glfwSetInputMode(static_cast<GLFWwindow*>(renderer.get_window()), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        transform.add_rotation(glm::radians(
            glm::vec3{
                0,
                -mouse_sensitivity * input.mouse_position_relative.x,
                0,
            }));
        transform.add_rotation(glm::angleAxis(glm::radians(mouse_sensitivity * input.mouse_position_relative.y), glm::vec3{ 1.0f, 0.0f, 0.0f }), false);
    }
    else if (input.mouse_released[GLFW_MOUSE_BUTTON_RIGHT])
    {
        glfwSetInputMode(static_cast<GLFWwindow*>(renderer.get_window()), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        int x, y;
        glfwGetWindowSize(static_cast<GLFWwindow*>(renderer.get_window()), &x, &y);
        glfwSetCursorPos(static_cast<GLFWwindow*>(renderer.get_window()), x / 2, y / 2);
    }
}

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

    // Initialize input manager
    Input input(renderer.get_window());

    Flan::ResourceHandle quad_handle = resources.load_mesh("Assets/Models/helmet.gltf");
    renderer.upload_mesh(quad_handle, resources);

    // Debug memory
    resources.get_allocator_instance()->debug_memory();

    // Define where the quad should be
    Flan::Transform quad_transform{
        {1, 0, 3},
        {1, 0, 0, 0},
        {1, 1, 1},
    };
    // Define where the quad should be
    Flan::Transform quad_transform2{
        {-1, 0, 3},
        {1, 0, 0, 0},
        {1, 1, 1},
    };
    Flan::Transform camera_transform{};

    float move_speed = 5.0f;
    float mouse_sensitivity = 0.4f;

    // Main loop
    while (!renderer.should_close()) {
        //Calculate deltatime
        float delta_time = calculate_delta_time();
        if (delta_time > 0.1f)
            delta_time = 0.1f;

        // Render
        renderer.begin_frame();
        //Update camera position
        input.update(static_cast<GLFWwindow*>(renderer.get_window()));
        update_camera(renderer, input, move_speed, delta_time, mouse_sensitivity, camera_transform);
        renderer.set_camera_transform(camera_transform);
        renderer.draw_model({ quad_handle, quad_transform });
        renderer.draw_model({ quad_handle, quad_transform2 });
        quad_transform.rotation *= glm::quat(glm::vec3{ 0, 2.0f * delta_time, 0 });
        quad_transform2.rotation *= glm::quat(glm::vec3{ 0, -2.0f * delta_time, 0 });
        renderer.end_frame();
    }

    puts("All is good!");
}
