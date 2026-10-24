// Phase 1 probe: the same counts as ufbx_probe.c, through Assimp.
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <chrono>
#include <cstdio>

static void walk(const aiScene *s, const aiNode *n, size_t &inst, size_t &inst_tris) {
    for (unsigned i = 0; i < n->mNumMeshes; i++) {
        inst++;
        inst_tris += s->mMeshes[n->mMeshes[i]]->mNumFaces;
    }
    for (unsigned i = 0; i < n->mNumChildren; i++) walk(s, n->mChildren[i], inst, inst_tris);
}

int main(int argc, char **argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: %s file.fbx\n", argv[0]); return 2; }
    Assimp::Importer importer;
    auto t0 = std::chrono::steady_clock::now();
    const aiScene *s = importer.ReadFile(argv[1], aiProcess_Triangulate);
    auto t1 = std::chrono::steady_clock::now();
    if (!s) { std::fprintf(stderr, "load failed: %s\n", importer.GetErrorString()); return 1; }
    size_t tris = 0, inst = 0, inst_tris = 0;
    for (unsigned i = 0; i < s->mNumMeshes; i++) tris += s->mMeshes[i]->mNumFaces;
    walk(s, s->mRootNode, inst, inst_tris);
    std::printf("lib,file,meshes,mesh_instances,nodes,unique_mesh_triangles,instanced_triangles,load_seconds\n");
    std::printf("assimp-v6.0.5,%s,%u,%zu,-,%zu,%zu,%.3f\n", argv[1], s->mNumMeshes, inst, tris, inst_tris,
                std::chrono::duration<double>(t1 - t0).count());
    return 0;
}
