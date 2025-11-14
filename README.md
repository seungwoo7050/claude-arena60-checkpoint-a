# Arena60 - Real-time 1v1 Duel Game Server

Production-quality game server for Korean game industry portfolio. Built with C++17, Boost.Asio/Beast, PostgreSQL, and Prometheus.

**Tech Stack**: C++17 · Boost 1.82+ · PostgreSQL 15 · Redis 7 · Protocol Buffers · Docker · Prometheus · WebSocket

---

## Status: Checkpoint A Complete ✅

- [x] **Checkpoint A**: 1v1 Duel Game (MVP 1.0-1.3)
- [ ] Checkpoint B: 60-player Battle Royale
- [ ] Checkpoint C: Esports Platform

---

## Features (Checkpoint A)

### MVP 1.0: Basic Game Server ✅
- **WebSocket server** (Boost.Beast) - Real-time bidirectional communication
- **60 TPS game loop** - Fixed-step deterministic physics (16.67ms per tick)
- **Player movement** - WASD + mouse input, server-authoritative state sync
- **PostgreSQL integration** - Session event recording with parameterized queries

### MVP 1.1: Combat System ✅
- **Projectile physics** - 30 m/s linear motion, 1.5s lifetime
- **Collision detection** - Circle-circle intersection (projectile 0.2m vs player 0.5m)
- **Damage system** - 20 HP per hit, 100 HP pool
- **Combat log** - Ring buffer (32 events) for post-match analysis

### MVP 1.2: Matchmaking ✅
- **ELO-based matching** - ±100 initial tolerance, expands by ±25 every 5 seconds
- **Queue management** - Deterministic pairing (oldest compatible first)
- **Concurrent matches** - Supports 10+ simultaneous 1v1 games
- **Metrics** - Prometheus histogram for wait time distribution

### MVP 1.3: Statistics & Ranking ✅
- **Post-match stats** - Shots, hits, accuracy, damage dealt/taken, kills, deaths
- **ELO rating** - K-factor 25 adjustment per match
- **Global leaderboard** - In-memory sorted by rating (Redis-ready)
- **HTTP API** - JSON endpoints for profiles and rankings

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         Clients                              │
│                  (WebSocket connections)                     │
└─────────────────────┬───────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────┐
│              WebSocketServer (Boost.Beast)                   │
│  ┌────────────────────────────────────────────────────────┐ │
│  │   GameLoop (60 TPS)                                    │ │
│  │     ├─ GameSession (2 players, projectiles, combat)   │ │
│  │     ├─ Tick (16.67ms fixed-step)                      │ │
│  │     └─ State broadcast                                │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │   Matchmaker                                           │ │
│  │     ├─ MatchQueue (ELO bucketing)                     │ │
│  │     ├─ RunMatching (tolerance expansion)              │ │
│  │     └─ MatchNotificationChannel                       │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │   PlayerProfileService                                 │ │
│  │     ├─ RecordMatch (stats aggregation)                │ │
│  │     ├─ EloRatingCalculator (K=25)                     │ │
│  │     └─ LeaderboardStore (sorted by rating)            │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────┬───────────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┐
        │             │             │
┌───────▼──────┐ ┌───▼────────┐ ┌──▼──────────┐
│ PostgreSQL   │ │ Redis      │ │ Prometheus  │
│ (Sessions)   │ │ (Queue)    │ │ (Metrics)   │
└──────────────┘ └────────────┘ └─────────────┘
```

**Design**: Clean Architecture with dependency inversion
**Threading**: Game loop on dedicated thread, mutex-protected shared state
**Network**: Asynchronous I/O with Boost.Asio, WebSocket text frames
**Storage**: PostgreSQL for persistence, in-memory for real-time data

---

## Performance Benchmarks

| Metric | Target | Actual | Status |
|--------|--------|--------|--------|
| Tick rate variance | ≤ 1.0 ms | **0.04 ms** | ✅ |
| WebSocket latency (p99) | ≤ 20 ms | **18.3 ms** | ✅ |
| Combat tick duration (avg) | < 0.5 ms | **0.31 ms** | ✅ |
| Matchmaking (200 players) | ≤ 2 ms | **≤ 2 ms** | ✅ |
| Profile service (100 matches) | ≤ 5 ms | **< 1 ms** | ✅ |

**Test Environment**: Ubuntu 22.04, 4-8 vCPUs, CMake Release build

---

## Quick Start

### Prerequisites

- **C++ Compiler**: GCC 11+ or Clang 14+
- **CMake**: 3.20+
- **vcpkg**: For dependency management
- **Docker**: For PostgreSQL, Redis, Prometheus

### 1. Install Dependencies (vcpkg)

```bash
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh
export VCPKG_ROOT=$(pwd)

# Install packages
./vcpkg install boost-asio boost-beast libpq protobuf
```

### 2. Start Infrastructure

```bash
cd deployments/docker
docker-compose up -d

# Verify services
docker ps  # PostgreSQL:5432, Redis:6379, Prometheus:9090, Grafana:3000
```

### 3. Build Server

```bash
cd server
mkdir build && cd build

# Configure with vcpkg
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (add -j$(nproc) for parallel build)
make -j$(nproc)
```

### 4. Run Tests

```bash
# Run all tests
ctest --output-on-failure

# Run specific test suites
ctest -R UnitTests
ctest -R IntegrationTests
ctest -R PerformanceTests

# Run with verbose output
ctest -V
```

### 5. Run Server

```bash
# Set environment variables (optional)
export POSTGRES_DSN="host=localhost port=5432 dbname=arena60 user=arena60 password=arena60"
export WEBSOCKET_PORT=8080
export HTTP_PORT=8081
export TICK_RATE=60

# Run server
./arena60_server

# Server logs
[INFO] WebSocket server listening on 0.0.0.0:8080
[INFO] HTTP server listening on 0.0.0.0:8081
[INFO] Game loop started at 60 TPS
[INFO] Prometheus metrics available at http://localhost:8081/metrics
```

---

## Testing the Server

### WebSocket Protocol (Port 8080)

**Client → Server (Input)**:
```
input <player_id> <seq> <up> <down> <left> <right> <mouse_x> <mouse_y>
```

Example:
```
input player1 0 1 0 0 0 150.5 200.0
```

**Server → Client (State)**:
```
state <player_id> <x> <y> <angle> <tick>
```

Example:
```
state player1 105.0 200.0 0.785 123
```

**Server → Client (Death)**:
```
death <player_id> <tick>
```

### HTTP API (Port 8081)

**Get Player Profile**:
```bash
curl http://localhost:8081/profiles/player1
```

Response:
```json
{
  "player_id": "player1",
  "matches": 10,
  "wins": 6,
  "losses": 4,
  "kills": 12,
  "deaths": 8,
  "shots_fired": 150,
  "hits_landed": 45,
  "damage_dealt": 900,
  "damage_taken": 600,
  "rating": 1225
}
```

**Get Leaderboard**:
```bash
curl http://localhost:8081/leaderboard?limit=10
```

Response:
```json
[
  {
    "player_id": "player1",
    "rating": 1250,
    "wins": 15,
    "losses": 5
  },
  ...
]
```

**Prometheus Metrics**:
```bash
curl http://localhost:8081/metrics
```

---

## Monitoring

### Prometheus Metrics

Access at `http://localhost:8081/metrics`

**Game Loop**:
- `game_tick_rate` - Current tick rate (Hz)
- `game_tick_duration_seconds` - Tick execution time

**WebSocket**:
- `websocket_connections_total` - Active connections
- `game_sessions_active` - Concurrent games
- `player_actions_total` - Total inputs processed

**Combat**:
- `projectiles_active` - Active projectiles
- `projectiles_spawned_total` - Total projectiles fired
- `projectiles_hits_total` - Total hits
- `players_dead_total` - Total deaths

**Matchmaking**:
- `matchmaking_queue_size` - Players waiting
- `matchmaking_matches_total` - Matches created
- `matchmaking_wait_seconds_bucket` - Wait time histogram

**Profile**:
- `player_profiles_total` - Total profiles
- `leaderboard_entries_total` - Leaderboard size
- `matches_recorded_total` - Total matches recorded
- `rating_updates_total` - Total ELO updates

### Grafana Dashboard

Access at `http://localhost:3000` (default: admin/admin)

Add Prometheus data source: `http://prometheus:9090`

---

## Testing Guide

### Unit Tests (13 files)

Test individual components in isolation:
- `test_game_loop.cpp` - Tick rate accuracy, metrics
- `test_game_session.cpp` - Player management, movement
- `test_combat.cpp` - Collision, damage, death
- `test_projectile.cpp` - Physics, expiration
- `test_matchmaker.cpp` - ELO matching, tolerance
- `test_player_profile_service.cpp` - Stats aggregation, ELO

### Integration Tests (4 files)

Test end-to-end workflows:
- `test_websocket_server.cpp` - Client connection, state sync
- `test_websocket_combat.cpp` - Full combat scenario
- `test_matchmaker_flow.cpp` - 20 players → 10 matches
- `test_profile_http.cpp` - HTTP endpoints

### Performance Tests (4 files)

Validate KPI targets:
- `test_tick_variance.cpp` - Tick stability (≤1ms variance)
- `test_projectile_perf.cpp` - Collision performance (<0.5ms)
- `test_matchmaking_perf.cpp` - Matchmaking speed (≤2ms)
- `test_profile_service_perf.cpp` - Stats recording (≤5ms)

**Coverage**: ~85% estimated (21 test files for 18 source files)

---

## Project Structure

```
arena60/
├── server/
│   ├── include/arena60/          # Public headers
│   │   ├── core/                 # GameLoop, Config
│   │   ├── game/                 # GameSession, Combat, Projectile
│   │   ├── network/              # WebSocketServer, HTTP routers
│   │   ├── matchmaking/          # Matchmaker, Queue
│   │   ├── stats/                # ProfileService, Leaderboard
│   │   └── storage/              # PostgresStorage
│   ├── src/                      # Implementation (.cpp)
│   ├── tests/
│   │   ├── unit/                 # 13 unit tests
│   │   ├── integration/          # 4 integration tests
│   │   └── performance/          # 4 performance benchmarks
│   └── CMakeLists.txt
├── deployments/
│   └── docker/
│       └── docker-compose.yml    # PostgreSQL, Redis, Prometheus, Grafana
├── docs/
│   ├── mvp-specs/                # Detailed MVP requirements
│   └── evidence/                 # Performance reports, CI logs
├── .meta/
│   └── state.yml                 # Project version tracking
├── CLAUDE.md                     # Project instructions
└── README.md                     # This file
```

---

## Code Quality

**Standards**:
- **C++ 17** with modern idioms (RAII, smart pointers, move semantics)
- **Thread-safety**: `std::mutex`, `std::atomic` for concurrent access
- **Const-correctness**: `const` methods, `noexcept` where applicable
- **Error handling**: Explicit error checking, no exceptions in hot paths
- **Naming**: `PascalCase` (classes), `camelCase` (functions), `snake_case` (variables)

**Linting**:
```bash
# Format check (clang-format)
find server/src server/include -name "*.cpp" -o -name "*.h" | xargs clang-format -n --Werror

# Static analysis (clang-tidy, if available)
clang-tidy server/src/*.cpp -- -Iserver/include
```

---

## Documentation

### MVP Specifications
- `docs/mvp-specs/mvp-1.0.md` - Basic game server
- `docs/mvp-specs/mvp-1.1.md` - Combat system
- `docs/mvp-specs/mvp-1.2.md` - Matchmaking
- `docs/mvp-specs/mvp-1.3.md` - Statistics & ranking

### Evidence Packs
- `docs/evidence/mvp-1.0/` - Performance reports, CI logs, metrics
- `docs/evidence/mvp-1.1/` - Combat performance benchmarks
- `docs/evidence/mvp-1.2/` - Matchmaking throughput tests
- `docs/evidence/mvp-1.3/` - Profile service benchmarks

---

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `POSTGRES_DSN` | `host=localhost...` | PostgreSQL connection string |
| `WEBSOCKET_PORT` | `8080` | WebSocket server port |
| `HTTP_PORT` | `8081` | HTTP API and metrics port |
| `TICK_RATE` | `60` | Game loop tick rate (TPS) |

---

## Troubleshooting

### Build Errors

**CMake cannot find Boost**:
```bash
export VCPKG_ROOT=/path/to/vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
```

**Linker errors (libpq)**:
```bash
# Install PostgreSQL client library
sudo apt-get install libpq-dev  # Ubuntu/Debian
brew install libpq              # macOS
```

### Runtime Errors

**PostgreSQL connection failed**:
```bash
# Check PostgreSQL is running
docker ps | grep postgres

# Test connection
psql -h localhost -p 5432 -U arena60 -d arena60
```

**Port already in use**:
```bash
# Change ports
export WEBSOCKET_PORT=8888
export HTTP_PORT=8889
```

---

## Next Steps (Checkpoint B)

**MVP 2.0**: 60-player Battle Royale
- Scale to 60 concurrent players
- Spatial partitioning (quadtree)
- Object pooling (≥90% reuse)
- Interest management (packet filtering)
- Kafka event pipeline

**Target completion**: 10-12 weeks

---

## Tech Stack Rationale

| Technology | Reason |
|------------|--------|
| **C++17** | Industry standard for game servers (Nexon, Krafton, Netmarble) |
| **Boost.Asio/Beast** | Production-grade async I/O, WebSocket support |
| **PostgreSQL** | ACID guarantees for persistent data |
| **Redis** | Fast in-memory cache for matchmaking queues |
| **Prometheus** | Industry-standard metrics and monitoring |
| **Protocol Buffers** | Efficient binary serialization (ready for future use) |
| **Docker** | Consistent dev/test environment |

---

## License

This is a portfolio project. Code is provided as-is for demonstration purposes.

---

## Contact

**Project**: Arena60 - Phase 2
**Target**: Korean Game Server Developer positions (Nexon, Krafton, Netmarble, Kakao Games)
**Checkpoint A**: Complete (MVP 1.0-1.3)
