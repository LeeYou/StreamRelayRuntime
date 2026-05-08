#include <cassert>
#include <string>

#include "observability/health.h"
#include "observability/metrics.h"
#include "observability/prometheus_exporter.h"
#include "observability/slo.h"
#include "observability/trace.h"
#include "ops/release_manifest.h"
#include "ops/runtime_config.h"

int main() {
    streamrelay::observability::MetricsRegistry metrics;
    metrics.increment_counter("gateway.frame_processed", 3);
    metrics.set_gauge("gateway.active_connections", 2);
    const auto rendered_metrics = streamrelay::observability::PrometheusExporter::render(metrics);
    assert(rendered_metrics.find("gateway_frame_processed_total 3") != std::string::npos);
    assert(rendered_metrics.find("gateway_active_connections 2") != std::string::npos);

    streamrelay::observability::HealthRegistry health;
    health.report(streamrelay::observability::HealthCheckResult{"message_bus", streamrelay::observability::HealthState::Passing, "ok"});
    assert(health.health() == streamrelay::observability::HealthState::Passing);
    health.report(streamrelay::observability::HealthCheckResult{"device_registry", streamrelay::observability::HealthState::Failing, "unavailable"});
    assert(health.readiness() == streamrelay::observability::HealthState::Failing);
    assert(health.render_text().find("device_registry=failing") != std::string::npos);

    streamrelay::observability::InMemoryTracer tracer;
    auto root = tracer.start_span(100, "gateway", "receive_frame");
    auto child = tracer.start_span(100, "router", "route", root.span_id);
    tracer.finish_span(root.span_id);
    assert(tracer.spans().size() == 2);
    assert(child.parent_span_id == root.span_id);
    assert(tracer.spans()[0].ended);

    streamrelay::observability::SloRegistry slo;
    slo.define(streamrelay::observability::SloTarget{"command_submit", 200, 0.01});
    slo.measure("command_submit", streamrelay::observability::SloMeasurement{150, 0.001});
    assert(slo.passing("command_submit"));
    slo.measure("command_submit", streamrelay::observability::SloMeasurement{250, 0.001});
    assert(!slo.passing("command_submit"));

    streamrelay::ops::RuntimeConfig config;
    config.environment = "prod";
    config.gateway_id = "gateway-a";
    config.public_port = 8080;
    config.internal_port = 9090;
    config.tls_enabled = true;
    assert(!streamrelay::ops::RuntimeConfigValidator::validate(config).ok());
    config.required_secrets.push_back("tls-cert");
    assert(streamrelay::ops::RuntimeConfigValidator::validate(config).ok());

    streamrelay::ops::ReleaseManifest manifest;
    manifest.version = "0.1.0";
    manifest.git_revision = "local";
    manifest.build_config = "Debug";
    manifest.artifacts.push_back(streamrelay::ops::ReleaseArtifact{"streamrelay_allinone", "build/Debug/streamrelay_allinone.exe", "sha256:local"});
    const auto rendered_manifest = streamrelay::ops::ReleaseManifestRenderer::render_text(manifest);
    assert(rendered_manifest.find("version=0.1.0") != std::string::npos);
    assert(rendered_manifest.find("artifact=streamrelay_allinone") != std::string::npos);

    return 0;
}
