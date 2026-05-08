#include "security/auth.h"

#include <utility>

namespace streamrelay::security {

core::Result<void> StaticTokenAuthenticator::add_token(std::string token, AuthClaims claims) {
    if (token.empty() || claims.tenant_id.empty() || claims.principal_id.empty()) {
        return core::make_error(core::ErrorCode::InvalidArgument, "token, tenant_id and principal_id are required");
    }
    tokens_[std::move(token)] = std::move(claims);
    return core::success();
}

core::Result<AuthClaims> StaticTokenAuthenticator::authenticate(const std::string& token, std::chrono::system_clock::time_point now) const {
    const auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        return core::make_error(core::ErrorCode::NotFound, "token not found");
    }
    if (it->second.expires_at <= now) {
        return core::make_error(core::ErrorCode::DeadlineExceeded, "token expired");
    }
    return it->second;
}

void AuthorizationPolicy::allow_role(std::string role, std::string resource, std::string action) {
    allowed_.insert(key(role, resource, action));
}

core::Result<void> AuthorizationPolicy::authorize(const AuthorizationRequest& request) const {
    for (const auto& role : request.claims.roles) {
        if (allowed_.find(key(role, request.resource, request.action)) != allowed_.end()) {
            return core::success();
        }
    }
    return core::make_error(core::ErrorCode::InvalidState, "authorization denied");
}

std::string AuthorizationPolicy::key(const std::string& role, const std::string& resource, const std::string& action) {
    return role + ":" + resource + ":" + action;
}

} 
