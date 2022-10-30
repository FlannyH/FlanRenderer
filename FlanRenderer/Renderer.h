#pragma once

#define GLFW_EXPOSE_NATIVE_WIN32

#include <iostream>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <vector>
#include "glm/vec3.hpp"
#include <fstream>
#include <chrono>
#include <wrl.h>
#include <string_view>

namespace Flan {
    using Microsoft::WRL::ComPtr;
    class RendererHigh
    {
    public:
        virtual bool create_window(int width, int height, std::string_view name) { return false; }
        virtual bool init() { return false; }

    protected:
        HWND m_hwnd;
    };
    class RendererDX12 : public RendererHigh {
    public:
        bool create_window(int width, int height, std::string_view name) override;
        bool init() override;
    private:
        void create_hwnd(int width, int height, std::string_view name);
        void create_command_queue();
        void create_command_allocator();
        void create_fence();
        void create_swapchain(int width, int height);
        void create_factory();
        void create_device();
        UINT frame_index;
        ComPtr<IDXGIFactory4> m_factory;
        ComPtr<ID3D12Device> device = nullptr;
        ComPtr<ID3D12CommandQueue> command_queue;
        [[maybe_unused]] ComPtr<ID3D12Debug1> m_debug_interface;// If we're in release mode, this variable will be unused
        static constexpr int m_backbuffer_count = 3;
    };
}

/*

Xcreate window
Xdeclare dx12 handles
Xcreate factory
X(create debug layer)
Xcreate device handle
Xcreate adapter
Xlink to device
Xcreate command queue
Xcreate command allocator
Xcreate sync fence
Xcreate viewport and swapchain
     
 */