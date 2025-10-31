# MVP 1.2 – Matchmaking Service

## Purpose
Deliver a deterministic matchmaking pipeline that can continuously pair duel players into new game sessions. The system must honour ELO ratings, maintain a Redis-backed waiting queue, and emit match creation events that other subsystems (game server, notifications) can consume.

## Domain Model
- **MatchRequest** – Immutable player enqueue record containing `player_id`, `elo`, `enqueued_at`, and `preferred_region`. Provides helpers for waiting time and expanding the search radius.
- **QueuedPlayer** – Internal representation stored in Redis sorted set. Encodes the same data as `MatchRequest` plus a monotonically increasing insertion counter to guarantee deterministic ordering.
- **Match** – Value object representing a confirmed pairing. Contains `match_id`, participating player ids, average ELO, created timestamp, and assigned region.
- **MatchQueue** – Abstraction that wraps Redis sorted set operations. Supports `Upsert`, `Remove`, `FetchOrdered`, and `Size`. An in-memory implementation is used for tests while the production implementation serialises to Redis commands.
- **Matchmaker** – Thread-safe orchestrator that coordinates queue access, runs the matching algorithm, emits Prometheus metrics, and invokes a callback when a `Match` is formed.
- **MatchNotificationChannel** – Observer that subscribers use to receive match creation events. Backed by a thread-safe queue to decouple matchmaking from the rest of the server.

## Requirements
1. **Redis-backed Queue**
   - Implement `MatchQueue` interface with a default `InMemoryMatchQueue` using `std::map<int, std::list<QueuedPlayer>>` to mimic Redis sorted set ordering while preserving insertion order within equal ELO bands.
   - Provide `RedisMatchQueue` stub that logs the equivalent Redis command strings (`ZADD`, `ZREM`, `ZRANGE`) for future integration. Stub must compile even without Redis headers.
   - Expose `Size()` and `Snapshot()` to support monitoring.

2. **Enqueue & Cancellation API**
   - `Matchmaker::Enqueue(const MatchRequest&)` adds or updates a player entry. If the player is already waiting, refresh `enqueued_at` and `elo`.
   - `Matchmaker::Cancel(const std::string& player_id)` removes the player. Return a boolean indicating whether a removal occurred.
   - Log queue mutations (`std::cout << "matchmaking enqueue " << player_id << " elo=" << elo`) and (`std::cout << "matchmaking cancel " << player_id`).

3. **Matching Algorithm**
   - Provide `Matchmaker::RunMatching(std::chrono::steady_clock::time_point now)` that:
     - Iterates ordered queue entries and forms two-player matches.
     - Uses ±100 ELO tolerance when the waiting time ≤5 seconds.
     - Expands tolerance by 25 every additional 5 seconds waited (e.g., ±125 after 5-10s, ±150 after 10-15s).
     - Ensures deterministic matches by always pairing oldest compatible players first.
     - Produces unique `match_id` strings (`match-<counter>`).
     - Supports creating 10+ matches per invocation without leaving stale players.

4. **Notification & Lifecycle**
   - Allow registering a match-created callback via `SetMatchCreatedCallback`.
   - Ensure callbacks are invoked without holding internal locks and include the full `Match` payload.
   - Maintain `MatchNotificationChannel` FIFO queue and expose `Poll()` for consumers that prefer pull-model access.

5. **Metrics & Monitoring**
   - Emit Prometheus formatted metrics:
     - `matchmaking_queue_size` (gauge)
     - `matchmaking_matches_total` (counter)
     - `matchmaking_wait_seconds_bucket` (histogram with buckets `[0,5,10,20,40,80]`).
   - Update metrics every time `RunMatching` executes.
   - Provide `Matchmaker::MetricsSnapshot()` returning the exposition string.

6. **Thread Safety & Concurrency**
   - Guard shared structures with mutexes and support concurrent enqueue/cancel operations from multiple threads.
   - Guarantee that `RunMatching` can run while other threads enqueue players without data races.

## Performance Requirements
- Running `RunMatching` on a queue of 200 players must complete in ≤2 ms on a single thread (validated via performance test with steady clock timing).

## Tests
1. `tests/unit/test_match_queue.cpp` – Validates ordering semantics, upsert/update, and cancellation behaviour of `InMemoryMatchQueue`.
2. `tests/unit/test_matchmaker.cpp` – Covers tolerance expansion, deterministic pairing, callback invocation, and metrics counters.
3. `tests/integration/test_matchmaker_flow.cpp` – Simulates 20 players joining, confirms 10 matches emitted, and verifies notification channel contents.
4. `tests/performance/test_matchmaking_perf.cpp` – Generates 200 mock players, measures runtime, and asserts the ≤2 ms requirement.

## Evidence
- `docs/evidence/mvp-1.2/ci.log`
- `docs/evidence/mvp-1.2/metrics.txt`
- `docs/evidence/mvp-1.2/performance-report.md`
- `docs/evidence/mvp-1.2/test-coverage.html`
- `docs/evidence/mvp-1.2/run.sh`
