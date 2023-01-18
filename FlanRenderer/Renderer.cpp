#include "Renderer.h"
#include "HelperFunctions.h"
#include "ModelResource.h"
#include "RootParameter.h"
#include "TextureResource.h"

namespace Flan {
    Flan::D3D12_Command::D3D12_Command(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type) {
        // Create command queue description
        D3D12_COMMAND_QUEUE_DESC queue_desc = {};
        queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queue_desc.Type = type;

        // Create command queue
        throw_if_failed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

        // Create command allocators for each backbuffer
        for (auto& frame : command_frames) {
            throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.command_allocator)));
        }

        // Create command list
        throw_if_failed(device->CreateCommandList(0, type, command_frames[0].command_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list)));
        command_list->Close();

        // Create a fence
        throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

        // Create fence event
        fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);

        initialized = true;
    }

    Flan::D3D12_Command::~D3D12_Command()
    {
        //if (initialized)
        //    assert(!command_queue && !command_list && !fence);
    }

    void Flan::D3D12_Command::begin_frame() {
        // Make sure we are initialized
        assert(initialized);

        // Reset command allocator
        command_frames[frame_index].command_allocator->Reset();

        // Reset command list
        command_list->Reset(command_frames[frame_index].command_allocator.Get(), nullptr);

        // Reset the fence event
        ResetEvent(fence_event);
    }

    void Flan::D3D12_Command::end_frame() {
        // Close the command list
        command_list->Close();

        // Execute command list
        ID3D12CommandList* const command_lists[]{
            command_list.Get(),
        };
        command_queue->ExecuteCommandLists(_countof(command_lists), &command_lists[0]);
        ++fence_value;
        command_frames[frame_index].fence_value = fence_value;
        command_queue->Signal(fence.Get(), fence_value);

        // Set frame index to the next buffer, wrapping around to 0 after the last buffer
        command_frames[frame_index].wait_fence(fence_event, fence.Get());
        frame_index = (frame_index + 1) % m_backbuffer_count;
    }

    void RendererDX12::set_camera_transform(const Transform& transform) {
        m_camera_transform.position = -transform.position;
        m_camera_transform.rotation = transform.rotation;
        m_camera_transform.scale = transform.scale;
    }

    void Flan::RendererDX12::create_hwnd(int width, int height, std::string_view name) {
        // Create window - use GLFW_NO_API, since we're not using OpenGL
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
        m_hwnd = glfwGetWin32Window(window);
    }

    void Flan::RendererDX12::create_fence() {
        // Create fences for each backbuffer
        assert(m_device);
        HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        ComPtr<ID3D12Fence> fences[m_backbuffer_count];
        UINT64 fence_values[m_backbuffer_count];
        for (unsigned i = 0u; i < m_backbuffer_count; ++i) {
            throw_if_failed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])));
            fence_values[i] = 0;
        }
    }

    void Flan::RendererDX12::create_swapchain(int width, int height) {
        // Define surface size
        m_surface_size.left = 0;
        m_surface_size.top = 0;
        m_surface_size.right = width;
        m_surface_size.bottom = height;

        // Define viewport
        m_viewport.TopLeftX = 0.0f;
        m_viewport.TopLeftY = 0.0f;
        m_viewport.Width = static_cast<float>(width);
        m_viewport.Height = static_cast<float>(height);
        m_viewport.MinDepth = 0.0f;
        m_viewport.MaxDepth = 1.0f;

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
        IDXGISwapChain1* new_swapchain = nullptr;
        m_factory->CreateSwapChainForHwnd(m_command.get_command_queue(), m_hwnd, &swapchain_desc, nullptr, nullptr, &new_swapchain);
        HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&new_swapchain));
        if (SUCCEEDED(swapchain_support)) {
            m_swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
        }

        if (!m_swapchain) {
            throw_fatal("Failed to create Direct3D swapchain!");
        }
        
        // Create RTV for each frame
        for (UINT i = 0; i < m_backbuffer_count; i++) {
            m_rtv_handles[i] = m_rtv_heap.allocate();
            throw_if_failed(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_render_targets[i])));
            m_device->CreateRenderTargetView(m_render_targets[i].Get(), nullptr, m_rtv_handles[i].cpu); 
        }

        // Create depth texture and depth stencil view for each frame
        DXGI_FORMAT dsv_format = DXGI_FORMAT_D32_FLOAT;
        DXGI_FORMAT texture_format = DXGI_FORMAT_D32_FLOAT;
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.PlaneSlice = 0;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
        dsv_desc.Format = dsv_format;
        dsv_desc.Texture2D.MipSlice = 0;

        D3D12_RESOURCE_DESC depth_resource_desc{};
        depth_resource_desc.Format = DXGI_FORMAT_D32_FLOAT;
        depth_resource_desc.Width = width;
        depth_resource_desc.Height = height;
        depth_resource_desc.SampleDesc.Count = 1;
        depth_resource_desc.SampleDesc.Quality = 0;
        depth_resource_desc.DepthOrArraySize = 1;
        depth_resource_desc.MipLevels = 0;
        depth_resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depth_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depth_resource_desc.Alignment = 0;
        depth_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        D3D12_CLEAR_VALUE optimized_clear_value = {};
        optimized_clear_value.Format = DXGI_FORMAT_D32_FLOAT;
        optimized_clear_value.DepthStencil = { 1.0f, 0 };
        
        for (UINT i = 0; i < m_backbuffer_count; i++) {
            // Create the texture resource
            D3D12_HEAP_PROPERTIES default_heap{
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                D3D12_MEMORY_POOL_UNKNOWN,
                0,
                0
            };
            m_device->CreateCommittedResource(
                &default_heap, 
                D3D12_HEAP_FLAG_NONE, 
                &depth_resource_desc, 
                D3D12_RESOURCE_STATE_DEPTH_WRITE, 
                &optimized_clear_value, 
                IID_PPV_ARGS(&m_depth_targets[i])
            );

            // Create the DSV
            m_dsv_handles[i] = m_dsv_heap.allocate();
            m_device->CreateDepthStencilView(m_depth_targets[i].Get(), &dsv_desc, m_dsv_handles[i].cpu);
        }

        m_frame_index = m_swapchain->GetCurrentBackBufferIndex();
    }

    void Transform::set_position(const glm::vec3 new_position)
    {
        position = new_position;
    }

    void Transform::add_position(const glm::vec3 new_position)
    {
        position += new_position;
    }

    void Transform::set_rotation(const glm::vec3 new_euler_angles)
    {
        rotation = glm::quat(new_euler_angles);
    }

    void Transform::set_rotation(const glm::quat new_quaternion)
    {
        rotation = new_quaternion;
    }

    void Transform::add_rotation(const glm::vec3 new_euler_angles)
    {
        rotation = glm::normalize(glm::normalize(glm::quat(new_euler_angles)) * glm::normalize(rotation));
    }

    void Transform::add_rotation(const glm::quat new_quaternion, bool world_space)
    {
        if (world_space)
            rotation = glm::normalize(glm::normalize(new_quaternion) * glm::normalize(rotation));
        else
            rotation = glm::normalize(rotation) * glm::normalize(glm::normalize(new_quaternion));
    }

    void Transform::set_scale(const glm::vec3 new_scale)
    {
        scale = new_scale;
    }

    void Transform::set_scale(const float new_scalar)
    {
        scale = glm::vec3(new_scalar);
    }

    glm::vec3 Transform::get_right_vector() const
    {
        return rotation * glm::vec3{ 1,0,0 };
    }

    glm::vec3 Transform::get_up_vector() const
    {
        return rotation * glm::vec3{ 0,1,0 };
    }

    glm::vec3 Transform::get_forward_vector() const
    {
        return rotation * glm::vec3{ 0,0,-1 };
    }


    void ConstBuffer::update_data(ID3D12Device* device, void* new_data, size_t size) {
        // Copy the new data to the constant buffer
        memcpy_s(buffer_data, buffer_size, new_data, size);
    }

    bool Flan::RendererDX12::create_window(int width, int height, std::string_view name) {
        // Create window handle
        create_hwnd(width, height, name);

        // Create render context
        create_swapchain(width, height);
        create_root_signature();
        create_pipeline_state_object();
        return true;
    }

    void Flan::RendererDX12::create_factory() {
        // Create factory
        UINT dxgi_factory_flags = 0;

#if _DEBUG
        // If we're in debug mode, create a debug layer for proper error tracking
        // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
        ID3D12Debug* debug_layer;
        throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer)));
        throw_if_failed(debug_layer->QueryInterface(IID_PPV_ARGS(&m_debug_interface)));
        m_debug_interface->EnableDebugLayer();
        m_debug_interface->SetEnableGPUBasedValidation(true);
        dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        debug_layer->Release();
#endif

        // Result is saved for debugging purposes, so it may be unused
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
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)))) {
                // Yes it does! We use this one.
                break;
            }

            // It doesn't? Unfortunate, let it go and try another adapter
            m_device = nullptr;
            adapter->Release();
            adapter_index++;
        }

        if (m_device == nullptr) {
            throw_fatal("Failed to create Direct3D device!");
        }

#if _DEBUG
        // If we're in debug mode, create the debug device handle
        ComPtr<ID3D12DebugDevice> device_debug;
        throw_if_failed(m_device->QueryInterface(device_debug.GetAddressOf()));
#endif
    }

    void Flan::RendererDX12::create_command()
    {
        m_command = D3D12_Command(m_device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        if (m_command.get_command_queue() == nullptr) {
            throw_fatal("Failed to create command queue!");
        }
    }

    void Flan::RendererDX12::create_pipeline_state_object()
    {
        assert(m_device);

        // todo: remove magic hardcoded shader description
        ShaderDescription shader_description;
        shader_description.binary_path = "Assets/Shaders/test";
        shader_description.parameters.push_back({ "Vertex Position", POSITION, 12, 0, DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Colour", COLOR, 12, 0,  DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Normal", NORMAL, 12, 0, DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Tangent", TANGENT, 12, 0, DXGI_FORMAT_R32G32B32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Texcoord 0", TEXCOORD, 8, 0,  DXGI_FORMAT_R32G32_FLOAT });
        shader_description.parameters.push_back({ "Vertex Texcoord 1", TEXCOORD, 8, 1,  DXGI_FORMAT_R32G32_FLOAT });

        // Create input assembly - this defines what our shader input is
        size_t offset = 0;
        D3D12_INPUT_ELEMENT_DESC input_element_descs[32]{};
        for (size_t i = 0; i < shader_description.parameters.size(); i++) {
            auto& param = shader_description.parameters[i];
            input_element_descs[i].SemanticName = SemanticNames[param.semantic];
            input_element_descs[i].SemanticIndex = param.semantic_index;
            input_element_descs[i].Format = param.format;
            input_element_descs[i].InputSlot = 0;
            input_element_descs[i].AlignedByteOffset = offset; offset += param.size_bytes;
            input_element_descs[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            input_element_descs[i].InstanceDataStepRate = 0;
        }

        // Load shader
        Shader shader = load_shader(shader_description.binary_path);

        // Create pipeline state description
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_state_description{};
        pipeline_state_description.InputLayout = { input_element_descs, (u32)shader_description.parameters.size() };
        pipeline_state_description.pRootSignature = m_root_signature.Get();
        pipeline_state_description.VS = shader.vs_bytecode;
        pipeline_state_description.PS = shader.ps_bytecode;

        // Create rasterizer description
        D3D12_RASTERIZER_DESC raster_desc;
        raster_desc.FillMode = D3D12_FILL_MODE_SOLID;
        raster_desc.CullMode = D3D12_CULL_MODE_NONE;
        raster_desc.FrontCounterClockwise = FALSE;
        raster_desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        raster_desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        raster_desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        raster_desc.DepthClipEnable = TRUE;
        raster_desc.MultisampleEnable = FALSE;
        raster_desc.AntialiasedLineEnable = FALSE;
        raster_desc.ForcedSampleCount = 0;
        raster_desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pipeline_state_description.RasterizerState = raster_desc;
        pipeline_state_description.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // Setup color and alpha blend modes
        D3D12_BLEND_DESC blend_desc{};
        blend_desc.AlphaToCoverageEnable = FALSE;
        blend_desc.IndependentBlendEnable = FALSE;
        constexpr D3D12_RENDER_TARGET_BLEND_DESC default_render_target_blend_desc = {
            FALSE,
            FALSE,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE,
            D3D12_BLEND_ZERO,
            D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        for (auto& i : blend_desc.RenderTarget)
            i = default_render_target_blend_desc;

        pipeline_state_description.BlendState = blend_desc;


        // Set up depth/stencil state
        pipeline_state_description.DepthStencilState.DepthEnable = TRUE;
        pipeline_state_description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pipeline_state_description.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        pipeline_state_description.DepthStencilState.StencilEnable = FALSE;
        pipeline_state_description.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        pipeline_state_description.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        pipeline_state_description.DepthStencilState.StencilEnable = FALSE;
        pipeline_state_description.SampleMask = UINT_MAX;

        // Setup render target output
        pipeline_state_description.NumRenderTargets = 1;
        pipeline_state_description.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pipeline_state_description.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pipeline_state_description.SampleDesc.Count = 1;

        // Create graphics pipeline state
        try {
            throw_if_failed(m_device->CreateGraphicsPipelineState(&pipeline_state_description, IID_PPV_ARGS(&m_pipeline_state_object)));
        }
        catch ([[maybe_unused]] std::exception& e) {
            puts("Failed to create Graphics Pipeline");
        }
    }

    void RendererDX12::create_descriptor_heaps()
    {
        // Create descriptor heaps
        //cbv_srv_uav_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //sample_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        m_rtv_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_dsv_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        m_cbv_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_srv_heap = DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        //cbv_srv_uav_heap.init(device.Get(), 16, true);
        //sample_heap.init(device.Get(), 1, true);
        m_rtv_heap.init(m_device.Get(), m_backbuffer_count, true);
        m_dsv_heap.init(m_device.Get(), m_backbuffer_count, true);
        m_cbv_heap.init(m_device.Get(), 16, true);
        m_srv_heap.init(m_device.Get(), 16, true);
    }

    void Flan::RendererDX12::create_root_signature()
    {
        // Create descriptor range for textures
        DescriptorRange texture_range {
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            16,
            0,
        };
        

        // Create root parameters
        RootParameter parameters[3];
        parameters[0].as_constants(32, D3D12_SHADER_VISIBILITY_VERTEX, 0); // Camera transform buffer
        parameters[1].as_constants(16, D3D12_SHADER_VISIBILITY_VERTEX, 1); // Camera transform buffer
        parameters[2].as_descriptor_table(D3D12_SHADER_VISIBILITY_PIXEL, &texture_range, 1); // Camera transform buffer

        // Create root signature
        RootSignatureDesc root_signature_desc{ &parameters[0], _countof(parameters), nullptr, 0, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT };
        m_root_signature = root_signature_desc.create(m_device.Get());
    }

    void RendererDX12::free_later(void* data_pointer) {
        m_to_be_deallocated[m_frame_index].push_back(data_pointer);
    }

    ConstBuffer Flan::RendererDX12::create_const_buffer(size_t buffer_size, bool temporary)
    {
        assert(m_device);

        // Create the const buffer
        ConstBuffer const_buffer{};

        // Allocate memory for the buffer in CPU space
        const_buffer.buffer_data = m_renderer_allocator.allocate(buffer_size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        const_buffer.buffer_size = buffer_size;

        // Get a handle where we can put the constant buffer
        const_buffer.handle = m_cbv_heap.allocate();

        // If this buffer is temporary, mark it for deallocation, which will free the buffer handle a few frames from now
        if (temporary) {
            m_cbv_heap.free_later(const_buffer.handle);
            free_later(const_buffer.buffer_data);
        }

        // Define upload properties
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1
        };

        // Define what we want to upload
        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            (const_buffer.buffer_size | 0xFF) + 1, // Constant buffers must be 256-byte aligned
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };

        // Create a constant buffer resource
        throw_if_failed(m_device->CreateCommittedResource(
            &upload_heap_props,
            D3D12_HEAP_FLAG_NONE,
            &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof(ID3D12Resource),
            (void**)&const_buffer.resource
        ));

        // Create a constant buffer description
        const_buffer.view_desc.BufferLocation = const_buffer.resource->GetGPUVirtualAddress();
        const_buffer.view_desc.SizeInBytes = (const_buffer.buffer_size | 0xFF) + 1;
        m_device->CreateConstantBufferView(&const_buffer.view_desc, const_buffer.handle.cpu);

        return const_buffer;
    }

    Shader Flan::RendererDX12::load_shader(const std::string& path)
    {
        Shader shader{};
        std::string vs_path = path + ".vs.cso";
        std::string ps_path = path + ".ps.cso";
        size_t vs_size = 0;
        size_t ps_size = 0;
        char* vs_data = nullptr;
        char* ps_data = nullptr;
        read_file(vs_path, vs_size, vs_data, false);
        read_file(ps_path, ps_size, ps_data, false);
        shader.vs_bytecode.BytecodeLength = vs_size;
        shader.ps_bytecode.BytecodeLength = ps_size;
        shader.vs_bytecode.pShaderBytecode = vs_data;
        shader.ps_bytecode.pShaderBytecode = ps_data;
        return shader;
    }

    bool Flan::RendererDX12::init(int w, int h) {
        create_factory();
        create_device();
        create_command();
        create_descriptor_heaps();
        // Create a window
        if (!create_window(w, h, "FlanRenderer (DirectX 12)")) {
            throw std::exception("Could not create window!");
        }

        // todo: remove these
        m_camera_matrix = create_const_buffer(sizeof(TransformBuffer));
        m_model_matrix = create_const_buffer(sizeof(ModelTransformBuffer));

        //
        //D3D12_SAMPLER_DESC samplerDesc = {};
        //samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        //samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        //m_device->CreateSampler(&samplerDesc, &sampler);

        return true;
    }

    void RendererDX12::begin_frame()
    {
        m_command.begin_frame();

        // Handle deferred frees
        for (void* pointer : m_to_be_deallocated[m_frame_index]) {
            m_renderer_allocator.release(pointer);
        }
        m_to_be_deallocated[m_frame_index].clear();
        m_cbv_heap.do_deferred_releases(m_frame_index);

        // Update camera constant buffer
        struct {

            glm::mat4 view;
            glm::mat4 projection;
        } camera_matrices;
        camera_matrices.view = m_camera_transform.get_view_matrix();
        // todo: un-hardcode this
        camera_matrices.projection = glm::perspectiveRH_ZO(glm::radians(90.f), 16.f/9.f, 0.1f, 1000.f);

        //m_camera_matrix.update_data(m_device.Get(), &camera_matrices, sizeof(camera_matrices));

        // Bind root signature
        auto* command_list = m_command.get_command_list();
        command_list->SetGraphicsRootSignature(m_root_signature.Get());
        command_list->SetGraphicsRoot32BitConstants(0, 32, &camera_matrices, 0);

        // Bind pipeline state
        command_list->SetPipelineState(m_pipeline_state_object);
    }

    void RendererDX12::end_frame()
    {
        // Record raster commands
        constexpr float clear_color[] = { 0.1f, 0.1f, 0.2f, 1.0f };
        auto* command_list = m_command.get_command_list();
        // Set backbuffer as render target
        D3D12_RESOURCE_BARRIER render_target_barrier;
        render_target_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        render_target_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        render_target_barrier.Transition.pResource = m_render_targets[m_frame_index].Get();
        render_target_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        render_target_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        render_target_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list->ResourceBarrier(1, &render_target_barrier);

        // Set up viewport
        command_list->OMSetRenderTargets(1, &m_rtv_handles[m_frame_index].cpu, FALSE, &m_dsv_handles[m_frame_index].cpu);
        command_list->RSSetViewports(1, &m_viewport); // Set viewport
        command_list->RSSetScissorRects(1, &m_surface_size); // todo: comment
        command_list->ClearRenderTargetView(m_rtv_handles[m_frame_index].cpu, clear_color, 0, nullptr); // Clear the screen
        command_list->ClearDepthStencilView(m_dsv_handles[m_frame_index].cpu, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr); // Clear the depth buffer
        command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // We draw triangles

        // Loop over each model
        for (size_t i = 0; i < m_model_queue_length; ++i) {
            // Get the model draw info for this entry
            ModelDrawInfo& curr_model_info = m_model_queue[i];

            // Get the mesh from the resource manager
            ModelResource* model_resource = m_resource_manager->get_resource<ModelResource>(curr_model_info.model_to_draw);
            auto vertex_buffer_view = model_resource->meshes_gpu->vertex_buffer_view;
            auto index_buffer_view = model_resource->meshes_gpu->index_buffer_view;
            auto n_verts = model_resource->meshes_cpu->n_indices;

            // todo: Get the albedo material from the mesh and bind the texture to the shader resource view
            TextureGPU& texture = model_resource->materials_gpu->tex_col;

            // Set root descriptor table
            ID3D12DescriptorHeap* desc_heap[] = {
                m_srv_heap.get_heap(),
            };

            // Create and set a model matrix for this model
            glm::mat4 model_matrix = curr_model_info.transform.get_model_matrix();
            command_list->SetDescriptorHeaps(_countof(desc_heap), desc_heap);
            command_list->SetGraphicsRoot32BitConstants(1, 16, &model_matrix, 0);
            command_list->SetGraphicsRootDescriptorTable(2, m_srv_heap.get_gpu_start());

            // Bind the vertex buffer
            command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view); // Bind vertex buffer
            command_list->IASetIndexBuffer(&index_buffer_view); // Bind index buffer

            // todo: Set up the sampler
            //command_list->

            // Submit draw call
            command_list->DrawIndexedInstanced(n_verts, 1, 0, 0, 0);
        }
        m_model_queue_length = 0;

        D3D12_RESOURCE_BARRIER present_barrier;
        present_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        present_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        present_barrier.Transition.pResource = m_render_targets[m_frame_index].Get();
        present_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        present_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        present_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        command_list->ResourceBarrier(1, &present_barrier);

        m_command.end_frame();

        // Update window
        m_swapchain->Present(0, 0);
        m_frame_index = m_swapchain->GetCurrentBackBufferIndex();

        // Update GLFW window
        glfwPollEvents();
        glfwSwapBuffers(window);
    }

    void RendererDX12::draw_model(ModelDrawInfo model)
    {
        // Make sure we have a queue
        if (m_model_queue == nullptr) {
            m_model_queue = (ModelDrawInfo*)m_renderer_allocator.allocate(sizeof(ModelDrawInfo) * 1024);
        }

        // Add the model to the queue
        m_model_queue[m_model_queue_length++] = model;
    }

    bool RendererDX12::should_close()
    {
        return glfwWindowShouldClose(window) != 0;
    }

    TextureGPU RendererDX12::upload_texture(const ResourceHandle texture_handle, bool is_srgb, bool unload_resource_afterwards) {
        // Get texture resource
        TextureResource* resource = m_resource_manager->get_resource<TextureResource>(texture_handle);
        if (resource->resource_type == ResourceType::Invalid) {
            return TextureGPU{ nullptr, 0 };
        }

        // Define heap properties
        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        // Define resource description based on the texture resource
        D3D12_RESOURCE_DESC resource_desc = {};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Width = resource->width;
        resource_desc.Height = resource->height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = is_srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // Create a texture resource
        TextureGPU texture_gpu;
        m_device->CreateCommittedResource(
            &heap_properties, 
            D3D12_HEAP_FLAG_NONE, 
            &resource_desc, 
            D3D12_RESOURCE_STATE_COMMON, 
            nullptr, 
            IID_PPV_ARGS(&texture_gpu.resource)
        );

        // Create a Shader Resource View
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = resource_desc.Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Texture2D.MipLevels = 1;
        srv_desc.Texture2D.MostDetailedMip = 0;
        texture_gpu.handle = m_srv_heap.allocate();
        m_device->CreateShaderResourceView(texture_gpu.resource, &srv_desc, texture_gpu.handle.cpu);

        // We're done!
        return texture_gpu;
    }

    void RendererDX12::upload_mesh(ResourceHandle handle, ResourceManager& resource_manager) {
        // Get the resource
        ModelResource* model = resource_manager.get_resource<ModelResource>(handle);

        // Allocate space for GPU mesh data
        model->meshes_gpu = (MeshGPU*)dynamic_allocate(model->n_meshes * sizeof(MeshGPU));
        model->materials_gpu = (MaterialGPU*)dynamic_allocate(sizeof(MaterialGPU) * model->n_materials);
        model->n_meshes = model->n_meshes;
        model->n_materials = model->n_materials;

        // Set it to 0 (so we actually get nullptr references
        memset(model->meshes_gpu, 0, model->n_meshes * sizeof(MeshGPU));

        //Parse all materials
        for (int i = 0; i < model->n_materials; i++)
        {
            model->materials_gpu[i].tex_col = upload_texture(model->materials_cpu[i].tex_col, true, true);
            model->materials_gpu[i].tex_nrm = upload_texture(model->materials_cpu[i].tex_nrm, false, true);
            model->materials_gpu[i].tex_mtl = upload_texture(model->materials_cpu[i].tex_mtl, false, true);
            model->materials_gpu[i].tex_rgh = upload_texture(model->materials_cpu[i].tex_rgh, false, true);
        }

        // For each mesh
        for (size_t i = 0; i < model->n_meshes; i++) {
            // Get the resource
            MeshCPU& mesh_cpu = model->meshes_cpu[i];
            MeshGPU& mesh_gpu = model->meshes_gpu[i];

            // Only the GPU needs this data, set the range accordingly
            mesh_gpu.vertex_buffer_range = { 0, 0 };
            mesh_gpu.index_buffer_range = { 0, 0 };

            // Upload the vertex buffer to the GPU
            {
                D3D12_HEAP_PROPERTIES upload_heap_props = {
                    D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
                    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                    D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

                D3D12_RESOURCE_DESC upload_buffer_desc = {
                    D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
                    0,
                    sizeof(Vertex) * mesh_cpu.n_verts,
                    1,
                    1,
                    1,
                    DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
                    {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
                    D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
                    D3D12_RESOURCE_FLAG_NONE,
                };

                throw_if_failed(m_device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
                                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&mesh_gpu.vertex_buffer_resource));

                // Bind the vertex buffer, copy the data to it, then unbind the vertex buffer
                throw_if_failed(mesh_gpu.vertex_buffer_resource->Map(0, &mesh_gpu.vertex_buffer_range, reinterpret_cast<void**>(&mesh_gpu.vertex_buffer_data)));
                memcpy_s(mesh_gpu.vertex_buffer_data, sizeof(Vertex) * mesh_cpu.n_verts, mesh_cpu.vertices, sizeof(Vertex) * mesh_cpu.n_verts);
                mesh_gpu.vertex_buffer_resource->Unmap(0, nullptr);

                // Init the buffer view
                mesh_gpu.vertex_buffer_view = D3D12_VERTEX_BUFFER_VIEW{
                    mesh_gpu.vertex_buffer_resource->GetGPUVirtualAddress(),
                    static_cast<u32>(sizeof(Vertex) * mesh_cpu.n_verts),
                    sizeof(Vertex),
                };
            }

            // Upload the index buffer to the GPU
            {
                D3D12_HEAP_PROPERTIES upload_heap_props = {
                    D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
                    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                    D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

                D3D12_RESOURCE_DESC upload_buffer_desc = {
                    D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
                    0,
                    sizeof(u32) * mesh_cpu.n_indices,
                    1,
                    1,
                    1,
                    DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
                    {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
                    D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
                    D3D12_RESOURCE_FLAG_NONE,
                };

                throw_if_failed(m_device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
                                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), (void**)&mesh_gpu.index_buffer_resource));

                // Bind the index buffer, copy the data to it, then unbind the index buffer
                throw_if_failed(mesh_gpu.index_buffer_resource->Map(0, &mesh_gpu.index_buffer_range, reinterpret_cast<void**>(&mesh_gpu.index_buffer_data)));
                memcpy_s(mesh_gpu.index_buffer_data, sizeof(u32) * mesh_cpu.n_indices, mesh_cpu.indices, sizeof(u32) * mesh_cpu.n_indices);
                mesh_gpu.index_buffer_resource->Unmap(0, nullptr);

                // Init the buffer view
                mesh_gpu.index_buffer_view = D3D12_INDEX_BUFFER_VIEW{
                    mesh_gpu.index_buffer_resource->GetGPUVirtualAddress(),
                    static_cast<u32>(sizeof(u32) * mesh_cpu.n_indices),
                    DXGI_FORMAT_R32_UINT,
                };
            }
        }
    }

    void Flan::D3D12_Command::CommandFrame::wait_fence(HANDLE fence_event, ID3D12Fence1* fence) {
        assert(fence && fence_event);

        // If the current completed fence value is lower than this frame's fence value,
        // we aren't done with this frame yet. We should wait.

        //if (fence->GetCompletedValue() < fence_value) {
            // We create an event to trigger when the fence value reaches this frame's fence value
            fence->SetEventOnCompletion(fence_value, fence_event);

            // And then we wait for that event to trigger
            WaitForSingleObject(fence_event, INFINITE);
        //}
    }

    void Flan::D3D12_Command::CommandFrame::release()
    {
        command_allocator.ReleaseAndGetAddressOf();
    }

    void Flan::D3D12_Command::release()
    {
        for (auto i = 0u; i < m_backbuffer_count; ++i) {
            command_frames[i].wait_fence(fence_event, fence.Get());
        }

        if (fence_event) {
            CloseHandle(fence_event);
            fence_event = nullptr;
        }

        command_queue->Release();
        command_list->Release();

        for (auto i = 0u; i < m_backbuffer_count; ++i) {
            command_frames[i].release();
        }
    }
}
