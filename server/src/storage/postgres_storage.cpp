#include "arena60/storage/postgres_storage.h"

#include <iostream>

namespace arena60 {

void PostgresStorage::ConnDeleter::operator()(PGconn* conn) const noexcept {
  if (conn) {
    PQfinish(conn);
  }
}

PostgresStorage::PostgresStorage(std::string dsn) : dsn_(std::move(dsn)) {}

PostgresStorage::~PostgresStorage() { Disconnect(); }

bool PostgresStorage::Connect() {
  if (connection_) {
    return true;
  }
  PGconn* raw = PQconnectdb(dsn_.c_str());
  if (PQstatus(raw) != CONNECTION_OK) {
    std::cerr << "postgres connection failed: " << PQerrorMessage(raw);
    PQfinish(raw);
    return false;
  }
  connection_.reset(raw);
  return true;
}

void PostgresStorage::Disconnect() { connection_.reset(); }

bool PostgresStorage::IsConnected() const noexcept { return connection_ != nullptr; }

bool PostgresStorage::RecordSessionEvent(const std::string& player_id, const std::string& event) {
  if (!connection_) {
    std::cerr << "postgres write skipped: no connection" << std::endl;
    return false;
  }
  const char* param_values[2] = {player_id.c_str(), event.c_str()};
  const int param_lengths[2] = {static_cast<int>(player_id.size()), static_cast<int>(event.size())};
  const int param_formats[2] = {0, 0};

  PGresult* result = PQexecParams(connection_.get(),
                                  "INSERT INTO session_events(player_id, event_type, created_at)"
                                  " VALUES($1, $2, NOW())",
                                  2, nullptr, param_values, param_lengths, param_formats, 0);
  if (PQresultStatus(result) != PGRES_COMMAND_OK) {
    std::cerr << "postgres insert failed: " << PQerrorMessage(connection_.get());
    PQclear(result);
    return false;
  }
  PQclear(result);
  return true;
}

}  // namespace arena60
