#pragma once
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>

namespace engine_labs::phase3 {
// Single owner push/pop, multiple thieves. Conservative SC variant of
// Chase-Lev: https://fzn.fr/readings/ppopp13.pdf . All indices AND ring slots
// are SC atomics: no relaxed-memory fence assumptions, no non-atomic slot race.
// Bounded ring: never resize/reclaim while thieves exist. IDs reference immutable
// task storage retained until join. External producers must use an inbox.
class ChaseLev {
    using Index = std::int64_t;
    const std::size_t capacity_, mask_;
    std::unique_ptr<std::atomic<std::size_t>[]> ring_;
    alignas(64) std::atomic<Index> top_{0};
    alignas(64) std::atomic<Index> bottom_{0};
public:
    explicit ChaseLev(std::size_t capacity)
      : capacity_(std::bit_ceil(capacity+1)), mask_(capacity_-1),
        ring_(std::make_unique<std::atomic<std::size_t>[]>(capacity_)) {
        if(capacity > (std::size_t{1}<<30)) throw std::invalid_argument("deque too large");
    }
    bool push(std::size_t id) { // owner only
        const auto b=bottom_.load(), t=top_.load();
        if(b-t>=static_cast<Index>(capacity_)) return false;
        ring_[static_cast<std::size_t>(b)&mask_].store(id);
        bottom_.store(b+1); // publishes slot before any thief observes availability
        return true;
    }
    std::optional<std::size_t> pop() { // owner only
        const auto b=bottom_.load()-1;
        bottom_.store(b); // SC store/load orders against thief top CAS
        auto t=top_.load();
        if(t>b) { bottom_.store(t); return {}; }
        const auto id=ring_[static_cast<std::size_t>(b)&mask_].load();
        if(t==b) { // last element: owner and thieves arbitrate on the same top CAS
            const bool won=top_.compare_exchange_strong(t,t+1);
            bottom_.store(b+1);
            if(!won) return {};
        }
        return id;
    }
    std::optional<std::size_t> steal() {
        auto t=top_.load();
        if(t>=bottom_.load()) return {};
        const auto id=ring_[static_cast<std::size_t>(t)&mask_].load();
        if(!top_.compare_exchange_strong(t,t+1)) return {};
        return id;
    }
};
}
