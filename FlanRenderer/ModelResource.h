#pragma once
#include "Resources.h"
#include <string>
#include "MaterialResource.h"
#include <tinygltf/tiny_gltf.h>

namespace Flan {
    struct ModelResource
    {
        static std::string name_string() { return "ModelResource"; }
        ResourceType resource_type;
        bool scheduled_for_unload;
        MeshCPU* meshes_cpu;
        MeshGPU* meshes_gpu;
        MaterialResource* materials_cpu;
        MaterialGPU* materials_gpu;
        size_t n_meshes;
        size_t n_materials;
        bool load(std::string path, ResourceManager* resource_manager);
        void unload();
        void traverse_nodes(std::vector<int>& node_indices, tinygltf::Model& model, glm::mat4 local_transform, std::unordered_map<int, MeshCPU>& primitives_processed);
        void create_vertex_array(MeshCPU& mesh_out, tinygltf::Primitive primitive_in, tinygltf::Model model, glm::mat4 trans_mat);
    };
}