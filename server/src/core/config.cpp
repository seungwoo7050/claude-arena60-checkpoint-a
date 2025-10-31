#include "arena60/core/config.h"

#include <cstdlib>
#include <stdexcept>

namespace {
constexpr double kDefaultTickRate = 60.0;
constexpr std::uint16_t kDefaultPort = 8080;
constexpr const char* kDefaultDsn = "postgresql://localhost:5432/arena60";

double ParseDoubleOrDefault(const char* value, double fallback) {
  if (!value) {
    return fallback;
  }
  try {
    return std::stod(value);
  } catch (const std::exception&) {
    return fallback;
  }
}

std::uint16_t ParsePortOrDefault(const char* value, std::uint16_t fallback) {
  if (!value) {
    return fallback;
  }
  try {
    const long parsed = std::stol(value);
    if (parsed < 0 || parsed > 65535) {
      return fallback;
    }
    return static_cast<std::uint16_t>(parsed);
  } catch (const std::exception&) {
    return fallback;
  }
}
}  // namespace

namespace arena60 {

GameConfig::GameConfig(std::uint16_t port, double tick_rate, std::string database_dsn)
    : port_(port), tick_rate_(tick_rate), database_dsn_(std::move(database_dsn)) {}

GameConfig GameConfig::FromEnv() {
  const char* env_port = std::getenv("ARENA60_PORT");
  const char* env_tick = std::getenv("ARENA60_TICK_RATE");
  const char* env_dsn = std::getenv("ARENA60_DATABASE_DSN");

  const auto port = ParsePortOrDefault(env_port, kDefaultPort);
  const auto tick_rate = ParseDoubleOrDefault(env_tick, kDefaultTickRate);
  const std::string dsn = env_dsn ? env_dsn : kDefaultDsn;

  return GameConfig{port, tick_rate, dsn};
}

}  // namespace arena60
