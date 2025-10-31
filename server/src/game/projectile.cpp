#include "arena60/game/projectile.h"

#include <cmath>
#include <stdexcept>

namespace arena60 {

namespace {
constexpr double kEpsilon = 1e-9;
}

Projectile::Projectile(std::string id, std::string owner_id, double x, double y, double dir_x,
                       double dir_y, double spawn_time_seconds)
    : id_(std::move(id)),
      owner_id_(std::move(owner_id)),
      x_(x),
      y_(y),
      dir_x_(dir_x),
      dir_y_(dir_y),
      spawn_time_(spawn_time_seconds) {
    const double magnitude = std::sqrt(dir_x_ * dir_x_ + dir_y_ * dir_y_);
    if (magnitude < kEpsilon) {
        throw std::invalid_argument("Projectile direction must be non-zero");
    }
    dir_x_ /= magnitude;
    dir_y_ /= magnitude;
}

void Projectile::Advance(double delta_seconds) {
    if (!active_) {
        return;
    }
    x_ += dir_x_ * kSpeed_ * delta_seconds;
    y_ += dir_y_ * kSpeed_ * delta_seconds;
}

bool Projectile::IsExpired(double now_seconds) const {
    if (!active_) {
        return true;
    }
    return (now_seconds - spawn_time_) >= kLifetime_;
}

void Projectile::Deactivate() { active_ = false; }

const std::string& Projectile::id() const noexcept { return id_; }

const std::string& Projectile::owner_id() const noexcept { return owner_id_; }

double Projectile::x() const noexcept { return x_; }

double Projectile::y() const noexcept { return y_; }

double Projectile::direction_x() const noexcept { return dir_x_; }

double Projectile::direction_y() const noexcept { return dir_y_; }

double Projectile::spawn_time() const noexcept { return spawn_time_; }

bool Projectile::active() const noexcept { return active_; }

double Projectile::radius() const noexcept { return kRadius_; }

double Projectile::Speed() noexcept { return kSpeed_; }

double Projectile::Lifetime() noexcept { return kLifetime_; }

}  // namespace arena60
