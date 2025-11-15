# Quickstart 35: Collision Detection - 충돌 감지

> **📚 학습 유형**: 기초 개념 (Fundamentals)
> **⏭️ 다음 단계**: 이 문서 완료 후 → MVP 1.1 Combat System 구현

## 🎯 목표
- **Circle-Circle Collision**: 원형 충돌 감지
- **AABB Collision**: 사각형 충돌 감지
- **공간 분할**: 성능 최적화
- **실전**: 발사체-플레이어 충돌 처리

## 📋 사전준비
- [Quickstart 30](30-cpp-for-game-server.md) 완료 (C++ 기초)
- [Quickstart 32](32-cpp-game-loop.md) 완료 (Game loop)
- 기본 수학 (피타고라스 정리)

---

## ⭕ Part 1: Circle-Circle Collision (20분)

### 1.1 원리

**두 원이 겹치는지 판별**하는 가장 간단한 방법:

```
조건: 두 원의 중심 간 거리 ≤ 두 반지름의 합

수식:
distance = √((x₁ - x₂)² + (y₁ - y₂)²)

if distance ≤ (radius₁ + radius₂):
    충돌!
```

**장점**:
- 구현이 매우 간단
- 계산이 빠름
- 대부분의 2D 게임에 적합 (Pong, 슈팅 게임 등)

### 1.2 기본 구현

**collision.h**:
```cpp
#pragma once
#include <cmath>

struct Circle {
    float x, y;       // 중심 좌표
    float radius;     // 반지름
};

// Circle-Circle 충돌 감지
bool check_collision(const Circle& a, const Circle& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float distance_squared = dx * dx + dy * dy;
    float radius_sum = a.radius + b.radius;

    // 최적화: sqrt() 호출 피하기
    return distance_squared <= (radius_sum * radius_sum);
}

// 거리 계산 (필요 시)
float calculate_distance(const Circle& a, const Circle& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// 충돌 깊이 계산 (밀어내기에 사용)
float collision_depth(const Circle& a, const Circle& b) {
    float distance = calculate_distance(a, b);
    float radius_sum = a.radius + b.radius;
    return radius_sum - distance;  // 양수면 충돌
}
```

**사용 예제**:
```cpp
#include "collision.h"
#include <iostream>

int main() {
    // 발사체 (projectile)
    Circle projectile{100.0f, 50.0f, 5.0f};

    // 플레이어
    Circle player{110.0f, 50.0f, 20.0f};

    if (check_collision(projectile, player)) {
        std::cout << "🎯 Hit! Projectile hit the player!\n";

        float depth = collision_depth(projectile, player);
        std::cout << "Collision depth: " << depth << "\n";
    } else {
        std::cout << "Miss!\n";
    }

    return 0;
}
```

### 1.3 게임 서버 통합

**game_entities.h**:
```cpp
#pragma once
#include <vector>
#include <memory>

struct Projectile {
    int id;
    float x, y;
    float vx, vy;      // 속도
    float radius = 5.0f;
    int owner_id;
    int damage = 20;
    bool active = true;
};

struct Player {
    int id;
    float x, y;
    float radius = 20.0f;
    int health = 100;
    bool is_alive = true;

    void take_damage(int damage) {
        health -= damage;
        if (health <= 0) {
            health = 0;
            is_alive = false;
        }
    }
};

class CollisionManager {
private:
    std::vector<Projectile>& projectiles_;
    std::vector<Player>& players_;

public:
    CollisionManager(std::vector<Projectile>& projectiles,
                     std::vector<Player>& players)
        : projectiles_(projectiles), players_(players) {}

    // 충돌 검사 및 처리
    void check_and_resolve_collisions() {
        for (auto& projectile : projectiles_) {
            if (!projectile.active) continue;

            for (auto& player : players_) {
                if (!player.is_alive) continue;

                // 자신이 쏜 발사체는 무시
                if (projectile.owner_id == player.id) continue;

                // 충돌 검사
                Circle proj_circle{projectile.x, projectile.y, projectile.radius};
                Circle player_circle{player.x, player.y, player.radius};

                if (check_collision(proj_circle, player_circle)) {
                    // 충돌 처리
                    handle_collision(projectile, player);
                }
            }
        }
    }

private:
    void handle_collision(Projectile& projectile, Player& player) {
        // 데미지 처리
        player.take_damage(projectile.damage);

        // 발사체 비활성화
        projectile.active = false;

        std::cout << "💥 Player " << player.id << " hit! "
                  << "Health: " << player.health << "\n";

        if (!player.is_alive) {
            std::cout << "💀 Player " << player.id << " eliminated!\n";
        }
    }
};
```

**사용 예제**:
```cpp
#include "game_entities.h"
#include <iostream>

int main() {
    // 플레이어 2명
    std::vector<Player> players = {
        {1, 50.0f, 100.0f},   // Player 1
        {2, 150.0f, 100.0f}   // Player 2
    };

    // 발사체 (Player 1이 Player 2를 향해 발사)
    std::vector<Projectile> projectiles = {
        {1, 60.0f, 100.0f, 10.0f, 0.0f, 5.0f, 1, 20, true}
    };

    CollisionManager collision_mgr(projectiles, players);

    // 게임 루프 시뮬레이션 (60 FPS)
    for (int tick = 0; tick < 100; ++tick) {
        // 발사체 이동
        for (auto& proj : projectiles) {
            if (proj.active) {
                proj.x += proj.vx;
                proj.y += proj.vy;
            }
        }

        // 충돌 검사
        collision_mgr.check_and_resolve_collisions();

        // 디버그 출력 (매 10틱)
        if (tick % 10 == 0) {
            std::cout << "Tick " << tick << ": Projectile at ("
                      << projectiles[0].x << ", " << projectiles[0].y << ")\n";
        }
    }

    return 0;
}
```

---

## 📦 Part 2: AABB Collision (Axis-Aligned Bounding Box) (15분)

### 2.1 원리

**사각형 충돌 감지**는 빠르고 간단합니다:

```
조건: 두 사각형이 모든 축에서 겹침

if (rect1.left < rect2.right &&
    rect1.right > rect2.left &&
    rect1.top < rect2.bottom &&
    rect1.bottom > rect2.top):
    충돌!
```

### 2.2 구현

```cpp
#pragma once

struct AABB {
    float x, y;           // 중심
    float width, height;  // 크기

    float left() const { return x - width / 2; }
    float right() const { return x + width / 2; }
    float top() const { return y - height / 2; }
    float bottom() const { return y + height / 2; }
};

bool check_collision(const AABB& a, const AABB& b) {
    return (a.left() < b.right() &&
            a.right() > b.left() &&
            a.top() < b.bottom() &&
            a.bottom() > b.top());
}

// 점이 AABB 안에 있는지 확인
bool point_in_aabb(float px, float py, const AABB& box) {
    return (px >= box.left() && px <= box.right() &&
            py >= box.top() && py <= box.bottom());
}
```

**사용 예제**:
```cpp
#include <iostream>

int main() {
    AABB box1{50.0f, 50.0f, 40.0f, 40.0f};  // 중심 (50, 50), 크기 40x40
    AABB box2{70.0f, 50.0f, 40.0f, 40.0f};  // 중심 (70, 50), 크기 40x40

    if (check_collision(box1, box2)) {
        std::cout << "Boxes collide!\n";
    }

    // 점 충돌 검사
    if (point_in_aabb(60.0f, 50.0f, box1)) {
        std::cout << "Point is inside box1!\n";
    }

    return 0;
}
```

---

## 🚀 Part 3: 최적화 - 공간 분할 (25분)

### 3.1 문제: Brute Force 충돌 검사

```cpp
// ❌ 비효율적: O(N * M)
for (auto& projectile : projectiles) {      // N개
    for (auto& player : players) {          // M개
        if (check_collision(...)) {
            // 충돌 처리
        }
    }
}

// 문제: 플레이어 60명, 발사체 200개 → 12,000번 검사!
```

### 3.2 해결책: Grid-Based Spatial Partitioning

**공간을 그리드로 나누어 근처 객체만 검사**:

```cpp
#pragma once
#include <vector>
#include <unordered_map>

class SpatialGrid {
private:
    float cell_size_;
    std::unordered_map<int, std::vector<int>> grid_;  // grid_key → entity_ids

    int get_grid_key(float x, float y) const {
        int grid_x = static_cast<int>(x / cell_size_);
        int grid_y = static_cast<int>(y / cell_size_);
        return grid_y * 10000 + grid_x;  // 간단한 해시
    }

public:
    explicit SpatialGrid(float cell_size = 100.0f)
        : cell_size_(cell_size) {}

    void clear() {
        grid_.clear();
    }

    // 엔티티 등록
    void insert(int entity_id, float x, float y) {
        int key = get_grid_key(x, y);
        grid_[key].push_back(entity_id);
    }

    // 근처 엔티티 조회
    std::vector<int> query_nearby(float x, float y, float radius) const {
        std::vector<int> results;

        // 반경 내 그리드 셀들 검사
        int min_x = static_cast<int>((x - radius) / cell_size_);
        int max_x = static_cast<int>((x + radius) / cell_size_);
        int min_y = static_cast<int>((y - radius) / cell_size_);
        int max_y = static_cast<int>((y + radius) / cell_size_);

        for (int gy = min_y; gy <= max_y; ++gy) {
            for (int gx = min_x; gx <= max_x; ++gx) {
                int key = gy * 10000 + gx;
                auto it = grid_.find(key);
                if (it != grid_.end()) {
                    results.insert(results.end(),
                                  it->second.begin(),
                                  it->second.end());
                }
            }
        }

        return results;
    }
};
```

### 3.3 최적화된 충돌 검사

```cpp
class OptimizedCollisionManager {
private:
    std::vector<Projectile>& projectiles_;
    std::vector<Player>& players_;
    SpatialGrid spatial_grid_;

public:
    OptimizedCollisionManager(std::vector<Projectile>& projectiles,
                              std::vector<Player>& players)
        : projectiles_(projectiles)
        , players_(players)
        , spatial_grid_(100.0f)  // 100x100 셀 크기
    {}

    void check_and_resolve_collisions() {
        // 1. 공간 그리드에 플레이어 등록
        spatial_grid_.clear();
        for (size_t i = 0; i < players_.size(); ++i) {
            auto& player = players_[i];
            if (player.is_alive) {
                spatial_grid_.insert(i, player.x, player.y);
            }
        }

        // 2. 발사체마다 근처 플레이어만 검사
        for (auto& projectile : projectiles_) {
            if (!projectile.active) continue;

            // 근처 플레이어 조회 (반경 50)
            auto nearby_ids = spatial_grid_.query_nearby(
                projectile.x, projectile.y, 50.0f
            );

            // 근처 플레이어와만 충돌 검사
            for (int player_idx : nearby_ids) {
                auto& player = players_[player_idx];

                if (projectile.owner_id == player.id) continue;

                Circle proj_circle{projectile.x, projectile.y, projectile.radius};
                Circle player_circle{player.x, player.y, player.radius};

                if (check_collision(proj_circle, player_circle)) {
                    handle_collision(projectile, player);
                }
            }
        }
    }

private:
    void handle_collision(Projectile& projectile, Player& player) {
        player.take_damage(projectile.damage);
        projectile.active = false;

        std::cout << "💥 Player " << player.id << " hit! Health: "
                  << player.health << "\n";
    }
};
```

**성능 비교**:
```
Brute Force: 60 players × 200 projectiles = 12,000 checks
Spatial Grid: 200 projectiles × ~3 nearby players = 600 checks
→ 20배 빠름!
```

---

## 🎮 Part 4: 실전 - MVP 1.1 Combat System (15분)

### 4.1 통합 예제

```cpp
#include "collision.h"
#include "game_entities.h"
#include <iostream>
#include <vector>

class CombatSystem {
private:
    std::vector<Player> players_;
    std::vector<Projectile> projectiles_;
    OptimizedCollisionManager collision_mgr_;
    int next_projectile_id_ = 1;

public:
    CombatSystem()
        : collision_mgr_(projectiles_, players_) {}

    void add_player(int id, float x, float y) {
        players_.push_back({id, x, y});
    }

    void shoot_projectile(int owner_id, float x, float y, float vx, float vy) {
        Projectile proj;
        proj.id = next_projectile_id_++;
        proj.x = x;
        proj.y = y;
        proj.vx = vx;
        proj.vy = vy;
        proj.owner_id = owner_id;
        proj.active = true;

        projectiles_.push_back(proj);

        std::cout << "🔫 Player " << owner_id << " shoots projectile "
                  << proj.id << "\n";
    }

    void update(float delta_time) {
        // 1. 발사체 이동
        for (auto& proj : projectiles_) {
            if (proj.active) {
                proj.x += proj.vx * delta_time;
                proj.y += proj.vy * delta_time;

                // 화면 밖 제거
                if (proj.x < 0 || proj.x > 800 || proj.y < 0 || proj.y > 600) {
                    proj.active = false;
                }
            }
        }

        // 2. 충돌 검사 및 처리
        collision_mgr_.check_and_resolve_collisions();

        // 3. 비활성 발사체 제거
        projectiles_.erase(
            std::remove_if(projectiles_.begin(), projectiles_.end(),
                [](const Projectile& p) { return !p.active; }),
            projectiles_.end()
        );
    }

    void print_status() {
        std::cout << "\n=== Combat Status ===\n";
        std::cout << "Players: " << players_.size() << "\n";
        std::cout << "Active Projectiles: " << projectiles_.size() << "\n";

        for (auto& player : players_) {
            std::cout << "Player " << player.id << ": Health " << player.health
                      << (player.is_alive ? " (Alive)" : " (Dead)") << "\n";
        }
    }
};

int main() {
    CombatSystem combat;

    // 플레이어 2명 추가
    combat.add_player(1, 100.0f, 300.0f);
    combat.add_player(2, 700.0f, 300.0f);

    // Player 1이 Player 2를 향해 발사
    combat.shoot_projectile(1, 100.0f, 300.0f, 50.0f, 0.0f);

    // 게임 루프 (60 FPS, 3초간)
    const float delta_time = 1.0f / 60.0f;
    for (int tick = 0; tick < 180; ++tick) {
        combat.update(delta_time);

        if (tick % 60 == 0) {
            combat.print_status();
        }
    }

    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(combat_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(combat_demo
    collision.cpp
    game_entities.cpp
    combat_system.cpp
)
```

---

## 🐛 자주 막히는 부분

### 문제 1: sqrt() 성능 문제

```cpp
// ❌ 느림: sqrt() 호출
float distance = std::sqrt(dx * dx + dy * dy);
if (distance <= radius_sum) {
    // 충돌
}

// ✅ 빠름: 제곱 비교
float distance_squared = dx * dx + dy * dy;
if (distance_squared <= radius_sum * radius_sum) {
    // 충돌
}
```

### 문제 2: 자기 자신과 충돌

```cpp
// ❌ 플레이어가 자신의 발사체에 맞음
for (auto& proj : projectiles) {
    for (auto& player : players) {
        if (check_collision(...)) {
            // 충돌!
        }
    }
}

// ✅ 소유자 확인
if (proj.owner_id == player.id) {
    continue;  // 자신의 발사체는 무시
}
```

### 문제 3: 부동소수점 오차

```cpp
// ❌ 정확한 비교
if (distance == radius_sum) {  // 거의 성립 안 함!

// ✅ 오차 허용
const float epsilon = 0.001f;
if (std::abs(distance - radius_sum) < epsilon) {
```

### 문제 4: 중복 충돌 처리

```cpp
// ❌ 같은 발사체가 여러 플레이어를 관통
for (auto& player : players) {
    if (check_collision(proj, player)) {
        handle_collision(proj, player);
    }
}

// ✅ 첫 충돌 후 발사체 비활성화
if (check_collision(proj, player)) {
    handle_collision(proj, player);
    proj.active = false;
    break;  // 더 이상 검사 안 함
}
```

### 문제 5: 공간 분할 버그

```cpp
// ❌ 셀 크기가 너무 작으면 오버헤드
SpatialGrid grid(10.0f);  // 10x10 셀 → 너무 많은 셀!

// ❌ 셀 크기가 너무 크면 효과 없음
SpatialGrid grid(1000.0f);  // 1000x1000 → 모든 엔티티가 한 셀

// ✅ 적절한 크기 (플레이어 반경의 2~5배)
SpatialGrid grid(100.0f);
```

---

## ✅ 완료 체크리스트

### Part 1: Circle-Circle Collision
- [ ] 충돌 감지 원리 이해
- [ ] 기본 구현
- [ ] 게임 서버 통합

### Part 2: AABB Collision
- [ ] AABB 구조체 구현
- [ ] 충돌 검사 함수
- [ ] 점-AABB 충돌 검사

### Part 3: 공간 분할 최적화
- [ ] Brute Force 문제 이해
- [ ] Spatial Grid 구현
- [ ] 성능 향상 확인 (20배+)

### Part 4: 실전 Combat System
- [ ] CombatSystem 클래스 구현
- [ ] 발사체 시스템 통합
- [ ] 데미지 처리 및 플레이어 제거

---

## 🚀 다음 단계

✅ **Collision Detection 완료!**

**실전 적용**:
- MVP 1.1 - Combat System (발사체, 충돌, 데미지)
- MVP 2.0 - 60-Player Scale (공간 분할 필수)

**다음 학습**:
- [**Quickstart 70**](70-google-test.md) - 충돌 감지 테스트
- [**MVP 1.1**](82-mvp-1.1-combat-system.md) - 전투 시스템 구현

---

## 📚 참고 자료

- [Circle Collision Detection](https://developer.mozilla.org/en-US/docs/Games/Techniques/2D_collision_detection)
- [Spatial Hashing](https://www.gamedev.net/tutorials/programming/general-and-gameplay-programming/spatial-hashing-r2697/)
- [Quadtrees](https://en.wikipedia.org/wiki/Quadtree)
- [Bounding Volume Hierarchies](https://en.wikipedia.org/wiki/Bounding_volume_hierarchy)

---

**Last Updated**: 2025-01-30
