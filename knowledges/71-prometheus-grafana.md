# Quickstart 71: Prometheus + Grafana - 모니터링

> **📚 학습 유형**: 기초 개념 (Fundamentals)
> **⏭️ 다음 단계**: 모든 MVP에서 KPI 달성 확인

## 🎯 목표
- **Prometheus**: 메트릭 수집 시스템
- **Grafana**: 시각화 대시보드
- **C++ 통합**: prometheus-cpp 라이브러리
- **실전**: Arena60 KPI 모니터링 (60 TPS, p99 latency 등)

## 📋 사전준비
- [Quickstart 32](32-cpp-game-loop.md) 완료 (Game loop)
- [Quickstart 60](60-postgresql-redis-docker.md) 완료 (Docker)
- Docker Desktop 설치

---

## 📊 Part 1: Prometheus 기초 (20분)

### 1.1 Prometheus란?

**Prometheus**는 **시계열 데이터베이스**로, 게임 서버의 성능 메트릭을 수집하고 저장합니다.

```
Arena60 필수 KPI:
- Server Tick Rate: 60 TPS (stable under load)
- Client Latency: p99 ≤ 50 ms
- State Sync: ≤ 16.67 ms (60 FPS)
- Concurrent Players: 60+
- Error Rate: ≤ 0.1%

Prometheus로 모두 측정 가능!
```

### 1.2 Docker로 Prometheus 실행

**docker-compose.yml** (추가):
```yaml
version: '3.8'

services:
  # ... postgres, redis ...

  prometheus:
    image: prom/prometheus:latest
    container_name: arena60-prometheus
    ports:
      - "9090:9090"
    volumes:
      - ./monitoring/prometheus/prometheus.yml:/etc/prometheus/prometheus.yml
      - prometheus_data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--storage.tsdb.retention.time=30d'
    restart: unless-stopped

volumes:
  postgres_data:
  redis_data:
  prometheus_data:
```

**monitoring/prometheus/prometheus.yml**:
```yaml
global:
  scrape_interval: 5s      # 5초마다 메트릭 수집
  evaluation_interval: 5s

scrape_configs:
  - job_name: 'arena60-game-server'
    static_configs:
      - targets: ['host.docker.internal:8081']  # 게임 서버 메트릭 엔드포인트
        labels:
          instance: 'game-server-1'
```

**실행**:
```bash
mkdir -p monitoring/prometheus
# prometheus.yml 작성 후
docker-compose up -d prometheus

# Prometheus UI 접속
open http://localhost:9090
```

---

## 🔧 Part 2: C++에서 Prometheus 메트릭 노출 (30분)

### 2.1 prometheus-cpp 설치

```bash
# Ubuntu/Debian
sudo apt-get install libprometheus-cpp-dev

# macOS (소스 빌드 필요)
git clone https://github.com/jupp0r/prometheus-cpp.git
cd prometheus-cpp
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON
make -j4
sudo make install
```

### 2.2 기본 메트릭 서버

**metrics_server.h**:
```cpp
#pragma once
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>
#include <memory>

class MetricsServer {
private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;

public:
    // 메트릭 패밀리
    prometheus::Family<prometheus::Counter>* requests_total_;
    prometheus::Family<prometheus::Gauge>* active_connections_;
    prometheus::Family<prometheus::Histogram>* request_duration_;
    prometheus::Family<prometheus::Gauge>* game_tick_rate_;

    MetricsServer(const std::string& bind_address = "0.0.0.0:8081")
        : registry_(std::make_shared<prometheus::Registry>())
        , exposer_(std::make_unique<prometheus::Exposer>(bind_address))
    {
        // Registry 등록
        exposer_->RegisterCollectable(registry_);

        // 메트릭 정의
        requests_total_ = &prometheus::BuildCounter()
            .Name("http_requests_total")
            .Help("Total number of HTTP requests")
            .Register(*registry_);

        active_connections_ = &prometheus::BuildGauge()
            .Name("websocket_connections_active")
            .Help("Number of active WebSocket connections")
            .Register(*registry_);

        request_duration_ = &prometheus::BuildHistogram()
            .Name("http_request_duration_seconds")
            .Help("HTTP request latency in seconds")
            .Register(*registry_);

        game_tick_rate_ = &prometheus::BuildGauge()
            .Name("game_tick_rate")
            .Help("Actual game server tick rate (TPS)")
            .Register(*registry_);

        std::cout << "✅ Metrics server running on " << bind_address << "\n";
    }
};
```

**사용 예제**:
```cpp
#include "metrics_server.h"
#include <thread>
#include <chrono>

int main() {
    // 메트릭 서버 시작 (백그라운드)
    MetricsServer metrics;

    // 메트릭 사용
    auto& request_counter = metrics.requests_total_->Add({{"endpoint", "/api/health"}});
    auto& connection_gauge = metrics.active_connections_->Add({});
    auto& tick_rate_gauge = metrics.game_tick_rate_->Add({});

    // 시뮬레이션: 게임 루프
    for (int i = 0; i < 100; ++i) {
        // 요청 카운터 증가
        request_counter.Increment();

        // 연결 수 업데이트
        connection_gauge.Set(42 + i % 10);

        // Tick rate 업데이트
        tick_rate_gauge.Set(60.0 - (i % 5));  // 55~60 TPS

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 메트릭 확인: http://localhost:8081/metrics
    std::cout << "Check metrics at http://localhost:8081/metrics\n";

    // 서버 계속 실행
    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(metrics_demo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(prometheus-cpp CONFIG REQUIRED)

add_executable(metrics_demo metrics_demo.cpp)
target_link_libraries(metrics_demo
    PRIVATE
        prometheus-cpp::core
        prometheus-cpp::pull
)
```

**빌드 & 실행**:
```bash
mkdir build && cd build
cmake ..
make
./metrics_demo

# 브라우저에서 http://localhost:8081/metrics 접속
```

**메트릭 출력 예시**:
```
# HELP http_requests_total Total number of HTTP requests
# TYPE http_requests_total counter
http_requests_total{endpoint="/api/health"} 100

# HELP websocket_connections_active Number of active WebSocket connections
# TYPE websocket_connections_active gauge
websocket_connections_active 45

# HELP game_tick_rate Actual game server tick rate (TPS)
# TYPE game_tick_rate gauge
game_tick_rate 58
```

---

## 🎮 Part 3: Arena60 게임 서버 메트릭 (25분)

### 3.1 필수 메트릭 정의

**game_metrics.h**:
```cpp
#pragma once
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>
#include <memory>

class GameMetrics {
private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;

public:
    // Gauge 메트릭
    prometheus::Gauge* game_tick_rate_;
    prometheus::Gauge* game_tick_duration_ms_;
    prometheus::Gauge* websocket_connections_;
    prometheus::Gauge* active_game_sessions_;
    prometheus::Gauge* player_count_;

    // Counter 메트릭
    prometheus::Counter* player_actions_total_;
    prometheus::Counter* projectiles_spawned_total_;
    prometheus::Counter* collision_checks_total_;
    prometheus::Counter* errors_total_;

    // Histogram 메트릭 (레이턴시 분포)
    prometheus::Histogram* tick_duration_histogram_;
    prometheus::Histogram* websocket_message_latency_;

    GameMetrics(const std::string& bind_address = "0.0.0.0:8081")
        : registry_(std::make_shared<prometheus::Registry>())
        , exposer_(std::make_unique<prometheus::Exposer>(bind_address))
    {
        exposer_->RegisterCollectable(registry_);

        // Tick Rate (목표: 60 TPS)
        auto& tick_rate_family = prometheus::BuildGauge()
            .Name("game_tick_rate")
            .Help("Actual game server tick rate (TPS)")
            .Register(*registry_);
        game_tick_rate_ = &tick_rate_family.Add({});

        // Tick Duration (목표: < 16.67 ms)
        auto& tick_duration_family = prometheus::BuildGauge()
            .Name("game_tick_duration_ms")
            .Help("Game tick processing time in milliseconds")
            .Register(*registry_);
        game_tick_duration_ms_ = &tick_duration_family.Add({});

        // WebSocket Connections
        auto& connections_family = prometheus::BuildGauge()
            .Name("websocket_connections_total")
            .Help("Number of active WebSocket connections")
            .Register(*registry_);
        websocket_connections_ = &connections_family.Add({});

        // Active Game Sessions
        auto& sessions_family = prometheus::BuildGauge()
            .Name("game_sessions_active")
            .Help("Number of active game sessions")
            .Register(*registry_);
        active_game_sessions_ = &sessions_family.Add({});

        // Player Count
        auto& players_family = prometheus::BuildGauge()
            .Name("players_active")
            .Help("Total number of active players")
            .Register(*registry_);
        player_count_ = &players_family.Add({});

        // Player Actions Counter
        auto& actions_family = prometheus::BuildCounter()
            .Name("player_actions_total")
            .Help("Total number of player input actions processed")
            .Register(*registry_);
        player_actions_total_ = &actions_family.Add({});

        // Projectiles Spawned
        auto& projectiles_family = prometheus::BuildCounter()
            .Name("projectiles_spawned_total")
            .Help("Total number of projectiles spawned")
            .Register(*registry_);
        projectiles_spawned_total_ = &projectiles_family.Add({});

        // Collision Checks
        auto& collisions_family = prometheus::BuildCounter()
            .Name("collision_checks_total")
            .Help("Total number of collision checks performed")
            .Register(*registry_);
        collision_checks_total_ = &collisions_family.Add({});

        // Errors
        auto& errors_family = prometheus::BuildCounter()
            .Name("errors_total")
            .Help("Total number of errors")
            .Register(*registry_);
        errors_total_ = &errors_family.Add({});

        // Tick Duration Histogram
        auto& tick_hist_family = prometheus::BuildHistogram()
            .Name("game_tick_duration_seconds")
            .Help("Game tick duration distribution")
            .Register(*registry_);
        tick_duration_histogram_ = &tick_hist_family.Add(
            {},
            prometheus::Histogram::BucketBoundaries{0.001, 0.005, 0.01, 0.016, 0.02, 0.05}
        );

        // WebSocket Latency Histogram
        auto& ws_latency_family = prometheus::BuildHistogram()
            .Name("websocket_message_latency_seconds")
            .Help("WebSocket message latency")
            .Register(*registry_);
        websocket_message_latency_ = &ws_latency_family.Add(
            {},
            prometheus::Histogram::BucketBoundaries{0.001, 0.005, 0.01, 0.02, 0.05, 0.1}
        );

        std::cout << "✅ Game metrics server running on " << bind_address << "\n";
    }

    void record_tick(double duration_seconds) {
        tick_duration_histogram_->Observe(duration_seconds);
        game_tick_duration_ms_->Set(duration_seconds * 1000.0);
    }
};
```

### 3.2 게임 루프 통합

```cpp
#include "game_metrics.h"
#include <chrono>
#include <thread>
#include <iostream>

class GameLoop {
private:
    GameMetrics metrics_;
    bool running_ = true;
    const int target_tps_ = 60;
    const std::chrono::milliseconds frame_time_{1000 / target_tps_};

public:
    void run() {
        auto last_time = std::chrono::steady_clock::now();
        int tick_count = 0;
        auto last_report_time = last_time;

        while (running_) {
            auto frame_start = std::chrono::steady_clock::now();

            // 게임 틱 처리
            process_tick();

            auto frame_end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                frame_end - frame_start
            );

            // 메트릭 기록
            double duration_seconds = elapsed.count() / 1'000'000.0;
            metrics_.record_tick(duration_seconds);

            tick_count++;

            // 1초마다 TPS 계산
            auto now = std::chrono::steady_clock::now();
            auto since_report = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_report_time
            );

            if (since_report >= std::chrono::seconds(1)) {
                double actual_tps = tick_count / (since_report.count() / 1000.0);
                metrics_.game_tick_rate_->Set(actual_tps);

                std::cout << "TPS: " << actual_tps << ", Tick Duration: "
                          << (duration_seconds * 1000) << " ms\n";

                tick_count = 0;
                last_report_time = now;
            }

            // 프레임 레이트 제한
            auto sleep_time = frame_time_ - elapsed;
            if (sleep_time.count() > 0) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
    }

private:
    void process_tick() {
        // 게임 로직 시뮬레이션
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // 메트릭 업데이트
        metrics_.websocket_connections_->Set(25);
        metrics_.active_game_sessions_->Set(10);
        metrics_.player_count_->Set(20);
        metrics_.player_actions_total_->Increment(15);
        metrics_.projectiles_spawned_total_->Increment(3);
        metrics_.collision_checks_total_->Increment(50);
    }
};

int main() {
    GameLoop loop;
    loop.run();
    return 0;
}
```

---

## 📈 Part 4: Grafana 대시보드 (20분)

### 4.1 Grafana 설치

**docker-compose.yml** (추가):
```yaml
services:
  # ... prometheus ...

  grafana:
    image: grafana/grafana:latest
    container_name: arena60-grafana
    ports:
      - "3000:3000"
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
      - GF_USERS_ALLOW_SIGN_UP=false
    volumes:
      - grafana_data:/var/lib/grafana
      - ./monitoring/grafana/provisioning:/etc/grafana/provisioning
    depends_on:
      - prometheus
    restart: unless-stopped

volumes:
  postgres_data:
  redis_data:
  prometheus_data:
  grafana_data:
```

**실행**:
```bash
docker-compose up -d grafana

# Grafana 접속
open http://localhost:3000
# 로그인: admin / admin
```

### 4.2 Prometheus 데이터소스 추가

**monitoring/grafana/provisioning/datasources/prometheus.yml**:
```yaml
apiVersion: 1

datasources:
  - name: Prometheus
    type: prometheus
    access: proxy
    url: http://prometheus:9090
    isDefault: true
    editable: false
```

### 4.3 Arena60 대시보드 생성

**Grafana UI에서 수동 생성**:

1. **New Dashboard** 클릭
2. **Add Panel** 클릭
3. **메트릭 쿼리 입력**:

**Panel 1: Tick Rate**
```promql
game_tick_rate
```
- Visualization: Stat
- Target: 60 TPS
- Thresholds: Green (>58), Yellow (55-58), Red (<55)

**Panel 2: Tick Duration**
```promql
game_tick_duration_ms
```
- Visualization: Gauge
- Target: < 16.67 ms
- Max: 20 ms

**Panel 3: Active Connections**
```promql
websocket_connections_total
```
- Visualization: Time series

**Panel 4: Player Actions (Rate)**
```promql
rate(player_actions_total[1m])
```
- Visualization: Time series
- Unit: ops/s

**Panel 5: Tick Duration Histogram (p50, p95, p99)**
```promql
histogram_quantile(0.50, rate(game_tick_duration_seconds_bucket[5m]))
histogram_quantile(0.95, rate(game_tick_duration_seconds_bucket[5m]))
histogram_quantile(0.99, rate(game_tick_duration_seconds_bucket[5m]))
```
- Visualization: Time series
- Thresholds: p99 < 50 ms

**Panel 6: Error Rate**
```promql
rate(errors_total[5m])
```
- Visualization: Stat
- Target: < 0.1%

**Panel 7: Active Game Sessions**
```promql
game_sessions_active
```
- Visualization: Stat

**Panel 8: Projectiles Spawned (Rate)**
```promql
rate(projectiles_spawned_total[1m])
```
- Visualization: Bar chart

### 4.4 대시보드 JSON Export

**monitoring/grafana/dashboards/arena60.json**:
```json
{
  "dashboard": {
    "title": "Arena60 - Checkpoint A",
    "panels": [
      {
        "id": 1,
        "title": "Tick Rate (Target: 60 TPS)",
        "targets": [
          {
            "expr": "game_tick_rate",
            "legendFormat": "TPS"
          }
        ],
        "type": "stat",
        "fieldConfig": {
          "defaults": {
            "thresholds": {
              "steps": [
                {"value": 0, "color": "red"},
                {"value": 55, "color": "yellow"},
                {"value": 58, "color": "green"}
              ]
            }
          }
        }
      },
      {
        "id": 2,
        "title": "Tick Duration (Target: < 16.67 ms)",
        "targets": [
          {
            "expr": "game_tick_duration_ms",
            "legendFormat": "Duration (ms)"
          }
        ],
        "type": "gauge",
        "fieldConfig": {
          "defaults": {
            "max": 20,
            "thresholds": {
              "steps": [
                {"value": 0, "color": "green"},
                {"value": 16.67, "color": "yellow"},
                {"value": 18, "color": "red"}
              ]
            }
          }
        }
      }
    ]
  }
}
```

---

## 🐛 자주 막히는 부분

### 문제 1: prometheus-cpp 링크 오류

```cmake
# ❌ 잘못된 링크
target_link_libraries(myapp prometheus-cpp)

# ✅ 올바른 링크
find_package(prometheus-cpp CONFIG REQUIRED)
target_link_libraries(myapp
    PRIVATE
        prometheus-cpp::core
        prometheus-cpp::pull
)
```

### 문제 2: 메트릭 엔드포인트 접근 불가

```yaml
# ❌ Docker 컨테이너 내에서 localhost
scrape_configs:
  - job_name: 'game-server'
    static_configs:
      - targets: ['localhost:8081']  # 작동 안 함!

# ✅ Docker 호스트 접근
      - targets: ['host.docker.internal:8081']  # macOS/Windows
      - targets: ['172.17.0.1:8081']             # Linux
```

### 문제 3: Histogram 버킷 설정 실수

```cpp
// ❌ 너무 적은 버킷
prometheus::Histogram::BucketBoundaries{0.01, 0.1}

// ✅ 적절한 버킷 (latency용)
prometheus::Histogram::BucketBoundaries{
    0.001,  // 1ms
    0.005,  // 5ms
    0.01,   // 10ms
    0.016,  // 16ms (60 FPS)
    0.02,   // 20ms
    0.05    // 50ms
}
```

### 문제 4: Grafana 대시보드가 데이터 안 보임

```
1. Prometheus에서 메트릭 확인:
   http://localhost:9090/graph
   쿼리: game_tick_rate

2. Prometheus 타겟 상태 확인:
   http://localhost:9090/targets

3. Grafana 데이터소스 테스트:
   Configuration → Data Sources → Prometheus → Test
```

### 문제 5: Rate() 함수 이해 부족

```promql
# ❌ Counter를 직접 사용 (증가만 보임)
player_actions_total

# ✅ rate()로 초당 증가율 계산
rate(player_actions_total[1m])

# ✅ irate()로 순간 증가율 (더 반응적)
irate(player_actions_total[1m])
```

---

## ✅ 완료 체크리스트

### Part 1: Prometheus 기초
- [ ] Docker로 Prometheus 실행
- [ ] prometheus.yml 설정
- [ ] Prometheus UI 접속 확인

### Part 2: C++ 메트릭 노출
- [ ] prometheus-cpp 설치
- [ ] 기본 메트릭 서버 구현
- [ ] http://localhost:8081/metrics 확인

### Part 3: Arena60 메트릭
- [ ] 필수 메트릭 정의 (Tick Rate, Latency 등)
- [ ] 게임 루프 통합
- [ ] 메트릭 수집 확인

### Part 4: Grafana 대시보드
- [ ] Grafana 실행 및 로그인
- [ ] Prometheus 데이터소스 추가
- [ ] Arena60 대시보드 생성 (8+ 패널)
- [ ] KPI 달성 확인

---

## 🚀 다음 단계

✅ **Prometheus + Grafana 완료!**

**KPI 달성 확인**:
- ✅ Server Tick Rate: 60 TPS
- ✅ Client Latency: p99 ≤ 50 ms
- ✅ State Sync: ≤ 16.67 ms
- ✅ Error Rate: ≤ 0.1%

**실전 적용**:
- 모든 MVP에서 Grafana 대시보드로 성능 모니터링
- 부하 테스트 시 병목 지점 파악
- 프로덕션 배포 전 KPI 검증

---

## 📚 참고 자료

- [Prometheus Documentation](https://prometheus.io/docs/introduction/overview/)
- [prometheus-cpp GitHub](https://github.com/jupp0r/prometheus-cpp)
- [Grafana Documentation](https://grafana.com/docs/)
- [PromQL Basics](https://prometheus.io/docs/prometheus/latest/querying/basics/)
- [Grafana Dashboard Best Practices](https://grafana.com/docs/grafana/latest/dashboards/build-dashboards/best-practices/)

---

**Last Updated**: 2025-01-30
