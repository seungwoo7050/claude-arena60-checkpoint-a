#include <gtest/gtest.h>

#include "arena60/storage/postgres_storage.h"

TEST(PostgresStorageTest, ConnectionFailureIsHandled) {
  arena60::PostgresStorage storage("postgresql://localhost:1/arena60");
  EXPECT_FALSE(storage.IsConnected());
  EXPECT_FALSE(storage.Connect());
  EXPECT_FALSE(storage.IsConnected());
}

TEST(PostgresStorageTest, RecordSessionEventFailsWithoutConnection) {
  arena60::PostgresStorage storage("postgresql://localhost:1/arena60");
  EXPECT_FALSE(storage.RecordSessionEvent("player", "start"));
}
