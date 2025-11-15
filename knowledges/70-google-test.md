# Quickstart 70: Google Test - 단위 테스트

> **📚 학습 유형**: 기초 개념 (Fundamentals)
> **⏭️ 다음 단계**: 모든 MVP에서 테스트 커버리지 ≥ 70% 달성

## 🎯 목표
- **Google Test**: C++ 단위 테스트 프레임워크
- **테스트 작성**: 기본 문법 및 어서션
- **CMake 통합**: 테스트 자동화
- **커버리지 측정**: 70% 이상 달성
- **실전**: 게임 서버 모듈 테스트

## 📋 사전준비
- [Quickstart 30](30-cpp-for-game-server.md) 완료 (C++ 기초)
- [Quickstart 31](31-cmake-build-system.md) 완료 (CMake)
- CMake 3.20+, GCC 11+ 또는 Clang 14+

---

## ✅ Part 1: Google Test 기초 (20분)

### 1.1 Google Test란?

**Google Test (gtest)**는 **구글이 만든 C++ 테스트 프레임워크**로, 게임 서버 코드의 정확성을 검증합니다.

```
왜 테스트가 필요한가?
- 버그 조기 발견
- 리팩토링 안전성
- 문서화 효과
- 자신감 향상
- Arena60 요구사항: 테스트 커버리지 ≥ 70%
```

### 1.2 설치

**Ubuntu/Debian**:
```bash
sudo apt-get update
sudo apt-get install libgtest-dev

# 소스 빌드 (필요한 경우)
cd /usr/src/gtest
sudo cmake .
sudo make
sudo cp lib/*.a /usr/lib
```

**macOS**:
```bash
brew install googletest
```

**CMake FetchContent** (권장):
```cmake
# CMakeLists.txt에서 자동 다운로드
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)
```

### 1.3 첫 번째 테스트

**math_utils.h**:
```cpp
#pragma once

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}

bool is_even(int n) {
    return n % 2 == 0;
}
```

**math_utils_test.cpp**:
```cpp
#include "math_utils.h"
#include <gtest/gtest.h>

// 테스트 케이스 1: 덧셈
TEST(MathUtilsTest, AddPositiveNumbers) {
    EXPECT_EQ(add(2, 3), 5);
    EXPECT_EQ(add(10, 20), 30);
}

TEST(MathUtilsTest, AddNegativeNumbers) {
    EXPECT_EQ(add(-5, -3), -8);
    EXPECT_EQ(add(-10, 5), -5);
}

// 테스트 케이스 2: 곱셈
TEST(MathUtilsTest, MultiplyNumbers) {
    EXPECT_EQ(multiply(3, 4), 12);
    EXPECT_EQ(multiply(-2, 5), -10);
    EXPECT_EQ(multiply(0, 100), 0);
}

// 테스트 케이스 3: 짝수 판별
TEST(MathUtilsTest, IsEven) {
    EXPECT_TRUE(is_even(2));
    EXPECT_TRUE(is_even(0));
    EXPECT_TRUE(is_even(-4));

    EXPECT_FALSE(is_even(1));
    EXPECT_FALSE(is_even(-3));
}

// 메인 함수
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

**CMakeLists.txt**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(math_utils_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Google Test 다운로드
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

# 테스트 활성화
enable_testing()

# 테스트 실행 파일
add_executable(math_utils_test math_utils_test.cpp)
target_link_libraries(math_utils_test GTest::gtest_main)

# CTest 통합
include(GoogleTest)
gtest_discover_tests(math_utils_test)
```

**빌드 & 실행**:
```bash
mkdir -p build && cd build
cmake ..
cmake --build .

# 테스트 실행
./math_utils_test

# 또는 CTest 사용
ctest --output-on-failure
```

**출력**:
```
[==========] Running 4 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 4 tests from MathUtilsTest
[ RUN      ] MathUtilsTest.AddPositiveNumbers
[       OK ] MathUtilsTest.AddPositiveNumbers (0 ms)
[ RUN      ] MathUtilsTest.AddNegativeNumbers
[       OK ] MathUtilsTest.AddNegativeNumbers (0 ms)
[ RUN      ] MathUtilsTest.MultiplyNumbers
[       OK ] MathUtilsTest.MultiplyNumbers (0 ms)
[ RUN      ] MathUtilsTest.IsEven
[       OK ] MathUtilsTest.IsEven (0 ms)
[----------] 4 tests from MathUtilsTest (0 ms total)

[==========] 4 tests from 1 test suite ran. (0 ms total)
[  PASSED  ] 4 tests.
```

---

## 🧪 Part 2: 어서션 (Assertions) (15분)

### 2.1 EXPECT vs ASSERT

```cpp
// EXPECT: 실패해도 테스트 계속 실행
TEST(AssertionTest, ExpectExample) {
    EXPECT_EQ(1 + 1, 2);       // 통과
    EXPECT_EQ(2 + 2, 5);       // 실패 (계속 실행)
    EXPECT_EQ(3 + 3, 6);       // 실행됨
}

// ASSERT: 실패하면 즉시 중단
TEST(AssertionTest, AssertExample) {
    ASSERT_EQ(1 + 1, 2);       // 통과
    ASSERT_EQ(2 + 2, 5);       // 실패 (중단!)
    ASSERT_EQ(3 + 3, 6);       // 실행 안 됨
}
```

**사용 가이드**:
- **EXPECT**: 일반적인 검증 (여러 조건 확인)
- **ASSERT**: 치명적 오류 (이후 테스트 의미 없음)

### 2.2 주요 어서션

```cpp
#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(AssertionsTest, BooleanAssertions) {
    EXPECT_TRUE(5 > 3);
    EXPECT_FALSE(2 > 10);
}

TEST(AssertionsTest, ComparisonAssertions) {
    EXPECT_EQ(10, 10);          // Equal
    EXPECT_NE(5, 10);           // Not Equal
    EXPECT_LT(5, 10);           // Less Than
    EXPECT_LE(5, 5);            // Less or Equal
    EXPECT_GT(10, 5);           // Greater Than
    EXPECT_GE(10, 10);          // Greater or Equal
}

TEST(AssertionsTest, FloatingPointAssertions) {
    double a = 0.1 + 0.2;
    double b = 0.3;

    // ❌ 부동소수점 오차로 실패할 수 있음
    // EXPECT_EQ(a, b);

    // ✅ 오차 허용 (기본 4 ULP)
    EXPECT_DOUBLE_EQ(a, b);

    // ✅ 수동 오차 설정
    EXPECT_NEAR(a, b, 0.0001);
}

TEST(AssertionsTest, StringAssertions) {
    std::string name = "Alice";

    EXPECT_EQ(name, "Alice");
    EXPECT_NE(name, "Bob");
    EXPECT_STREQ(name.c_str(), "Alice");  // C 문자열
}

TEST(AssertionsTest, ExceptionAssertions) {
    auto throw_error = []() { throw std::runtime_error("Error!"); };
    auto no_throw = []() { return 42; };

    EXPECT_THROW(throw_error(), std::runtime_error);
    EXPECT_NO_THROW(no_throw());
    EXPECT_ANY_THROW(throw_error());
}

TEST(AssertionsTest, CustomMessages) {
    int expected = 10;
    int actual = 20;

    EXPECT_EQ(expected, actual) << "Expected " << expected
                                 << " but got " << actual;
}
```

---

## 🎮 Part 3: 게임 서버 테스트 실전 (30분)

### 3.1 ELO Calculator 테스트

**elo_calculator.h**:
```cpp
#pragma once
#include <cmath>

class EloCalculator {
private:
    int k_factor_;

public:
    explicit EloCalculator(int k_factor = 32) : k_factor_(k_factor) {}

    double calculate_expected_score(int rating_a, int rating_b) const {
        return 1.0 / (1.0 + std::pow(10.0, (rating_b - rating_a) / 400.0));
    }

    struct RatingChange {
        int winner_new_rating;
        int loser_new_rating;
        int winner_change;
        int loser_change;
    };

    RatingChange calculate_rating_change(int winner_rating, int loser_rating) const {
        double expected_winner = calculate_expected_score(winner_rating, loser_rating);

        int winner_change = static_cast<int>(std::round(k_factor_ * (1.0 - expected_winner)));
        int loser_change = static_cast<int>(std::round(k_factor_ * (0.0 - (1.0 - expected_winner))));

        RatingChange result;
        result.winner_new_rating = winner_rating + winner_change;
        result.loser_new_rating = loser_rating + loser_change;
        result.winner_change = winner_change;
        result.loser_change = loser_change;

        return result;
    }
};
```

**elo_calculator_test.cpp**:
```cpp
#include "elo_calculator.h"
#include <gtest/gtest.h>
#include <cmath>

class EloCalculatorTest : public ::testing::Test {
protected:
    EloCalculator calc{32};  // K-factor = 32
};

TEST_F(EloCalculatorTest, ExpectedScoreEqualRating) {
    // 동일 레이팅: 50% 승률
    double expected = calc.calculate_expected_score(1200, 1200);
    EXPECT_NEAR(expected, 0.5, 0.01);
}

TEST_F(EloCalculatorTest, ExpectedScoreHigherRating) {
    // 400점 차이: 약 91% 승률
    double expected = calc.calculate_expected_score(1600, 1200);
    EXPECT_GT(expected, 0.90);
    EXPECT_LT(expected, 0.92);
}

TEST_F(EloCalculatorTest, RatingChangeEqualRating) {
    // 동일 레이팅 승부: 승자 +16, 패자 -16
    auto result = calc.calculate_rating_change(1200, 1200);

    EXPECT_EQ(result.winner_change, 16);
    EXPECT_EQ(result.loser_change, -16);
    EXPECT_EQ(result.winner_new_rating, 1216);
    EXPECT_EQ(result.loser_new_rating, 1184);
}

TEST_F(EloCalculatorTest, RatingChangeUpsetVictory) {
    // 약자(1000)가 강자(1400)를 이김
    auto result = calc.calculate_rating_change(1000, 1400);

    // 약자는 많이 올라감
    EXPECT_GT(result.winner_change, 25);
    // 강자는 많이 떨어짐
    EXPECT_LT(result.loser_change, -25);

    // 제로섬 확인
    EXPECT_EQ(result.winner_change + result.loser_change, 0);
}

TEST_F(EloCalculatorTest, RatingChangeFavoriteWins) {
    // 강자(1400)가 약자(1000)를 이김 (예상 결과)
    auto result = calc.calculate_rating_change(1400, 1000);

    // 강자는 적게 올라감
    EXPECT_LT(result.winner_change, 10);
    // 약자는 적게 떨어짐
    EXPECT_GT(result.loser_change, -10);
}

TEST_F(EloCalculatorTest, RatingNeverNegative) {
    // 극단적인 경우에도 레이팅은 음수가 되지 않음
    EloCalculator high_k_calc(100);
    auto result = high_k_calc.calculate_rating_change(800, 2000);

    EXPECT_GE(result.loser_new_rating, 0);
}
```

### 3.2 Collision Detection 테스트

**collision.h**:
```cpp
#pragma once
#include <cmath>

struct Circle {
    float x, y;
    float radius;
};

bool check_collision(const Circle& a, const Circle& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    return distance <= (a.radius + b.radius);
}
```

**collision_test.cpp**:
```cpp
#include "collision.h"
#include <gtest/gtest.h>

TEST(CollisionTest, NoCollision) {
    Circle a{0.0f, 0.0f, 5.0f};
    Circle b{20.0f, 0.0f, 5.0f};

    EXPECT_FALSE(check_collision(a, b));
}

TEST(CollisionTest, TouchingEdge) {
    Circle a{0.0f, 0.0f, 5.0f};
    Circle b{10.0f, 0.0f, 5.0f};  // 거리 = 10, 반지름 합 = 10

    EXPECT_TRUE(check_collision(a, b));
}

TEST(CollisionTest, Overlapping) {
    Circle a{0.0f, 0.0f, 10.0f};
    Circle b{5.0f, 0.0f, 10.0f};

    EXPECT_TRUE(check_collision(a, b));
}

TEST(CollisionTest, SamePosition) {
    Circle a{100.0f, 100.0f, 5.0f};
    Circle b{100.0f, 100.0f, 5.0f};

    EXPECT_TRUE(check_collision(a, b));
}

TEST(CollisionTest, DiagonalCollision) {
    Circle a{0.0f, 0.0f, 5.0f};
    Circle b{3.0f, 4.0f, 5.0f};  // 거리 = 5

    EXPECT_TRUE(check_collision(a, b));
}
```

### 3.3 Test Fixtures (공통 설정)

```cpp
#include <gtest/gtest.h>
#include <memory>

class Player {
public:
    int id;
    int health;
    float x, y;

    Player(int id, int health, float x, float y)
        : id(id), health(health), x(x), y(y) {}

    void take_damage(int damage) {
        health -= damage;
        if (health < 0) health = 0;
    }

    bool is_alive() const { return health > 0; }
};

class GameTest : public ::testing::Test {
protected:
    // 각 테스트 전에 실행
    void SetUp() override {
        player1 = std::make_unique<Player>(1, 100, 0.0f, 0.0f);
        player2 = std::make_unique<Player>(2, 100, 50.0f, 50.0f);
    }

    // 각 테스트 후에 실행
    void TearDown() override {
        // 정리 작업 (필요 시)
    }

    std::unique_ptr<Player> player1;
    std::unique_ptr<Player> player2;
};

TEST_F(GameTest, PlayerTakesDamage) {
    EXPECT_EQ(player1->health, 100);

    player1->take_damage(20);
    EXPECT_EQ(player1->health, 80);
    EXPECT_TRUE(player1->is_alive());
}

TEST_F(GameTest, PlayerDies) {
    player1->take_damage(100);
    EXPECT_EQ(player1->health, 0);
    EXPECT_FALSE(player1->is_alive());
}

TEST_F(GameTest, PlayerCannotHaveNegativeHealth) {
    player1->take_damage(150);
    EXPECT_EQ(player1->health, 0);
}
```

---

## 📊 Part 4: 테스트 커버리지 측정 (15분)

### 4.1 gcov + lcov 설정

**CMakeLists.txt** (커버리지 추가):
```cmake
cmake_minimum_required(VERSION 3.20)
project(arena60_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 커버리지 플래그
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# 소스 파일
add_library(game_logic
    src/elo_calculator.cpp
    src/collision.cpp
)

# 테스트
add_executable(game_tests
    tests/elo_calculator_test.cpp
    tests/collision_test.cpp
)

target_link_libraries(game_tests
    PRIVATE
        game_logic
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(game_tests)
```

**커버리지 측정**:
```bash
# 커버리지 활성화 빌드
mkdir -p build-coverage && cd build-coverage
cmake -DENABLE_COVERAGE=ON ..
cmake --build .

# 테스트 실행
./game_tests

# 커버리지 데이터 수집
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/googletest/*' --output-file coverage_filtered.info

# HTML 리포트 생성
genhtml coverage_filtered.info --output-directory coverage_report

# 리포트 열기
open coverage_report/index.html  # macOS
xdg-open coverage_report/index.html  # Linux
```

### 4.2 커버리지 해석

```
커버리지 목표: ≥ 70%

좋은 커버리지:
- Line Coverage: 코드 라인 실행 비율
- Branch Coverage: 분기 (if/else) 실행 비율
- Function Coverage: 함수 호출 비율

예시:
Lines executed: 85.2% (234/275)
Branches executed: 72.1% (89/123)
Functions executed: 91.7% (22/24)

✅ 70% 이상 달성!
```

---

## 🏗️ Part 5: 통합 테스트 (Integration Tests) (10분)

### 5.1 Database 통합 테스트

```cpp
#include <gtest/gtest.h>
#include <pqxx/pqxx>

class DatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        conn = std::make_unique<pqxx::connection>(
            "host=localhost dbname=gamedb_test user=gameuser password=gamepass123"
        );

        // 테스트 전 데이터 초기화
        pqxx::work txn(*conn);
        txn.exec("TRUNCATE TABLE users RESTART IDENTITY CASCADE");
        txn.commit();
    }

    std::unique_ptr<pqxx::connection> conn;
};

TEST_F(DatabaseTest, CreateAndRetrieveUser) {
    pqxx::work txn(*conn);

    // 사용자 생성
    txn.exec_params(
        "INSERT INTO users (username, email, password_hash) VALUES ($1, $2, $3)",
        "testuser", "test@example.com", "hashed_password"
    );
    txn.commit();

    // 사용자 조회
    pqxx::work txn2(*conn);
    auto result = txn2.exec_params(
        "SELECT username, email FROM users WHERE username = $1",
        "testuser"
    );

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]["username"].as<std::string>(), "testuser");
    EXPECT_EQ(result[0]["email"].as<std::string>(), "test@example.com");
}

TEST_F(DatabaseTest, UpdateUserElo) {
    pqxx::work txn(*conn);

    // 사용자 생성
    txn.exec_params(
        "INSERT INTO users (username, email, password_hash, elo_rating) "
        "VALUES ($1, $2, $3, $4)",
        "player1", "player1@example.com", "hash", 1200
    );
    txn.commit();

    // ELO 업데이트
    pqxx::work txn2(*conn);
    txn2.exec_params(
        "UPDATE users SET elo_rating = $1 WHERE username = $2",
        1250, "player1"
    );
    txn2.commit();

    // 확인
    pqxx::work txn3(*conn);
    auto result = txn3.exec_params(
        "SELECT elo_rating FROM users WHERE username = $1",
        "player1"
    );

    EXPECT_EQ(result[0]["elo_rating"].as<int>(), 1250);
}
```

---

## 🐛 자주 막히는 부분

### 문제 1: "undefined reference to testing::..."

```cmake
# ❌ 잘못된 링크
target_link_libraries(my_test gtest)

# ✅ 올바른 링크
target_link_libraries(my_test GTest::gtest_main)
```

### 문제 2: 부동소수점 비교 실패

```cpp
// ❌ 정확한 비교 (실패 가능)
EXPECT_EQ(0.1 + 0.2, 0.3);

// ✅ 오차 허용
EXPECT_DOUBLE_EQ(0.1 + 0.2, 0.3);
EXPECT_NEAR(0.1 + 0.2, 0.3, 0.0001);
```

### 문제 3: 테스트 격리 실패

```cpp
// ❌ 전역 상태 공유
int global_counter = 0;

TEST(BadTest, Increment) {
    global_counter++;
    EXPECT_EQ(global_counter, 1);  // 다른 테스트 실행 후 실패!
}

// ✅ Test Fixture 사용
class CounterTest : public ::testing::Test {
protected:
    void SetUp() override {
        counter = 0;  // 매 테스트마다 초기화
    }
    int counter;
};
```

### 문제 4: 커버리지가 낮게 나옴

```cpp
// 테스트되지 않은 에러 처리 코드
int divide(int a, int b) {
    if (b == 0) {
        throw std::invalid_argument("Division by zero");  // 테스트 안 됨!
    }
    return a / b;
}

// ✅ 예외 케이스도 테스트
TEST(DivideTest, ThrowsOnZeroDivision) {
    EXPECT_THROW(divide(10, 0), std::invalid_argument);
}
```

---

## ✅ 완료 체크리스트

### Part 1: Google Test 기초
- [ ] Google Test 설치 및 확인
- [ ] 첫 번째 테스트 작성 및 실행
- [ ] CMake 통합

### Part 2: 어서션
- [ ] EXPECT vs ASSERT 이해
- [ ] 주요 어서션 사용 (EQ, NE, TRUE, FALSE 등)
- [ ] 부동소수점 비교 (DOUBLE_EQ, NEAR)

### Part 3: 게임 서버 테스트
- [ ] ELO Calculator 테스트
- [ ] Collision Detection 테스트
- [ ] Test Fixtures 사용

### Part 4: 테스트 커버리지
- [ ] gcov/lcov 설정
- [ ] 커버리지 70% 이상 달성
- [ ] HTML 리포트 생성

### Part 5: 통합 테스트
- [ ] Database 통합 테스트
- [ ] 테스트 격리 (SetUp/TearDown)

---

## 🚀 다음 단계

✅ **Google Test 완료!**

**실전 적용**:
- MVP 1.0 - Basic Game Server 테스트
- MVP 1.1 - Combat System 테스트
- MVP 1.2 - Matchmaking 테스트
- MVP 1.3 - Statistics & Ranking 테스트

**모든 MVP에서 테스트 커버리지 ≥ 70% 달성 필수!**

---

## 📚 참고 자료

- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [Google Test Advanced](https://google.github.io/googletest/advanced.html)
- [CMake GoogleTest Module](https://cmake.org/cmake/help/latest/module/GoogleTest.html)
- [lcov Documentation](http://ltp.sourceforge.net/coverage/lcov.php)

---

**Last Updated**: 2025-01-30
