#include "arena60/game/combat.h"

#include <algorithm>

namespace arena60 {

HealthComponent::HealthComponent(int max_hp) : max_(max_hp), current_(max_hp) {}

int HealthComponent::current() const noexcept { return current_; }

int HealthComponent::max() const noexcept { return max_; }

bool HealthComponent::is_alive() const noexcept { return current_ > 0; }

bool HealthComponent::ApplyDamage(int amount) {
    if (amount <= 0) {
        return false;
    }
    if (!is_alive()) {
        return false;
    }
    current_ = std::max(0, current_ - amount);
    return current_ == 0;
}

void HealthComponent::Reset() { current_ = max_; }

CombatLog::CombatLog(std::size_t capacity) : capacity_(capacity) {}

void CombatLog::Add(const CombatEvent& event) {
    if (capacity_ == 0) {
        return;
    }
    events_.push_back(event);
    if (events_.size() > capacity_) {
        events_.pop_front();
    }
}

std::vector<CombatEvent> CombatLog::Snapshot() const {
    return std::vector<CombatEvent>(events_.begin(), events_.end());
}

std::size_t CombatLog::Size() const noexcept { return events_.size(); }

std::size_t CombatLog::Capacity() const noexcept { return capacity_; }

}  // namespace arena60
