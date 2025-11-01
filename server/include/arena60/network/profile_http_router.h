#pragma once

#include <boost/beast/http.hpp>
#include <functional>
#include <memory>
#include <string>

#include "arena60/stats/player_profile_service.h"

namespace arena60 {

class ProfileHttpRouter {
   public:
    using MetricsProvider = std::function<std::string()>;

    ProfileHttpRouter(MetricsProvider metrics_provider,
                      std::shared_ptr<PlayerProfileService> profile_service);

    boost::beast::http::response<boost::beast::http::string_body> Handle(
        const boost::beast::http::request<boost::beast::http::string_body>& request) const;

   private:
    boost::beast::http::response<boost::beast::http::string_body> HandleMetrics(
        const boost::beast::http::request<boost::beast::http::string_body>& request) const;
    boost::beast::http::response<boost::beast::http::string_body> HandleProfile(
        const boost::beast::http::request<boost::beast::http::string_body>& request,
        const std::string& player_id) const;
    boost::beast::http::response<boost::beast::http::string_body> HandleLeaderboard(
        const boost::beast::http::request<boost::beast::http::string_body>& request,
        std::size_t limit) const;

    static std::size_t ParseLimit(const std::string& query);

    MetricsProvider metrics_provider_;
    std::shared_ptr<PlayerProfileService> profile_service_;
};

}  // namespace arena60

