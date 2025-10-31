#pragma once

#include <cstdint>
#include <string>

namespace arena60 {

struct PlayerState {
    std::string player_id;
    double x{0.0};
    double y{0.0};
    double facing_radians{0.0};
    std::uint64_t last_sequence{0};
};

}  // namespace arena60
