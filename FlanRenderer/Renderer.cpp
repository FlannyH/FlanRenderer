#include "Renderer.h"

void throw_if_failed(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}

void Flan::RendererDX12::create_hwnd(int width, int height, std::string_view name) {
    // Create window - use GLFW_NO_API, since we're not using OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
    m_hwnd = glfwGetWin32Window(window);
}

void Flan::RendererDX12::create_command_queue() {
    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    throw_if_failed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));
}

void Flan::RendererDX12::create_command_allocator() {
    // Create command allocator
    ComPtr<ID3D12CommandAllocator> command_allocator;
    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                   IID_PPV_ARGS(&command_allocator)));
}

void Flan::RendererDX12::create_fence() {
    // Create fence
    HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ComPtr<ID3D12Fence> fences[m_backbuffer_count];
    UINT64 fence_values[m_backbuffer_count];
    for (unsigned i = 0u; i < m_backbuffer_count; ++i) {
        throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])));
        fence_values[i] = 0;
    }
}

void Flan::RendererDX12::create_swapchain(int width, int height) {
    // Declare variables we need for swapchain
    ComPtr<ID3D12DescriptorHeap> render_target_view_heap;
    ComPtr<IDXGISwapChain3> swapchain = nullptr;
    ComPtr<ID3D12Resource> render_targets[m_backbuffer_count];
    UINT render_target_view_descriptor_size;
    D3D12_VIEWPORT viewport;
    D3D12_RECT surface_size;

    // Define surface size
    surface_size.left = 0;
    surface_size.top = 0;
    surface_size.right = width;
    surface_size.bottom = height;

    // Define viewport
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Create swapchain description
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
    swapchain_desc.BufferCount = m_backbuffer_count;
    swapchain_desc.Width = width;
    swapchain_desc.Height = height;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.SampleDesc.Count = 1;

    // Create swapchain
    IDXGISwapChain1* new_swapchain;
    m_factory->CreateSwapChainForHwnd(command_queue.Get(), m_hwnd, &swapchain_desc, nullptr, nullptr, &new_swapchain);
    HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&new_swapchain));
    if (SUCCEEDED(swapchain_support)) {
        swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
    }

    if (!swapchain) {
        throw std::exception();
    }

    frame_index = swapchain->GetCurrentBackBufferIndex();
}

bool Flan::RendererDX12::create_window(int width, int height, std::string_view name) {
    // Create window handle
    create_hwnd(width, height, name);

    // Create render context
    create_command_queue();
    create_command_allocator();
    create_fence();
    create_swapchain(width, height);
    return true;
}

void Flan::RendererDX12::create_factory() {
    // Create factory
    UINT dxgi_factory_flags = 0;

#if _DEBUG
    // If we're in debug mode, create a debug layer for proper error tracking
    // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
    ComPtr<ID3D12Debug> debug_layer;
    throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer)));
    throw_if_failed(debug_layer->QueryInterface(IID_PPV_ARGS(&m_debug_interface)));
    m_debug_interface->EnableDebugLayer();
    m_debug_interface->SetEnableGPUBasedValidation(true);
    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    debug_layer->Release();
#endif

    // This is for debugging purposes, so it may be unused
    [[maybe_unused]] HRESULT result = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&m_factory));
}

void Flan::RendererDX12::create_device() {
    // Create adapter
    ComPtr<IDXGIAdapter1> adapter;
    UINT adapter_index = 0;
    while (m_factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Ignore software renderer - we want a hardware adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // We should have a hardware adapter now, but does it support Direct3D 12.0?
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
            // Yes it does! We use this one.
            break;
        }

        // It doesn't? Unfortunate, let it go and try another adapter
        device = nullptr;
        adapter->Release();
        adapter_index++;
    }

    if (device == nullptr) {
        throw std::exception("Failed to create DirectX 12 device!");
    }

#if _DEBUG
    // If we're in debug mode, create the debug device handle
    ComPtr<ID3D12DebugDevice> device_debug;
    throw_if_failed(device->QueryInterface(device_debug.GetAddressOf()));
#endif
}

bool Flan::RendererDX12::init() {
    create_factory();
    create_device();
    return true;
}
