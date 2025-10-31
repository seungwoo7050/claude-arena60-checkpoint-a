#pragma once

#include <libpq-fe.h>

#include <atomic>
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

    std::string MetricsSnapshot() const;
    double LastQueryDurationSeconds() const noexcept;

    const std::string& dsn() const noexcept { return dsn_; }

   private:
    struct ConnDeleter {
        void operator()(PGconn* conn) const noexcept;
    };

    std::string dsn_;
    std::unique_ptr<PGconn, ConnDeleter> connection_;
    std::atomic<double> last_query_seconds_{0.0};
};

}  // namespace arena60
