#pragma once

#include <string>
#include <vector>

namespace streamrelay::ops {

struct ReleaseArtifact {
    std::string name;
    std::string path;
    std::string checksum;
};

struct ReleaseManifest {
    std::string version;
    std::string git_revision;
    std::string build_config;
    std::vector<ReleaseArtifact> artifacts;
};

class ReleaseManifestRenderer {
public:
    static std::string render_text(const ReleaseManifest& manifest);
};

} 
