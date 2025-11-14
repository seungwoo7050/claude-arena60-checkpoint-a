---
name: "Arena60 - Production Battle Arena Games"
description: "Three production-quality games: 1v1 Duel, 60-player Battle Royale, Esports Platform"
category: "Backend Service / Game Server"
author: "Arena60 Project - Phase 2"
tags: ["c++", "game-server", "production", "portfolio", "korean-game-industry"]
lastUpdated: "2025-01-30"
---

# Arena60 – Production Battle Arena Games

## Project Overview

Phase 2 of Arena60: Build three production-quality games for Korean game industry portfolio.

**Prerequisites**: Phase 1 (mini-gameserver) complete  
**Duration**: 24 weeks  
**Deliverables**: 3 checkpoints, each a complete playable game

## Three Games (Checkpoints)

**Checkpoint A** (8-10 weeks): 1v1 Duel Game → Entry-level portfolio  
**Checkpoint B** (10-12 weeks): 60-player Battle Royale → Mid-level portfolio  
**Checkpoint C** (6-8 weeks): Esports Platform → Senior-level portfolio

## Tech Stack

**Core**: C++17, boost.asio/beast 1.82+, PostgreSQL 15+, Redis 7+, Protocol Buffers 3.21+, boost::lockfree 1.82+  
**Infrastructure**: Kafka 3.5+, Prometheus, Grafana, Docker, Kubernetes  
**Build**: CMake 3.20+, GCC 11+/Clang 14+, Google Test  
**Target**: Nexon, Krafton, Netmarble, Kakao Games (100% match)

## Project Structure

```
# No binary files in repository

arena60/
├── server/
│   ├── src/
│   │   ├── core/           # Game loop, tick manager
│   │   ├── game/           # Combat, movement, physics
│   │   ├── network/        # WebSocket server
│   │   ├── storage/        # PostgreSQL, Redis
│   │   └── monitoring/     # Prometheus metrics
│   ├── include/
│   ├── tests/
│   │   ├── unit/
│   │   ├── integration/
│   │   └── performance/
│   └── CMakeLists.txt
├── deployments/
│   ├── docker/
│   │   └── docker-compose.yml
│   └── kubernetes/         # Checkpoint C
├── monitoring/
│   ├── prometheus/
│   └── grafana/
├── docs/
│   ├── mvp-specs/         # Detailed MVP requirements
│   │   ├── mvp-1.0.md
│   │   ├── mvp-1.1.md
│   │   └── ...
│   └── evidence/
│       ├── checkpoint-a/
│       ├── checkpoint-b/
│       └── checkpoint-c/
├── .meta/
│   └── state.yml          # Version tracking
└── README.md
```

## Key Performance Indicators (KPIs)

**Must achieve ALL**:
- Server Tick Rate: 60 TPS (stable under load)
- Client Latency: p99 ≤ 50 ms
- State Sync: ≤ 16.67 ms (60 FPS)
- Concurrent Players: 60+ (Checkpoint B)
- Test Coverage: ≥ 70%
- Error Rate: ≤ 0.1%

## Checkpoint A: 1v1 Duel Game

**Product**: Complete 1v1 real-time combat game  
**Target**: Entry to Junior Game Server Developer  
**Timeline**: 8-10 weeks

### MVPs

**MVP 1.0**: Basic Game Server
- WebSocket server (boost.beast)
- 60 TPS game loop
- Player movement (WASD + mouse)
- Database integration (PostgreSQL)

**MVP 1.1**: Combat System
- Projectile system (click to shoot)
- Collision detection (circle-circle)
- Damage system (20 HP per hit)
- Death mechanics (health → 0)

**MVP 1.2**: Matchmaking
- ELO-based matching (±100 ELO)
- Redis queue (sorted set)
- Concurrent match support (10+ games)
- Match creation and notification

**MVP 1.3**: Statistics & Ranking
- Post-match stats (damage, accuracy, kills)
- ELO rating adjustment (K=25)
- Global leaderboard (Redis sorted set)
- Player profile API

### Deliverables

- System architecture diagram
- Technical summary
- Demo video (5 min)
- Performance benchmarks
- All MVP evidence packs
- Working game (playable)

---

## Checkpoint B: 60-player Battle Royale

**Product**: Large-scale battle royale  
**Target**: Mid-level Game Server Developer  
**Timeline**: 10-12 weeks

### MVPs

**MVP 2.0**: 60-Player Core
- Scale to 60 concurrent players
- Spatial partitioning (quadtree/grid)
- Delta compression (state updates)
- Performance optimization
- Object pool (Projectile, Reuse Item)
- Interest Management (View-Based Filtering)
- **Performance Targets (MVP 2.0)**:
    - Object reuse rate: ≥ 90%
    - Packet filtering: ≥ 80% reduction
    - Memory allocation: < 100 allocs/sec

**MVP 2.1**: Skills System
- 3 unique abilities per player
- Cooldown management
- Skill effects (Dash, Shield, Area Blast)
- Server-side validation
- Component-based design pattern

**MVP 2.2**: Item & Loot
- Collectible items (health, weapons, buffs)
- Inventory system (10 items max)
- Item spawning (random positions)
- Pickup collision detection

**MVP 2.3**: Safe Zone & Game Flow
- Shrinking safe zone (force encounters)
- Zone damage (10 HP/sec outside)
- Match duration: ~6-8 minutes
- Game end conditions

**MVP 2.4**: Leaderboard & Spectator
- Real-time kill leaderboard
- Death spectator mode
- Kill feed broadcast
- Final placement ranking

**MVP 2.5**: Economy & Events
- In-game currency (coins)
- Shop system (skins, emotes)
- Kafka event pipeline
- Lock-free queue (boost::lockfree)
- Event consumers (analytics, replay)
- **Performance Targets (MVP 2.5)**:
    - Queue contention: < 1ms p99
    - Throughput: ≥ 10k events/sec

### Deliverables

- All Checkpoint A features +
- 60-player load test results
- Spatial partitioning visualization
- Kafka event logs
- Updated architecture diagram

---

## Checkpoint C: Esports Platform

**Product**: Tournament & esports infrastructure  
**Target**: Senior Game Server Developer  
**Timeline**: 6-8 weeks

### MVPs

**MVP 3.0**: Multi-Region Deployment
- Kubernetes deployment (3 regions)
- HorizontalPodAutoscaler (2-20 replicas)
- Load balancer (Nginx)
- Database replication

**MVP 3.1**: Tournament System
- Bracket generation (single-elimination)
- Match scheduling
- Player registration
- Tournament dashboard

**MVP 3.2**: Replay Recording
- Frame recording (60 FPS)
- Compression (zstd/LZ4)
- Storage (MinIO/S3)
- Playback API

**MVP 3.3**: Live Spectator
- External viewer support (1000+)
- 30-second delay (public)
- Multiple camera modes
- Broadcasting features

**MVP 3.4**: Anti-Cheat
- Speed hack detection
- Aimbot detection
- Server-side validation
- Ban system (temporary/permanent)

**MVP 3.5**: Analytics Dashboard
- ClickHouse integration
- Kill heatmaps
- Weapon balance stats
- Player retention metrics

### Deliverables

- All Checkpoint B features +
- Multi-region architecture diagram
- Tournament bracket samples
- Replay files (3+ samples)
- Anti-cheat detection logs
- Analytics dashboard screenshots

---

## Development Guidelines

### Code Style

**Naming**:
- Classes: `PascalCase` (GameSession, PlayerState)
- Functions: `camelCase` (processInputs, updatePhysics)
- Variables: `snake_case` (player_id_, tick_count_)
- Constants: `UPPER_CASE` (TARGET_TPS, MAX_PLAYERS)

**Patterns**:
- RAII for all resources
- Smart pointers (std::unique_ptr, std::shared_ptr)
- Const-correctness
- Explicit error handling
- Thread-safe design

### Architecture

**Clean Architecture Layers**:
```
Delivery (WebSocket handlers)
    ↓
Use Case (Business logic)
    ↓
Domain (Entities, interfaces)
    ↓
Infrastructure (DB, Redis, Kafka)
    ↓
Core (Game loop, physics)
```

**Dependency Rule**: Dependencies point inward.

### Testing

**Required per MVP**:
- Unit tests (isolated classes)
- Integration tests (DB, Redis, Kafka)
- Performance tests (tick rate, latency)
- Coverage ≥ 70%

**Example**:
```cpp
// tests/unit/test_collision.cpp
TEST(CollisionTest, ProjectileHitsPlayer) {
    Projectile p{.pos_x=100, .pos_y=100, .radius=5};
    Player victim{.pos_x=110, .pos_y=100, .radius=20};
    
    EXPECT_TRUE(checkCollision(p, victim));
}

// tests/performance/benchmark_tick_rate.cpp
TEST(PerformanceTest, SixtyTPSUnderLoad) {
    GameLoop loop;
    loop.setTargetTPS(60);
    
    // Add 60 players
    for (int i = 0; i < 60; i++) {
        loop.addPlayer(createMockPlayer());
    }
    
    auto metrics = loop.runForDuration(std::chrono::seconds(60));
    
    EXPECT_GE(metrics.avg_tps, 60.0);
    EXPECT_LT(metrics.p99_tick_duration_ms, 20.0);
}
```

### Git Workflow

**Branches**:
- `main`: Production-ready
- `develop`: Integration
- `feature/mvp-X.Y`: MVP development
- `hotfix/*`: Emergency fixes

**Commits**:
```
<type>: <subject>

<body>

<footer>
```

**Types**: feat, fix, docs, test, refactor, perf, chore

**Example**:
```
feat: implement MVP 1.1 - Combat System

- Add Projectile class with collision
- Implement damage system
- Tests: 15/15 passing, 85% coverage

Closes #12
```

### Quality Gates

**All must pass**:
1. Build (Release + Debug)
2. Unit tests (≥70% coverage)
3. Integration tests
4. Lint (clang-format, clang-tidy)
5. Performance (60 TPS if applicable)

## Monitoring

### Prometheus Metrics

**Required**:
- `game_tick_rate`: Actual TPS
- `game_tick_duration_seconds`: Tick time
- `websocket_connections_total`: Active connections
- `game_sessions_active`: Current games
- `player_actions_total`: Inputs processed
- `database_query_duration_seconds`: DB latency

**Checkpoint B additions**:
- `object_pool_reuse_rate`: Reuse percentage
- `object_pool_active_count`: Currently in use
- `interest_management_filtered_ratio`: Packet reduction
- `lockfree_queue_contention_ms`: p99 wait time
- `lockfree_queue_throughput`: Events/sec

### Grafana Dashboards

**Checkpoint A** (8+ panels):
- Tick rate over time
- Active connections
- Request latency (p50, p95, p99)
- Error rate
- CPU/Memory usage
- Database connections
- Active game sessions
- Player count per session

**Checkpoint B** (12+ panels):
- Add: Spatial partition metrics
- Kafka throughput
- Object pool metrics (reuse rate, active/free)
- Interest management (filtered packets, AOI updates)
- Lock-free queue (contention, latency)
- Item spawn rate
- Player density heatmap

**Checkpoint C** (18+ panels):
- Add: Multi-region metrics
- Replay storage usage
- Spectator count
- Tournament progress
- Anti-cheat detections

## Deployment

**Checkpoint A**: Docker Compose (single server)  
**Checkpoint B**: Multi-instance + Kafka cluster  
**Checkpoint C**: Kubernetes (3 regions: Seoul, Tokyo, Virginia)

## Security

**Implementation**:
1. Input validation (all client data)
2. Authentication (JWT tokens)
3. Anti-cheat (server-side validation)
4. Rate limiting (prevent DoS)
5. SQL injection (parameterized queries only)

## Common Issues

**Tick rate instability**:
- Cause: Heavy computation in loop
- Solution: Profile (perf), optimize, thread pool

**WebSocket drops**:
- Cause: Network timeouts
- Solution: Ping/pong, keepalive, reconnect

**Database bottleneck**:
- Cause: N+1 queries, missing indexes
- Solution: EXPLAIN ANALYZE, add indexes, cache

## Reference Resources

**Official Docs**:
- [boost.asio](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)
- [PostgreSQL](https://www.postgresql.org/docs/)
- [Protocol Buffers](https://protobuf.dev/)

**Learning**:
- [Game Programming Patterns](https://gameprogrammingpatterns.com/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)

## Changelog

### v4.0 (2025-01-30)
- Separated Phase 1 and 2
- Restructured to follow best practices
- Added .meta/state.yml for version tracking
- Enhanced quality gates

---

**Target**: Korean Game Server Developer (Entry → Senior)  
**Tech Match**: 100% with Nexon, Krafton, Netmarble, Kakao Games

**For execution**: See `phase2-prompts.md`
