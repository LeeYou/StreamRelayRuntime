#include "control/secure_control_service.h"

namespace streamrelay::control {

SecureControlService::SecureControlService(ControlService& control, security::StaticTokenAuthenticator& authenticator, security::AuthorizationPolicy& policy)
    : control_(control), authenticator_(authenticator), policy_(policy) {}

core::Result<SubmitCommandResponse> SecureControlService::submit_command(const SecureSubmitCommandRequest& request, std::chrono::system_clock::time_point now) {
    auto claims = authenticator_.authenticate(request.token, now);
    if (!claims.ok()) {
        return claims.error();
    }

    auto authorize = policy_.authorize(security::AuthorizationRequest{claims.value(), "command", "submit"});
    if (!authorize.ok()) {
        return authorize.error();
    }

    auto command = request.command;
    if (command.tenant_id.empty()) {
        command.tenant_id = claims.value().tenant_id;
    }
    if (command.operator_id.empty()) {
        command.operator_id = claims.value().principal_id;
    }
    if (command.tenant_id != claims.value().tenant_id) {
        return core::make_error(core::ErrorCode::InvalidState, "command tenant does not match authenticated principal");
    }

    return control_.submit_command(command, now);
}

} 
