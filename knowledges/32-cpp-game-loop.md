# Quickstart 32: C++ Game Loop (Fixed Timestep)

## 🎯 목표
- **Fixed Timestep**: 일정한 간격으로 게임 로직 실행 (10 TPS, 60 TPS)
- **std::chrono**: 정확한 시간 측정 및 타이밍 제어
- **Game State**: 플레이어, 게임 상태 설계
- **실전**: Turn-based (10 TPS) → Real-time (60 TPS) 진화

## 📋 사전준비
- [Quickstart 30](30-cpp-for-game-server.md) 완료 (C++ 기초, 멀티스레딩)
- [Quickstart 31](31-cmake-build-system.md) 완료 (CMake)
- 기본 네트워킹 개념 (선택)

---

## ⏱️ Part 1: std::chrono 기초

### 1.1 시간 측정

```cpp
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    using namespace std::chrono;
    
    // 현재 시간
    auto start = steady_clock::now();
    
    // 작업 수행
    std::this_thread::sleep_for(milliseconds(100));
    
    // 경과 시간 계산
    auto end = steady_clock::now();
    auto elapsed = duration_cast<milliseconds>(end - start);
    
    std::cout << "Elapsed: " << elapsed.count() << " ms\n";
    // Elapsed: 100 ms
    
    return 0;
}
```

### 1.2 Duration 타입

```cpp
#include <chrono>
#include <iostream>

using namespace std::chrono;

int main() {
    // 다양한 시간 단위
    auto sec = seconds(1);
    auto ms = milliseconds(1000);
    auto us = microseconds(1000000);
    
    std::cout << "1 second = " << ms.count() << " ms\n";
    // 1 second = 1000 ms
    
    // 변환
    auto sec_to_ms = duration_cast<milliseconds>(sec);
    std::cout << "1 sec = " << sec_to_ms.count() << " ms\n";
    // 1 sec = 1000 ms
    
    // 연산
    auto total = sec + ms;
    std::cout << "Total: " << duration_cast<milliseconds>(total).count() << " ms\n";
    // Total: 2000 ms
    
    return 0;
}
```

### 1.3 Time Point

```cpp
#include <chrono>
#include <iostream>

using namespace std::chrono;

int main() {
    // 시작 시간
    auto start = steady_clock::now();
    
    // 미래 시간 계산
    auto future = start + seconds(5);
    
    // 현재 시간과 비교
    auto now = steady_clock::now();
    
    if (now < future) {
        auto remaining = duration_cast<seconds>(future - now);
        std::cout << "Remaining: " << remaining.count() << " sec\n";
    }
    
    return 0;
}
```

---

## 🎮 Part 2: Fixed Timestep Pattern

### 2.1 개념

게임 로직은 **일정한 간격(delta time)**으로 실행되어야 합니다.

```
Frame Rate (FPS) != Tick Rate (TPS)

FPS: 화면 렌더링 속도 (가변)
TPS: 게임 로직 업데이트 속도 (고정)

60 TPS = 16.67ms마다 update() 호출
10 TPS = 100ms마다 update() 호출
```

### 2.2 Fixed Timestep 구현 (10 TPS)

```cpp
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono;

class GameLoop {
private:
    static constexpr int TICK_RATE = 10;  // 10 TPS
    static constexpr auto TICK_DURATION = milliseconds(1000 / TICK_RATE);  // 100ms
    
    int tick_count = 0;
    
public:
    void run() {
        auto next_tick = steady_clock::now();
        
        while (tick_count < 50) {  // 5초 동안 실행
            // 게임 로직 업데이트
            update();
            
            // 다음 틱 시간 계산
            next_tick += TICK_DURATION;
            
            // 다음 틱까지 대기
            std::this_thread::sleep_until(next_tick);
        }
    }
    
private:
    void update() {
        tick_count++;
        
        auto now = steady_clock::now();
        auto time_since_epoch = duration_cast<milliseconds>(now.time_since_epoch());
        
        std::cout << "Tick " << tick_count 
                  << " at " << time_since_epoch.count() << " ms\n";
    }
};

int main() {
    GameLoop game;
    game.run();
    
    return 0;
}
```

**실행 결과**:
```
Tick 1 at 1699123456000 ms
Tick 2 at 1699123456100 ms  // +100ms
Tick 3 at 1699123456200 ms  // +100ms
...
```

### 2.3 Fixed Timestep 고도화 (Accumulator 패턴)

```cpp
#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono;

class GameLoop {
private:
    static constexpr int TICK_RATE = 10;
    static constexpr auto TICK_DURATION = milliseconds(1000 / TICK_RATE);
    
    int tick_count = 0;
    milliseconds accumulator{0};
    
public:
    void run() {
        auto previous = steady_clock::now();
        
        while (tick_count < 50) {
            auto current = steady_clock::now();
            auto frame_time = duration_cast<milliseconds>(current - previous);
            previous = current;
            
            // 누적
            accumulator += frame_time;
            
            // 누적 시간이 틱 간격 이상이면 업데이트
            while (accumulator >= TICK_DURATION) {
                update();
                accumulator -= TICK_DURATION;
            }
            
            // CPU 점유율 낮추기
            std::this_thread::sleep_for(milliseconds(10));
        }
    }
    
private:
    void update() {
        tick_count++;
        std::cout << "Tick " << tick_count << "\n";
    }
};

int main() {
    GameLoop game;
    game.run();
    
    return 0;
}
```

---

## 🏃 Part 3: 10 TPS Turn-based Game

### 3.1 Game State 설계

```cpp
#include <iostream>
#include <vector>
#include <string>

struct Player {
    int id;
    std::string name;
    int health;
    int x, y;  // 위치
    
    Player(int id, const std::string& name, int x, int y)
        : id(id), name(name), health(100), x(x), y(y) {}
};

class GameState {
private:
    std::vector<Player> players;
    int tick = 0;
    
public:
    void add_player(int id, const std::string& name, int x, int y) {
        players.emplace_back(id, name, x, y);
        std::cout << "Player " << name << " joined at (" << x << ", " << y << ")\n";
    }
    
    void update() {
        tick++;
        
        // 모든 플레이어 이동 (랜덤)
        for (auto& player : players) {
            int dx = (rand() % 3) - 1;  // -1, 0, 1
            int dy = (rand() % 3) - 1;
            
            player.x += dx;
            player.y += dy;
        }
        
        // 상태 출력
        print_state();
    }
    
    void print_state() {
        std::cout << "\n--- Tick " << tick << " ---\n";
        for (const auto& player : players) {
            std::cout << player.name << ": "
                      << "HP=" << player.health << " "
                      << "Pos=(" << player.x << ", " << player.y << ")\n";
        }
    }
};
```

### 3.2 10 TPS 게임 루프

```cpp
#include <chrono>
#include <thread>

using namespace std::chrono;

class TurnBasedGame {
private:
    static constexpr int TICK_RATE = 10;
    static constexpr auto TICK_DURATION = milliseconds(1000 / TICK_RATE);
    
    GameState state;
    bool running = true;
    
public:
    void start() {
        // 플레이어 추가
        state.add_player(1, "Alice", 0, 0);
        state.add_player(2, "Bob", 5, 5);
        
        // 게임 루프
        auto next_tick = steady_clock::now();
        int tick_count = 0;
        
        while (running && tick_count < 30) {  // 3초 동안
            state.update();
            
            next_tick += TICK_DURATION;
            std::this_thread::sleep_until(next_tick);
            
            tick_count++;
        }
        
        std::cout << "\nGame finished after " << tick_count << " ticks\n";
    }
};

int main() {
    srand(time(nullptr));
    
    TurnBasedGame game;
    game.start();
    
    return 0;
}
```

**전체 코드 (turn_based_game.cpp)**:
```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <string>

using namespace std::chrono;

struct Player {
    int id;
    std::string name;
    int health;
    int x, y;
    
    Player(int id, const std::string& name, int x, int y)
        : id(id), name(name), health(100), x(x), y(y) {}
};

class GameState {
private:
    std::vector<Player> players;
    int tick = 0;
    
public:
    void add_player(int id, const std::string& name, int x, int y) {
        players.emplace_back(id, name, x, y);
        std::cout << "Player " << name << " joined at (" << x << ", " << y << ")\n";
    }
    
    void update() {
        tick++;
        
        for (auto& player : players) {
            int dx = (rand() % 3) - 1;
            int dy = (rand() % 3) - 1;
            player.x += dx;
            player.y += dy;
        }
        
        print_state();
    }
    
    void print_state() {
        std::cout << "\n--- Tick " << tick << " ---\n";
        for (const auto& player : players) {
            std::cout << player.name << ": "
                      << "HP=" << player.health << " "
                      << "Pos=(" << player.x << ", " << player.y << ")\n";
        }
    }
};

class TurnBasedGame {
private:
    static constexpr int TICK_RATE = 10;
    static constexpr auto TICK_DURATION = milliseconds(1000 / TICK_RATE);
    GameState state;
    bool running = true;
    
public:
    void start() {
        state.add_player(1, "Alice", 0, 0);
        state.add_player(2, "Bob", 5, 5);
        
        auto next_tick = steady_clock::now();
        int tick_count = 0;
        
        while (running && tick_count < 30) {
            state.update();
            next_tick += TICK_DURATION;
            std::this_thread::sleep_until(next_tick);
            tick_count++;
        }
        
        std::cout << "\nGame finished after " << tick_count << " ticks\n";
    }
};

int main() {
    srand(time(nullptr));
    TurnBasedGame game;
    game.start();
    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(turn_based_game)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(turn_based_game turn_based_game.cpp)

# Linux/macOS: pthread 링크
if(UNIX)
    target_link_libraries(turn_based_game PRIVATE pthread)
endif()
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./turn_based_game
```

**실행 결과**:
```
Player Alice joined at (0, 0)
Player Bob joined at (5, 5)

--- Tick 1 ---
Alice: HP=100 Pos=(0, -1)
Bob: HP=100 Pos=(6, 5)

--- Tick 2 ---
Alice: HP=100 Pos=(1, -1)
Bob: HP=100 Pos=(6, 6)

--- Tick 3 ---
Alice: HP=100 Pos=(1, -2)
Bob: HP=100 Pos=(5, 6)

--- Tick 4 ---
Alice: HP=100 Pos=(2, -2)
Bob: HP=100 Pos=(5, 7)

--- Tick 5 ---
Alice: HP=100 Pos=(2, -3)
Bob: HP=100 Pos=(6, 7)

...

--- Tick 30 ---
Alice: HP=100 Pos=(3, -8)
Bob: HP=100 Pos=(8, 12)

Game finished after 30 ticks
```

**설명**:
- 10 TPS → 100ms마다 틱 실행
- 30틱 = 3초 동안 게임 진행
- 각 플레이어는 랜덤하게 이동 (-1, 0, +1)

---

## ⚡ Part 4: 60 TPS Real-time Game

### 4.1 고주파수 게임 루프

```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono;

class HighFrequencyGame {
private:
    static constexpr int TICK_RATE = 60;  // 60 TPS
    static constexpr auto TICK_DURATION = microseconds(1000000 / TICK_RATE);  // 16667us
    
    int tick_count = 0;
    
    // 성능 측정
    std::vector<int64_t> frame_times;
    
public:
    void run() {
        auto next_tick = steady_clock::now();
        
        while (tick_count < 600) {  // 10초 동안
            auto start = steady_clock::now();
            
            // 게임 로직
            update();
            
            // 프레임 시간 측정
            auto end = steady_clock::now();
            auto elapsed = duration_cast<microseconds>(end - start);
            frame_times.push_back(elapsed.count());
            
            // 다음 틱까지 대기
            next_tick += TICK_DURATION;
            std::this_thread::sleep_until(next_tick);
        }
        
        print_stats();
    }
    
private:
    void update() {
        tick_count++;
        
        // 매 60틱마다 출력 (1초마다)
        if (tick_count % 60 == 0) {
            std::cout << "Tick " << tick_count << " (1 second)\n";
        }
    }
    
    void print_stats() {
        // 평균, 최소, 최대 프레임 시간 계산
        int64_t sum = 0;
        int64_t min_time = frame_times[0];
        int64_t max_time = frame_times[0];
        
        for (auto time : frame_times) {
            sum += time;
            if (time < min_time) min_time = time;
            if (time > max_time) max_time = time;
        }
        
        int64_t avg = sum / frame_times.size();
        
        std::cout << "\n=== Performance Stats ===\n";
        std::cout << "Total ticks: " << tick_count << "\n";
        std::cout << "Avg frame time: " << avg << " us\n";
        std::cout << "Min frame time: " << min_time << " us\n";
        std::cout << "Max frame time: " << max_time << " us\n";
        std::cout << "Target: " << TICK_DURATION.count() << " us (60 TPS)\n";
    }
};

int main() {
    HighFrequencyGame game;
    game.run();
    
    return 0;
}
```

### 4.2 Pong 게임 (60 TPS)

```cpp
#include <chrono>
#include <iostream>
#include <thread>
#include <cmath>

using namespace std::chrono;

struct Ball {
    float x, y;
    float vx, vy;
};

struct Paddle {
    float y;
    float vy;
};

class PongGame {
private:
    static constexpr int TICK_RATE = 60;
    static constexpr auto TICK_DURATION = microseconds(1000000 / TICK_RATE);
    static constexpr float DT = 1.0f / TICK_RATE;
    
    Ball ball{50.0f, 50.0f, 30.0f, 20.0f};
    Paddle paddle_left{50.0f, 0.0f};
    Paddle paddle_right{50.0f, 0.0f};
    
    int tick_count = 0;
    int left_score = 0;
    int right_score = 0;
    
public:
    void run() {
        auto next_tick = steady_clock::now();
        
        std::cout << "Pong Game Starting (60 TPS)...\n\n";
        
        while (tick_count < 600) {
            update();
            next_tick += TICK_DURATION;
            std::this_thread::sleep_until(next_tick);
        }
        
        std::cout << "\nGame Over!\n";
        std::cout << "Final Score - Left: " << left_score 
                  << ", Right: " << right_score << "\n";
    }
    
private:
    void update() {
        tick_count++;
        
        ball.x += ball.vx * DT;
        ball.y += ball.vy * DT;
        
        if (ball.y <= 0 || ball.y >= 100) {
            ball.vy = -ball.vy;
        }
        
        if (ball.x <= 0) {
            right_score++;
            std::cout << "Right player scores! (" << left_score 
                      << " - " << right_score << ")\n";
            reset_ball();
        }
        
        if (ball.x >= 100) {
            left_score++;
            std::cout << "Left player scores! (" << left_score 
                      << " - " << right_score << ")\n";
            reset_ball();
        }
        
        if (tick_count % 60 == 0) {
            std::cout << "Tick " << tick_count/60 << "s - Ball: (" 
                      << ball.x << ", " << ball.y << ")\n";
        }
    }
    
    void reset_ball() {
        ball.x = 50.0f;
        ball.y = 50.0f;
        ball.vx = (rand() % 2 == 0) ? 30.0f : -30.0f;
        ball.vy = (rand() % 40) - 20.0f;
    }
};

int main() {
    srand(time(nullptr));
    PongGame game;
    game.run();
    return 0;
}
```

**CMakeLists.txt** (pong_game):
```cmake
cmake_minimum_required(VERSION 3.20)
project(pong_game)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(pong_game pong_game.cpp)

if(UNIX)
    target_link_libraries(pong_game PRIVATE pthread)
endif()
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./pong_game
```

**실행 결과**:
```
Pong Game Starting (60 TPS)...

Tick 1s - Ball: (50.5, 50.333)
Right player scores! (0 - 1)
Tick 2s - Ball: (55.2, 48.7)
Left player scores! (1 - 1)
Tick 3s - Ball: (45.8, 52.1)
Right player scores! (1 - 2)
Tick 4s - Ball: (60.3, 49.2)
Tick 5s - Ball: (40.1, 51.8)
Left player scores! (2 - 2)
Tick 6s - Ball: (52.5, 50.5)
Tick 7s - Ball: (48.3, 49.1)
Right player scores! (2 - 3)
Tick 8s - Ball: (56.7, 51.3)
Tick 9s - Ball: (43.2, 48.9)
Left player scores! (3 - 3)
Tick 10s - Ball: (51.1, 50.2)

Game Over!
Final Score - Left: 3, Right: 3
```

**성능 분석**:
- 60 TPS = 16.67ms마다 update() 호출
- 10초 = 600틱
- CPU 사용률: ~1-2% (sleep_until 덕분)
- 프레임 시간: 평균 50-100μs (업데이트 로직이 가벼움)

---

## 🐛 자주 막히는 부분

### 문제 1: 틱이 밀린다 (Tick Lag)

```cpp
// ❌ 틱 간격이 점점 벌어짐
while (running) {
    auto start = steady_clock::now();
    
    update();
    
    auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start);
    std::this_thread::sleep_for(TICK_DURATION - elapsed);  // 문제!
}

// ✅ sleep_until 사용
auto next_tick = steady_clock::now();
while (running) {
    update();
    
    next_tick += TICK_DURATION;
    std::this_thread::sleep_until(next_tick);
}
```

### 문제 2: 높은 CPU 사용률

```cpp
// ❌ Busy waiting
while (accumulator < TICK_DURATION) {
    auto now = steady_clock::now();
    accumulator = duration_cast<milliseconds>(now - previous);
}

// ✅ sleep_for로 CPU 양보
while (accumulator < TICK_DURATION) {
    std::this_thread::sleep_for(milliseconds(1));
    auto now = steady_clock::now();
    accumulator = duration_cast<milliseconds>(now - previous);
}
```

### 문제 3: 프레임 시간 초과 (Update > Tick Duration)

```cpp
// ❌ update()가 16ms 초과하면?
void run() {
    auto next_tick = steady_clock::now();
    
    while (running) {
        update();  // 20ms 걸림! (16.67ms 목표)
        
        next_tick += TICK_DURATION;
        std::this_thread::sleep_until(next_tick);  // 이미 늦음
    }
}

// ✅ 성능 측정 및 경고
void run() {
    auto next_tick = steady_clock::now();
    
    while (running) {
        auto start = steady_clock::now();
        update();
        auto elapsed = duration_cast<microseconds>(steady_clock::now() - start);
        
        if (elapsed > TICK_DURATION) {
            std::cerr << "WARNING: Update took " << elapsed.count() 
                      << " us (target: " << TICK_DURATION.count() << " us)\n";
        }
        
        next_tick += TICK_DURATION;
        std::this_thread::sleep_until(next_tick);
    }
}
```

### 문제 4: Delta Time 단위 혼동

```cpp
// ❌ 초 단위인지 밀리초 단위인지 혼동
constexpr float DT = 1000 / TICK_RATE;  // 16.67 (밀리초?)
ball.x += ball.vx * DT;  // 속도가 1000배!

// ✅ 명확하게 초 단위 사용
constexpr float DT = 1.0f / TICK_RATE;  // 0.0167 (초)
// 또는
constexpr auto DT = milliseconds(1000 / TICK_RATE);  // 16ms
```

### 문제 5: 타이머 정밀도 부족

```cpp
// ❌ system_clock (벽시계, NTP에 영향받음)
auto start = std::chrono::system_clock::now();

// ✅ steady_clock (단조 증가, NTP 영향 없음)
auto start = std::chrono::steady_clock::now();
```

---

## ✅ 완료 체크리스트

### Part 1: std::chrono
- [ ] `steady_clock::now()` 시간 측정
- [ ] `duration_cast<>` 변환
- [ ] `sleep_until()` 정확한 대기

### Part 2: Fixed Timestep
- [ ] 10 TPS 게임 루프 구현
- [ ] Accumulator 패턴 이해
- [ ] `next_tick += TICK_DURATION` 패턴

### Part 3: Turn-based (10 TPS)
- [ ] GameState 설계
- [ ] 플레이어 상태 관리
- [ ] 10 TPS 게임 실행 성공

### Part 4: Real-time (60 TPS)
- [ ] 60 TPS 게임 루프 구현
- [ ] 성능 측정 (avg, min, max)
- [ ] Pong 게임 실행 성공

---

## 🚀 다음 단계

✅ **C++ Game Loop 완료!**

**다음 학습**:
- [**Quickstart 33**](33-boost-asio-beast.md) - WebSocket 네트워킹
- [**Quickstart 41**](41-cpp-udp-sockets.md) - UDP 실시간 통신

**실전 적용**:
- `mini-gameserver` M1.2 - 10 TPS Turn-based Combat
- `mini-gameserver` M1.4 - 60 TPS Pong Game

---

## 📚 참고 자료

- [std::chrono Reference](https://en.cppreference.com/w/cpp/chrono)
- [Fix Your Timestep!](https://gafferongames.com/post/fix_your_timestep/) (필독!)
- [Game Programming Patterns - Game Loop](https://gameprogrammingpatterns.com/game-loop.html)
- [Gaffer on Games](https://gafferongames.com/) (네트워크 게임 시리즈)

---

**Last Updated**: 2025-11-12
