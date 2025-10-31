# MVP 1.1 – Combat System

## Purpose
Deliver a deterministic, low-latency combat loop for the Checkpoint A duel game. Players must be able to fire projectiles using mouse input, receive authoritative hit validation from the server, and resolve death states so the match can end without desyncs.

## Domain Model
- **HealthComponent** – Tracks `current_hp` (starts at 100), `max_hp`, and exposes `ApplyDamage(amount)` returning whether the player was killed this tick.
- **Projectile** – Immutable identifier, owner id, position `(x, y)`, normalized direction `(dx, dy)`, spawn timestamp, travel speed (30 m/s), collision radius (0.2 m), lifetime (1.5 s), and active flag. Provides `Advance(dt)` and `IsExpired(now)`.
- **ProjectilePool** – Container owned by `GameSession` that updates all active projectiles, prunes expired ones, and performs collision queries.
- **CombatEvent** – Value object describing server-side combat events (`hit`, `death`) with player ids, damage, and tick.
- **CombatLog** – Ring buffer (last 32 events) for debugging and telemetry.

## Requirements
1. **Player Health Tracking**
   - Extend `PlayerState` with `health` and `is_alive` fields. All players spawn with 100 HP and `is_alive=true`.
   - Reject movement inputs when `is_alive=false`.
   - Expose `GetPlayer`/`Snapshot` health data for state replication.

2. **Projectile Firing**
   - Extend input frame format to `input <player_id> <seq> <up> <down> <left> <right> <mouse_x> <mouse_y> <fire>`.
   - On `fire=1`, spawn a projectile at the player's current position plus a 0.3 m forward offset along the aim direction. Ignore requests from dead players or zero-length aim vectors. Enforce a fire rate limit of 10 shots per second per player.
   - Log each spawn: `std::cout << "projectile spawn " << projectile_id << " owner=" << player_id << std::endl;`.

3. **Projectile Simulation**
   - Update all projectiles once per tick using the authoritative delta from the game loop (60 TPS). Use straight-line motion at 30 m/s. Remove projectiles if expired (`lifetime > 1.5s`) or after a hit.
   - Emit Prometheus metrics:
     - `projectiles_active` (gauge)
     - `projectiles_spawned_total` (counter)
     - `projectiles_hits_total` (counter)

4. **Collision Detection**
   - Model players as circles with radius 0.5 m. Use circle-circle intersection between projectile radius 0.2 m and player radius.
   - Ensure projectiles cannot hit their owner.
   - When a hit occurs, enqueue a `CombatEvent` with type `hit` containing shooter id, target id, projectile id, damage, and tick.

5. **Damage Resolution & Death**
   - Apply 20 HP damage per hit. Clamp at zero. If health reaches 0, mark `is_alive=false` and emit a `CombatEvent` of type `death`.
   - Broadcast deaths to clients via an additional websocket message: `death <player_id> <tick>`. Ensure the message is sent exactly once per death.
   - Record hits/deaths in the combat log ring buffer (size 32). Provide a method on `GameSession` to snapshot the buffer for debugging.

6. **Metrics & Logging**
   - Extend `WebSocketServer::MetricsSnapshot` to include projectile metrics and active deaths count (`players_dead_total` counter).
   - Ensure all combat state transitions are logged: hits (`std::cout << "hit " << shooter << "->" << target << " dmg=" << damage << std::endl;`) and deaths (`std::cout << "death " << player_id << std::endl;`).

## Performance Requirements
- Projectile update + collision pass for two players with ≤32 projectiles must complete within 0.5 ms per tick on a single thread (validated by performance test).

## Tests
1. `tests/unit/test_projectile.cpp` – Verifies advance, expiration, and fire-rate limiter behavior.
2. `tests/unit/test_combat.cpp` – Covers damage application, death transition, and collision logic.
3. `tests/integration/test_websocket_combat.cpp` – Spins websocket server, fires projectiles, asserts death broadcasts and state replication include health.
4. `tests/performance/test_projectile_perf.cpp` – Benchmarks 120 ticks with 32 projectiles and asserts runtime within requirement.

## Evidence
- `docs/evidence/mvp-1.1/ci.log`
- `docs/evidence/mvp-1.1/metrics.txt`
- `docs/evidence/mvp-1.1/performance-report.md`
- `docs/evidence/mvp-1.1/test-coverage.html`
- `docs/evidence/mvp-1.1/run.sh`
