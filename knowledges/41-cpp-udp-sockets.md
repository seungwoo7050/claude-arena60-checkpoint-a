# Quickstart 41: C++ UDP Sockets

## 🎯 목표
- **UDP vs TCP**: 프로토콜 차이 이해
- **POSIX UDP Sockets**: `sendto()`, `recvfrom()`
- **Custom Reliability**: Sequence number, ACK, Retransmission
- **실전**: 게임 서버에서 UDP를 사용하는 이유

## 📋 사전준비
- [Quickstart 30](30-cpp-for-game-server.md) 완료 (TCP sockets)
- [Quickstart 32](32-cpp-game-loop.md) 완료 (Game loop)
- [Quickstart 40](40-protobuf-basics.md) 권장 (직렬화)

---

## 📡 Part 1: UDP vs TCP

### 1.1 프로토콜 비교

| 특성 | TCP | UDP |
|------|-----|-----|
| **연결** | Connection-oriented (3-way handshake) | Connectionless |
| **신뢰성** | 보장 (패킷 순서, 재전송) | 보장 안 함 |
| **속도** | 느림 (오버헤드 큼) | 빠름 (오버헤드 작음) |
| **패킷 손실** | 자동 재전송 | 손실 가능 |
| **순서** | 순서 보장 | 순서 보장 안 함 |
| **헤더 크기** | 20+ bytes | 8 bytes |
| **사용 예** | HTTP, FTP, Email | DNS, VoIP, **게임** |

### 1.2 왜 게임은 UDP를 사용하는가?

**TCP의 문제**:
```
1. Head-of-Line Blocking
   - 패킷 1이 손실되면, 패킷 2, 3도 대기
   - 게임에서는 최신 상태만 중요 (과거 패킷은 무용지물)

2. Congestion Control
   - 네트워크 혼잡 시 자동으로 속도 감소
   - 게임에서는 일정한 전송 속도 필요

3. 재전송 지연
   - 패킷 손실 시 재전송 대기
   - 게임에서는 100ms 지연도 치명적
```

**UDP의 장점**:
```
1. 낮은 지연 (Low Latency)
   - 패킷 손실 시 즉시 다음 패킷 전송
   - Head-of-Line Blocking 없음

2. 제어 가능
   - 직접 신뢰성 계층 구현 (Selective ACK)
   - 중요한 패킷만 재전송

3. 대역폭 효율
   - 작은 헤더 (8 bytes)
   - 불필요한 재전송 없음
```

---

## 🔌 Part 2: UDP Socket 기초

### 2.1 UDP 서버

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class UDPServer {
private:
    int sockfd;
    sockaddr_in server_addr;
    
public:
    UDPServer(int port) {
        // UDP 소켓 생성
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);  // SOCK_DGRAM = UDP
        if (sockfd < 0) {
            throw std::runtime_error("socket() failed");
        }
        
        // 주소 재사용
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // 서버 주소 설정
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        // 바인드
        if (bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sockfd);
            throw std::runtime_error("bind() failed");
        }
        
        std::cout << "UDP Server listening on port " << port << "\n";
    }
    
    ~UDPServer() {
        close(sockfd);
    }
    
    void run() {
        char buffer[1024];
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        while (true) {
            // 데이터 수신
            ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                                (sockaddr*)&client_addr, &client_len);
            
            if (n < 0) {
                std::cerr << "recvfrom() failed\n";
                continue;
            }
            
            buffer[n] = '\0';
            
            std::cout << "Received from " 
                      << inet_ntoa(client_addr.sin_addr) << ":" 
                      << ntohs(client_addr.sin_port)
                      << " - " << buffer << "\n";
            
            // Echo back
            sendto(sockfd, buffer, n, 0,
                  (sockaddr*)&client_addr, client_len);
        }
    }
};

int main() {
    try {
        UDPServer server(8080);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

### 2.2 UDP 클라이언트

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class UDPClient {
private:
    int sockfd;
    sockaddr_in server_addr;
    
public:
    UDPClient(const std::string& server_ip, int port) {
        // UDP 소켓 생성
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("socket() failed");
        }
        
        // 서버 주소 설정
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr) <= 0) {
            close(sockfd);
            throw std::runtime_error("Invalid server IP");
        }
    }
    
    ~UDPClient() {
        close(sockfd);
    }
    
    void send_message(const std::string& message) {
        // 데이터 전송
        ssize_t n = sendto(sockfd, message.c_str(), message.size(), 0,
                          (sockaddr*)&server_addr, sizeof(server_addr));
        
        if (n < 0) {
            std::cerr << "sendto() failed\n";
            return;
        }
        
        std::cout << "Sent: " << message << "\n";
        
        // 응답 수신
        char buffer[1024];
        sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                    (sockaddr*)&from_addr, &from_len);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::cout << "Received: " << buffer << "\n";
        }
    }
};

int main() {
    try {
        UDPClient client("127.0.0.1", 8080);
        
        client.send_message("Hello UDP!");
        client.send_message("Packet 1");
        client.send_message("Packet 2");
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(udp_sockets)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# UDP Server
add_executable(udp_server udp_server.cpp)

# UDP Client
add_executable(udp_client udp_client.cpp)

# Linux/macOS: 네트워킹 라이브러리 자동 포함 (libc)
# Windows: ws2_32.lib 필요
if(WIN32)
    target_link_libraries(udp_server PRIVATE ws2_32)
    target_link_libraries(udp_client PRIVATE ws2_32)
endif()
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .

# 터미널 1: 서버 실행
./udp_server

# 터미널 2: 클라이언트 실행
./udp_client
```

**실행 결과**:

**터미널 1 (Server)**:
```
UDP Server listening on port 8080
Received from 127.0.0.1:54321 - Hello UDP!
Received from 127.0.0.1:54321 - Packet 1
Received from 127.0.0.1:54321 - Packet 2
```

**터미널 2 (Client)**:
```
Sent: Hello UDP!
Received: Hello UDP!
Sent: Packet 1
Received: Packet 1
Sent: Packet 2
Received: Packet 2
```

**주요 차이점**:
```cpp
// TCP: connect() 필요
connect(sockfd, ...);
send(sockfd, buffer, size, 0);
recv(sockfd, buffer, size, 0);

// UDP: sendto/recvfrom에 주소 명시
sendto(sockfd, buffer, size, 0, &server_addr, sizeof(server_addr));
recvfrom(sockfd, buffer, size, 0, &client_addr, &client_len);
```

---

## 🔄 Part 3: Custom Reliability

### 3.1 패킷 구조 (Sequence Number)

```cpp
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)  // 패킹 (정렬 없음)
struct Packet {
    uint32_t sequence;     // 시퀀스 번호
    uint16_t payload_size; // 페이로드 크기
    char payload[1024];    // 실제 데이터
    
    void serialize(char* buffer) const {
        std::memcpy(buffer, this, sizeof(Packet));
    }
    
    void deserialize(const char* buffer) {
        std::memcpy(this, buffer, sizeof(Packet));
    }
};
#pragma pack(pop)
```

### 3.2 신뢰성 있는 UDP 전송

```cpp
#include <iostream>
#include <queue>
#include <chrono>
#include <map>

using namespace std::chrono;

class ReliableUDPSender {
private:
    int sockfd;
    sockaddr_in server_addr;
    
    uint32_t next_seq = 0;
    
    // 재전송 큐
    struct PendingPacket {
        Packet packet;
        steady_clock::time_point sent_time;
        int retry_count;
    };
    
    std::map<uint32_t, PendingPacket> pending_acks;
    
    static constexpr auto TIMEOUT = milliseconds(100);  // 100ms 타임아웃
    static constexpr int MAX_RETRIES = 3;
    
public:
    ReliableUDPSender(const std::string& server_ip, int port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        
        std::memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
    }
    
    void send_reliable(const std::string& message) {
        Packet packet;
        packet.sequence = next_seq++;
        packet.payload_size = message.size();
        std::memcpy(packet.payload, message.c_str(), message.size());
        
        // 전송
        send_packet(packet);
        
        // 재전송 큐에 추가
        PendingPacket pending;
        pending.packet = packet;
        pending.sent_time = steady_clock::now();
        pending.retry_count = 0;
        
        pending_acks[packet.sequence] = pending;
        
        std::cout << "Sent packet seq=" << packet.sequence << "\n";
    }
    
    void process_ack(uint32_t ack_seq) {
        auto it = pending_acks.find(ack_seq);
        if (it != pending_acks.end()) {
            std::cout << "ACK received for seq=" << ack_seq << "\n";
            pending_acks.erase(it);
        }
    }
    
    void check_timeouts() {
        auto now = steady_clock::now();
        
        for (auto& [seq, pending] : pending_acks) {
            auto elapsed = duration_cast<milliseconds>(now - pending.sent_time);
            
            if (elapsed > TIMEOUT) {
                if (pending.retry_count < MAX_RETRIES) {
                    std::cout << "Timeout seq=" << seq 
                              << ", retry " << (pending.retry_count + 1) << "\n";
                    
                    send_packet(pending.packet);
                    pending.sent_time = now;
                    pending.retry_count++;
                } else {
                    std::cerr << "Max retries exceeded for seq=" << seq << "\n";
                    pending_acks.erase(seq);
                }
            }
        }
    }
    
private:
    void send_packet(const Packet& packet) {
        char buffer[sizeof(Packet)];
        packet.serialize(buffer);
        
        sendto(sockfd, buffer, sizeof(Packet), 0,
              (sockaddr*)&server_addr, sizeof(server_addr));
    }
};
```

### 3.3 ACK 패킷

```cpp
#pragma pack(push, 1)
struct AckPacket {
    uint32_t ack_sequence;  // 확인하는 시퀀스 번호
    
    void serialize(char* buffer) const {
        std::memcpy(buffer, this, sizeof(AckPacket));
    }
    
    void deserialize(const char* buffer) {
        std::memcpy(this, buffer, sizeof(AckPacket));
    }
};
#pragma pack(pop)

class ReliableUDPReceiver {
private:
    int sockfd;
    uint32_t last_received_seq = 0;
    
public:
    ReliableUDPReceiver(int port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        
        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        bind(sockfd, (sockaddr*)&addr, sizeof(addr));
    }
    
    void receive() {
        char buffer[sizeof(Packet)];
        sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                            (sockaddr*)&from_addr, &from_len);
        
        if (n == sizeof(Packet)) {
            Packet packet;
            packet.deserialize(buffer);
            
            std::cout << "Received seq=" << packet.sequence << "\n";
            
            // ACK 전송
            send_ack(packet.sequence, from_addr);
            
            // 순서대로 처리
            if (packet.sequence == last_received_seq + 1) {
                process_packet(packet);
                last_received_seq = packet.sequence;
            } else if (packet.sequence <= last_received_seq) {
                std::cout << "Duplicate packet seq=" << packet.sequence << "\n";
            } else {
                std::cout << "Out-of-order packet seq=" << packet.sequence << "\n";
            }
        }
    }
    
private:
    void send_ack(uint32_t seq, const sockaddr_in& to_addr) {
        AckPacket ack;
        ack.ack_sequence = seq;
        
        char buffer[sizeof(AckPacket)];
        ack.serialize(buffer);
        
        sendto(sockfd, buffer, sizeof(AckPacket), 0,
              (sockaddr*)&to_addr, sizeof(to_addr));
        
        std::cout << "Sent ACK for seq=" << seq << "\n";
    }
    
    void process_packet(const Packet& packet) {
        std::string message(packet.payload, packet.payload_size);
        std::cout << "Processed: " << message << "\n";
    }
};
```

---

## 🎮 Part 4: 게임 서버 UDP 패턴

### 4.1 게임 상태 패킷

```cpp
#pragma pack(push, 1)
struct GameStatePacket {
    uint32_t sequence;
    uint32_t tick;
    
    struct PlayerState {
        uint32_t player_id;
        float x, y;
        float vx, vy;
        uint16_t health;
    };
    
    uint8_t player_count;
    PlayerState players[4];  // 최대 4명
    
    size_t get_size() const {
        return sizeof(sequence) + sizeof(tick) + sizeof(player_count) +
               sizeof(PlayerState) * player_count;
    }
};
#pragma pack(pop)

class GameServer {
private:
    int sockfd;
    uint32_t current_tick = 0;
    uint32_t next_seq = 0;
    
public:
    void broadcast_state(const std::vector<sockaddr_in>& clients) {
        GameStatePacket packet;
        packet.sequence = next_seq++;
        packet.tick = current_tick;
        packet.player_count = 2;
        
        // 플레이어 1
        packet.players[0].player_id = 1;
        packet.players[0].x = 10.0f;
        packet.players[0].y = 20.0f;
        packet.players[0].vx = 1.0f;
        packet.players[0].vy = 0.0f;
        packet.players[0].health = 100;
        
        // 플레이어 2
        packet.players[1].player_id = 2;
        packet.players[1].x = 50.0f;
        packet.players[1].y = 30.0f;
        packet.players[1].vx = -0.5f;
        packet.players[1].vy = 0.5f;
        packet.players[1].health = 80;
        
        // 모든 클라이언트에게 전송
        char buffer[sizeof(GameStatePacket)];
        std::memcpy(buffer, &packet, packet.get_size());
        
        for (const auto& client : clients) {
            sendto(sockfd, buffer, packet.get_size(), 0,
                  (sockaddr*)&client, sizeof(client));
        }
        
        current_tick++;
    }
};
```

### 4.2 클라이언트 입력 패킷

```cpp
#pragma pack(push, 1)
struct InputPacket {
    uint32_t sequence;
    uint32_t client_tick;  // 클라이언트의 현재 틱
    
    // 입력
    float move_x;  // -1.0 ~ 1.0
    float move_y;
    uint8_t buttons;  // 비트마스크 (shoot, jump 등)
};
#pragma pack(pop)

class GameClient {
private:
    int sockfd;
    sockaddr_in server_addr;
    uint32_t next_seq = 0;
    uint32_t client_tick = 0;
    
public:
    void send_input(float move_x, float move_y, uint8_t buttons) {
        InputPacket packet;
        packet.sequence = next_seq++;
        packet.client_tick = client_tick;
        packet.move_x = move_x;
        packet.move_y = move_y;
        packet.buttons = buttons;
        
        sendto(sockfd, &packet, sizeof(packet), 0,
              (sockaddr*)&server_addr, sizeof(server_addr));
        
        client_tick++;
    }
};
```

---

## 🐛 자주 막히는 부분

### 문제 1: UDP 패킷 손실

```cpp
// UDP는 패킷 손실 보장 안 함
// 해결: 중요한 패킷만 재전송

// ✅ 중요도 플래그
enum PacketPriority {
    UNRELIABLE,    // 위치 업데이트 (손실 OK)
    RELIABLE       // 점수, 아이템 획득 (재전송 필요)
};

struct Packet {
    uint32_t sequence;
    PacketPriority priority;
    // ...
};
```

### 문제 2: 패킷 순서 뒤바뀜

```cpp
// UDP는 순서 보장 안 함
// 해결: Sequence number로 순서 확인

void on_receive(const Packet& packet) {
    if (packet.sequence <= last_received_seq) {
        // 오래된 패킷 무시
        return;
    }
    
    // 처리
    last_received_seq = packet.sequence;
}
```

### 문제 3: 패킷 크기 제한

```cpp
// UDP 최대 크기: 65535 bytes
// 하지만 MTU (Maximum Transmission Unit) 고려 필요

// ❌ 큰 패킷 전송 (단편화 발생)
char buffer[10000];  // MTU 초과!

// ✅ MTU 이하 (1200~1400 bytes 권장)
char buffer[1200];
```

### 문제 4: Firewall/NAT 문제

```cpp
// UDP는 연결 개념 없음 → Firewall 통과 어려움
// 해결: NAT hole punching, STUN/TURN 서버

// 또는 초기 연결은 TCP로 하고,
// 이후 UDP로 전환
```

### 문제 5: 재전송 폭풍 (Retransmission Storm)

```cpp
// ❌ 모든 패킷 재전송
for (auto& packet : pending) {
    if (timeout) {
        resend(packet);  // 네트워크 혼잡!
    }
}

// ✅ Exponential backoff
void resend(Packet& packet) {
    packet.timeout *= 2;  // 100ms → 200ms → 400ms
    if (packet.retry_count < MAX_RETRIES) {
        send(packet);
    }
}
```

---

## ✅ 완료 체크리스트

### Part 1: UDP vs TCP
- [ ] TCP와 UDP 차이 이해
- [ ] 게임에서 UDP를 사용하는 이유 이해
- [ ] Head-of-Line Blocking 개념

### Part 2: UDP Socket
- [ ] UDP 서버 구현
- [ ] UDP 클라이언트 구현
- [ ] `sendto()`, `recvfrom()` 사용

### Part 3: Custom Reliability
- [ ] Sequence number 구현
- [ ] ACK 패킷 전송
- [ ] 재전송 로직 구현

### Part 4: 게임 패턴

**완전한 예제 (reliable_udp.cpp)**:
```cpp
#include <iostream>
#include <map>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>

using namespace std::chrono;

#pragma pack(push, 1)
struct Packet {
    uint32_t sequence;
    uint16_t payload_size;
    char payload[1024];
    
    void set_payload(const std::string& msg) {
        payload_size = msg.size();
        std::memcpy(payload, msg.c_str(), msg.size());
    }
    
    std::string get_payload() const {
        return std::string(payload, payload_size);
    }
};

struct AckPacket {
    uint32_t ack_sequence;
};
#pragma pack(pop)

int main() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    
    bind(sockfd, (sockaddr*)&addr, sizeof(addr));
    
    std::cout << "Reliable UDP Server listening on port 8080\n";
    
    uint32_t last_received = 0;
    
    while (true) {
        char buffer[sizeof(Packet)];
        sockaddr_in from;
        socklen_t from_len = sizeof(from);
        
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                            (sockaddr*)&from, &from_len);
        
        if (n == sizeof(Packet)) {
            Packet* packet = reinterpret_cast<Packet*>(buffer);
            
            std::cout << "Received seq=" << packet->sequence 
                      << ": " << packet->get_payload() << "\n";
            
            // ACK 전송
            AckPacket ack;
            ack.ack_sequence = packet->sequence;
            sendto(sockfd, &ack, sizeof(ack), 0, 
                  (sockaddr*)&from, from_len);
            
            std::cout << "Sent ACK for seq=" << packet->sequence << "\n";
            
            last_received = packet->sequence;
        }
    }
    
    close(sockfd);
    return 0;
}
```

**CMakeLists.txt** (reliable_udp):
```cmake
cmake_minimum_required(VERSION 3.20)
project(reliable_udp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(reliable_udp_server reliable_udp.cpp)
add_executable(reliable_udp_client reliable_udp_client.cpp)

if(UNIX)
    target_link_libraries(reliable_udp_server PRIVATE pthread)
    target_link_libraries(reliable_udp_client PRIVATE pthread)
endif()

if(WIN32)
    target_link_libraries(reliable_udp_server PRIVATE ws2_32)
    target_link_libraries(reliable_udp_client PRIVATE ws2_32)
endif()
```

**실행 결과**:
```
Reliable UDP Server listening on port 8080
Received seq=0: Message 1
Sent ACK for seq=0
Received seq=1: Message 2
Sent ACK for seq=1
Received seq=2: Message 3
Sent ACK for seq=2
Timeout seq=3, retry 1
Received seq=3: Message 4
Sent ACK for seq=3
```

### Part 4: 게임 패턴 (계속)
- [ ] 게임 상태 패킷 설계
- [ ] 클라이언트 입력 패킷 전송
- [ ] Broadcast 구현

---

## 🚀 다음 단계

✅ **UDP Sockets 완료!**

**다음 학습**:
- [**Quickstart 42**](42-snapshot-delta-sync.md) - Snapshot/Delta 압축
- [**Quickstart 40**](40-protobuf-basics.md) - Protobuf로 직렬화 최적화

**실전 적용**:
- `mini-gameserver` M1.5 - UDP + Custom Reliability
- `mini-gameserver` M1.6 - Delta Compression

---

## 📚 참고 자료

- [Gaffer on Games - UDP vs TCP](https://gafferongames.com/post/udp_vs_tcp/)
- [Gaffer on Games - Reliability and Flow Control](https://gafferongames.com/post/reliability_and_flow_control/)
- [Valve - Source Multiplayer Networking](https://developer.valvesoftware.com/wiki/Source_Multiplayer_Networking)
- [POSIX Socket API](https://man7.org/linux/man-pages/man2/socket.2.html)

---

**Last Updated**: 2025-11-12
