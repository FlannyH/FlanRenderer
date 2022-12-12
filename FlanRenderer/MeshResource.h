#pragma once
#include "Resources.h"
#include <string>
#include "MaterialResource.h"

namespace Flan {
    struct ModelResource
    {
        static std::string name_string() { return "ModelResource"; }
        ResourceType resource_type;
        bool scheduled_for_unload;
        MeshCPU* meshes;
        size_t n_meshes;
        MaterialResource* materials;
        size_t n_materials;
        int n_meshes;
        int n_materials;
        bool load(std::string path, ResourceManager* resource_manager);
        void unload();
        void traverse_nodes(std::vector<int>& node_indices, tinygltf::Model& model, glm::mat4 local_transform, std::unordered_map<int, MeshCPU>& primitives_processed);
        void create_vertex_array(MeshCPU& mesh_out, tinygltf::Primitive primitive_in, tinygltf::Model model, glm::mat4 trans_mat);
    };
}