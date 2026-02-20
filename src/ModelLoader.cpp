#include "ModelLoader.h"
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// this is allllll claude

void Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 &indices[0], GL_STATIC_DRAW);

    // Vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, position));

    // Vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, normal));

    // Vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                         (void*)offsetof(Vertex, texCoords));

    glBindVertexArray(0);
}

void Mesh::draw(GLuint shaderProgram) {
    // Bind textures
    for (unsigned int i = 0; i < textures.size(); i++) {
        glActiveTexture(GL_TEXTURE0 + i);

        std::string name = textures[i].type;
        if (name == "texture_diffuse") {
            glUniform1i(glGetUniformLocation(shaderProgram, "textureDiffuse"), i);
        }

        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }

    // Draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
                   GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Reset to defaults
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::cleanup() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (EBO) glDeleteBuffers(1, &EBO);
    VAO = VBO = EBO = 0;
}

ModelLoader::~ModelLoader() {
    cleanup();
}

bool ModelLoader::loadModel(const std::string& path) {
    Assimp::Importer importer;

    // Import settings for optimal rendering
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |           // Convert to triangles
        aiProcess_FlipUVs |                // Flip texture coordinates
        aiProcess_GenNormals |             // Generate normals if missing
        aiProcess_CalcTangentSpace |       // Calculate tangent space
        aiProcess_JoinIdenticalVertices |  // Optimize vertices
        aiProcess_SortByPType              // Sort by primitive type
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return false;
    }

    m_directory = path.substr(0, path.find_last_of('/'));

    processNode(scene->mRootNode, scene);

    std::cout << "Model loaded: " << path << " (" << m_meshes.size()
              << " meshes)" << std::endl;

    return true;
}

void ModelLoader::processNode(aiNode* node, const aiScene* scene) {
    // Process all meshes in the current node
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(processMesh(mesh, scene));
    }

    // Recursively process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh ModelLoader::processMesh(aiMesh* mesh, const aiScene* scene) {
    Mesh result;

    // Process vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        // Position
        vertex.position = glm::vec3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        );

        // Normals
        if (mesh->HasNormals()) {
            vertex.normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        // Texture coordinates
        if (mesh->mTextureCoords[0]) {
            vertex.texCoords = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        } else {
            vertex.texCoords = glm::vec2(0.0f, 0.0f);
        }

        result.vertices.push_back(vertex);
    }

    // Process indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            result.indices.push_back(face.mIndices[j]);
        }
    }

    // Process materials
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // Load diffuse textures
        std::vector<Texture> diffuseMaps = loadMaterialTextures(material,
            aiTextureType_DIFFUSE, "texture_diffuse");
        result.textures.insert(result.textures.end(), diffuseMaps.begin(), diffuseMaps.end());
    }

    result.setupMesh();
    return result;
}

void ModelLoader::draw(GLuint shaderProgram) {
    for (auto& mesh : m_meshes) {
        mesh.draw(shaderProgram);
    }
}

void ModelLoader::cleanup() {
    for (auto& mesh : m_meshes) {
        mesh.cleanup();
    }
    m_meshes.clear();

    // Clean up loaded textures
    for (auto& pair : m_texturesLoaded) {
        glDeleteTextures(1, &pair.second.id);
    }
    m_texturesLoaded.clear();
}

std::vector<Texture> ModelLoader::loadMaterialTextures(aiMaterial* mat, aiTextureType type, const std::string& typeName) {
    std::vector<Texture> textures;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        std::string texturePath = std::string(str.C_Str());

        // Check if texture was loaded before
        if (m_texturesLoaded.find(texturePath) != m_texturesLoaded.end()) {
            textures.push_back(m_texturesLoaded[texturePath]);
        } else {
            Texture texture;
            texture.id = loadTextureFromFile(texturePath);
            texture.type = typeName;
            texture.path = texturePath;
            textures.push_back(texture);
            m_texturesLoaded[texturePath] = texture;

            std::cout << "Loaded texture: " << texturePath << std::endl;
        }
    }

    return textures;
}

GLuint ModelLoader::loadTextureFromFile(const std::string& filename) {
    std::string fullPath = m_directory + "/" + filename;

    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(fullPath.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format = GL_RGB;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "Failed to load texture: " << fullPath << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
