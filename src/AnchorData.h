#pragma once
#include <vector>

struct AnchorData {
    float lx, ly;                       // [0,1] latent coords
    std::vector<float>        vertices; // flattened xyz, size = numVerts*3
    std::vector<unsigned int> indices;  // triangle indices (shared topology)
};
