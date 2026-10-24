// Offline preprocessing only; ufbx never enters experiment targets.
#include "ufbx.h"
#include "workload_io.hpp"
#include <bit>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>

namespace fs = std::filesystem;
namespace workload = ecl::workload;
using Box = std::array<float, 6>;
Box empty_box() {
    const float inf = std::numeric_limits<float>::infinity();
    return {inf,inf,inf,-inf,-inf,-inf};
}
void include(Box& a, const Box& b) {
    for (int k=0;k<3;++k) { a[k]=std::min(a[k],b[k]); a[k+3]=std::max(a[k+3],b[k+3]); }
}
void write_box(std::ofstream& out, const Box& b) {
    for (float f:b) {
        const auto v=std::bit_cast<std::uint32_t>(f);
        for (int k=0;k<4;++k) out.put(static_cast<char>((v>>(8*k))&255));
    }
}
void json_box(std::ostream& out,const Box& b) {
    out << '[';
    for (int i=0;i<6;++i) { if(i) out << ','; out << b[i]; }
    out << ']';
}
void process(const fs::path& input,const fs::path& outdir,std::uint64_t expected) {
    ufbx_load_opts opts{};
    ufbx_error error{};
    std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(
        ufbx_load_file(input.string().c_str(),&opts,&error),ufbx_free_scene);
    if (!scene) { char msg[2048]; ufbx_format_error(msg,sizeof(msg),&error); throw std::runtime_error(msg); }
    auto stem=input.stem().string();
    std::ofstream instances(outdir/(stem+".instances.bin"),std::ios::binary);
    std::ofstream triangles(outdir/(stem+".triangles.bin"),std::ios::binary);
    instances.exceptions(std::ios::badbit|std::ios::failbit);
    triangles.exceptions(std::ios::badbit|std::ios::failbit);
    Box overall=empty_box(), tri_union=empty_box();
    std::uint64_t objects=0, tris=0;
    for (auto* mesh:scene->meshes) {
        std::vector<std::uint32_t> indices(mesh->max_face_triangles*3);
        for (auto* node:mesh->instances) {
            Box instance=empty_box();
            // Tight world bounds: transform vertices, never transform only two local corners.
            std::vector<Box> vertices;
            vertices.reserve(mesh->num_vertices);
            for(auto v:mesh->vertices) {
                auto p=ufbx_transform_position(&node->geometry_to_world,v);
                Box b{float(p.x),float(p.y),float(p.z),float(p.x),float(p.y),float(p.z)};
                for(float f:b) if(!std::isfinite(f)) throw std::runtime_error("nonfinite world vertex");
                include(instance,b); vertices.push_back(b);
            }
            if(vertices.empty()) throw std::runtime_error("empty mesh instance");
            write_box(instances,instance); ++objects; include(overall,instance);
            for(auto face:mesh->faces) {
                const auto count=ufbx_triangulate_face(indices.data(),indices.size(),mesh,face);
                for(std::uint32_t t=0;t<count;++t) {
                    Box b=empty_box();
                    for(int k=0;k<3;++k) include(b,vertices.at(mesh->vertex_indices.data[indices[t*3+k]]));
                    write_box(triangles,b); include(tri_union,b); ++tris;
                }
            }
        }
    }
    instances.close(); triangles.close();
    if(tris!=expected) throw std::runtime_error("unexpected triangle count for "+stem);
    // Both aggregates use the same rounded binary32 world positions: exact equality is stronger than tolerance.
    if(overall!=tri_union) throw std::runtime_error("triangle union differs from scene bounds");
    std::ofstream meta(outdir/(stem+".meta.json"));
    meta.exceptions(std::ios::badbit|std::ios::failbit);
    meta << std::setprecision(std::numeric_limits<float>::max_digits10);
    meta << "{\n  \"source\":\"" << input.filename().string() << "\",\n  \"source_sha256\":\""
         << workload::sha256(workload::read_bytes(input)) << "\",\n  \"ufbx_version\":\"0.23.1\",\n"
         << "  \"ufbx_header_version\":" << UFBX_HEADER_VERSION << ",\n  \"ufbx_source_version\":" << ufbx_source_version
         << ",\n  \"format\":\"little_endian_float32_min_xyz_max_xyz\",\n  \"transform\":\"geometry_to_world\",\n"
         << "  \"object_count\":" << objects << ",\n  \"triangle_count\":" << tris << ",\n  \"overall_aabb\":";
    json_box(meta,overall); meta << ",\n  \"triangle_union_aabb\":"; json_box(meta,tri_union);
    meta << ",\n  \"triangle_union_matches\":true,\n  \"files\":{";
    for(int k=0;k<2;++k) {
        auto name=stem+(k ? ".triangles.bin" : ".instances.bin");
        if(k) meta << ',';
        meta << '\"' << name << "\":\"" << workload::sha256(workload::read_bytes(outdir/name)) << '\"';
    }
    meta << "}\n}\n";
    std::cout << stem << ": " << objects << " instances, " << tris << " triangles\n";
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("usage: bistro_aabb <Bistro_v5_2 directory> <output directory>");
        if(UFBX_HEADER_VERSION!=ufbx_source_version || UFBX_HEADER_VERSION!=ufbx_pack_version(0,23,1))
            throw std::runtime_error("ufbx version mismatch");
        fs::create_directories(argv[2]);
        process(fs::path(argv[1])/"BistroInterior.fbx",argv[2],1046609);
        process(fs::path(argv[1])/"BistroInterior_Wine.fbx",argv[2],1320323);
        process(fs::path(argv[1])/"BistroExterior.fbx",argv[2],2832120);
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
