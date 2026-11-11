#include "graphics/render.hpp"

#include <algorithm>
#include <glad/glad.h>

namespace
{
    constexpr u32 initial_vertex_cap = 1 << 16;
    constexpr u32 initial_index_cap = 1 << 18;

    struct AttribInfo
    {
        VertexFormat flag;
        GLint components;
        GLuint offset;
    };

    // index in this table = attribute location, so shaders can use layout(location = N)
    constexpr AttribInfo attribs[] = {
        {VertexFormat::Position, 3, offsetof(Vertex, position)},
        {VertexFormat::Normal, 3, offsetof(Vertex, normal)},
        {VertexFormat::UV0, 2, offsetof(Vertex, uv0)},
    };

    bool has(VertexFormat fmt, VertexFormat flag) { return (fmt & flag) == flag; }

    uin make_buffer(GLsizeiptr bytes)
    {
        uin buf = 0;
        glGenBuffers(1, &buf);
        glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
        glBufferStorage(GL_COPY_WRITE_BUFFER, bytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
        return buf;
    }

    uin make_shader(GLenum type,const char* src) {
        uin shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);

        return shader;
    }

    uin make_program(uin* shaders, int shader_count) {
        uin program = glCreateProgram();

        for (int i = 0; i < shader_count; i++) {
            glAttachShader(program,shaders[i]);
        }

        glLinkProgram(program);
        for (int i = 0; i < shader_count; i++) {
            glDeleteShader(shaders[i]);
        }
    }

    // immutable storage can't be resized, so allocate a bigger one and copy over
    uin grow_buffer(uin old, GLsizeiptr usedBytes, GLsizeiptr newBytes)
    {
        uin buf = make_buffer(newBytes);
        glBindBuffer(GL_COPY_READ_BUFFER, old);
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, usedBytes);
        glDeleteBuffers(1, &old);
        return buf;
    }

    void upload(uin buf, GLintptr offset, GLsizeiptr bytes, const void* data)
    {
        glBindBuffer(GL_COPY_WRITE_BUFFER, buf);
        glBufferSubData(GL_COPY_WRITE_BUFFER, offset, bytes, data);
    }

    u32 grow_cap(u32 cap, u32 needed)
    {
        while (cap < needed)
            cap *= 2;
        return cap;
    }
} // namespace


RenderMgr::RenderMgr(const RenderConfiguration& cfg)
{ drawCalls.reserve(1024); }

RenderMgr::~RenderMgr()
{
    for (auto& entry : pools)
    {
        FormatPool& pool = entry.second;
        glDeleteVertexArrays(1, &pool.vao);
        glDeleteBuffers(1, &pool.vbo);
        glDeleteBuffers(1,&pool.ibo);
    }
}

ShaderHandle RenderMgr::create_shader(const ShaderDescriptor& descriptor) {

    uin* _shaders = new uin[2]; // todo: allow multiple stages
    _shaders[0] = make_shader(GL_VERTEX_SHADER,descriptor.vertexSource.c_str());
    _shaders[1] = make_shader(GL_FRAGMENT_SHADER,descriptor.fragmentSource.c_str());

    shaders.push_back(GpuShader(make_program(_shaders, 2)))
}

MaterialHandle RenderMgr::create_material(const ShaderHandle& handle)
{
    materials.push_back(GpuMaterial(shaders.at((u32)handle)));

    return MaterialHandle(materials.size() - 1);
}

MeshHandle RenderMgr::create_mesh(const MeshDescriptor& descriptor)
{
    if (descriptor.vertices.empty() || descriptor.indices.empty())
        throw_error("mesh has no vertices or indices");

    FormatPool& pool = get_pool(descriptor.format);

    const u32 vtxCount = u32(descriptor.vertices.size());
    const u32 idxCount = u32(descriptor.indices.size());

    if (pool.vertexCt + vtxCount > pool.vertexCap)
    {
        const u32 cap = grow_cap(pool.vertexCap, pool.vertexCt + vtxCount);
        pool.vbo = grow_buffer(pool.vbo, int(pool.vertexCt) * sizeof(Vertex), GLsizeiptr(cap) * sizeof(Vertex));
        pool.vertexCap = cap;

        glBindVertexArray(pool.vao);
        glBindVertexBuffer(0, pool.vbo, 0, sizeof(Vertex));
        glBindVertexArray(0);
    }

    if (pool.indexCt + idxCount > pool.indexCap)
    {
        const u32 cap = grow_cap(pool.indexCap, pool.indexCt + idxCount);
        pool.ibo = grow_buffer(pool.ibo, GLsizeiptr(pool.indexCt) * sizeof(u32), GLsizeiptr(cap) * sizeof(u32));
        pool.indexCap = cap;

        glBindVertexArray(pool.vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pool.ibo);
        glBindVertexArray(0);
    }

    upload(pool.vbo, GLintptr(pool.vertexCt) * sizeof(Vertex), GLsizeiptr(descriptor.vertices.size_bytes()),
           descriptor.vertices.data());
    upload(pool.ibo, GLintptr(pool.indexCt) * sizeof(u32), GLsizeiptr(descriptor.indices.size_bytes()),
           descriptor.indices.data());

    meshes.push_back({descriptor.format, pool.vertexCt, pool.indexCt, idxCount});

    pool.vertexCt += vtxCount;
    pool.indexCt += idxCount;

    return MeshHandle(meshes.size() - 1);
}

void RenderMgr::submit_draw_call(const DrawCall& drawCall)
{
    if (drawCall.mesh >= meshes.size() || drawCall.material >= materials.size())
        throw_error("draw call references a mesh or material that doesn't exist");
    if (drawCalls.size() >= cfg.maxDrawCalls)
        return;

    drawCalls.push_back(drawCall);
}

void RenderMgr::flush()
{
    std::sort(drawCalls.begin(), drawCalls.end(),
              [this](const DrawCall& a, const DrawCall& b)
              {
                  if (a.material != b.material)
                      return a.material < b.material;
                  return meshes[a.mesh].fmt < meshes[b.mesh].fmt;
              });

    uin curProgram = 0;
    uin curVao = 0;

    for (const DrawCall& dc : drawCalls)
    {
        const GpuMesh& mesh = meshes[dc.mesh];

        const GpuMaterial& material = materials[dc.material];
        program = shaders[material.shaderHandle].handle;
        if (program != curProgram)
        {
            glUseProgram(program);
            curProgram = program;
        }
        material.apply();

        const uin vao = pools.at(mesh.fmt).vao;
        if (vao != curVao)
        {
            glBindVertexArray(vao);
            curVao = vao;
        }

        glDrawElementsInstancedBaseVertexBaseInstance(
            GL_TRIANGLES, GLsizei(mesh.idx), GL_UNSIGNED_INT,
            reinterpret_cast<const void*>(std::uintptr_t(mesh.first_idx) * sizeof(u32)), 1, GLint(mesh.base_vtx), 0);
    }

    glBindVertexArray(0);
    drawCalls.clear();
}

RenderMgr::FormatPool& RenderMgr::get_pool(VertexFormat fmt)
{
    if (auto it = pools.find(fmt); it != pools.end())
        return it->second;

    if (!has(fmt, VertexFormat::Position))
        throw_error("vertex format has no position");
    if ((fmt & (VertexFormat::UV1 | VertexFormat::UV2)) != VertexFormat::None)
        throw_error("UV1/UV2 aren't in Vertex yet");

    FormatPool pool;
    pool.vertexCap = initial_vertex_cap;
    pool.indexCap = initial_index_cap;
    pool.vbo = make_buffer(GLsizeiptr(pool.vertexCap) * sizeof(Vertex));
    pool.ibo = make_buffer(GLsizeiptr(pool.indexCap) * sizeof(u32));

    glGenVertexArrays(1, &pool.vao);
    glBindVertexArray(pool.vao);

    for (GLuint loc = 0; loc < std::size(attribs); ++loc)
    {
        const AttribInfo& a = attribs[loc];
        if (!has(fmt, a.flag))
            continue;

        glEnableVertexAttribArray(loc);
        glVertexAttribFormat(loc, a.components, GL_FLOAT, GL_FALSE, a.offset);
        glVertexAttribBinding(loc, 0);
    }

    glBindVertexBuffer(0, pool.vbo, 0, sizeof(Vertex));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pool.ibo);

    glBindVertexArray(0);

    return pools.emplace(fmt, pool).first->second;
}
