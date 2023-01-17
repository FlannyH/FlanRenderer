#include "Resources.h"
#include "HelperFunctions.h"
#include "ModelResource.h"
#include "Descriptor.h"
#include "Renderer.h"
#include "TextureResource.h"

namespace Flan {

    ResourceManager::ResourceManager()
    {
    }
    ResourceManager::~ResourceManager()
    {
    }
    ResourceHandle ResourceManager::load_mesh(const std::string& path)
    {
        // Generate a hash for the resource
        ResourceHandle handle = std::hash<std::string>{}(path);

        // Load mesh from gltf
        ModelResource* model = (ModelResource*)dynamic_allocate(sizeof(ModelResource));
        model->load(path, this);

        // Add the resource to the resources map
        loaded_resource_data[handle] = model;
        loaded_resource_type[handle] = ResourceType::Model;

        // Give the handle back to the player
        return handle;
    }

    ResourceHandle ResourceManager::load_texture(const std::string& path) {
        // Generate a hash for the resource
        ResourceHandle handle = std::hash<std::string>{}(path);

        // Load mesh from gltf
        TextureResource* texture = (TextureResource*)dynamic_allocate(sizeof(TextureResource));
        texture->load(path, this);

        // Add the resource to the resources map
        loaded_resource_data[handle] = texture;
        loaded_resource_type[handle] = ResourceType::Texture;

        // Give the handle back to the player
        return handle;
    }
}
