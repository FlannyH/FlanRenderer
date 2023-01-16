#include "ModelResource.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include <tinygltf/tiny_gltf.h>

namespace Flan {
    bool ModelResource::load(std::string path, ResourceManager* resource_manager)
    {
        //Load GLTF file
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string error;
        std::string warning;

        loader.LoadASCIIFromFile(&model, &error, &warning, path);

        if (!error.empty()) {
            printf("[ERROR] %s\n", error.c_str());
            return false;
        }

        std::string path_to_model_folder = path.substr(0, path.find_last_of('/')) + "/";

        //Parse materials
        /*
        std::vector<MaterialResource> materials_vector;
        {
            for (auto& model_material : model.materials)
            {
                //Create material
                MaterialResource pbr_material;

                //Set PBR multipliers
                pbr_material.mul_col = glm::vec4(
                    model_material.pbrMetallicRoughness.baseColorFactor[0],
                    model_material.pbrMetallicRoughness.baseColorFactor[1],
                    model_material.pbrMetallicRoughness.baseColorFactor[2],
                    model_material.pbrMetallicRoughness.baseColorFactor[3]
                );

                //Find base colour texture
                int index_texture_colour = model_material.pbrMetallicRoughness.baseColorTexture.index;
                if (index_texture_colour != -1)
                {
                    //Find file path parts
                    int index_image_colour = model.textures[index_texture_colour].source;
                    auto image = model.images[index_image_colour];
                    std::string file_path_from_model = image.uri;
                    std::string path_from_model_folder_to_texture_folder = file_path_from_model.substr(0, file_path_from_model.find_last_of('/')) + "/";
                    std::string file_extension = file_path_from_model.substr(file_path_from_model.find_last_of('.'));
                    std::string file_name_root = file_path_from_model.substr(file_path_from_model.find_last_of('/') + 1, file_path_from_model.find_last_of('alb.') - file_path_from_model.find_last_of('/') - 4);
                    std::string path_without_extension = path_to_model_folder + path_from_model_folder_to_texture_folder + file_name_root;

                    //Create textures - TODO: reassess whether this is scuffed or not
                    ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "MdlRes - TexRes's - " + path;

                    ResourceHandle handle_texture_alb = resource_manager->load_resource_from_disk<TextureResource>(path_without_extension + "alb" + file_extension);
                    ResourceHandle handle_texture_nrm = resource_manager->load_resource_from_disk<TextureResource>(path_without_extension + "nrm" + file_extension);
                    ResourceHandle handle_texture_mtl = resource_manager->load_resource_from_disk<TextureResource>(path_without_extension + "mtl" + file_extension);
                    ResourceHandle handle_texture_rgh = resource_manager->load_resource_from_disk<TextureResource>(path_without_extension + "rgh" + file_extension);

                    pbr_material.tex_col = handle_texture_alb;
                    pbr_material.tex_nrm = handle_texture_nrm;
                    pbr_material.tex_mtl = handle_texture_mtl;
                    pbr_material.tex_rgh = handle_texture_rgh;
                }
                materials_vector.push_back(pbr_material);
            }
        }
        */

        //Go through each node and add it to the primitive vector
        std::unordered_map<int, MeshCPU> primitives;
        {
            //Get nodes
            auto& scene = model.scenes[model.defaultScene];
            traverse_nodes(scene.nodes, model, glm::mat4(1.0f), primitives);
        }

        //Populate resource
        {
            ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "MdlRes - Mesh - " + path;
            meshes_cpu = (MeshCPU*)dynamic_allocate(sizeof(MeshCPU) * primitives.size());
            ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "MdlRes - Material - " + path;
            materials = (MaterialResource*)dynamic_allocate(sizeof(MaterialResource) * primitives.size());
            n_meshes = 0;
            n_materials = 0;

            int i = 0;
            for (auto& [material_id, mesh] : primitives)
            {
                meshes_cpu[n_meshes] = mesh;
                materials[n_materials] = materials[material_id];
                n_meshes += 1;
                n_materials += 1;
            }
        }

        resource_type = ResourceType::Model;
        scheduled_for_unload = false;
        return true;
    }

    void ModelResource::traverse_nodes(std::vector<int>& node_indices, tinygltf::Model& model, glm::mat4 local_transform, std::unordered_map<int, MeshCPU>& primitives_processed)
    {
        //Loop over all nodes
        for (auto& node_index : node_indices)
        {
            //Get node
            auto& node = model.nodes[node_index];

            //Convert matrix in gltf model to glm::mat4. If the matrix doesn't exist, just set it to identity matrix
            glm::mat4 local_matrix(1.0f);
            int i = 0;
            for (const auto& value : node.matrix) { local_matrix[i / 4][i % 4] = static_cast<float>(value); i++; }
            local_matrix = local_transform * local_matrix;

            //If it has a mesh, process it
            if (node.mesh != -1)
            {
                //Get mesh
                auto& mesh = model.meshes[node.mesh];
                auto& primitives = mesh.primitives;
                for (auto& primitive : primitives)
                {
                    printf("Creating vertex array for mesh '%s'\n", node.name.c_str());
                    //primitive.material
                    MeshCPU mesh_buffer_data{};
                    create_vertex_array(mesh_buffer_data, primitive, model, local_matrix);
                    primitives_processed[primitive.material] = mesh_buffer_data;
                }
            }

            //If it has children, process those
            if (!node.children.empty())
            {
                traverse_nodes(node.children, model, local_matrix, primitives_processed);
            }
        }
    }

    void ModelResource::unload()
    {
        //dynamic_free(meshes);
        //dynamic_free(materials);
        //dynamic_free(this);
    }
    

    template <typename src_type, typename dst_type>
    std::vector<dst_type> pad_components_to_type(src_type* source, size_t n_comp_src, size_t n_comp_dst, size_t n_items, bool normalized) {
        std::vector<dst_type> out_vector;

        // For each item
        for (size_t i = 0; i < n_items; ++i) {
            // For each component
            dst_type to_add;
            for (size_t comp_i = 0; comp_i < n_comp_dst; ++comp_i) {
                // Add first elements
                if (n_comp_src >= comp_i) {
                    to_add[comp_i] = static_cast<dst_type::value_type>(source[comp_i + n_comp_src * i]);
                    if (normalized) to_add[comp_i] /= std::numeric_limits<src_type>::max();
                }
                else {
                    to_add[comp_i] = 0;
                }
            }
            out_vector.push_back(to_add);
            printf("out_vector[%i][0] = %f\n", i, out_vector[i][0]);
        }

        return out_vector;
    }
    template <typename glm_type>
    std::vector<glm_type> gltf_to_glm(void* pointer, tinygltf::Accessor accessor) {
        std::vector<glm_type> out;

        // Get number of components
        size_t n_components = 0;
        switch (accessor.type) {
        case TINYGLTF_TYPE_SCALAR:
            n_components = 1;
            break;
        case TINYGLTF_TYPE_VEC2:
            n_components = 2;
            break;
        case TINYGLTF_TYPE_VEC3:
            n_components = 3;
            break;
        case TINYGLTF_TYPE_VEC4:
            n_components = 4;
            break;
        default:
            printf("unknown gltf type %i!\n", accessor.type);
            break;
        }

        // Get component type and convert to glm type
        switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_FLOAT:
            out = pad_components_to_type<float, glm_type>((float*)pointer, n_components, glm_type::length(), accessor.count, accessor.normalized);
            break;
        case TINYGLTF_COMPONENT_TYPE_BYTE:
            out = pad_components_to_type<int8_t, glm_type>((int8_t*)pointer, n_components, glm_type::length(), accessor.count, accessor.normalized);
            break;
        case TINYGLTF_COMPONENT_TYPE_SHORT:
            out = pad_components_to_type<int16_t, glm_type>((int16_t*)pointer, n_components, glm_type::length(), accessor.count, accessor.normalized);
            break;
        case TINYGLTF_COMPONENT_TYPE_INT:
            out = pad_components_to_type<int32_t, glm_type>((int32_t*)pointer, n_components, glm_type::length(), accessor.count, accessor.normalized);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            out = pad_components_to_type<uint8_t, glm_type>((uint8_t*)pointer, n_components, glm_type::length(), accessor.count, accessor.normalized);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            out = pad_components_to_type<uint16_t, glm_type>((uint16_t*)pointer, n_components, glm_type::length(), accessor.count, accessor.normalized);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            out = pad_components_to_type<uint32_t, glm_type>((uint32_t*)pointer, n_components, glm_type::length(), accessor.count, accessor.normalized);
            break;
        default:
            printf("unknown gltf component type %i!\n", accessor.type);
        }
        return out;
    }


    void ModelResource::create_vertex_array(MeshCPU& mesh_out, tinygltf::Primitive primitive_in, tinygltf::Model model, glm::mat4 trans_mat)
    {
        std::vector<glm::vec3> position_pointer;
        std::vector<glm::vec3> normal_pointer;
        std::vector<glm::vec4> tangent_pointer;
        std::vector<glm::vec4> colour_pointer;
        std::vector<glm::vec2> texcoord_pointer;
        std::vector<int> indices;

        for (auto& attrib : primitive_in.attributes)
        {
            //Structure binding type beat but I'm too lazy to switch to C++17 lmao
            auto& name = attrib.first;
            auto& accessor_index = attrib.second;

            //Get accessor
            auto& accessor = model.accessors[accessor_index];

            //Get bufferview
            auto& bufferview_index = accessor.bufferView;
            auto& bufferview = model.bufferViews[bufferview_index];

            //Find location in buffer
            auto& buffer_base = model.buffers[bufferview.buffer].data;
            void* buffer_pointer = &buffer_base[bufferview.byteOffset];
            assert(bufferview.byteStride == 0 && "byte_stride is not zero!");

            printf("\nname: %s\n", name.c_str());

            if (name._Equal("POSITION"))
            {
                position_pointer = gltf_to_glm<glm::vec3>(buffer_pointer, accessor);
            }
            else if (name._Equal("NORMAL"))
            {
                normal_pointer = gltf_to_glm<glm::vec3>(buffer_pointer, accessor);
            }
            else if (name._Equal("TANGENT"))
            {
                tangent_pointer = gltf_to_glm<glm::vec4>(buffer_pointer, accessor);
            }
            else if (name._Equal("TEXCOORD_0"))
            {
                texcoord_pointer = gltf_to_glm<glm::vec2>(buffer_pointer, accessor);
            }
            else if (name._Equal("COLOR_0"))
            {
                // todo: add type checking here cuz it's not always a short vector 3 now is it
                colour_pointer = gltf_to_glm<glm::vec4>(buffer_pointer, accessor);
            }
        }

        //Find indices
        {
            //Get accessor
            auto& accessor = model.accessors[primitive_in.indices];

            //Get bufferview
            auto& bufferview_index = accessor.bufferView;
            auto& bufferview = model.bufferViews[bufferview_index];

            //Find location in buffer
            auto& buffer_base = model.buffers[bufferview.buffer].data;
            void* buffer_pointer = &buffer_base[bufferview.byteOffset];
            int buffer_length = accessor.count;
            indices.reserve(buffer_length);
            assert(bufferview.byteStride == 0 && "byte_stride is not zero!");

            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                auto* indices_raw = static_cast<uint16_t*>(buffer_pointer);
                for (int i = 0; i < buffer_length; i++)
                {
                    indices.push_back(indices_raw[i]);
                }
            }
            if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                auto* indices_raw = static_cast<uint32_t*>(buffer_pointer);
                for (int i = 0; i < buffer_length; i++)
                {
                    indices.push_back(indices_raw[i]);
                }
            }
        }

        //Create vertex array
        {
            ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "mesh loading - vertex buffers";
            mesh_out.vertices = static_cast<Vertex*>(dynamic_allocate(sizeof(Vertex) * indices.size()));
            ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "mesh loading - index buffers";
            mesh_out.indices = static_cast<u32*>(dynamic_allocate(sizeof(u32) * indices.size()));
            ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "unknown";
            mesh_out.n_verts = 0;
            mesh_out.n_indices = 0;
            int i = 0;
            for (int index : indices)
            {
                Vertex vertex;
                if (!position_pointer.empty()) { vertex.position = trans_mat * glm::vec4(position_pointer[index], 1.0f); }
                if (!normal_pointer.empty()) { vertex.normal = glm::mat3(trans_mat) * (normal_pointer[index]); }
                if (!tangent_pointer.empty()) { vertex.tangent = glm::mat3(trans_mat) * (glm::vec3(tangent_pointer[index])); }
                if (!colour_pointer.empty()) {
                    vertex.colour = {
                        std::min(1.0f, powf(colour_pointer[index].x, 1.0f / 2.2f)),
                        std::min(1.0f, powf(colour_pointer[index].y, 1.0f / 2.2f)),
                        std::min(1.0f, powf(colour_pointer[index].z, 1.0f / 2.2f)),
                    };
                }
                if (!texcoord_pointer.empty()) { vertex.texcoord0 = texcoord_pointer[index]; }
                mesh_out.vertices[mesh_out.n_verts] = vertex;
                mesh_out.indices[i] = i;
                mesh_out.n_verts++;
                mesh_out.n_indices++;
                i++;
            }
        }
    }
}