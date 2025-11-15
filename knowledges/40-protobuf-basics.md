# Quickstart 12: Protocol Buffers 기초

## 🎯 목표
- **Protocol Buffers란**: 구조화된 데이터 직렬화
- **메시지 정의**: .proto 파일 작성
- **코드 생성**: protoc 컴파일러 사용
- **C++ 통합**: 메시지 읽기/쓰기
- **실전**: 게임 패킷 정의 및 전송

## 📋 사전준비
- [Quickstart 00](00-setup-linux-macos.md) 완료 (Protobuf 설치됨)
- [Quickstart 04](04-cpp-for-game-server.md) 완료 (C++ 기초)
- [Quickstart 10](10-cmake-build-system.md) 완료 (CMake)

---

## 📦 Part 1: Protocol Buffers란?

### 1.1 데이터 직렬화 비교

```cpp
// JSON (사람이 읽기 쉬움, 크기 큼)
{
    "id": 123,
    "name": "Alice",
    "position": {"x": 10.5, "y": 20.3}
}
// 크기: ~70 bytes

// Protobuf (바이너리, 크기 작음, 빠름)
// 크기: ~15 bytes (4배 이상 효율적!)
// 속도: JSON의 20-100배
```

**장점**:
- ✅ **작은 크기** - 네트워크 대역폭 절약
- ✅ **빠른 속도** - 파싱 오버헤드 적음
- ✅ **타입 안전** - 컴파일 시 에러 검증
- ✅ **하위 호환** - 필드 추가/삭제 가능

**단점**:
- ❌ 사람이 읽을 수 없음 (바이너리)
- ❌ 디버깅 어려움 (별도 도구 필요)

### 1.2 언제 사용하나?

```
✅ 사용:
- 게임 서버 ↔ 클라이언트 통신
- 마이크로서비스 간 통신
- 로그 저장 (디스크 공간 절약)
- 실시간 데이터 전송

❌ 비추천:
- REST API (JSON이 더 적합)
- 사람이 직접 읽어야 하는 설정 파일
- 작은 프로토타입 (오버헤드)
```

---

## 📝 Part 2: .proto 파일 작성

### 2.1 기본 메시지

```protobuf
// player.proto
syntax = "proto3";

package game;

message Player {
    int32 id = 1;           // 필드 번호 (변경 금지!)
    string name = 2;
    float x = 3;
    float y = 4;
}
```

**필드 번호 규칙**:
- `1-15`: 1바이트 인코딩 (자주 쓰는 필드)
- `16-2047`: 2바이트 인코딩
- 한 번 할당하면 **절대 변경 금지** (하위 호환 깨짐)

### 2.2 데이터 타입

```protobuf
syntax = "proto3";

message DataTypes {
    // 정수
    int32 small_int = 1;      // -2^31 ~ 2^31-1
    int64 big_int = 2;        // -2^63 ~ 2^63-1
    uint32 unsigned_int = 3;  // 0 ~ 2^32-1
    
    // 실수
    float x = 4;              // 32비트
    double precise_x = 5;     // 64비트
    
    // 문자열
    string name = 6;          // UTF-8
    bytes raw_data = 7;       // 임의 바이너리
    
    // 불리언
    bool is_active = 8;       // true/false
}
```

### 2.3 중첩 메시지

```protobuf
syntax = "proto3";

message Position {
    float x = 1;
    float y = 2;
    float z = 3;
}

message Player {
    int32 id = 1;
    string name = 2;
    Position position = 3;    // 중첩 메시지
}
```

### 2.4 반복 필드 (배열)

```protobuf
syntax = "proto3";

message GameState {
    repeated Player players = 1;  // Player 배열
    repeated int32 scores = 2;     // int 배열
}

message Player {
    int32 id = 1;
    string name = 2;
}
```

### 2.5 열거형 (Enum)

```protobuf
syntax = "proto3";

enum Direction {
    UNKNOWN = 0;  // 첫 번째는 0이어야 함
    UP = 1;
    DOWN = 2;
    LEFT = 3;
    RIGHT = 4;
}

message PlayerInput {
    int32 player_id = 1;
    Direction direction = 2;
}
```

### 2.6 Oneof (여러 타입 중 하나)

```protobuf
syntax = "proto3";

message Packet {
    oneof payload {
        PlayerJoin join = 1;
        PlayerMove move = 2;
        PlayerAttack attack = 3;
    }
}

message PlayerJoin {
    string name = 1;
}

message PlayerMove {
    float x = 1;
    float y = 2;
}

message PlayerAttack {
    int32 target_id = 1;
}
```

---

## 🔨 Part 3: 코드 생성 및 사용

### 3.1 protoc 컴파일

```bash
# .proto 파일 작성
cat > player.proto << 'EOF'
syntax = "proto3";

package game;

message Player {
    int32 id = 1;
    string name = 2;
    float x = 3;
    float y = 4;
}
EOF

# C++ 코드 생성
protoc --cpp_out=. player.proto

# 생성된 파일:
# player.pb.h  (헤더)
# player.pb.cc (구현)
```

### 3.2 C++에서 사용

```cpp
// main.cpp
#include <iostream>
#include <fstream>
#include "player.pb.h"

int main() {
    // 메시지 생성
    game::Player player;
    player.set_id(1);
    player.set_name("Alice");
    player.set_x(10.5f);
    player.set_y(20.3f);
    
    // 메시지 출력
    std::cout << "Player ID: " << player.id() << std::endl;
    std::cout << "Name: " << player.name() << std::endl;
    std::cout << "Position: (" << player.x() << ", " << player.y() << ")" << std::endl;
    
    // 직렬화 (바이너리로 변환)
    std::string serialized;
    if (player.SerializeToString(&serialized)) {
        std::cout << "Serialized size: " << serialized.size() << " bytes" << std::endl;
    }
    
    // 파일에 저장
    std::ofstream output("player.bin", std::ios::binary);
    player.SerializeToOstream(&output);
    output.close();
    
    // 파일에서 읽기
    game::Player loaded_player;
    std::ifstream input("player.bin", std::ios::binary);
    if (loaded_player.ParseFromIstream(&input)) {
        std::cout << "Loaded: " << loaded_player.name() << std::endl;
    }
    input.close();
    
    return 0;
}
```

### 3.3 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(ProtobufExample)

set(CMAKE_CXX_STANDARD 17)

# Protobuf 찾기
find_package(Protobuf REQUIRED)

# .proto 파일 컴파일
set(PROTO_FILES
    player.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# 실행 파일
add_executable(proto_example
    main.cpp
    ${PROTO_SRCS}
)

# 헤더 경로 (생성된 .pb.h)
target_include_directories(proto_example PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# 라이브러리 링크
target_link_libraries(proto_example
    ${Protobuf_LIBRARIES}
)
```

**빌드**:
```bash
cmake -B build
cmake --build build
./build/proto_example
```

---

## 🎮 Part 4: 게임 서버 실전 예제

### 4.1 게임 패킷 정의

```protobuf
// game_packets.proto
syntax = "proto3";

package game;

// 플레이어 위치
message Position {
    float x = 1;
    float y = 2;
}

// 패킷 타입
enum PacketType {
    UNKNOWN = 0;
    JOIN = 1;
    MOVE = 2;
    ATTACK = 3;
    LEAVE = 4;
}

// 조인 요청
message JoinRequest {
    string player_name = 1;
}

// 조인 응답
message JoinResponse {
    int32 player_id = 1;
    bool success = 2;
    string message = 3;
}

// 이동 명령
message MoveCommand {
    int32 player_id = 1;
    Position target_position = 2;
    float speed = 3;
}

// 게임 상태 (서버 → 클라이언트 브로드캐스트)
message GameState {
    int64 timestamp = 1;
    repeated PlayerState players = 2;
}

message PlayerState {
    int32 player_id = 1;
    string name = 2;
    Position position = 3;
    int32 health = 4;
}

// 통합 패킷 (모든 메시지 포함)
message Packet {
    PacketType type = 1;
    
    oneof payload {
        JoinRequest join_request = 2;
        JoinResponse join_response = 3;
        MoveCommand move_command = 4;
        GameState game_state = 5;
    }
}
```

### 4.2 서버에서 패킷 처리

```cpp
#include <iostream>
#include <string>
#include "game_packets.pb.h"

class GameServer {
public:
    void handle_packet(const std::string& data) {
        game::Packet packet;
        
        // 바이너리 데이터 → Protobuf 메시지
        if (!packet.ParseFromString(data)) {
            std::cerr << "Failed to parse packet" << std::endl;
            return;
        }
        
        // 패킷 타입별 처리
        switch (packet.type()) {
            case game::PacketType::JOIN:
                handle_join(packet.join_request());
                break;
            
            case game::PacketType::MOVE:
                handle_move(packet.move_command());
                break;
            
            default:
                std::cerr << "Unknown packet type: " << packet.type() << std::endl;
        }
    }

private:
    void handle_join(const game::JoinRequest& request) {
        std::cout << "Player joined: " << request.player_name() << std::endl;
        
        // 응답 생성
        game::Packet response;
        response.set_type(game::PacketType::JOIN);
        
        auto* join_resp = response.mutable_join_response();
        join_resp->set_player_id(next_player_id_++);
        join_resp->set_success(true);
        join_resp->set_message("Welcome!");
        
        // 직렬화 후 전송
        std::string serialized;
        response.SerializeToString(&serialized);
        send_to_client(serialized);
    }
    
    void handle_move(const game::MoveCommand& cmd) {
        std::cout << "Player " << cmd.player_id() 
                  << " moving to (" 
                  << cmd.target_position().x() << ", "
                  << cmd.target_position().y() << ")"
                  << std::endl;
        
        // 게임 상태 업데이트 후 브로드캐스트
        broadcast_game_state();
    }
    
    void broadcast_game_state() {
        game::Packet packet;
        packet.set_type(game::PacketType::UNKNOWN);  // GameState 전용
        
        auto* state = packet.mutable_game_state();
        state->set_timestamp(get_timestamp());
        
        // 모든 플레이어 추가
        for (const auto& player : players_) {
            auto* player_state = state->add_players();
            player_state->set_player_id(player.id);
            player_state->set_name(player.name);
            player_state->mutable_position()->set_x(player.x);
            player_state->mutable_position()->set_y(player.y);
            player_state->set_health(player.health);
        }
        
        // 직렬화 후 모든 클라이언트에 전송
        std::string serialized;
        packet.SerializeToString(&serialized);
        broadcast_to_all(serialized);
    }
    
    void send_to_client(const std::string& data) {
        // WebSocket 또는 TCP로 전송
        std::cout << "Sending " << data.size() << " bytes to client" << std::endl;
    }
    
    void broadcast_to_all(const std::string& data) {
        std::cout << "Broadcasting " << data.size() << " bytes" << std::endl;
    }
    
    int64_t get_timestamp() {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }
    
    struct Player {
        int id;
        std::string name;
        float x, y;
        int health;
    };
    
    std::vector<Player> players_;
    int next_player_id_ = 1;
};

int main() {
    GameServer server;
    
    // 클라이언트 조인 패킷 시뮬레이션
    game::Packet join_packet;
    join_packet.set_type(game::PacketType::JOIN);
    join_packet.mutable_join_request()->set_player_name("Alice");
    
    std::string serialized;
    join_packet.SerializeToString(&serialized);
    
    server.handle_packet(serialized);
    
    return 0;
}
```

### 4.3 클라이언트에서 패킷 전송

```cpp
#include <iostream>
#include "game_packets.pb.h"

class GameClient {
public:
    void join_game(const std::string& name) {
        game::Packet packet;
        packet.set_type(game::PacketType::JOIN);
        packet.mutable_join_request()->set_player_name(name);
        
        send(packet);
    }
    
    void move_to(float x, float y) {
        game::Packet packet;
        packet.set_type(game::PacketType::MOVE);
        
        auto* move_cmd = packet.mutable_move_command();
        move_cmd->set_player_id(player_id_);
        move_cmd->mutable_target_position()->set_x(x);
        move_cmd->mutable_target_position()->set_y(y);
        move_cmd->set_speed(5.0f);
        
        send(packet);
    }
    
    void handle_server_packet(const std::string& data) {
        game::Packet packet;
        if (!packet.ParseFromString(data)) {
            return;
        }
        
        if (packet.has_join_response()) {
            auto& resp = packet.join_response();
            if (resp.success()) {
                player_id_ = resp.player_id();
                std::cout << "Joined! Player ID: " << player_id_ << std::endl;
            }
        } else if (packet.has_game_state()) {
            auto& state = packet.game_state();
            std::cout << "Game state update: " << state.players_size() 
                      << " players" << std::endl;
        }
    }

private:
    void send(const game::Packet& packet) {
        std::string serialized;
        packet.SerializeToString(&serialized);
        
        // WebSocket 또는 TCP로 전송
        std::cout << "Sending packet (type=" << packet.type() 
                  << ", size=" << serialized.size() << " bytes)" << std::endl;
    }
    
    int player_id_ = -1;
};

int main() {
    GameClient client;
    
    client.join_game("Alice");
    client.move_to(10.5f, 20.3f);
    
    return 0;
}
```

---

## 🐛 자주 막히는 부분

### 문제 1: "protoc: command not found"
```bash
# 해결:
brew install protobuf  # macOS
sudo apt install protobuf-compiler  # Linux

protoc --version
```

### 문제 2: "Cannot find protobuf library"
```cmake
# CMakeLists.txt에서
find_package(Protobuf REQUIRED)

# 실패 시 직접 경로 지정
set(Protobuf_INCLUDE_DIR "/opt/homebrew/include")
set(Protobuf_LIBRARY "/opt/homebrew/lib/libprotobuf.dylib")
```

### 문제 3: 필드 번호 중복
```protobuf
# ❌ 잘못된 예
message Player {
    int32 id = 1;
    string name = 1;  // 에러: 중복!
}

# ✅ 올바른 예
message Player {
    int32 id = 1;
    string name = 2;  // 고유 번호
}
```

### 문제 4: ParseFromString 실패
```cpp
// 원인: 잘못된 바이너리 데이터

game::Player player;
if (!player.ParseFromString(data)) {
    std::cerr << "Parse failed! Invalid protobuf data" << std::endl;
    // 데이터 검증 필요
}
```

### 문제 5: 하위 호환성 깨짐
```protobuf
# 버전 1
message Player {
    int32 id = 1;
    string name = 2;
}

# 버전 2 (필드 추가 - ✅ OK)
message Player {
    int32 id = 1;
    string name = 2;
    int32 level = 3;  // 새 필드 (하위 호환)
}

# 버전 3 (필드 번호 변경 - ❌ 절대 금지!)
message Player {
    int32 id = 1;
    int32 level = 2;  // name의 번호를 바꿈 → 깨짐!
    string name = 3;
}
```

**필드 삭제 방법**:
```protobuf
message Player {
    int32 id = 1;
    reserved 2;  // name 필드 삭제, 번호 예약
    int32 level = 3;
}
```

---

## ✅ 완료 체크리스트

### 기본
- [ ] .proto 파일 작성
- [ ] protoc 컴파일 성공
- [ ] C++ 코드 생성 확인 (.pb.h, .pb.cc)

### 메시지 사용
- [ ] 메시지 생성 및 필드 설정
- [ ] SerializeToString (직렬화)
- [ ] ParseFromString (역직렬화)
- [ ] 파일 저장/로드 (SerializeToOstream)

### 고급 기능
- [ ] 중첩 메시지 사용
- [ ] repeated 필드 (배열)
- [ ] enum 정의 및 사용
- [ ] oneof 사용 (여러 타입 중 하나)

### 게임 서버
- [ ] 게임 패킷 정의 (JOIN, MOVE, GameState)
- [ ] 서버에서 패킷 처리
- [ ] 클라이언트에서 패킷 전송
- [ ] 바이너리 크기 확인 (JSON 대비)

---

## 🚀 다음 단계

✅ Protocol Buffers 기초 완료!

**다음 학습**:
- **Database**: [Quickstart 13: PostgreSQL & Redis](13-postgresql-redis-docker.md) - 데이터 영속화

**실전 적용**:
- `mini-gameserver` M1.5 - UDP + Protobuf로 Pong 게임
- M1.6 - 상태 동기화 최적화

---

## 📚 참고 자료

- [Protocol Buffers 공식 문서](https://protobuf.dev/)
- [Proto3 Language Guide](https://protobuf.dev/programming-guides/proto3/)
- [C++ Generated Code Guide](https://protobuf.dev/reference/cpp/cpp-generated/)
- [Protobuf Best Practices](https://protobuf.dev/programming-guides/dos-donts/)

---

**Last Updated**: 2025-11-12
