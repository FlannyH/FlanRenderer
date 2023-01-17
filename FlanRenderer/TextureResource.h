#pragma once
#include "Resources.h"

namespace tinygltf {
    struct Image;
}

namespace Flan {
    struct Pixel32
    {
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        uint8_t a = 255;
    };

    struct TextureResource
    {
        static std::string name_string() { return "TextureResource"; }
        ResourceType resource_type = ResourceType::Texture;
        bool scheduled_for_unload = false;
        int width = 0;
        int height = 0;
        Pixel32* data = nullptr;
        char* name = nullptr;
        bool load(std::string path, ResourceManager const* resource_manager, bool silent = false);
        bool load(tinygltf::Image image, ResourceManager const* resource_manager);
        void unload();
        TextureResource(int width_, int height_, Pixel32* data_, char* name_)
        {
            scheduled_for_unload = false;
            resource_type = ResourceType::Texture;
            width = width_;
            height = height_;
            data = data_;
            name = name_;
        };
        void schedule_unload()
        {
            scheduled_for_unload = true;
        }
    };
}
