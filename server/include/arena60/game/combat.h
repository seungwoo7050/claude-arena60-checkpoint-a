#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace arena60 {

class HealthComponent {
   public:
    explicit HealthComponent(int max_hp = 100);

    int current() const noexcept;
    int max() const noexcept;
    bool is_alive() const noexcept;

    // Applies damage and returns true if the component transitioned to dead state.
    bool ApplyDamage(int amount);
    void Reset();

   private:
    int max_;
    int current_;
};

enum class CombatEventType { Hit, Death };

struct CombatEvent {
    CombatEventType type{CombatEventType::Hit};
    std::string shooter_id;
    std::string target_id;
    std::string projectile_id;
    int damage{0};
    std::uint64_t tick{0};
};

class CombatLog {
   public:
    explicit CombatLog(std::size_t capacity = 32);

    void Add(const CombatEvent& event);
    std::vector<CombatEvent> Snapshot() const;
    std::size_t Size() const noexcept;
    std::size_t Capacity() const noexcept;

   private:
    std::size_t capacity_;
    std::deque<CombatEvent> events_;
};

}  // namespace arena60
