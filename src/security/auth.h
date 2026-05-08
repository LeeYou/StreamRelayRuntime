#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/result.h"

namespace streamrelay::security {

struct AuthClaims {
    std::string tenant_id;
    std::string principal_id;
    std::unordered_set<std::string> roles;
    std::chrono::system_clock::time_point expires_at{};
};

class StaticTokenAuthenticator {
public:
    core::Result<void> add_token(std::string token, AuthClaims claims);
    core::Result<AuthClaims> authenticate(const std::string& token, std::chrono::system_clock::time_point now) const;

private:
    std::unordered_map<std::string, AuthClaims> tokens_;
};

struct AuthorizationRequest {
    AuthClaims claims;
    std::string resource;
    std::string action;
};

class AuthorizationPolicy {
public:
    void allow_role(std::string role, std::string resource, std::string action);
    core::Result<void> authorize(const AuthorizationRequest& request) const;

private:
    static std::string key(const std::string& role, const std::string& resource, const std::string& action);
    std::unordered_set<std::string> allowed_;
};

} 
