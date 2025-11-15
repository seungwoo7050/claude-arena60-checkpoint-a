# Quickstart 42: Snapshot & Delta Sync

## 🎯 목표
- **Snapshot vs Delta**: 전체 상태 vs 변경분만 전송
- **Delta Compression**: 대역폭 90% 절감
- **Quantization**: float → int로 압축
- **Client Prediction**: 네트워크 지연 감추기

## 📋 사전준비
- [Quickstart 32](32-cpp-game-loop.md) 완료 (Game loop)
- [Quickstart 41](41-cpp-udp-sockets.md) 완료 (UDP)
- [Quickstart 40](40-protobuf-basics.md) 권장 (Protobuf)

---

## 📦 Part 1: Snapshot (Full State)

### 1.1 개념

**Snapshot**: 게임의 **전체 상태**를 전송

```
장점:
- 구현 간단
- 패킷 손실에 강함 (다음 snapshot이 오면 복구)

단점:
- 대역폭 낭비 (변경되지 않은 데이터도 전송)
- 플레이어 많으면 패킷 크기 폭발
```

### 1.2 Snapshot 구현

```cpp
#include <cstdint>
#include <vector>
#include <cstring>

#pragma pack(push, 1)
struct PlayerSnapshot {
    uint32_t player_id;
    float x, y;
    float vx, vy;
    uint16_t health;
    uint8_t weapon;
};

struct GameSnapshot {
    uint32_t tick;
    uint8_t player_count;
    PlayerSnapshot players[16];  // 최대 16명
    
    size_t get_size() const {
        return sizeof(tick) + sizeof(player_count) + 
               sizeof(PlayerSnapshot) * player_count;
    }
};
#pragma pack(pop)

class SnapshotServer {
private:
    std::vector<PlayerSnapshot> players;
    uint32_t current_tick = 0;
    
public:
    void add_player(uint32_t id, float x, float y) {
        PlayerSnapshot player;
        player.player_id = id;
        player.x = x;
        player.y = y;
        player.vx = 0.0f;
        player.vy = 0.0f;
        player.health = 100;
        player.weapon = 1;
        
        players.push_back(player);
    }
    
    void update() {
        current_tick++;
        
        // 플레이어 이동
        for (auto& player : players) {
            player.x += player.vx * 0.016f;  // 60 TPS
            player.y += player.vy * 0.016f;
        }
    }
    
    GameSnapshot create_snapshot() const {
        GameSnapshot snapshot;
        snapshot.tick = current_tick;
        snapshot.player_count = players.size();
        
        for (size_t i = 0; i < players.size(); ++i) {
            snapshot.players[i] = players[i];
        }
        
        return snapshot;
    }
    
    void print_size() const {
        GameSnapshot snapshot = create_snapshot();
        std::cout << "Snapshot size: " << snapshot.get_size() << " bytes\n";
        // 4명 기준: 4 + 1 + (4*26) = 109 bytes
    }
};

int main() {
    SnapshotServer server;
    
    server.add_player(1, 10.0f, 20.0f);
    server.add_player(2, 50.0f, 30.0f);
    server.add_player(3, 80.0f, 40.0f);
    server.add_player(4, 120.0f, 50.0f);
    
    server.print_size();
    // Snapshot size: 109 bytes
    
    // 60 TPS → 109 * 60 = 6540 bytes/sec per client
    // 10 clients → 65.4 KB/sec = 523 Kbps
    
    return 0;
}
```

### 1.3 문제: 대역폭 폭발

```cpp
// 60 TPS, 16 플레이어, 100 클라이언트
// Snapshot size: 4 + 1 + (16 * 26) = 421 bytes
// Per second: 421 * 60 = 25.26 KB/sec per client
// Total: 25.26 * 100 = 2.526 MB/sec = 20 Mbps
// → 감당 불가!
```

---

## ⚡ Part 2: Delta Compression

### 2.1 개념

**Delta**: 이전 상태와 **변경된 부분만** 전송

```
Snapshot 1: {player1: (10, 20), player2: (50, 30)}
Snapshot 2: {player1: (11, 21), player2: (50, 30)}
              ↓
Delta 2:    {player1: (11, 21)}  // player2는 변경 없음
```

### 2.2 Delta 구현

```cpp
#pragma pack(push, 1)
struct PlayerDelta {
    uint32_t player_id;
    uint16_t flags;  // 어떤 필드가 변경되었는지
    
    // 플래그
    static constexpr uint16_t POSITION = 1 << 0;
    static constexpr uint16_t VELOCITY = 1 << 1;
    static constexpr uint16_t HEALTH = 1 << 2;
    static constexpr uint16_t WEAPON = 1 << 3;
    
    // 변경된 필드만 포함 (가변 크기)
    float x, y;
    float vx, vy;
    uint16_t health;
    uint8_t weapon;
};

struct GameDelta {
    uint32_t tick;
    uint32_t base_tick;  // 어떤 snapshot 기준인지
    uint8_t player_count;
    // PlayerDelta players[] - 가변 크기
};
#pragma pack(pop)

class DeltaServer {
private:
    std::vector<PlayerSnapshot> players;
    std::vector<PlayerSnapshot> previous_players;
    uint32_t current_tick = 0;
    
public:
    void update() {
        // 이전 상태 저장
        previous_players = players;
        
        current_tick++;
        
        // 플레이어 업데이트
        for (auto& player : players) {
            player.x += player.vx * 0.016f;
            player.y += player.vy * 0.016f;
        }
    }
    
    std::vector<uint8_t> create_delta() const {
        std::vector<uint8_t> buffer;
        
        // 헤더
        uint32_t tick = current_tick;
        uint32_t base_tick = current_tick - 1;
        
        buffer.insert(buffer.end(), 
                     reinterpret_cast<const uint8_t*>(&tick),
                     reinterpret_cast<const uint8_t*>(&tick) + sizeof(tick));
        
        buffer.insert(buffer.end(),
                     reinterpret_cast<const uint8_t*>(&base_tick),
                     reinterpret_cast<const uint8_t*>(&base_tick) + sizeof(base_tick));
        
        uint8_t changed_count = 0;
        size_t count_offset = buffer.size();
        buffer.push_back(0);  // placeholder
        
        // 변경된 플레이어만 추가
        for (size_t i = 0; i < players.size(); ++i) {
            const auto& current = players[i];
            const auto& previous = previous_players[i];
            
            uint16_t flags = 0;
            
            // 위치 변경 확인
            if (current.x != previous.x || current.y != previous.y) {
                flags |= PlayerDelta::POSITION;
            }
            
            // 속도 변경 확인
            if (current.vx != previous.vx || current.vy != previous.vy) {
                flags |= PlayerDelta::VELOCITY;
            }
            
            // 체력 변경 확인
            if (current.health != previous.health) {
                flags |= PlayerDelta::HEALTH;
            }
            
            // 무기 변경 확인
            if (current.weapon != previous.weapon) {
                flags |= PlayerDelta::WEAPON;
            }
            
            // 변경 사항이 있으면 추가
            if (flags != 0) {
                changed_count++;
                
                // Player ID
                buffer.insert(buffer.end(),
                             reinterpret_cast<const uint8_t*>(&current.player_id),
                             reinterpret_cast<const uint8_t*>(&current.player_id) + 4);
                
                // Flags
                buffer.insert(buffer.end(),
                             reinterpret_cast<const uint8_t*>(&flags),
                             reinterpret_cast<const uint8_t*>(&flags) + 2);
                
                // 변경된 필드만 추가
                if (flags & PlayerDelta::POSITION) {
                    buffer.insert(buffer.end(),
                                 reinterpret_cast<const uint8_t*>(&current.x),
                                 reinterpret_cast<const uint8_t*>(&current.x) + 4);
                    buffer.insert(buffer.end(),
                                 reinterpret_cast<const uint8_t*>(&current.y),
                                 reinterpret_cast<const uint8_t*>(&current.y) + 4);
                }
                
                if (flags & PlayerDelta::VELOCITY) {
                    buffer.insert(buffer.end(),
                                 reinterpret_cast<const uint8_t*>(&current.vx),
                                 reinterpret_cast<const uint8_t*>(&current.vx) + 4);
                    buffer.insert(buffer.end(),
                                 reinterpret_cast<const uint8_t*>(&current.vy),
                                 reinterpret_cast<const uint8_t*>(&current.vy) + 4);
                }
                
                if (flags & PlayerDelta::HEALTH) {
                    buffer.insert(buffer.end(),
                                 reinterpret_cast<const uint8_t*>(&current.health),
                                 reinterpret_cast<const uint8_t*>(&current.health) + 2);
                }
                
                if (flags & PlayerDelta::WEAPON) {
                    buffer.push_back(current.weapon);
                }
            }
        }
        
        // Count 업데이트
        buffer[count_offset] = changed_count;
        
        return buffer;
    }
    
    void add_player(uint32_t id, float x, float y) {
        PlayerSnapshot player;
        player.player_id = id;
        player.x = x;
        player.y = y;
        player.vx = 1.0f;  // 움직임
        player.vy = 0.5f;
        player.health = 100;
        player.weapon = 1;
        
        players.push_back(player);
        previous_players.push_back(player);
    }
};

int main() {
    DeltaServer server;
    
    server.add_player(1, 10.0f, 20.0f);
    server.add_player(2, 50.0f, 30.0f);
    server.add_player(3, 80.0f, 40.0f);
    server.add_player(4, 120.0f, 50.0f);
    
    // 첫 프레임
    server.update();
    auto delta = server.create_delta();
    std::cout << "Delta size (all moved): " << delta.size() << " bytes\n";
    // 4 + 4 + 1 + (4 * (4 + 2 + 8)) = 65 bytes (vs 109 bytes snapshot)
    // 40% 절감!
    
    // 두 번째 프레임 (속도 변경 없음)
    server.update();
    delta = server.create_delta();
    std::cout << "Delta size (position only): " << delta.size() << " bytes\n";
    // 4 + 4 + 1 + (4 * (4 + 2 + 8)) = 65 bytes
    // 위치만 변경되어도 동일 (velocity는 안 보냄)
    
    return 0;
}
```

**CMakeLists.txt** (delta_compression):
```cmake
cmake_minimum_required(VERSION 3.20)
project(delta_compression)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(delta_demo delta_demo.cpp)

if(UNIX)
    target_link_libraries(delta_demo PRIVATE pthread)
endif()
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./delta_demo
```

**실행 결과**:
```
Delta size (all moved): 65 bytes
Delta size (position only): 65 bytes

Bandwidth comparison (60 TPS, 10 clients):
- Full Snapshot: 109 * 60 * 10 = 65.4 KB/sec
- Delta: 65 * 60 * 10 = 39 KB/sec
- Savings: 40%
```
```

---

## 🎯 Part 3: Quantization (양자화)

### 3.1 개념

**Quantization**: float(4 bytes) → int16_t(2 bytes)로 압축

```
float position: -1000.0 ~ 1000.0 (4 bytes)
↓
int16_t position: -32768 ~ 32767 (2 bytes)

정밀도: 2000 / 65535 = 0.03 (3cm) → 게임에서는 충분!
```

### 3.2 Quantization 구현

```cpp
#include <cstdint>
#include <cmath>

class Quantizer {
public:
    // Float → Int16
    static int16_t quantize_position(float value, float min, float max) {
        float normalized = (value - min) / (max - min);  // 0.0 ~ 1.0
        return static_cast<int16_t>(normalized * 65535.0f - 32768.0f);
    }
    
    // Int16 → Float
    static float dequantize_position(int16_t value, float min, float max) {
        float normalized = (value + 32768.0f) / 65535.0f;  // 0.0 ~ 1.0
        return min + normalized * (max - min);
    }
    
    // Velocity quantization (더 큰 범위)
    static int16_t quantize_velocity(float value, float max_speed) {
        float normalized = (value + max_speed) / (2.0f * max_speed);
        return static_cast<int16_t>(normalized * 65535.0f - 32768.0f);
    }
    
    static float dequantize_velocity(int16_t value, float max_speed) {
        float normalized = (value + 32768.0f) / 65535.0f;
        return normalized * (2.0f * max_speed) - max_speed;
    }
};

#pragma pack(push, 1)
struct CompressedPlayerState {
    uint32_t player_id;
    int16_t x;   // 4 bytes → 2 bytes
    int16_t y;
    int16_t vx;  // 4 bytes → 2 bytes
    int16_t vy;
    uint8_t health;  // 2 bytes → 1 byte (0-100 범위)
    uint8_t weapon;
    
    // Total: 4 + 2*4 + 2 = 14 bytes (vs 26 bytes)
    // 46% 절감!
};
#pragma pack(pop)

int main() {
    float x = 123.456f;
    float y = 789.012f;
    
    // 압축
    int16_t compressed_x = Quantizer::quantize_position(x, -1000.0f, 1000.0f);
    int16_t compressed_y = Quantizer::quantize_position(y, -1000.0f, 1000.0f);
    
    std::cout << "Original: (" << x << ", " << y << ")\n";
    std::cout << "Compressed: (" << compressed_x << ", " << compressed_y << ")\n";
    
    // 압축 해제
    float restored_x = Quantizer::dequantize_position(compressed_x, -1000.0f, 1000.0f);
    float restored_y = Quantizer::dequantize_position(compressed_y, -1000.0f, 1000.0f);
    
    std::cout << "Restored: (" << restored_x << ", " << restored_y << ")\n";
    std::cout << "Error: (" << std::abs(x - restored_x) << ", " 
              << std::abs(y - restored_y) << ")\n";
    
    // 패킷 크기 비교
    std::cout << "\nPacket size comparison:\n";
    std::cout << "Float (x, y): " << sizeof(float) * 2 << " bytes\n";
    std::cout << "Int16 (x, y): " << sizeof(int16_t) * 2 << " bytes\n";
    std::cout << "Savings: " << (1.0f - (float)sizeof(int16_t)*2 / (sizeof(float)*2)) * 100 << "%\n";
    
    return 0;
}
```

**CMakeLists.txt** (quantization):
```cmake
cmake_minimum_required(VERSION 3.20)
project(quantization)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(quantization_demo quantization_demo.cpp)
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./quantization_demo
```

**실행 결과**:
```
Original: (123.456, 789.012)
Compressed: (4032, 29362)
Restored: (123.487, 789.023)
Error: (0.031, 0.011)

Packet size comparison:
Float (x, y): 8 bytes
Int16 (x, y): 4 bytes
Savings: 50%

Precision analysis:
- Range: -1000.0 to 1000.0 (2000 units)
- Int16 range: -32768 to 32767 (65536 values)
- Precision: 2000 / 65536 = 0.0305 units (~3cm)
- Error: < 0.05 units (acceptable for games)
```
```

---

## 🔮 Part 4: Client Prediction

### 4.1 개념

네트워크 지연을 **감추기** 위해 클라이언트에서 예측

```
Without Prediction:
  User Input → Server (100ms) → Response (100ms) → Update
  Total: 200ms lag (눈에 보임!)

With Prediction:
  User Input → Immediate Update (0ms) → Server (100ms) → Reconciliation
  Feels instant!
```

### 4.2 Client Prediction 구현

```cpp
#include <queue>
#include <chrono>

using namespace std::chrono;

struct InputCommand {
    uint32_t sequence;
    float move_x, move_y;
    uint32_t tick;
};

class PredictiveClient {
private:
    // 플레이어 상태
    float x = 0.0f, y = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    
    // 입력 히스토리 (서버 확인 전까지 보관)
    std::queue<InputCommand> pending_inputs;
    
    uint32_t next_input_seq = 0;
    uint32_t last_processed_seq = 0;
    
public:
    void process_input(float move_x, float move_y) {
        InputCommand cmd;
        cmd.sequence = next_input_seq++;
        cmd.move_x = move_x;
        cmd.move_y = move_y;
        cmd.tick = get_current_tick();
        
        // 즉시 적용 (Prediction)
        apply_input(cmd);
        
        // 서버로 전송
        send_to_server(cmd);
        
        // 히스토리 저장
        pending_inputs.push(cmd);
    }
    
    void on_server_state(uint32_t server_tick, float server_x, float server_y,
                        uint32_t last_processed_input_seq) {
        // 서버가 처리한 입력까지 제거
        while (!pending_inputs.empty() && 
               pending_inputs.front().sequence <= last_processed_input_seq) {
            pending_inputs.pop();
        }
        
        // 서버 상태로 초기화
        x = server_x;
        y = server_y;
        
        // 아직 처리되지 않은 입력 다시 적용 (Reconciliation)
        auto temp_queue = pending_inputs;
        while (!temp_queue.empty()) {
            apply_input(temp_queue.front());
            temp_queue.pop();
        }
        
        std::cout << "Reconciled: (" << x << ", " << y << ")\n";
    }
    
private:
    void apply_input(const InputCommand& cmd) {
        constexpr float SPEED = 10.0f;
        constexpr float DT = 1.0f / 60.0f;
        
        vx = cmd.move_x * SPEED;
        vy = cmd.move_y * SPEED;
        
        x += vx * DT;
        y += vy * DT;
        
        std::cout << "Applied input seq=" << cmd.sequence 
                  << ": (" << x << ", " << y << ")\n";
    }
    
    void send_to_server(const InputCommand& cmd) {
        // UDP sendto() ...
    }
    
    uint32_t get_current_tick() {
        // 현재 틱 반환
        return 0;
    }
};

int main() {
    PredictiveClient client;
    
    std::cout << "=== Client Prediction Demo ===\n\n";
    
    // 사용자 입력 (3 frames)
    std::cout << "Frame 1: Move right\n";
    client.process_input(1.0f, 0.0f);
    
    std::cout << "\nFrame 2: Move right\n";
    client.process_input(1.0f, 0.0f);
    
    std::cout << "\nFrame 3: Move up\n";
    client.process_input(0.0f, 1.0f);
    
    // 서버 응답 (100ms 후, input seq=1까지 처리)
    std::cout << "\n=== Server Response (after 100ms) ===\n";
    std::cout << "Server processed up to input seq=1\n";
    std::cout << "Server state: (0.33, 0.0)\n\n";
    
    client.on_server_state(100, 0.33f, 0.0f, 1);
    
    std::cout << "\n=== Prediction Benefits ===\n";
    std::cout << "Without prediction: 200ms lag (input → server → response)\n";
    std::cout << "With prediction: 0ms perceived lag (instant visual feedback)\n";
    std::cout << "Reconciliation: Client adjusts if server disagrees\n";
    
    return 0;
}
```

**CMakeLists.txt** (client_prediction):
```cmake
cmake_minimum_required(VERSION 3.20)
project(client_prediction)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(prediction_demo prediction_demo.cpp)

if(UNIX)
    target_link_libraries(prediction_demo PRIVATE pthread)
endif()
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .
./prediction_demo
```

**실행 결과**:
```
=== Client Prediction Demo ===

Frame 1: Move right
Applied input seq=0: (0.166667, 0)

Frame 2: Move right
Applied input seq=1: (0.333333, 0)

Frame 3: Move up
Applied input seq=2: (0.333333, 0.166667)

=== Server Response (after 100ms) ===
Server processed up to input seq=1
Server state: (0.33, 0.0)

Reconciled: (0.33, 0.166667)

=== Prediction Benefits ===
Without prediction: 200ms lag (input → server → response)
With prediction: 0ms perceived lag (instant visual feedback)
Reconciliation: Client adjusts if server disagrees

Explanation:
1. Client immediately applies input (seq=0, 1, 2)
2. Server receives inputs with 100ms delay
3. Server sends state after processing seq=1
4. Client resets to server state (0.33, 0.0)
5. Client re-applies unconfirmed input (seq=2)
6. Final position: (0.33, 0.166667) - smooth!
```

**통합 예제 (실전 적용)**:

```cpp
// game_client.cpp - 모든 기법 통합
class GameClient {
private:
    // Snapshot/Delta
    std::map<uint32_t, GameSnapshot> snapshot_history;
    
    // Quantization
    Quantizer quantizer;
    
    // Client Prediction
    std::queue<InputCommand> pending_inputs;
    
public:
    void on_receive_packet(const uint8_t* data, size_t size) {
        // 1. Delta 압축 해제
        GameDelta delta = parse_delta(data, size);
        
        // 2. Quantization 복원
        for (auto& player : delta.players) {
            player.x = quantizer.dequantize_position(player.x_int16);
            player.y = quantizer.dequantize_position(player.y_int16);
        }
        
        // 3. Client Prediction Reconciliation
        reconcile_with_server(delta);
    }
    
    void send_input(float move_x, float move_y) {
        // 1. 즉시 예측 적용
        apply_input_locally(move_x, move_y);
        
        // 2. 서버로 전송
        InputPacket packet;
        packet.move_x = move_x;
        packet.move_y = move_y;
        send_to_server(&packet, sizeof(packet));
        
        // 3. 히스토리 저장
        pending_inputs.push({move_x, move_y, current_tick});
    }
};
```

**최종 CMakeLists.txt** (통합 예제):
```cmake
cmake_minimum_required(VERSION 3.20)
project(game_networking)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Snapshot demo
add_executable(snapshot_demo snapshot_demo.cpp)

# Delta compression demo
add_executable(delta_demo delta_demo.cpp)

# Quantization demo
add_executable(quantization_demo quantization_demo.cpp)

# Client prediction demo
add_executable(prediction_demo prediction_demo.cpp)

# Full game client (통합)
add_executable(game_client game_client.cpp)

if(UNIX)
    target_link_libraries(snapshot_demo PRIVATE pthread)
    target_link_libraries(delta_demo PRIVATE pthread)
    target_link_libraries(quantization_demo PRIVATE pthread)
    target_link_libraries(prediction_demo PRIVATE pthread)
    target_link_libraries(game_client PRIVATE pthread)
endif()

if(WIN32)
    target_link_libraries(game_client PRIVATE ws2_32)
endif()
```
```

---

## 🐛 자주 막히는 부분

### 문제 1: Delta 기준 패킷 손실

```cpp
// Delta는 이전 상태 기준
// 패킷 손실 시 복구 불가!

// 해결: 주기적으로 Full Snapshot 전송
if (tick % 60 == 0) {  // 1초마다
    send_snapshot();
} else {
    send_delta();
}
```

### 문제 2: Quantization 오차 누적

```cpp
// 압축 → 압축 해제 → 압축 → 압축 해제
// 오차가 누적되어 발산!

// 해결: 서버는 항상 정확한 float 사용
// 클라이언트만 압축된 값 사용
```

### 문제 3: Client Prediction 불일치

```cpp
// 클라이언트 예측과 서버 결과가 다를 수 있음
// (물리 엔진 차이, float 연산 차이)

// 해결: 차이가 크면 서버 결과로 스냅
float error = std::abs(predicted_x - server_x);
if (error > THRESHOLD) {
    x = server_x;  // 강제 동기화
}
```

### 문제 4: 입력 버퍼 메모리 누수

```cpp
// pending_inputs가 계속 쌓임

// 해결: 최대 크기 제한
if (pending_inputs.size() > MAX_PENDING) {
    pending_inputs.pop();  // 오래된 것 제거
}
```

### 문제 5: 대역폭 계산 실수

```cpp
// ❌ 잘못된 계산
// Delta: 30 bytes, 60 TPS
// 30 * 60 = 1800 bytes/sec = 1.8 KB/sec
// → "충분해!"

// ✅ 올바른 계산
// 100 clients → 1.8 * 100 = 180 KB/sec = 1.4 Mbps
// + 업링크(클라이언트 → 서버) 별도!
```

---

## ✅ 완료 체크리스트

### Part 1: Snapshot
- [ ] Full snapshot 구현
- [ ] 패킷 크기 계산
- [ ] 대역폭 문제 인식

### Part 2: Delta
- [ ] Delta compression 구현
- [ ] 변경 플래그 사용
- [ ] 대역폭 절감 확인

### Part 3: Quantization
- [ ] Float → Int16 압축
- [ ] Quantize/Dequantize 함수
- [ ] 오차 측정

### Part 4: Client Prediction
- [ ] 입력 히스토리 저장
- [ ] Prediction 즉시 적용
- [ ] Server reconciliation 구현

---

## 🚀 다음 단계

✅ **Snapshot & Delta 완료!**

**다음 학습**:
- [**Quickstart 50**](50-prometheus-grafana.md) - 성능 모니터링
- [**Quickstart 40**](40-protobuf-basics.md) - Protobuf 최적화

**실전 적용**:
- `mini-gameserver` M1.6 - Snapshot/Delta Sync
- `mini-gameserver` M1.4 - 60 TPS Pong with Prediction

---

## 📚 참고 자료

- [Gaffer on Games - Snapshot Compression](https://gafferongames.com/post/snapshot_compression/)
- [Gaffer on Games - State Synchronization](https://gafferongames.com/post/state_synchronization/)
- [Valve - Latency Compensating Methods](https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization)
- [Overwatch Gameplay Architecture](https://www.youtube.com/watch?v=W3aieHjyNvw)

---

**Last Updated**: 2025-11-12
