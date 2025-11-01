#include "arena60/network/profile_http_router.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace arena60 {

namespace http = boost::beast::http;

ProfileHttpRouter::ProfileHttpRouter(MetricsProvider metrics_provider,
                                     std::shared_ptr<PlayerProfileService> profile_service)
    : metrics_provider_(std::move(metrics_provider)),
      profile_service_(std::move(profile_service)) {}

http::response<http::string_body> ProfileHttpRouter::Handle(
    const http::request<http::string_body>& request) const {
    http::response<http::string_body> response;
    response.version(request.version());
    response.keep_alive(false);

    if (request.method() != http::verb::get) {
        response.result(http::status::method_not_allowed);
        response.set(http::field::content_type, "text/plain");
        response.body() = "Method Not Allowed";
        response.prepare_payload();
        return response;
    }

    const std::string target{request.target()};
    if (target == "/metrics") {
        return HandleMetrics(request);
    }

    if (target.rfind("/profiles/", 0) == 0) {
        auto remainder = target.substr(std::string("/profiles/").size());
        const auto query_pos = remainder.find('?');
        if (query_pos != std::string::npos) {
            remainder = remainder.substr(0, query_pos);
        }
        if (remainder.empty()) {
            response.result(http::status::not_found);
            response.set(http::field::content_type, "application/json");
            response.body() = "{\"error\":\"not found\"}";
            response.prepare_payload();
            return response;
        }
        return HandleProfile(request, remainder);
    }

    if (target.rfind("/leaderboard", 0) == 0) {
        std::string query;
        const auto query_pos = target.find('?');
        if (query_pos != std::string::npos) {
            query = target.substr(query_pos + 1);
        }
        const auto limit = ParseLimit(query);
        return HandleLeaderboard(request, limit);
    }

    response.result(http::status::not_found);
    response.set(http::field::content_type, "text/plain");
    response.body() = "Not Found";
    response.prepare_payload();
    return response;
}

http::response<http::string_body> ProfileHttpRouter::HandleMetrics(
    const http::request<http::string_body>& request) const {
    http::response<http::string_body> response;
    response.version(request.version());
    response.keep_alive(false);
    response.result(http::status::ok);
    response.set(http::field::content_type, "text/plain; version=0.0.4");
    if (metrics_provider_) {
        response.body() = metrics_provider_();
    }
    response.prepare_payload();
    return response;
}

http::response<http::string_body> ProfileHttpRouter::HandleProfile(
    const http::request<http::string_body>& request, const std::string& player_id) const {
    http::response<http::string_body> response;
    response.version(request.version());
    response.keep_alive(false);

    if (!profile_service_) {
        response.result(http::status::service_unavailable);
        response.set(http::field::content_type, "application/json");
        response.body() = "{\"error\":\"profiles unavailable\"}";
        response.prepare_payload();
        return response;
    }

    auto profile = profile_service_->GetProfile(player_id);
    if (!profile) {
        response.result(http::status::not_found);
        response.set(http::field::content_type, "application/json");
        response.body() = "{\"error\":\"not found\"}";
        response.prepare_payload();
        return response;
    }

    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.body() = profile_service_->SerializeProfile(*profile);
    response.prepare_payload();
    return response;
}

http::response<http::string_body> ProfileHttpRouter::HandleLeaderboard(
    const http::request<http::string_body>& request, std::size_t limit) const {
    http::response<http::string_body> response;
    response.version(request.version());
    response.keep_alive(false);

    if (!profile_service_) {
        response.result(http::status::service_unavailable);
        response.set(http::field::content_type, "application/json");
        response.body() = "{\"error\":\"profiles unavailable\"}";
        response.prepare_payload();
        return response;
    }

    auto profiles = profile_service_->TopProfiles(limit);
    response.result(http::status::ok);
    response.set(http::field::content_type, "application/json");
    response.body() = profile_service_->SerializeLeaderboard(profiles);
    response.prepare_payload();
    return response;
}

std::size_t ProfileHttpRouter::ParseLimit(const std::string& query) {
    std::size_t limit = 10;
    if (query.empty()) {
        return limit;
    }
    const std::string prefix = "limit=";
    auto pos = query.find(prefix);
    if (pos == std::string::npos) {
        return limit;
    }
    pos += prefix.size();
    std::size_t end = pos;
    while (end < query.size() && std::isdigit(static_cast<unsigned char>(query[end]))) {
        ++end;
    }
    if (end == pos) {
        return limit;
    }
    try {
        const auto parsed = static_cast<std::size_t>(std::stoul(query.substr(pos, end - pos)));
        if (parsed == 0) {
            return 1;
        }
        return std::min<std::size_t>(50, parsed);
    } catch (const std::exception&) {
        return limit;
    }
}

}  // namespace arena60

