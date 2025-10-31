#pragma once

#include <libpq-fe.h>

#include <memory>
#include <string>

namespace arena60 {

class PostgresStorage {
 public:
  explicit PostgresStorage(std::string dsn);
  ~PostgresStorage();

  bool Connect();
  void Disconnect();

  bool IsConnected() const noexcept;

  bool RecordSessionEvent(const std::string& player_id, const std::string& event);

  const std::string& dsn() const noexcept { return dsn_; }

 private:
  struct ConnDeleter {
    void operator()(PGconn* conn) const noexcept;
  };

  std::string dsn_;
  std::unique_ptr<PGconn, ConnDeleter> connection_;
};

}  // namespace arena60
