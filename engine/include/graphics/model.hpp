#pragma once
#include "graphics/render.hpp"
#include "math/quaternion.hpp"

#include <array>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace filesys
{
    class FilesystemOperator;
}

namespace graphics
{
    using ModelHandle = u32;
    constexpr ModelHandle invalid_model = UINT32_MAX;
    constexpr MaterialHandle invalid_material = UINT32_MAX;

    // ---- format-neutral data every loader produces ----

    struct MeshData
    {
        std::string name;
        std::vector<math::vec3> positions;
        std::vector<math::vec3> normals;
        std::vector<math::vec4> tangents;
        std::array<std::vector<math::vec2>, 3> uvs;
        std::vector<math::vec4> colors;
        std::vector<std::array<u16, 4>> joints;
        std::vector<math::vec4> weights;
        std::vector<u32> indices;
        u32 material = 0;
    };

    enum class TextureFormat : u8
    {
        R8,
        RG8,
        RGB8,
        RGBA8,
        BC1,
        BC2,
        BC3,
        BC4,
        BC5
    };

    struct TextureData
    {
        std::string name;
        u32 width = 0, height = 0;
        TextureFormat format = TextureFormat::RGBA8;
        std::vector<std::vector<byte>> mips; // mip 0 first, already deswizzled
        bool srgb = false;
        std::string cacheKey;
    };

    using ParamValue = std::variant<float, int, math::vec2, math::vec3, math::vec4>;

    struct MaterialData
    {
        std::string name;
        std::string shader;
        std::unordered_map<std::string, ParamValue> params;
        std::unordered_map<std::string, u32> textures; // sampler name -> ModelData::textures index
        RenderState state;
    };

    struct NodeData
    {
        std::string name;
        int parent = -1;
        math::vec3 translation{};
        math::quat rotation = math::quat::identity();
        math::vec3 scale{1.0f, 1.0f, 1.0f};
        std::vector<u32> meshes; // ModelData::meshes indices
        int skin = -1;
    };

    enum class AnimPath : u8
    {
        Translation,
        Rotation,
        Scale
    };
    enum class AnimInterp : u8
    {
        Step,
        Linear,
        CubicSpline
    };

    struct AnimationData
    {
        struct Channel
        {
            u32 node = 0;
            AnimPath path = AnimPath::Translation;
            AnimInterp interp = AnimInterp::Linear;
            std::vector<float> times;
            std::vector<math::vec4> values; // cubic: in-tangent, value, out-tangent per key
        };
        std::string name;
        float duration = 0.0f;
        std::vector<Channel> channels;
    };

    struct SkinData
    {
        std::vector<u32> joints;
        std::vector<math::mat4> inverseBind;
    };

    struct ModelData
    {
        std::vector<MeshData> meshes;
        std::vector<MaterialData> materials;
        std::vector<TextureData> textures;
        std::vector<NodeData> nodes; // parent-first
        std::vector<AnimationData> animations;
        std::vector<SkinData> skins;
    };

    struct LoadContext
    {
        std::string baseDir;
        std::function<std::vector<byte>(const std::string& path)> read;
    };



    class ModelLoader
    {
    public:
        virtual ~ModelLoader() = default;
        virtual bool handles(std::string_view extension) const = 0; // lowercase, with the dot
        virtual ModelData load(std::span<const byte> data, const LoadContext& ctx) = 0;
    };

    // ---- uploaded, shared, immutable ----

    struct ModelAsset
    {
        struct Part
        {
            MeshHandle mesh;
            u32 materialSlot;
            u32 node;
            int skin = -1;
        };

        std::vector<Part> parts;
        std::vector<MaterialHandle> materials; // default per slot
        std::vector<NodeData> nodes; // rest pose
        std::vector<math::mat4> restWorld;
        std::vector<AnimationData> animations;
        std::vector<SkinData> skins;
    };

    using MaterialResolver = std::function<MaterialHandle(const MaterialData&, std::span<const TextureHandle>)>;

    class ModelCache
    {
    public:
        ModelCache(RenderMgr& renderer, filesys::FilesystemOperator& fs) : renderer(renderer), fs(fs) {}

        void add_loader(std::unique_ptr<ModelLoader> loader) { loaders.push_back(std::move(loader)); }
        void set_material_resolver(MaterialResolver r) { resolver = std::move(r); }

        ModelHandle load(const std::string& path);
        ModelHandle generate(std::string key,const ModelData& data);
        const ModelAsset& get(ModelHandle h) ret(assets[h]);

    private:
        ModelData read(const std::string& path);
        ModelAsset upload(const ModelData& data);

        RenderMgr& renderer;
        filesys::FilesystemOperator& fs;
        MaterialResolver resolver;
        std::vector<std::unique_ptr<ModelLoader>> loaders;
        std::vector<ModelAsset> assets;
        std::unordered_map<std::string, ModelHandle> byPath;
        std::unordered_map<std::string, TextureHandle> textureCache;
    };

    void compute_node_world(std::span<const NodeData> nodes, std::span<math::mat4> out);

    void submit_model(RenderMgr& r, const ModelAsset& model, const math::mat4& world,
                      std::span<const MaterialHandle> overrides = {}, std::span<const math::mat4> pose = {});

    std::unique_ptr<ModelLoader> make_gltf_loader();
} // namespace graphics
