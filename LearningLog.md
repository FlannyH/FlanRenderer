# What I learned from this project
This is a project I made as a self study goal at BUas. 
The goal was to create a 3D renderer in Direct3D 12, and have it be capable of drawing a textured model.
Currently the renderer is unable to draw textured models, but the rest works fine.

Along with the actual code, I am writing this document to explain what I learned while writing it, like Direct3D graphics concepts.

I will now explain all the graphics concepts I learned.

## Finding the GPU Device to use with D3D12
### Factory
In Direct3D 12, a **factory** is used to enumerate the graphics adapters the API has access to. 

### Adapter 
In my code, I keep querying the factory until an adapter is found that indicates that it is not a software adapter, and that it supports at least Direct3D feature level 12.0. The adapter can then be used to create a device object.

### Device
A device is our interface to the Direct3D API. It's how we create things like vertex buffers, textures, render targets, etc.

So in short: create a factory -> enumerate all adapters -> choose the first one that fits -> create a device object from the adapter.

## Executing Commands on the GPU
For running commands on the GPU using Direct3D 12, I used a design used in [this tutorial series](https://youtu.be/UgSYl-le9GU) This tutorial series has been a great help to me for getting something running while I got the hang of this graphics API. It makes use of a `D3D12_Command` class, which abstracts parts of the process.

I will now explain some of the concepts before talking about this design.

### Command List
A command list is a group of commands that the GPU will run, and it's used to set up buffers and parameters to facilitate rendering. Examples of things that are done via the command list include:
- Setting and clearing the render target
- Binding vertex buffers
- Setting up buffers and parameters for use during rendering (e.g. constant buffers, parameters)

### Command Queue
A command queue is a queue that you can submit command lists to. It's also used to signal fences, which is done to synchronize the CPU and the GPU.

### Command Allocator
A command allocator is used to allocate the memory used by command lists.

### Fence
A fence is used to synchronize the GPU and CPU. If the CPU wants to submit a new command list to a command queue, and then execute it, this particular command queue must already be completed by the GPU, which it might not be.

### Using the fence to sync the CPU and GPU
To make sure the GPU is caught up before we submit more commands, each frame gets assigned a fence value. This fence value is incremented by our code after calling ExecuteCommandLists, and then signalled to the command queue to increment its fence value when it's finished executing the command lists.

The next time the command queue is used, we wait for the command queue's fence value to update to the new value, which indicates it's done executing the command list, and then starts the new frame.

## Descriptors, Heaps, and Resources
### Resource
A resource is a chunk of memory on the GPU. Types of resources include but are not limited to: memory buffers, textures, vertex buffers, index buffers, constant buffers, depth buffers, and render targets.

### Descriptor
A descriptor "describes" the properties and layout of a resource. They are managed by descriptor heaps.

### Descriptor Heap
A descriptor heap functions to hold and manage collection of descriptors. In this renderer, I wrote a wrapper around it that handles allocation of descriptors within a descriptor heap, [heavily inspired once again by this channel's tutorial series.](https://youtu.be/xIZnkXqpHdQ)

 _(todo: get more comfortable with Descriptor Tables and explain them here)_

### Root Signature and Root Parameters
A root signature is bound to the command list, and contains Root Parameters that will be passed to the shaders. In the case of my renderer, I upload two parameters every frame:
1. A 32-bit constants parameter with 32 values, to store two matrices: the camera's view matrix and projection matrix.
2. A 32-bit constants parameter with 16 values, to store one matrix: the model transform matrix. 

These Root Parameters are then used as constants in the shader.

## Initializing the Window and the Render Targets
After creating a window and storing the HWND window handle, it's time to set up a swapchain.


### Swapchain
The swapchain is used to allocate the backbufers that will be rendered to, and it handles swapping between the different backbuffers at the end of a frame.

After creating a swapchain, you can use the allocated backbuffers to create Render Target Views.

### Render Target View 
In the Renderer, there is a Descriptor Heap for Render Target Views (RTV)
For every backbuffer, it allocates a descriptor in that heap, gets the render target buffer resource from the swapchain, and assigns the resource to that descriptor.

The RTV is used to bind the Render Target to the command list, which then tells the command list where to render pixels to.

### Depth Stencil View
Much like the RTV, there is a Descriptor Heap for Depth Stencil Views (DSV)
For every backbuffer, it allocates a descriptor in that heap, creates a depth buffer texture resource, and then assigns the texture resource to that descriptor

The DSV is used to bind the Depth Buffer to the command list, which the command list can then use for depth testing and sorting.

## Initializing the Render Pipeline
### Input Layout
The input layout determines the layout of the vertex data. It essentially sets up the vertex struct that the shader will use.

### Shaders
Shaders are written in HLSL, and are loaded as compiled binaries during runtime.

### Pipeline State Object
The Pipeline State Object (PSO) is an object that stores the current shader, input layout, a pointer to the Root Signature, the primitive topology type, the current rasterizer state, and information about render targets.