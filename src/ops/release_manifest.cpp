#include "ops/release_manifest.h"

#include <sstream>

namespace streamrelay::ops {

std::string ReleaseManifestRenderer::render_text(const ReleaseManifest& manifest) {
    std::ostringstream out;
    out << "version=" << manifest.version << '\n';
    out << "git_revision=" << manifest.git_revision << '\n';
    out << "build_config=" << manifest.build_config << '\n';
    for (const auto& artifact : manifest.artifacts) {
        out << "artifact=" << artifact.name << ',' << artifact.path << ',' << artifact.checksum << '\n';
    }
    return out.str();
}

} 
