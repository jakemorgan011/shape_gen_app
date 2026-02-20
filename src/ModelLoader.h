#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>
#include <string>
#include <map>

// this is all claude

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

struct Texture {
    GLuint id;
    std::string type;
    std::string path;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    GLuint VAO, VBO, EBO;

    void setupMesh();
    void draw(GLuint shaderProgram);
    void cleanup();
};

class ModelLoader {
public:
    ModelLoader() = default;
    ~ModelLoader();

    bool loadModel(const std::string& path);
    void draw(GLuint shaderProgram);
    void cleanup();

    const std::vector<Mesh>& getMeshes() const { return m_meshes; }

private:
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName);
    GLuint loadTextureFromFile(const std::string& filename);

    std::vector<Mesh> m_meshes;
    std::string m_directory;
    std::map<std::string, Texture> m_texturesLoaded; // Cache loaded textures
};
