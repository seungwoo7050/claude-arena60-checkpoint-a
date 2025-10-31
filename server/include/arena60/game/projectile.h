#pragma once

#include <string>

namespace arena60 {

class Projectile {
   public:
    Projectile(std::string id, std::string owner_id, double x, double y, double dir_x, double dir_y,
               double spawn_time_seconds);

    void Advance(double delta_seconds);
    bool IsExpired(double now_seconds) const;
    void Deactivate();

    const std::string& id() const noexcept;
    const std::string& owner_id() const noexcept;
    double x() const noexcept;
    double y() const noexcept;
    double direction_x() const noexcept;
    double direction_y() const noexcept;
    double spawn_time() const noexcept;
    bool active() const noexcept;
    double radius() const noexcept;

    static double Speed() noexcept;
    static double Lifetime() noexcept;

   private:
    std::string id_;
    std::string owner_id_;
    double x_;
    double y_;
    double dir_x_;
    double dir_y_;
    double spawn_time_;
    bool active_{true};

    static constexpr double kSpeed_ = 30.0;    // meters per second
    static constexpr double kLifetime_ = 1.5;  // seconds
    static constexpr double kRadius_ = 0.2;    // meters
};

}  // namespace arena60
