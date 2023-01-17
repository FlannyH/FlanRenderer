#pragma once
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Resources.h"

namespace Flan {
    struct MaterialResource
    {
        static std::string name_string() { return "MaterialResource"; }
        ResourceHandle tex_col{ 0 };
        ResourceHandle tex_nrm{ 0 };
        ResourceHandle tex_rgh{ 0 };
        ResourceHandle tex_mtl{ 0 };
        ResourceHandle tex_emm{ 0 };
        glm::vec4 mul_col{ 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 mul_emm{ 1.0f, 1.0f, 1.0f };
        glm::vec2 mul_tex{ 1.0f, 1.0f };
        float mul_nrm = 1.0f;
        float mul_rgh = 1.0f;
        float mul_mtl = 1.0f;
    };
}