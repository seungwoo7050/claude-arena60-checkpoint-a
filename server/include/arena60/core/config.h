#pragma once

#include <cstdint>
#include <string>

namespace arena60 {

class GameConfig {
    std::uint16_t port_;
    std::uint16_t metrics_port_;
    double tick_rate_;
    std::string database_dsn_;

   public:
    GameConfig(std::uint16_t port, std::uint16_t metrics_port, double tick_rate,
               std::string database_dsn);

    static GameConfig FromEnv();

    std::uint16_t port() const noexcept { return port_; }
    std::uint16_t metrics_port() const noexcept { return metrics_port_; }
    double tick_rate() const noexcept { return tick_rate_; }
    const std::string& database_dsn() const noexcept { return database_dsn_; }
};

}  // namespace arena60
