#pragma once

#include <glm/glm.hpp>
#include "ModelLoader.h"

// Static utilities for computing mesh bounds and NDC layout.
// All methods work in the coordinate space used by the NDC render pass:
//   - Meshes lie in the XZ plane (Y ≈ 0)
//   - Rendered with rotate(-90°, X) + scale + translate, identity view/proj
//   - After that transform: NDC.x = pos.x + v.x * scale.x
//                           NDC.y = pos.y + v.z * scale.z
class MeshBounds {
public:
    // Raw mesh-space XZ extent
    struct XZBounds {
        float minX =  1e30f, maxX = -1e30f;
        float minZ =  1e30f, maxZ = -1e30f;

        float width()   const { return maxX - minX; }
        float depth()   const { return maxZ - minZ; }
        float centerX() const { return (minX + maxX) * 0.5f; }
        float centerZ() const { return (minZ + maxZ) * 0.5f; }
        bool  valid()   const { return minX <= maxX && minZ <= maxZ; }
    };

    // Screen-space NDC rectangle
    struct NDCRect {
        float minX = 0.f, maxX = 0.f;
        float minY = 0.f, maxY = 0.f;

        float width()   const { return maxX - minX; }
        float height()  const { return maxY - minY; }
        float centerX() const { return (minX + maxX) * 0.5f; }
        float centerY() const { return (minY + maxY) * 0.5f; }
    };

    // Inner corner of an L-shaped mesh, in mesh space
    struct LCorner {
        float armRightX;  // max X of vertices in the upper-Z half → right edge of vertical arm
        float baseTopZ;   // max Z of vertices in the right-X half → top edge of horizontal base
    };

    // Compute raw XZ bounding box by iterating all mesh vertices.
    static XZBounds ComputeXZBounds(const ModelLoader& model);

    // Convert an XZ mesh bounding box to a screen NDC rect.
    // pos and scale are the object's world position and scale vector.
    static NDCRect ToNDCRect(const XZBounds& b, glm::vec3 pos, glm::vec3 scale);

    // Convenience: compute XZ bounds and immediately convert to NDC.
    static NDCRect ComputeNDCRect(const ModelLoader& model, glm::vec3 pos, glm::vec3 scale);

    // For an L-shaped mesh: find the inner corner in mesh space.
    // Splits the bounding box at its midpoints and finds the extent of each arm.
    static LCorner FindLCorner(const ModelLoader& model, const XZBounds& bounds);
};
