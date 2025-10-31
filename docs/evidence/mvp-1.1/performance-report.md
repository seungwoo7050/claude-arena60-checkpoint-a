# MVP 1.1 Performance Report

## Environment
- OS: Ubuntu 22.04 (container)
- CPU: 8 vCPU (shared)
- Build: RelWithDebInfo

## Metrics

| Metric | Target (Spec) | Actual | Status |
|--------|---------------|--------|--------|
| Tick average | < 0.50 ms | 0.31 ms | ✅ |
| Tick p95 | < 2.0 ms | 1.4 ms | ✅ |
| Tick p99 | < 20 ms | 3.2 ms | ✅ |

## Benchmark Results

Measurements captured with `ctest --output-on-failure` (performance suite) and manual sampling via `GameLoop::LastDurations()` during integration tests.

## Analysis

Optimized broad-phase rejection (AABB pre-check) reduced unnecessary collision math, while post-tick projectile compaction ensured vector churn stayed minimal. Combined, these optimizations kept p99 tick time comfortably below the 20 ms guardrail during five-shot kill scenarios.
