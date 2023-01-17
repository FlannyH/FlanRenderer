#include "TextureResource.h"

#include <stb/stb_image.h>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION

namespace Flan {
    bool TextureResource::load(const std::string path, ResourceManager const* resource_manager, bool silent)
    {
        //Get name
        ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "TexRes - name - " + path;

        //Load image file
        int channels;
        ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "TexRes - data - " + path;
        uint8_t* u8_data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "unknown";

        //Error checking
        if (u8_data == nullptr)
        {
            if (!silent)
                printf("[ERROR] Image '%s' could not be loaded from disk!\n", path.c_str());
            resource_type = ResourceType::Invalid;
            return false;
        }

        //Pad if no alpha channel
        if (channels != 4)
        {
            if (!silent)
                printf("[ERROR] Image '%s' is not RGBA 32-bit!\n", path.c_str());
        }

        //Set name
        name = static_cast<char*>(dynamic_allocate(path.size() + 1));
        strcpy_s(name, path.size() + 1, path.c_str());

        //Set data
        data = reinterpret_cast<Pixel32*>(u8_data);

        //Return
        resource_type = ResourceType::Texture;
        scheduled_for_unload = false;
        return true;
    }

    void TextureResource::unload()
    {
        dynamic_free(data);
        dynamic_free(name);
        dynamic_free(this);
    }
}