# Quickstart 10: CMake 빌드 시스템

## 🎯 목표
- **CMake 기초**: CMakeLists.txt 작성 및 이해
- **빌드 타겟**: 실행 파일, 라이브러리 생성
- **의존성 관리**: 외부 라이브러리 찾기 및 링크
- **멀티 타겟**: 여러 실행 파일 동시 관리
- **빌드 옵션**: Debug/Release, 컴파일 플래그 설정

## 📋 사전준비
- [Quickstart 00](00-setup-linux-macos.md) 완료 (CMake 3.20+ 설치됨)
- [Quickstart 04](04-cpp-for-game-server.md) 완료 (C++ 기초)
- 기본 C++ 프로젝트 구조 이해

---

## 🏗️ Part 1: CMake 기초

### 1.1 CMake란?

```
전통적 방식 (플랫폼 종속):
- Linux: Makefile 작성 → make
- Windows: Visual Studio 프로젝트 파일
- macOS: Xcode 프로젝트 파일

CMake 방식 (플랫폼 독립):
- CMakeLists.txt 하나 작성
- CMake가 각 플랫폼에 맞는 빌드 파일 생성
- Linux: Makefile, Windows: VS, macOS: Xcode
```

### 1.2 최소 CMakeLists.txt

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(HelloWorld)

add_executable(hello main.cpp)
```

```cpp
// main.cpp
#include <iostream>

int main() {
    std::cout << "Hello, CMake!" << std::endl;
    return 0;
}
```

**빌드 및 실행**:
```bash
# 디렉토리 구조
hello-world/
├── CMakeLists.txt
└── main.cpp

# 빌드
mkdir build
cd build
cmake ..
make

# 실행
./hello
# Hello, CMake!
```

### 1.3 CMake 빌드 프로세스

```bash
# 1단계: Configure (CMakeLists.txt 읽기)
cmake -B build
# build/ 디렉토리에 Makefile 생성 (또는 VS 프로젝트)

# 2단계: Build (컴파일 및 링크)
cmake --build build
# 실행 파일 생성: build/hello

# 3단계: 실행
./build/hello

# 한 줄로:
cmake -B build && cmake --build build && ./build/hello
```

---

## 📦 Part 2: 프로젝트 구조화

### 2.1 소스 파일 분리

```
project/
├── CMakeLists.txt
├── include/
│   └── greeter.h
└── src/
    ├── main.cpp
    └── greeter.cpp
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(Greeter VERSION 1.0.0)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 실행 파일 생성
add_executable(greeter
    src/main.cpp
    src/greeter.cpp
)

# 헤더 경로 추가
target_include_directories(greeter PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
```

```cpp
// include/greeter.h
#pragma once
#include <string>

class Greeter {
public:
    std::string greet(const std::string& name);
};
```

```cpp
// src/greeter.cpp
#include "greeter.h"

std::string Greeter::greet(const std::string& name) {
    return "Hello, " + name + "!";
}
```

```cpp
// src/main.cpp
#include <iostream>
#include "greeter.h"

int main() {
    Greeter g;
    std::cout << g.greet("CMake") << std::endl;
    return 0;
}
```

### 2.2 여러 소스 파일 (변수 사용)

```cmake
cmake_minimum_required(VERSION 3.15)
project(GameServer)

set(CMAKE_CXX_STANDARD 17)

# 소스 파일 목록
set(SOURCES
    src/main.cpp
    src/server.cpp
    src/player.cpp
    src/game_state.cpp
)

# 헤더 파일 목록 (선택, 명시적 관리)
set(HEADERS
    include/server.h
    include/player.h
    include/game_state.h
)

add_executable(game_server ${SOURCES} ${HEADERS})

target_include_directories(game_server PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
```

### 2.3 여러 타겟 (실행 파일)

```cmake
cmake_minimum_required(VERSION 3.15)
project(MultiTarget)

set(CMAKE_CXX_STANDARD 17)

# 공통 소스
set(COMMON_SOURCES
    src/utils.cpp
    src/logger.cpp
)

# Echo Server
add_executable(echo_server
    src/echo_server_main.cpp
    ${COMMON_SOURCES}
)

# HTTP Server
add_executable(http_server
    src/http_server_main.cpp
    src/http_parser.cpp
    ${COMMON_SOURCES}
)

# 각 타겟에 헤더 경로 추가
target_include_directories(echo_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(http_server PRIVATE ${CMAKE_SOURCE_DIR}/include)
```

**빌드**:
```bash
cmake --build build

# 결과:
# build/echo_server
# build/http_server
```

---

## 🔗 Part 3: 라이브러리 및 링크

### 3.1 정적 라이브러리 생성

```cmake
cmake_minimum_required(VERSION 3.15)
project(LibraryExample)

set(CMAKE_CXX_STANDARD 17)

# 정적 라이브러리 생성
add_library(mylib STATIC
    src/utils.cpp
    src/logger.cpp
)

target_include_directories(mylib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# 실행 파일에서 라이브러리 사용
add_executable(app src/main.cpp)

target_link_libraries(app PRIVATE mylib)
```

**라이브러리 타입**:
- `STATIC`: 정적 라이브러리 (.a, .lib) - 실행 파일에 포함
- `SHARED`: 동적 라이브러리 (.so, .dll) - 런타임 로드
- `INTERFACE`: 헤더 온리 라이브러리

### 3.2 외부 라이브러리 찾기 (pthread)

```cmake
cmake_minimum_required(VERSION 3.15)
project(ThreadedServer)

set(CMAKE_CXX_STANDARD 17)

# pthread 라이브러리 찾기
find_package(Threads REQUIRED)

add_executable(server
    src/main.cpp
    src/threaded_server.cpp
)

# pthread 링크
target_link_libraries(server PRIVATE
    Threads::Threads
)
```

### 3.3 외부 라이브러리 찾기 (Boost)

```cmake
cmake_minimum_required(VERSION 3.15)
project(BoostExample)

set(CMAKE_CXX_STANDARD 17)

# Boost 찾기 (1.70 이상, system 컴포넌트)
find_package(Boost 1.70 REQUIRED COMPONENTS system)

add_executable(async_server src/main.cpp)

# Boost 헤더 경로
target_include_directories(async_server PRIVATE
    ${Boost_INCLUDE_DIRS}
)

# Boost 라이브러리 링크
target_link_libraries(async_server PRIVATE
    ${Boost_LIBRARIES}
    Threads::Threads
)
```

**Boost가 안 보일 때**:
```cmake
# 직접 경로 지정
set(BOOST_ROOT "/opt/homebrew/opt/boost")  # macOS Homebrew
# set(BOOST_ROOT "/usr/local")             # Linux

find_package(Boost 1.70 REQUIRED COMPONENTS system)
```

### 3.4 pkg-config 사용

```cmake
# protobuf 같은 라이브러리
find_package(PkgConfig REQUIRED)
pkg_check_modules(PROTOBUF REQUIRED protobuf)

add_executable(proto_app src/main.cpp)

target_include_directories(proto_app PRIVATE
    ${PROTOBUF_INCLUDE_DIRS}
)

target_link_libraries(proto_app PRIVATE
    ${PROTOBUF_LIBRARIES}
)
```

---

## ⚙️ Part 4: 빌드 설정

### 4.1 Debug vs Release

```cmake
cmake_minimum_required(VERSION 3.15)
project(Optimized)

set(CMAKE_CXX_STANDARD 17)

# 컴파일 플래그 설정
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_executable(app src/main.cpp)
```

**빌드 타입 지정**:
```bash
# Debug 빌드 (디버깅 심볼, 최적화 없음)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Release 빌드 (최적화 O3, 디버깅 심볼 없음)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 빌드 타입 확인
file build/app
# Debug: "not stripped" (심볼 포함)
# Release: "stripped" (심볼 제거)
```

### 4.2 조건부 컴파일

```cmake
# 플랫폼별 설정
if(APPLE)
    message(STATUS "Building for macOS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
elseif(UNIX)
    message(STATUS "Building for Linux")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
elseif(WIN32)
    message(STATUS "Building for Windows")
endif()

# 빌드 타입별 설정
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DENABLE_LOGGING)
endif()
```

### 4.3 옵션 추가

```cmake
cmake_minimum_required(VERSION 3.15)
project(ConfigurableApp)

# 사용자 옵션 (ON/OFF 토글)
option(ENABLE_TESTS "Build tests" OFF)
option(USE_BOOST "Use Boost.Asio" ON)

if(USE_BOOST)
    find_package(Boost REQUIRED COMPONENTS system)
    target_link_libraries(app PRIVATE ${Boost_LIBRARIES})
endif()

if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

**사용**:
```bash
# Boost 사용 + 테스트 빌드
cmake -B build -DUSE_BOOST=ON -DENABLE_TESTS=ON

# Boost 안 쓰고 + 테스트 안 빌드
cmake -B build -DUSE_BOOST=OFF -DENABLE_TESTS=OFF
```

---

## 📂 Part 5: 복잡한 프로젝트 구조

### 5.1 서브디렉토리 사용

```
project/
├── CMakeLists.txt        # 루트
├── src/
│   ├── CMakeLists.txt    # 서브
│   └── main.cpp
├── lib/
│   ├── CMakeLists.txt    # 서브
│   ├── utils.cpp
│   └── logger.cpp
└── tests/
    ├── CMakeLists.txt    # 서브
    └── test_main.cpp
```

```cmake
# 루트 CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(BigProject)

set(CMAKE_CXX_STANDARD 17)

# 서브디렉토리 추가
add_subdirectory(lib)
add_subdirectory(src)

# 테스트는 옵션
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

```cmake
# lib/CMakeLists.txt
add_library(mylib STATIC
    utils.cpp
    logger.cpp
)

target_include_directories(mylib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)
```

```cmake
# src/CMakeLists.txt
add_executable(app main.cpp)

target_link_libraries(app PRIVATE mylib)
```

### 5.2 실전: mini-gameserver 구조

```cmake
# mini-gameserver/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(MiniGameServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 빌드 옵션
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Boost 찾기
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(Threads REQUIRED)

# 공통 헤더 경로
include_directories(${CMAKE_SOURCE_DIR}/include)

# Milestone 1.1: Echo Server
add_executable(echo_server
    src/milestone-1.1/main.cpp
    src/milestone-1.1/echo_server.cpp
)
target_link_libraries(echo_server PRIVATE Threads::Threads)

# Milestone 1.3: WebSocket Server
add_executable(websocket_server
    src/milestone-1.3/main.cpp
    src/milestone-1.3/websocket_server.cpp
    src/milestone-1.3/websocket_session.cpp
)
target_link_libraries(websocket_server PRIVATE
    ${Boost_LIBRARIES}
    Threads::Threads
)

# Milestone 1.4: Pong Game
add_executable(pong_server
    src/milestone-1.4/main.cpp
    src/milestone-1.4/pong_game.cpp
)
target_link_libraries(pong_server PRIVATE
    ${Boost_LIBRARIES}
    Threads::Threads
)
```

---

## 🐛 자주 막히는 부분

### 문제 1: "CMake Error: Could not find CMAKE_ROOT"
```bash
# 원인: CMake 설치 안 됨

# 해결:
brew install cmake  # macOS
sudo apt install cmake  # Linux

cmake --version
```

### 문제 2: "Cannot find Boost"
```cmake
# 해결 1: BOOST_ROOT 지정
set(BOOST_ROOT "/opt/homebrew/opt/boost")
find_package(Boost REQUIRED)

# 해결 2: 경로 직접 지정
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)
target_link_libraries(app boost_system)
```

### 문제 3: "undefined reference to pthread_create"
```cmake
# 원인: pthread 링크 안 됨

# 해결:
find_package(Threads REQUIRED)
target_link_libraries(your_target PRIVATE Threads::Threads)
```

### 문제 4: 빌드 디렉토리 꼬임
```bash
# 증상: 이상한 에러, 캐시 문제

# 해결: 클린 빌드
rm -rf build
cmake -B build
cmake --build build
```

### 문제 5: 헤더 파일 못 찾음
```bash
# 증상: fatal error: 'myheader.h' file not found

# 해결: include_directories 확인
target_include_directories(your_target PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src  # 추가 경로
)
```

---

## ✅ 완료 체크리스트

### 기본
- [ ] 최소 CMakeLists.txt 작성 (3줄)
- [ ] cmake + make 빌드 성공
- [ ] 실행 파일 생성 및 실행

### 프로젝트 구조
- [ ] 소스/헤더 분리 (src/, include/)
- [ ] 여러 타겟 빌드 (echo_server, http_server)
- [ ] 공통 소스 재사용

### 라이브러리
- [ ] pthread 링크 성공
- [ ] Boost 찾기 및 링크 (선택)
- [ ] 정적 라이브러리 생성 및 사용

### 빌드 설정
- [ ] Debug/Release 빌드 구분
- [ ] 컴파일 플래그 설정 (-Wall, -O3)
- [ ] 조건부 컴파일 (플랫폼별)

### 실전
- [ ] mini-gameserver 구조 이해
- [ ] 서브디렉토리 사용
- [ ] 빌드 에러 해결 경험

---

## 🚀 다음 단계

✅ CMake 빌드 시스템 완료!

**다음 학습**:
- **Boost.Asio**: [Quickstart 11: Boost.Asio & Beast](11-boost-asio-beast.md) - 비동기 I/O
- **Protobuf**: [Quickstart 12: Protobuf Basics](12-protobuf-basics.md) - 데이터 직렬화

**실전 적용**:
- `mini-gameserver` - CMakeLists.txt 수정 및 확장
- 새 타겟 추가 (실험용 서버)

---

## 📚 참고 자료

- [CMake 공식 문서](https://cmake.org/documentation/)
- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)
- [Modern CMake](https://cliutils.gitlab.io/modern-cmake/)
- [Awesome CMake](https://github.com/onqtam/awesome-cmake)

---

**Last Updated**: 2025-11-12
