# MVP 1.0 – Basic Game Server

## Purpose
Provide the minimal online infrastructure for the Checkpoint A duel game: a single match server that accepts player WebSocket connections, runs a deterministic 60 ticks-per-second simulation loop, processes keyboard + mouse movement input, and persists session lifecycle events to PostgreSQL.

## Domain Model
- **GameConfig** – immutable configuration (tick rate, listen port, database DSN).
- **GameLoop** – fixed-step scheduler executing update callbacks at 60 TPS with jitter monitoring.
- **MovementInput** – normalized input flags (up, down, left, right) and mouse aim vector.
- **PlayerState** – per-player snapshot with position `(x, y)` in meters, facing angle in radians, and last processed sequence id.
- **GameSession** – holds active players, applies `MovementInput`, emits state frames.
- **WebSocketServer** – boost::beast websocket acceptor translating text frames to `MovementInput` and returning authoritative state frames.
- **PostgresStorage** – thin libpq-based gateway that records session start/end events and surfaces connection health.

## Requirements
1. **Configuration Loading**  
   Load tick rate, port, and database DSN from environment variables with sane defaults. Expose via `GameConfig`.
2. **Deterministic Game Loop**  
   Implement `GameLoop` that executes callbacks every 16.67 ms ±1 ms. Provide start/stop, metrics for actual TPS, and graceful shutdown on stop.
3. **Player Movement System**  
   Implement `GameSession` with `PlayerState` supporting two concurrent players. Interpret WASD flags as cardinal velocity (5 m/s speed). Mouse vector sets facing angle via `atan2`. Clamp speed diagonally using normalized vector.
4. **WebSocket Input/Output**  
   Implement `WebSocketServer` (Boost.Beast) accepting text frames formatted as:  
   `input <player_id> <seq> <up> <down> <left> <right> <mouse_x> <mouse_y>`  
   Apply input via `GameSession` and respond with `state <player_id> <x> <y> <angle> <tick>` once per tick to each connection. Support graceful disconnects.
5. **PostgreSQL Integration**  
   Provide `PostgresStorage` using libpq. On server startup attempt connection; expose `isConnected()`. Implement `recordSessionEvent(player_id, event)` storing events via parameterized `INSERT`. When DB unavailable, log error but keep server running.
6. **Metrics & Logging**  
   Emit `game_tick_rate` and `game_tick_duration_seconds` via a simple Prometheus exposition string builder. Log key lifecycle events (startup, new connection, disconnect, DB errors).

## Performance Requirements
- Tick execution variance ≤ 1.0 ms over a 120 tick sample (verified in tests).
- WebSocket echo latency (input → state) ≤ 20 ms under test harness (loopback).

## Tests
1. `tests/unit/test_game_loop.cpp` – verifies tick interval accuracy and stop behavior.
2. `tests/unit/test_game_session.cpp` – covers movement integration (diagonal speed clamp, facing angle).
3. `tests/unit/test_postgres_storage.cpp` – validates DSN parsing, connection failure path, and parameterized query formatting.
4. `tests/integration/test_websocket_server.cpp` – spins server, sends sample input, asserts state response and latency.
5. `tests/performance/test_tick_variance.cpp` – runs 120 ticks and ensures variance requirement.

## Evidence
- `docs/evidence/mvp-1.0/ci.log`
- `docs/evidence/mvp-1.0/metrics.txt` (Prometheus scrape)
- `docs/evidence/mvp-1.0/performance-report.md`
- `docs/evidence/mvp-1.0/test-coverage.html`
- `docs/evidence/mvp-1.0/run.sh`
