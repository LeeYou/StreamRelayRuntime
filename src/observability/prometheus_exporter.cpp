#include "observability/prometheus_exporter.h"

#include <algorithm>
#include <sstream>

namespace streamrelay::observability {

std::string PrometheusExporter::render(const MetricsRegistry& metrics) {
    std::ostringstream out;
    for (const auto& item : metrics.counters()) {
        out << sanitize_name(item.first) << "_total " << item.second << '\n';
    }
    for (const auto& item : metrics.gauges()) {
        out << sanitize_name(item.first) << ' ' << item.second << '\n';
    }
    return out.str();
}

std::string PrometheusExporter::sanitize_name(const std::string& name) {
    std::string out = name;
    std::replace_if(out.begin(), out.end(), [](char c) {
        return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');
    }, '_');
    return out;
}

} 
