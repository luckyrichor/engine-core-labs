// Phase 1 probe: can ufbx load a Bistro FBX, and do its triangle counts match
// the NVIDIA ORCA page? Prints one CSV line; never used by the core libraries.
#include <stdio.h>
#include <time.h>
#include "ufbx.h"

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s file.fbx\n", argv[0]); return 2; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    ufbx_load_opts opts = { 0 };
    ufbx_error error;
    ufbx_scene *scene = ufbx_load_file(argv[1], &opts, &error);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (!scene) {
        char buf[1024];
        ufbx_format_error(buf, sizeof(buf), &error);
        fprintf(stderr, "load failed: %s\n", buf);
        return 1;
    }
    size_t mesh_tris = 0, instance_tris = 0, instances = 0;
    for (size_t i = 0; i < scene->meshes.count; i++) {
        const ufbx_mesh *m = scene->meshes.data[i];
        mesh_tris += m->num_triangles;
        instance_tris += m->num_triangles * m->instances.count;
        instances += m->instances.count;
    }
    double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("lib,file,meshes,mesh_instances,nodes,unique_mesh_triangles,instanced_triangles,load_seconds\n");
    printf("ufbx-v0.23.1,%s,%zu,%zu,%zu,%zu,%zu,%.3f\n", argv[1], scene->meshes.count, instances,
           scene->nodes.count, mesh_tris, instance_tris, secs);
    ufbx_free_scene(scene);
    return 0;
}
