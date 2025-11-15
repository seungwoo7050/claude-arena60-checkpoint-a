# Arena60 - Real-time 1v1 Duel Game Server

> **🎯 For Clone Coding Learners**: This README provides a complete learning path to build a production-quality game server from scratch. Follow the **[Learning Path](#-learning-path-for-clone-coding)** section below to get started.

Production-quality game server for Korean game industry portfolio. Built with C++17, Boost.Asio/Beast, PostgreSQL, and Prometheus.

**Tech Stack**: C++17 · Boost 1.82+ · PostgreSQL 15 · Redis 7 · Protocol Buffers · Docker · Prometheus · WebSocket

---

## 📖 Table of Contents

- [Status](#status-checkpoint-a-complete-)
- [**Learning Path (Start Here!)**](#-learning-path-for-clone-coding) ⭐
- [Features](#features-checkpoint-a)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [Documentation](#documentation)
- [Testing](#testing-guide)
- [Monitoring](#monitoring)

---

## Status: Checkpoint A Complete ✅

- [x] **Checkpoint A**: 1v1 Duel Game (MVP 1.0-1.3) ← **You are here**
- [ ] Checkpoint B: 60-player Battle Royale
- [ ] Checkpoint C: Esports Platform

---

## 🎓 Learning Path for Clone Coding

This section guides you through building Arena60 from scratch, step by step.

### Prerequisites

Before starting, ensure you have:

1. **Basic C++ knowledge** (variables, functions, classes, pointers)
2. **Linux/macOS terminal** familiarity
3. **Git** installed
4. **Docker Desktop** (for databases)
5. **Time commitment**: ~80-100 hours for Checkpoint A

**Don't have C++ experience?** Start with `knowledges/30-cpp-for-game-server.md`

---

### 📚 Step 0: Understand the Project Structure

```
arena60-checkpoint-a/
├── knowledges/           # 📖 Learning materials (READ THESE FIRST!)
│   ├── 30-39: Basic      # C++, CMake, Game Loop, WebSocket
│   ├── 40-49: Advanced   # Protobuf, Collision, JWT, ELO
│   ├── 60-79: Tools      # PostgreSQL, Redis, Docker, Testing, Monitoring
│   └── 80-89: MVPs       # Step-by-step implementation guides
│
├── server/               # 💻 Your implementation goes here
│   ├── include/          # Header files (.h)
│   ├── src/              # Implementation files (.cpp)
│   └── tests/            # Unit/integration tests
│
├── docs/
│   ├── mvp-specs/        # Detailed requirements for each MVP
│   └── evidence/         # Performance benchmarks, proof of completion
│
├── deployments/docker/   # Infrastructure (PostgreSQL, Redis, Prometheus)
├── CLAUDE.md             # Project instructions for AI
└── README.md             # This file
```

**Key Concept**: The `knowledges/` directory contains all the learning materials you need. Read them in order!

---

### 📖 Step 1: Foundation (Week 1-2, ~20 hours)

Learn the core technologies before writing code.

**Read in this order:**

1. **[30-cpp-for-game-server.md](knowledges/30-cpp-for-game-server.md)** (3h)
   - C++ essentials: pointers, references, RAII
   - Smart pointers, multithreading
   - Socket programming basics

2. **[31-cmake-build-system.md](knowledges/31-cmake-build-system.md)** (2h)
   - CMake basics: `CMakeLists.txt`
   - Linking libraries (Boost, PostgreSQL)
   - Build configurations (Debug/Release)

3. **[32-cpp-game-loop.md](knowledges/32-cpp-game-loop.md)** (4h)
   - Fixed-step game loop (60 TPS target)
   - Frame pacing and delta time
   - Tick variance measurement

4. **[33-boost-asio-beast.md](knowledges/33-boost-asio-beast.md)** (5h)
   - Boost.Asio async I/O
   - Boost.Beast HTTP/WebSocket
   - Event-driven architecture

5. **[34-websocket-game-server.md](knowledges/34-websocket-game-server.md)** (6h)
   - WebSocket protocol for games
   - State synchronization
   - Client-server architecture

**Practice**: After reading, set up your dev environment:
```bash
# Install dependencies
./tools/setup-dev.sh

# Build "Hello World" WebSocket server
cd examples/hello-websocket
mkdir build && cd build
cmake .. && make
./hello_server
```

---

### 🎮 Step 2: MVP 1.0 - Basic Game Server (Week 3-4, ~30 hours)

**Goal**: WebSocket server + 60 TPS game loop + player movement

**Read:**
- **[81-mvp-1.0-basic-game-server.md](knowledges/81-mvp-1.0-basic-game-server.md)** (10h)
  - Complete implementation guide
  - Step-by-step code examples
  - Testing instructions

**Additional Resources:**
- `docs/mvp-specs/mvp-1.0.md` - Formal requirements
- `knowledges/60-postgresql-redis-docker.md` - Database setup
- `knowledges/70-google-test.md` - Unit testing

**Implementation Checklist:**
- [ ] WebSocket server accepts connections (port 8080)
- [ ] Game loop runs at 60 TPS (variance ≤ 1ms)
- [ ] Players can join and move (WASD input)
- [ ] Server broadcasts state 60 times/sec
- [ ] PostgreSQL stores session events
- [ ] Tests pass: `ctest -R MVP1_0`
- [ ] Metrics exposed: `curl localhost:8081/metrics`

**Expected Duration**: 30 hours (15 hours coding, 15 hours debugging/testing)

**Success Criteria**:
```bash
# 1. Build passes
cd server/build && make -j$(nproc)

# 2. Tests pass
ctest -R MVP1_0 --output-on-failure

# 3. Performance check
./tests/performance/test_tick_variance
# Expected: Tick variance ≤ 1.0 ms

# 4. Manual test
wscat -c ws://localhost:8080
> input player1 0 1 0 0 0 100 200
< state player1 105.0 200.0 0.0 60
```

---

### ⚔️ Step 3: MVP 1.1 - Combat System (Week 5-6, ~25 hours)

**Goal**: Projectile shooting + collision detection + damage system

**Read:**
- **[35-collision-detection.md](knowledges/35-collision-detection.md)** (4h)
  - Circle-circle collision
  - Spatial partitioning for performance

- **[82-mvp-1.1-combat-system.md](knowledges/82-mvp-1.1-combat-system.md)** (8h)
  - Projectile physics
  - Damage system
  - Death mechanics

**Additional Resources:**
- `knowledges/42-snapshot-delta-sync.md` - Efficient state updates
- `docs/mvp-specs/mvp-1.1.md` - Combat specifications

**Implementation Checklist:**
- [ ] Click to shoot projectiles
- [ ] Projectile-player collision detection
- [ ] Damage system (20 HP per hit, 100 HP pool)
- [ ] Death mechanics (health → 0)
- [ ] Combat log (ring buffer, 32 events)
- [ ] Tests pass: `ctest -R MVP1_1`
- [ ] Performance: Collision < 0.5ms per tick

**Expected Duration**: 25 hours

**Success Criteria**:
```bash
# Performance test
./tests/performance/test_projectile_perf
# Expected: Combat tick duration < 0.5 ms (avg)

# Integration test
./tests/integration/test_websocket_combat
# Expected: Full combat scenario works end-to-end
```

---

### 🔄 Step 4: MVP 1.2 - Matchmaking (Week 7-8, ~20 hours)

**Goal**: ELO-based matchmaking queue + concurrent matches

**Read:**
- **[44-elo-db-integration.md](knowledges/44-elo-db-integration.md)** (5h)
  - ELO rating algorithm
  - PostgreSQL integration
  - Match recording

- **[45-matchmaking-system.md](knowledges/45-matchmaking-system.md)** (5h)
  - Matchmaking queue (Redis)
  - ELO-based pairing
  - Tolerance expansion

- **[83-mvp-1.2-matchmaking.md](knowledges/83-mvp-1.2-matchmaking.md)** (6h)
  - Implementation guide

**Additional Resources:**
- `knowledges/60-postgresql-redis-docker.md` - Redis setup
- `docs/mvp-specs/mvp-1.2.md` - Matchmaking specs

**Implementation Checklist:**
- [ ] Players can join matchmaking queue
- [ ] ELO-based matching (±100 initial tolerance)
- [ ] Tolerance expands every 5 seconds
- [ ] Supports 10+ concurrent matches
- [ ] Match notifications to players
- [ ] Tests pass: `ctest -R MVP1_2`
- [ ] Performance: Matchmaking ≤ 2ms for 200 players

**Expected Duration**: 20 hours

**Success Criteria**:
```bash
# Integration test
./tests/integration/test_matchmaker_flow
# Expected: 20 players → 10 matches created

# Performance test
./tests/performance/test_matchmaking_perf
# Expected: ≤ 2ms for 200 players
```

---

### 📊 Step 5: MVP 1.3 - Statistics & Ranking (Week 9-10, ~25 hours)

**Goal**: Post-match stats + ELO updates + leaderboard + HTTP API

**Read:**
- **[84-mvp-1.3-stats-ranking.md](knowledges/84-mvp-1.3-stats-ranking.md)** (8h)
  - Stats aggregation
  - ELO calculator
  - Leaderboard system
  - HTTP API design

**Additional Resources:**
- `knowledges/43-jwt-game-integration.md` - JWT authentication (optional)
- `docs/mvp-specs/mvp-1.3.md` - Stats specifications

**Implementation Checklist:**
- [ ] Post-match stats (shots, hits, accuracy, damage, K/D)
- [ ] ELO rating updates (K=25)
- [ ] Global leaderboard (sorted by rating)
- [ ] HTTP API endpoints:
  - `GET /profiles/:player_id`
  - `GET /leaderboard?limit=10`
- [ ] Tests pass: `ctest -R MVP1_3`
- [ ] Performance: Profile service ≤ 5ms per match

**Expected Duration**: 25 hours

**Success Criteria**:
```bash
# HTTP API test
curl http://localhost:8081/profiles/player1
# Expected: JSON response with stats

curl http://localhost:8081/leaderboard?limit=10
# Expected: Top 10 players sorted by ELO

# Performance test
./tests/performance/test_profile_service_perf
# Expected: < 1ms for 100 matches
```

---

### 📈 Step 6: Monitoring & Finalization (Week 11-12, ~15 hours)

**Goal**: Prometheus metrics + Grafana dashboards + final polish

**Read:**
- **[71-prometheus-grafana.md](knowledges/71-prometheus-grafana.md)** (5h)
  - Prometheus metrics
  - Grafana dashboards
  - KPI monitoring

**Tasks:**
1. **Add Prometheus metrics** (5h)
   - Game tick rate, duration
   - WebSocket connections
   - Combat events
   - Matchmaking queue size

2. **Create Grafana dashboard** (3h)
   - 8+ panels showing KPIs
   - Real-time monitoring

3. **Final testing** (4h)
   - Run all tests: `ctest`
   - Performance benchmarks
   - Load testing

4. **Documentation** (3h)
   - Update README with your results
   - Add evidence packs to `docs/evidence/`

**Success Criteria**:
- ✅ All tests pass: `ctest` (21 test files)
- ✅ Test coverage ≥ 70%
- ✅ All KPIs met (see [Performance Benchmarks](#performance-benchmarks))
- ✅ Grafana dashboard shows live metrics

---

### 🎯 Final Checklist - Checkpoint A Complete

Before claiming completion, verify:

**Technical Requirements:**
- [ ] Server builds without errors: `make -j$(nproc)`
- [ ] All tests pass: `ctest` (21 test files)
- [ ] Test coverage ≥ 70%: `lcov` report

**Performance (KPIs):**
- [ ] Tick rate variance ≤ 1.0 ms
- [ ] WebSocket latency (p99) ≤ 20 ms
- [ ] Combat tick duration (avg) < 0.5 ms
- [ ] Matchmaking (200 players) ≤ 2 ms
- [ ] Profile service (100 matches) ≤ 5 ms

**Features:**
- [ ] MVP 1.0: WebSocket server, 60 TPS game loop, player movement
- [ ] MVP 1.1: Projectiles, collision, damage, death
- [ ] MVP 1.2: Matchmaking queue, ELO-based pairing, concurrent matches
- [ ] MVP 1.3: Stats, ELO updates, leaderboard, HTTP API

**Documentation:**
- [ ] Performance reports in `docs/evidence/`
- [ ] README updated with your results
- [ ] Git commit history shows incremental progress

**Monitoring:**
- [ ] Prometheus metrics: `curl localhost:8081/metrics`
- [ ] Grafana dashboard: `http://localhost:3000`

**🎉 Congratulations!** You've built a production-quality game server!

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

| Metric | Target | Actual | Status | Evidence |
|--------|--------|--------|--------|----------|
| Tick rate variance | ≤ 1.0 ms | **0.04 ms** | ✅ | [1.0](./docs/evidence/mvp-1.0/performance-report.md) |
| WebSocket latency (p99) | ≤ 20 ms | **18.3 ms** | ✅ | [1.0](./docs/evidence/mvp-1.0/performance-report.md) |
| Combat tick duration (avg) | < 0.5 ms | **0.31 ms** | ✅ | [1.1](./docs/evidence/mvp-1.1/performance-report.md) |
| Matchmaking (200 players) | ≤ 2 ms | **≤ 2 ms** | ✅ | [1.2](./docs/evidence/mvp-1.2/performance-report.md) |
| Profile service (100 matches) | ≤ 5 ms | **< 1 ms** | ✅ | [1.3](./docs/evidence/mvp-1.3/performance-report.md) |

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

### Option 1: wscat (Quick Test)

**Install**:
```bash
npm install -g wscat
```

**Usage**:
```bash
# Connect to server
wscat -c ws://localhost:8080

# Send movement input (W key pressed, mouse at 150, 200)
> input player1 0 1 0 0 0 150.5 200.0

# Server responds with state
< state player1 100.0 200.0 0.0 60
< state player1 105.0 200.0 0.0 61
< state player1 110.0 200.0 0.0 62

# Send fire input (mouse click at 300, 100)
> input player1 1 0 0 0 0 300.0 100.0

# Server responds with state
< state player1 115.0 200.0 1.047 63
```

**Input Format**:
- `player_id`: Unique player identifier (e.g., "player1")
- `seq`: Sequence number (incremental, for debugging)
- `up down left right`: Movement keys (1 = pressed, 0 = released)
- `mouse_x mouse_y`: Mouse cursor position (world coordinates)

### Option 2: Python Test Client (Automated)

**Install**:
```bash
pip install websockets
```

**Usage**:
```bash
# Run automated test
python tools/test_client.py

# Run with custom player ID
python tools/test_client.py --player player2

# Run multiple clients (stress test)
python tools/test_client.py --clients 10
```

See `tools/README.md` for detailed usage.

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

## Documentation

### 📚 Learning Materials (`knowledges/`)

**Foundation (30-39):**
- `30-cpp-for-game-server.md` - C++ essentials for game servers
- `31-cmake-build-system.md` - CMake build configuration
- `32-cpp-game-loop.md` - 60 TPS fixed-step game loop
- `33-boost-asio-beast.md` - Async I/O and WebSocket
- `34-websocket-game-server.md` - Real-time game networking
- `35-collision-detection.md` - Circle collision, spatial partitioning

**Advanced (40-49):**
- `40-protobuf-basics.md` - Binary serialization (future use)
- `42-snapshot-delta-sync.md` - Efficient state synchronization
- `43-jwt-game-integration.md` - JWT authentication
- `44-elo-db-integration.md` - ELO rating + PostgreSQL
- `45-matchmaking-system.md` - Matchmaking queue system

**Tools & Infrastructure (60-79):**
- `60-postgresql-redis-docker.md` - Database setup and integration
- `70-google-test.md` - Unit testing with Google Test
- `71-prometheus-grafana.md` - Metrics and monitoring

**Implementation Guides (80-89):**
- `80-arena60-checkpoint-a-overview.md` - Project overview
- `81-mvp-1.0-basic-game-server.md` - Step-by-step MVP 1.0
- `82-mvp-1.1-combat-system.md` - Step-by-step MVP 1.1
- `83-mvp-1.2-matchmaking.md` - Step-by-step MVP 1.2
- `84-mvp-1.3-stats-ranking.md` - Step-by-step MVP 1.3

### 📋 Specifications (`docs/mvp-specs/`)

- `mvp-1.0.md` - Basic game server (formal requirements)
- `mvp-1.1.md` - Combat system (detailed specs)
- `mvp-1.2.md` - Matchmaking (algorithm specifications)
- `mvp-1.3.md` - Statistics & ranking (API contracts)

### 📊 Evidence Packs (`docs/evidence/`)

- `mvp-1.0/` - Performance reports, CI logs, metrics
- `mvp-1.1/` - Combat performance benchmarks
- `mvp-1.2/` - Matchmaking throughput tests
- `mvp-1.3/` - Profile service benchmarks

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
├── knowledges/              # 📖 Learning materials
│   ├── 30-cpp-for-game-server.md
│   ├── 31-cmake-build-system.md
│   ├── 32-cpp-game-loop.md
│   ├── 33-boost-asio-beast.md
│   ├── 34-websocket-game-server.md
│   ├── 35-collision-detection.md
│   ├── 40-protobuf-basics.md
│   ├── 42-snapshot-delta-sync.md
│   ├── 43-jwt-game-integration.md
│   ├── 44-elo-db-integration.md
│   ├── 45-matchmaking-system.md
│   ├── 60-postgresql-redis-docker.md
│   ├── 70-google-test.md
│   ├── 71-prometheus-grafana.md
│   ├── 80-arena60-checkpoint-a-overview.md
│   ├── 81-mvp-1.0-basic-game-server.md
│   ├── 82-mvp-1.1-combat-system.md
│   ├── 83-mvp-1.2-matchmaking.md
│   └── 84-mvp-1.3-stats-ranking.md
│
├── server/                  # 💻 Implementation
│   ├── include/arena60/
│   │   ├── core/            # GameLoop, Config
│   │   ├── game/            # GameSession, Combat, Projectile
│   │   ├── network/         # WebSocketServer, HTTP routers
│   │   ├── matchmaking/     # Matchmaker, Queue
│   │   ├── stats/           # ProfileService, Leaderboard
│   │   └── storage/         # PostgresStorage
│   ├── src/                 # Implementation (.cpp)
│   ├── tests/
│   │   ├── unit/            # 13 unit tests
│   │   ├── integration/     # 4 integration tests
│   │   └── performance/     # 4 performance benchmarks
│   └── CMakeLists.txt
│
├── deployments/
│   └── docker/
│       └── docker-compose.yml    # PostgreSQL, Redis, Prometheus, Grafana
│
├── docs/
│   ├── mvp-specs/           # Detailed MVP requirements
│   └── evidence/            # Performance reports, CI logs
│
├── .meta/
│   └── state.yml            # Project version tracking
│
├── CLAUDE.md                # Project instructions
└── README.md                # This file
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

---

## Acknowledgments

**Learning Resources**:
- Valve - [Source Multiplayer Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking)
- Gaffer on Games - [Game Networking](https://gafferongames.com/)
- Glenn Fiedler - [Networking for Game Programmers](https://gafferongames.com/categories/game-networking/)

**Technologies**:
- [Boost C++ Libraries](https://www.boost.org/)
- [PostgreSQL](https://www.postgresql.org/)
- [Prometheus](https://prometheus.io/)
- [Google Test](https://github.com/google/googletest)
