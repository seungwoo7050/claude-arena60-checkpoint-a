#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#include "arena60/core/config.h"

class EnvVarGuard {
   public:
    EnvVarGuard(const char* name)
        : name_(name), previous_(std::getenv(name) ? std::getenv(name) : "") {
        if (std::getenv(name_)) {
            has_previous_ = true;
        }
    }

    ~EnvVarGuard() {
        if (has_previous_) {
            setenv(name_, previous_.c_str(), 1);
        } else {
            unsetenv(name_);
        }
    }

   private:
    const char* name_;
    std::string previous_;
    bool has_previous_{false};
};

TEST(GameConfigTest, ReadsEnvironmentVariables) {
    EnvVarGuard port_guard("ARENA60_PORT");
    EnvVarGuard metrics_guard("ARENA60_METRICS_PORT");
    EnvVarGuard tick_guard("ARENA60_TICK_RATE");
    EnvVarGuard dsn_guard("ARENA60_DATABASE_DSN");

    setenv("ARENA60_PORT", "12345", 1);
    setenv("ARENA60_METRICS_PORT", "54321", 1);
    setenv("ARENA60_TICK_RATE", "75.0", 1);
    setenv("ARENA60_DATABASE_DSN", "postgresql://example.com:5432/arena", 1);

    const auto config = arena60::GameConfig::FromEnv();

    EXPECT_EQ(12345, config.port());
    EXPECT_EQ(54321, config.metrics_port());
    EXPECT_DOUBLE_EQ(75.0, config.tick_rate());
    EXPECT_EQ("postgresql://example.com:5432/arena", config.database_dsn());
}
