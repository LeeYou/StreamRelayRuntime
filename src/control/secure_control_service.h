#pragma once

#include <chrono>
#include <string>

#include "control/control_service.h"
#include "security/auth.h"

namespace streamrelay::control {

struct SecureSubmitCommandRequest {
    std::string token;
    SubmitCommandRequest command;
};

class SecureControlService {
public:
    SecureControlService(ControlService& control, security::StaticTokenAuthenticator& authenticator, security::AuthorizationPolicy& policy);

    core::Result<SubmitCommandResponse> submit_command(const SecureSubmitCommandRequest& request, std::chrono::system_clock::time_point now);

private:
    ControlService& control_;
    security::StaticTokenAuthenticator& authenticator_;
    security::AuthorizationPolicy& policy_;
};

} 
