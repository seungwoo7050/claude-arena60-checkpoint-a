# MVP 1.3 – Statistics & Ranking

## Purpose
Deliver post-match analytics and ranking features so duel players receive immediate feedback on their performance. The server must automatically compute combat statistics, adjust ELO ratings, maintain a global leaderboard, and expose a lightweight HTTP API for player profiles.

## Domain Model
- **PlayerMatchStats** – Immutable per-player record for a finished duel match. Captures `player_id`, `match_id`, `shots_fired`, `hits_landed`, `kills`, `deaths`, `damage_dealt`, `damage_taken`, and exposes an `Accuracy()` helper.
- **MatchResult** – Value object combining a completed duel outcome. Contains the `match_id`, `winner_id`, `loser_id`, completion timestamp, and a vector of `PlayerMatchStats` entries for all participants.
- **MatchStatsCollector** – Aggregates `GameSession` data into a `MatchResult` when a death event occurs. Responsible for mapping combat log events back to per-player totals.
- **EloRatingCalculator** – Stateless helper implementing K-factor 25 ELO adjustments and returning new rating pairs for winner/loser combinations.
- **LeaderboardStore** – Abstract interface for persisting global ranking. Exposes `Upsert`, `Erase`, `TopN`, and `Get` semantics. Backed by an in-memory implementation and a Redis command stub.
- **PlayerProfileService** – Thread-safe orchestrator that stores cumulative lifetime stats, current ratings, and synchronises leaderboard writes. Provides `RecordMatch`, `GetProfile`, `TopProfiles`, and Prometheus snapshot helpers.
- **ProfileHttpRouter** – Request handler that serves `/metrics`, `/profiles/<player_id>`, and `/leaderboard?limit=N` endpoints over HTTP.

## Requirements
1. **Match Result Extraction**
   - Implement `MatchStatsCollector` with `Collect(const CombatEvent& death_event, const GameSession& session, std::chrono::system_clock::time_point completed_at)`.
   - The collector must read the latest `PlayerState` snapshot and `CombatLogSnapshot` to compute accurate `shots_fired`, `hits_landed`, `kills`, `deaths`, `damage_dealt`, and `damage_taken` per player.
   - Damage per hit is 20 HP; deaths must count as kills for the shooter. Accuracy = hits / max(1, shots).
   - Return a `MatchResult` where `winner_id == death_event.shooter_id`, `loser_id == death_event.target_id`, and `match_id` takes the form `match-<tick>-<winner_id>-vs-<loser_id>`.
   - Log a summary line: `std::cout << "match complete " << match_id << " winner=" << winner_id << " loser=" << loser_id << std::endl;`.

2. **Ratings & Profiles**
   - Provide `EloRatingCalculator::Update(int winner_rating, int loser_rating)` returning a struct with `winner_new` and `loser_new` integer ratings using K=25.
   - Implement `PlayerProfileService` that:
     - Starts players at 1200 rating on first sight.
     - Maintains cumulative totals for matches, wins, losses, kills, deaths, shots, hits, damage dealt, damage taken.
     - Applies `MatchResult` updates atomically under mutex protection and updates ratings via the calculator.
     - Emits Prometheus metrics with gauges for `player_profiles_total` and `leaderboard_entries_total` plus counters `matches_recorded_total` and `rating_updates_total`.
     - Provides JSON serialisation helpers `SerializeProfile` and `SerializeLeaderboard` producing stable key ordering.

3. **Leaderboard Integration**
   - Implement `InMemoryLeaderboardStore` using an ordered structure (`std::map<int, std::set<std::string>, std::greater<int>>`) guaranteeing deterministic ordering by score then player id.
   - Provide `RedisLeaderboardStore` stub that logs `ZADD`, `ZREM`, and `ZRANGE` commands to `std::cout` without requiring Redis headers.
   - Ensure `PlayerProfileService::RecordMatch` upserts winner and loser ratings into the leaderboard store.

4. **Player Profile API**
   - Extend the existing metrics HTTP server into a `ProfileHttpRouter` capable of handling:
     - `GET /metrics` – existing behaviour.
     - `GET /profiles/<player_id>` – Returns JSON object with profile fields (rating, matches, wins, losses, kills, deaths, shots_fired, hits_landed, damage_dealt, damage_taken, accuracy).
     - `GET /leaderboard?limit=N` – Returns JSON array ordered by rating desc, limited to `N` (default 10, clamp 1-50).
   - Responses must include `Content-Type: application/json` for profile/leaderboard endpoints and HTTP 404 when the player is unknown.
   - All routes must be safe for concurrent requests.

5. **Server Integration**
   - Wire `WebSocketServer` to notify `PlayerProfileService` whenever `MatchStatsCollector` returns a `MatchResult`. Use a callback registered via `SetMatchCompletedCallback`.
   - Update `main.cpp` to instantiate `PlayerProfileService`, configure leaderboard store, and expose combined metrics/profile router on the configured metrics port.
   - Register Prometheus snapshot provider to concatenate game loop, websocket, Postgres, matchmaking, and profile metrics.

## Performance Requirements
- Recording 100 consecutive `MatchResult` objects through `PlayerProfileService::RecordMatch` must complete in ≤5 ms on a single thread (validated by performance test using steady clock timing).

## Tests
1. `tests/unit/test_match_stats.cpp` – Verifies `MatchStatsCollector` builds accurate stats from controlled combat logs.
2. `tests/unit/test_player_profile_service.cpp` – Covers ELO updates, cumulative stat aggregation, leaderboard ordering, and Prometheus metrics snapshot content.
3. `tests/unit/test_leaderboard_store.cpp` – Ensures deterministic ordering, updates, and removals for the in-memory leaderboard.
4. `tests/integration/test_profile_http.cpp` – Spins up the HTTP server and validates JSON responses for `/profiles/<id>` and `/leaderboard` while ensuring `/metrics` still works.
5. `tests/performance/test_profile_service_perf.cpp` – Measures 100 match recordings and asserts the ≤5 ms bound.

## Evidence
- `docs/evidence/mvp-1.3/ci.log`
- `docs/evidence/mvp-1.3/metrics.txt`
- `docs/evidence/mvp-1.3/performance-report.md`
- `docs/evidence/mvp-1.3/test-coverage.html`
- `docs/evidence/mvp-1.3/run.sh`
