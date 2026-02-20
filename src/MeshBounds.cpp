#include "MeshBounds.h"
#include <algorithm>

MeshBounds::XZBounds MeshBounds::ComputeXZBounds(const ModelLoader& model) {
    XZBounds b;
    for (const auto& mesh : model.getMeshes())
        for (const auto& v : mesh.vertices) {
            b.minX = std::min(b.minX, v.position.x);
            b.maxX = std::max(b.maxX, v.position.x);
            b.minZ = std::min(b.minZ, v.position.z);
            b.maxZ = std::max(b.maxZ, v.position.z);
        }
    return b;
}

MeshBounds::NDCRect MeshBounds::ToNDCRect(const XZBounds& b, glm::vec3 pos, glm::vec3 scale) {
    return {
        pos.x + b.minX * scale.x,
        pos.x + b.maxX * scale.x,
        pos.y + b.minZ * scale.z,
        pos.y + b.maxZ * scale.z,
    };
}

MeshBounds::NDCRect MeshBounds::ComputeNDCRect(const ModelLoader& model, glm::vec3 pos, glm::vec3 scale) {
    return ToNDCRect(ComputeXZBounds(model), pos, scale);
}

MeshBounds::LCorner MeshBounds::FindLCorner(const ModelLoader& model, const XZBounds& bounds) {
    float xMid = bounds.centerX();
    float zMid = bounds.centerZ();

    LCorner corner{ bounds.minX, bounds.minZ };
    for (const auto& mesh : model.getMeshes())
        for (const auto& v : mesh.vertices) {
            if (v.position.z > zMid)
                corner.armRightX = std::max(corner.armRightX, v.position.x);
            if (v.position.x > xMid)
                corner.baseTopZ  = std::max(corner.baseTopZ,  v.position.z);
        }
    return corner;
}
