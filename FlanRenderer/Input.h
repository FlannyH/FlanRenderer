#pragma once
#include <unordered_map>
#include <glfw/glfw3.h>
#include <glm/vec2.hpp>

#include "renderer.h"

class Input
{
public:
    Input(GLFWwindow* window);
    void update(GLFWwindow* window);
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void scroll_callback(GLFWwindow* window, double scroll_x, double double_scroll_y);

    inline static std::unordered_map<int, bool> keys_held;
    std::unordered_map<int, bool> keys_prev;
    std::unordered_map<int, bool> keys_pressed;
    std::unordered_map<int, bool> keys_released;
    inline static std::unordered_map<int, bool> mouse_held;
    std::unordered_map<int, bool> mouse_prev;
    std::unordered_map<int, bool> mouse_pressed;
    std::unordered_map<int, bool> mouse_released;
    glm::ivec2 mouse_position_curr{ 0, 0 };
    glm::ivec2 mouse_position_prev{ 0, 0 };
    glm::ivec2 mouse_position_relative{ 0, 0 };
    inline static float mouse_scroll;
};
