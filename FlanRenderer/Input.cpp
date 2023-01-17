#include "Input.h"

Input::Input(GLFWwindow* window)
{
    glfwSetMouseButtonCallback(static_cast<GLFWwindow*>(window), mouse_button_callback);
    glfwSetKeyCallback(static_cast<GLFWwindow*>(window), key_callback);
    glfwSetScrollCallback(static_cast<GLFWwindow*>(window), scroll_callback);
}

void Input::update(GLFWwindow* window)
{
    //Keyboard
    for (auto& key : keys_held)
    {
        //Assume no release nor press at first, taking a sort of "false until proven true" approach
        keys_pressed[key.first] = false;
        keys_released[key.first] = false;

        //Press
        if (!keys_prev[key.first] && keys_held[key.first])
            keys_pressed[key.first] = true;

        //Release
        if (keys_prev[key.first] && !keys_held[key.first])
            keys_released[key.first] = true;

        keys_prev[key.first] = keys_held[key.first];
    }

    //Mouse buttons
    for (auto& key : mouse_held)
    {
        //Assume no release nor press at first, taking a sort of "false until proven true" approach
        mouse_pressed[key.first] = false;
        mouse_released[key.first] = false;

        //Press
        if (!mouse_prev[key.first] && mouse_held[key.first])
            mouse_pressed[key.first] = true;

        //Release
        if (mouse_prev[key.first] && !mouse_held[key.first])
            mouse_released[key.first] = true;

        mouse_prev[key.first] = mouse_held[key.first];
    }


    //Mouse position
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    mouse_position_prev = mouse_position_curr;
    mouse_position_curr = { static_cast<int>(mouse_x), static_cast<int>(mouse_y) };
    mouse_position_relative = mouse_position_curr - mouse_position_prev;
}

void Input::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    keys_held[key] = action;
}

void Input::mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    mouse_held[button] = action;
}

void Input::scroll_callback(GLFWwindow* window, double scroll_x, double scroll_y)
{
    mouse_scroll += static_cast<float>(scroll_y);
}