#pragma once

#include <string>

#include "observability/metrics.h"

namespace streamrelay::observability {

class PrometheusExporter {
public:
    static std::string render(const MetricsRegistry& metrics);

private:
    static std::string sanitize_name(const std::string& name);
};

} 
