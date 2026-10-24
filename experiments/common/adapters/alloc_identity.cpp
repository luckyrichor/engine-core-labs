#include "../alloc_adapter.hpp"
#include <stdexcept>
#ifdef ECL_ALLOC_JEMALLOC
#include <jemalloc/jemalloc.h>
#elif defined(ECL_ALLOC_MIMALLOC)
#include <mimalloc.h>
#elif defined(ECL_ALLOC_GPERFTOOLS)
#include <gperftools/malloc_extension.h>
#elif defined(ECL_ALLOC_GOOGLE)
#include <tcmalloc/malloc_extension.h>
#endif
#ifdef __linux__
#include <dlfcn.h>
#endif
namespace ecl::adapter {
std::string Allocator::identity() const {
    std::string name=ECL_ALLOC_NAME;
#ifdef ECL_ALLOC_JEMALLOC
    const char* version=nullptr;std::size_t size=sizeof(version);
    if (mallctl("version",&version,&size,nullptr,0)!=0 || !version) throw std::runtime_error("jemalloc mallctl failed");
    if (!std::string(version).starts_with(std::string(ECL_ALLOC_VERSION)+"-")) throw std::runtime_error("jemalloc version mismatch");
    void* probe=std::malloc(31);
    unsigned arena=0;std::size_t arena_size=sizeof(arena);
    const int owned=mallctl("arenas.lookup",&arena,&arena_size,&probe,sizeof(probe));
    std::free(probe);
    if (owned!=0) throw std::runtime_error("malloc is not jemalloc");
    name+=" version=";name+=version;
#elif defined(ECL_ALLOC_MIMALLOC)
    if (mi_version()<=0) throw std::runtime_error("mimalloc version failed");
    void* probe=std::malloc(31);
    const bool owned=mi_is_in_heap_region(probe);std::free(probe);
    if (!owned) throw std::runtime_error("malloc is not mimalloc");
    name+=" version="+std::to_string(mi_version());
#elif defined(ECL_ALLOC_GPERFTOOLS)
    std::size_t value=0;
    if (!MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes",&value)) throw std::runtime_error("gperftools extension failed");
    void* probe=std::malloc(31);
    const bool owned=MallocExtension::instance()->GetOwnership(probe)==MallocExtension::kOwned;
    std::free(probe);
    if (!owned) throw std::runtime_error("malloc is not gperftools");
    name+=" extension=active";
#elif defined(ECL_ALLOC_GOOGLE)
    if (!tcmalloc::MallocExtension::GetNumericProperty("generic.current_allocated_bytes").has_value()) throw std::runtime_error("google tcmalloc extension failed");
    void* probe=std::malloc(31);
    const bool owned=tcmalloc::MallocExtension::GetOwnership(probe)==tcmalloc::MallocExtension::Ownership::kOwned;
    std::free(probe);
    if (!owned) throw std::runtime_error("malloc is not google tcmalloc");
    name+=" extension=active";
#endif
#ifdef __linux__
#ifndef ECL_ALLOC_LOCAL
    Dl_info info{};
    auto* symbol=dlsym(RTLD_DEFAULT,"malloc");
    if (!symbol || !dladdr(symbol,&info) || !info.dli_fname) throw std::runtime_error("malloc provider unknown");
    const std::string provider=info.dli_fname;
#if defined(ECL_ALLOC_JEMALLOC)
    const char* expected="jemalloc";
#elif defined(ECL_ALLOC_MIMALLOC)
    const char* expected="mimalloc";
#elif defined(ECL_ALLOC_GPERFTOOLS)
    const char* expected="tcmalloc";
#elif defined(ECL_ALLOC_GOOGLE)
    const char* expected="alloc_replay_google";
#else
    const char* expected="libc.so";
#endif
    if (provider.find(expected)==std::string::npos) throw std::runtime_error("unexpected malloc provider: "+provider);
    name+=" malloc_provider="+provider;
#endif
#endif
    return name;
}
}
