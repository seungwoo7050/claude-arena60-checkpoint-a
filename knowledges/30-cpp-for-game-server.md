# Quickstart 04: C++로 게임 서버 만들기

## 🎯 목표
- **C++ 핵심**: 포인터, 참조, RAII 패턴 마스터
- **메모리 관리**: 스마트 포인터 (unique_ptr, shared_ptr) 실전
- **멀티스레딩**: std::thread, mutex, lock_guard 사용
- **Socket 프로그래밍**: POSIX 소켓으로 TCP 서버 구현
- **CMake**: 빌드 시스템 설정 및 사용

## 📋 사전준비
- [Quickstart 00](00-setup-linux-macos.md) 완료 (GCC/Clang, CMake 설치됨)
- [Quickstart 02](02-vscode-clion-setup.md) 완료 (CLion 디버깅 가능)
- C 기초 문법 이해 (선택, 없어도 진행 가능)

---

## 🔧 Part 1: C++ 핵심 개념

### 1.1 포인터 vs 참조

```cpp
#include <iostream>
using namespace std;

void demonstratePointerAndReference() {
    int value = 42;
    
    // 포인터: 메모리 주소 저장
    int* ptr = &value;
    cout << "Value: " << value << endl;           // 42
    cout << "Pointer: " << ptr << endl;           // 0x7fff... (주소)
    cout << "Dereferenced: " << *ptr << endl;     // 42 (값)
    
    // 포인터로 값 변경
    *ptr = 100;
    cout << "After change: " << value << endl;    // 100
    
    // 참조: 별명 (alias)
    int& ref = value;
    ref = 200;
    cout << "After ref change: " << value << endl; // 200
    
    // 포인터는 nullptr 가능, 참조는 불가
    int* nullable = nullptr;
    // int& ref2 = nullptr;  // ❌ 컴파일 에러!
}

// 함수 인자: 값 vs 포인터 vs 참조
void byValue(int x) {
    x = 100;  // 원본 변경 안됨
}

void byPointer(int* x) {
    *x = 100;  // 원본 변경됨
}

void byReference(int& x) {
    x = 100;  // 원본 변경됨 (포인터보다 깔끔)
}

int main() {
    int num = 42;
    
    byValue(num);
    cout << num << endl;  // 42 (변경 안됨)
    
    byPointer(&num);
    cout << num << endl;  // 100
    
    byReference(num);
    cout << num << endl;  // 100
}
```

### 1.2 RAII (Resource Acquisition Is Initialization)

```cpp
#include <iostream>
#include <fstream>
#include <string>

// 나쁜 예: 수동 메모리 관리
void badExample() {
    int* data = new int[1000];
    
    // 작업 중 예외 발생 시 메모리 누수!
    if (someCondition) {
        // delete[] data;  // 잊어버림!
        return;
    }
    
    delete[] data;  // 정상 경로에서만 해제
}

// 좋은 예: RAII 패턴
class FileHandler {
private:
    std::ofstream file;

public:
    FileHandler(const std::string& filename) {
        file.open(filename);
        std::cout << "File opened: " << filename << std::endl;
    }
    
    ~FileHandler() {
        if (file.is_open()) {
            file.close();
            std::cout << "File closed automatically" << std::endl;
        }
    }
    
    void write(const std::string& data) {
        file << data << std::endl;
    }
};

void goodExample() {
    FileHandler handler("log.txt");
    handler.write("Server started");
    
    // 함수 종료 시 자동으로 소멸자 호출 → 파일 닫힘
    // 예외 발생해도 안전!
}

// 실전 예: Socket RAII
class Socket {
private:
    int sockfd;

public:
    Socket(int domain, int type, int protocol) {
        sockfd = socket(domain, type, protocol);
        if (sockfd < 0) {
            throw std::runtime_error("Socket creation failed");
        }
    }
    
    ~Socket() {
        if (sockfd >= 0) {
            close(sockfd);
            std::cout << "Socket closed" << std::endl;
        }
    }
    
    int getFd() const { return sockfd; }
    
    // 복사 방지 (소켓은 unique 리소스)
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
};
```

### 1.3 스마트 포인터

```cpp
#include <memory>
#include <iostream>
#include <vector>

class Player {
private:
    std::string name;
    int health;

public:
    Player(const std::string& n) : name(n), health(100) {
        std::cout << "Player " << name << " created" << std::endl;
    }
    
    ~Player() {
        std::cout << "Player " << name << " destroyed" << std::endl;
    }
    
    void takeDamage(int damage) {
        health -= damage;
        std::cout << name << " HP: " << health << std::endl;
    }
};

void demonstrateSmartPointers() {
    // unique_ptr: 단독 소유 (복사 불가, 이동만 가능)
    {
        std::unique_ptr<Player> player1 = std::make_unique<Player>("Alice");
        player1->takeDamage(10);
        
        // std::unique_ptr<Player> player2 = player1;  // ❌ 컴파일 에러
        std::unique_ptr<Player> player2 = std::move(player1);  // ✅ 소유권 이전
        // player1은 이제 nullptr
        
    }  // player2 소멸 → Player 자동 삭제
    
    // shared_ptr: 공유 소유 (참조 카운팅)
    {
        std::shared_ptr<Player> player1 = std::make_shared<Player>("Bob");
        {
            std::shared_ptr<Player> player2 = player1;  // ✅ 복사 가능
            std::cout << "Ref count: " << player1.use_count() << std::endl;  // 2
            player2->takeDamage(20);
        }  // player2 소멸, 하지만 player1이 아직 소유 중
        
        std::cout << "Ref count: " << player1.use_count() << std::endl;  // 1
    }  // player1 소멸 → Player 자동 삭제
    
    // weak_ptr: 순환 참조 방지
    std::shared_ptr<Player> player = std::make_shared<Player>("Charlie");
    std::weak_ptr<Player> weakPlayer = player;
    
    if (auto locked = weakPlayer.lock()) {  // shared_ptr로 승격
        locked->takeDamage(5);
    }
}

// 실전 예: 게임 오브젝트 관리
class GameWorld {
private:
    std::vector<std::shared_ptr<Player>> players;

public:
    void addPlayer(const std::string& name) {
        players.push_back(std::make_shared<Player>(name));
    }
    
    void removePlayer(size_t index) {
        if (index < players.size()) {
            players.erase(players.begin() + index);
            // Player 자동 삭제 (다른 곳에서 참조 안하면)
        }
    }
    
    std::shared_ptr<Player> getPlayer(size_t index) {
        return players.at(index);
    }
};
```

### 1.4 이동 의미론 (Move Semantics)

```cpp
#include <iostream>
#include <vector>
#include <string>

class Buffer {
private:
    size_t size;
    char* data;

public:
    // 생성자
    Buffer(size_t s) : size(s), data(new char[s]) {
        std::cout << "Buffer created (" << size << " bytes)" << std::endl;
    }
    
    // 복사 생성자 (비용 높음)
    Buffer(const Buffer& other) : size(other.size), data(new char[other.size]) {
        std::copy(other.data, other.data + size, data);
        std::cout << "Buffer copied (expensive!)" << std::endl;
    }
    
    // 이동 생성자 (비용 낮음)
    Buffer(Buffer&& other) noexcept : size(other.size), data(other.data) {
        other.data = nullptr;  // 소유권 이전
        other.size = 0;
        std::cout << "Buffer moved (cheap!)" << std::endl;
    }
    
    // 소멸자
    ~Buffer() {
        delete[] data;
        std::cout << "Buffer destroyed" << std::endl;
    }
    
    size_t getSize() const { return size; }
};

Buffer createBuffer() {
    Buffer buf(1024);
    return buf;  // 이동 생성자 호출 (RVO로 최적화 가능)
}

int main() {
    Buffer buf1 = createBuffer();  // 이동 생성
    
    std::vector<Buffer> buffers;
    buffers.push_back(std::move(buf1));  // 명시적 이동
    
    // buf1은 이제 빈 상태 (사용 금지)
}
```

---

## 🧵 Part 2: 멀티스레딩

### 2.1 std::thread 기초

```cpp
#include <iostream>
#include <thread>
#include <chrono>

void printNumbers(int count, const std::string& prefix) {
    for (int i = 1; i <= count; ++i) {
        std::cout << prefix << i << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main() {
    // 스레드 생성 및 시작
    std::thread t1(printNumbers, 5, "Thread1: ");
    std::thread t2(printNumbers, 5, "Thread2: ");
    
    // 메인 스레드 작업
    std::cout << "Main thread working..." << std::endl;
    
    // 스레드 종료 대기
    t1.join();
    t2.join();
    
    std::cout << "All threads finished" << std::endl;
    return 0;
}

// 람다로 스레드 생성
void lambdaThreadExample() {
    int counter = 0;
    
    std::thread worker([&counter]() {
        for (int i = 0; i < 10; ++i) {
            ++counter;
        }
    });
    
    worker.join();
    std::cout << "Counter: " << counter << std::endl;
}
```

### 2.2 Mutex와 동기화

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <vector>

class ThreadSafeCounter {
private:
    int count;
    std::mutex mtx;

public:
    ThreadSafeCounter() : count(0) {}
    
    void increment() {
        std::lock_guard<std::mutex> lock(mtx);  // RAII 패턴
        ++count;
    }  // lock 자동 해제
    
    int getCount() {
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }
};

void testThreadSafety() {
    ThreadSafeCounter counter;
    std::vector<std::thread> threads;
    
    // 10개 스레드가 각각 1000번 증가
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&counter]() {
            for (int j = 0; j < 1000; ++j) {
                counter.increment();
            }
        });
    }
    
    // 모든 스레드 대기
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Final count: " << counter.getCount() << std::endl;  // 10000
}

// 데드락 주의!
void deadlockExample() {
    std::mutex mtx1, mtx2;
    
    std::thread t1([&]() {
        std::lock_guard<std::mutex> lock1(mtx1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock2(mtx2);  // 데드락!
    });
    
    std::thread t2([&]() {
        std::lock_guard<std::mutex> lock2(mtx2);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::lock_guard<std::mutex> lock1(mtx1);  // 데드락!
    });
    
    t1.join();
    t2.join();
}

// 데드락 해결: std::scoped_lock (C++17)
void deadlockSolution() {
    std::mutex mtx1, mtx2;
    
    std::thread t1([&]() {
        std::scoped_lock lock(mtx1, mtx2);  // 동시에 잠금
        // 작업...
    });
    
    std::thread t2([&]() {
        std::scoped_lock lock(mtx1, mtx2);  // 안전함
        // 작업...
    });
    
    t1.join();
    t2.join();
}
```

### 2.3 조건 변수 (Condition Variable)

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(std::move(value));
        }
        cv.notify_one();  // 대기 중인 스레드 깨우기
    }
    
    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        
        // 큐가 비어있으면 대기
        cv.wait(lock, [this]() { return !queue.empty(); });
        
        T value = std::move(queue.front());
        queue.pop();
        return value;
    }
    
    bool empty() {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }
};

void producerConsumerExample() {
    ThreadSafeQueue<int> queue;
    
    // Producer
    std::thread producer([&queue]() {
        for (int i = 1; i <= 10; ++i) {
            queue.push(i);
            std::cout << "Produced: " << i << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    // Consumer
    std::thread consumer([&queue]() {
        for (int i = 0; i < 10; ++i) {
            int value = queue.pop();
            std::cout << "Consumed: " << value << std::endl;
        }
    });
    
    producer.join();
    consumer.join();
}
```

---

## 🔌 Part 3: Socket 프로그래밍 (POSIX)

### 3.1 TCP Echo Server

```cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdexcept>

class EchoServer {
private:
    int server_fd;
    int port;

public:
    EchoServer(int p) : server_fd(-1), port(p) {}
    
    ~EchoServer() {
        if (server_fd >= 0) {
            close(server_fd);
        }
    }
    
    void start() {
        // 1. 소켓 생성
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            throw std::runtime_error("Socket creation failed");
        }
        
        // 2. 주소 재사용 옵션
        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error("setsockopt failed");
        }
        
        // 3. 주소 구조체 설정
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;  // 모든 인터페이스
        address.sin_port = htons(port);        // 호스트 → 네트워크 바이트 순서
        
        // 4. 바인딩
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            throw std::runtime_error("Bind failed");
        }
        
        // 5. 리스닝
        if (listen(server_fd, 5) < 0) {
            throw std::runtime_error("Listen failed");
        }
        
        std::cout << "Echo Server listening on port " << port << std::endl;
        
        // 6. 클라이언트 수락 루프
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            
            handleClient(client_fd);
        }
    }
    
    void handleClient(int client_fd) {
        char buffer[1024] = {0};
        
        // 데이터 수신
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::cout << "Received: " << buffer << std::endl;
            
            // Echo 응답
            write(client_fd, buffer, bytes_read);
        }
        
        close(client_fd);
    }
};

int main() {
    try {
        EchoServer server(8080);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### 3.2 멀티스레드 Echo Server

```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class ThreadedEchoServer {
private:
    int server_fd;
    int port;
    std::vector<std::thread> threads;

public:
    ThreadedEchoServer(int p) : server_fd(-1), port(p) {}
    
    ~ThreadedEchoServer() {
        if (server_fd >= 0) {
            close(server_fd);
        }
        
        // 모든 스레드 종료 대기
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    
    void start() {
        setupSocket();
        
        std::cout << "Threaded Echo Server on port " << port << std::endl;
        
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
            if (client_fd < 0) {
                continue;
            }
            
            // 새 스레드에서 클라이언트 처리
            threads.emplace_back(&ThreadedEchoServer::handleClient, this, client_fd);
            
            // Detach로 자동 정리 (또는 주기적으로 join)
            if (threads.size() > 100) {
                cleanupThreads();
            }
        }
    }

private:
    void setupSocket() {
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            throw std::runtime_error("Socket creation failed");
        }
        
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        
        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            throw std::runtime_error("Bind failed");
        }
        
        if (listen(server_fd, 5) < 0) {
            throw std::runtime_error("Listen failed");
        }
    }
    
    void handleClient(int client_fd) {
        char buffer[4096] = {0};
        
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::cout << "[Thread " << std::this_thread::get_id() << "] "
                      << "Received: " << buffer << std::endl;
            
            write(client_fd, buffer, bytes_read);
        }
        
        close(client_fd);
    }
    
    void cleanupThreads() {
        threads.erase(
            std::remove_if(threads.begin(), threads.end(),
                [](std::thread& t) {
                    if (t.joinable()) {
                        t.join();
                        return true;
                    }
                    return false;
                }),
            threads.end()
        );
    }
};

int main() {
    ThreadedEchoServer server(8080);
    server.start();
    return 0;
}
```

---

## 🏗️ Part 4: CMake 빌드 시스템

### 4.1 기본 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.15)
project(EchoServer VERSION 1.0.0 LANGUAGES CXX)

# C++17 사용
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 컴파일 옵션
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# 실행 파일 생성
add_executable(echo_server
    main.cpp
    echo_server.cpp
    echo_server.h
)

# 라이브러리 링크 (pthread)
target_link_libraries(echo_server pthread)
```

### 4.2 여러 타겟 관리

```cmake
cmake_minimum_required(VERSION 3.15)
project(GameServer)

set(CMAKE_CXX_STANDARD 17)

# 공통 소스
set(COMMON_SOURCES
    src/socket.cpp
    src/thread_pool.cpp
)

# Echo Server
add_executable(echo_server
    src/echo_server_main.cpp
    ${COMMON_SOURCES}
)
target_link_libraries(echo_server pthread)

# HTTP Server
add_executable(http_server
    src/http_server_main.cpp
    src/http_parser.cpp
    ${COMMON_SOURCES}
)
target_link_libraries(http_server pthread)

# 헤더 경로
target_include_directories(echo_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(http_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
```

### 4.3 외부 라이브러리 연동 (Boost.Asio)

```cmake
cmake_minimum_required(VERSION 3.15)
project(AsyncServer)

set(CMAKE_CXX_STANDARD 17)

# Boost 찾기
find_package(Boost 1.70 REQUIRED COMPONENTS system)

add_executable(async_server
    src/main.cpp
    src/async_server.cpp
)

target_include_directories(async_server PRIVATE 
    ${Boost_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(async_server 
    ${Boost_LIBRARIES}
    pthread
)
```

### 4.4 빌드 및 실행

```bash
# 프로젝트 구조
project/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── echo_server.cpp
│   └── echo_server.h
└── build/

# 빌드
mkdir build
cd build
cmake ..
make

# 또는 한 번에
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# 실행
./build/echo_server

# Release 빌드
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 병렬 빌드 (빠름!)
cmake --build build -j8
```

---

## 🐛 자주 막히는 부분

### 문제 1: Segmentation Fault
```cpp
// 원인 1: nullptr 역참조
int* ptr = nullptr;
*ptr = 10;  // ❌ Crash!

// 해결:
if (ptr != nullptr) {
    *ptr = 10;
}

// 원인 2: 배열 범위 초과
int arr[5];
arr[10] = 42;  // ❌ Undefined behavior

// 해결: std::vector 사용
std::vector<int> vec(5);
vec.at(10) = 42;  // ✅ 예외 발생 (디버깅 가능)
```

### 문제 2: Memory Leak
```cpp
// 나쁜 예
void leak() {
    int* data = new int[1000];
    // delete[] 없음! 메모리 누수
}

// 좋은 예 1: 스마트 포인터
void noLeak1() {
    auto data = std::make_unique<int[]>(1000);
    // 자동 해제
}

// 좋은 예 2: 컨테이너
void noLeak2() {
    std::vector<int> data(1000);
    // 자동 해제
}
```

### 문제 3: 데이터 경쟁 (Data Race)
```cpp
// 문제: 여러 스레드가 공유 변수 접근
int counter = 0;

void increment() {
    for (int i = 0; i < 10000; ++i) {
        ++counter;  // ❌ 데이터 경쟁!
    }
}

// 해결: mutex
std::mutex mtx;
int counter = 0;

void increment() {
    for (int i = 0; i < 10000; ++i) {
        std::lock_guard<std::mutex> lock(mtx);
        ++counter;  // ✅ 안전
    }
}

// 또는: atomic
std::atomic<int> counter{0};

void increment() {
    for (int i = 0; i < 10000; ++i) {
        ++counter;  // ✅ 안전 (lock-free)
    }
}
```

### 문제 4: "Address already in use"
```cpp
// 해결: SO_REUSEADDR 옵션
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

// 또는 프로세스 종료
lsof -i :8080
kill -9 <PID>
```

### 문제 5: CMake 에러 - "Cannot find Boost"
```bash
# macOS
brew install boost
export BOOST_ROOT=/opt/homebrew/opt/boost  # CMake에 경로 알림

# Linux
sudo apt install libboost-all-dev

# CMakeLists.txt에서 경로 지정
set(BOOST_ROOT "/usr/local/include")
find_package(Boost REQUIRED)
```

---

## ✅ 완료 체크리스트

### C++ 기초
- [ ] 포인터 vs 참조 이해
- [ ] RAII 패턴 구현 (생성자/소멸자)
- [ ] unique_ptr, shared_ptr 사용
- [ ] 이동 의미론 (std::move) 이해

### 멀티스레딩
- [ ] std::thread 생성 및 join
- [ ] mutex + lock_guard로 동기화
- [ ] 조건 변수 (Producer-Consumer)
- [ ] 데이터 경쟁 방지

### Socket 프로그래밍
- [ ] socket() → bind() → listen() → accept() 흐름
- [ ] read() / write() 사용
- [ ] 멀티스레드 Echo Server 동작
- [ ] nc로 테스트 성공

### CMake
- [ ] CMakeLists.txt 작성
- [ ] cmake + make 빌드 성공
- [ ] Debug vs Release 빌드 구분
- [ ] 외부 라이브러리 링크 (pthread)

### 실전 검증
- [ ] Echo Server 컴파일 및 실행
- [ ] 100개 동시 연결 테스트
- [ ] Valgrind 메모리 체크 (선택)

---

## 🚀 다음 단계

✅ C++ 게임 서버 기초 완료!

**다음 학습**:
- **CMake 심화**: [Quickstart 10: CMake Build System](10-cmake-build-system.md)
- **Boost.Asio**: [Quickstart 11: Boost.Asio & Beast](11-boost-asio-beast.md) - 비동기 I/O

**실전 프로젝트**:
- `~/work/codex-mini-gameserver/mini-gameserver` - Milestone 1.1부터 시작!

---

## 📚 참고 자료

- [C++ Reference](https://en.cppreference.com/)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)
- [C++ Concurrency in Action (책)](https://www.manning.com/books/c-plus-plus-concurrency-in-action-second-edition)

---

**Last Updated**: 2025-11-12
