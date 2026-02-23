#pragma once

// IMPORTANT: Include GLAD before any other OpenGL headers (including wx/glcanvas.h)
#include <glad/glad.h>

#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ModelLoader.h"
#include "MeshBounds.h"
#include "ShapeTrainer.h"
#include <memory>

class GLCanvas : public wxGLCanvas {
public:
    GLCanvas(wxWindow* parent, const wxGLAttributes& canvasAttrs,
             ShapeTrainer* trainer = nullptr);
    virtual ~GLCanvas();

    std::vector<AnchorData> CollectAnchorData() const;

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseClick(wxMouseEvent& event);

    void InitGL();
    void Render();
    void CreateShaders();
    void CreateToonShader();
    void CreateSkybox();
    void RenderSkybox();
    void CreateMenu();
    void RenderMenu();
    GLuint LoadTexture(const std::string& path);
    void UpdateScaleAnimations(float deltaTime);
    void UpdateTranslationAnimations(float deltaTime);
    glm::vec3 GetScaledSize(size_t objectIndex);
    void CreateFramebuffer();
    void RenderToFramebuffer();
    void RenderFramebufferToScreen();

    // Anchor plane / map
    void InitAnchorObjects();
    void OnMouseRelease(wxMouseEvent& event);
    void CreateBorderShader();
    void RenderBorder(const MeshBounds::NDCRect& rect, glm::vec4 color);
    glm::vec2 MouseToNDC(wxPoint mousePos) const;
    void LoadCursors();

    wxGLContext* m_context;
    bool m_glInitialized;

    GLuint m_shaderProgram;
    GLuint m_skyboxShaderProgram;
    GLuint m_postProcessShader;
    GLuint m_toonShaderProgram;

    GLuint m_framebuffer;
    GLuint m_framebufferTexture;
    GLuint m_renderbuffer;
    GLuint m_quadVAO, m_quadVBO;
    int m_fbWidth, m_fbHeight;

    GLuint m_skyboxVAO, m_skyboxVBO;
    GLuint m_skyboxTexture;

    // Debug border for map area
    GLuint m_borderShaderProgram;
    GLuint m_borderVAO, m_borderVBO;
    MeshBounds::NDCRect m_mapBounds;

    // Drag state
    int       m_dragIndex  = -1;
    glm::vec2 m_dragOffset = glm::vec2(0.0f);

    struct SceneObject {
        std::unique_ptr<ModelLoader> model;
        glm::vec3 position;
        glm::vec3 targetPos;
        glm::vec3 rotation;
        glm::vec3 scale;
        float currentScaleMultiplier;
        float targetScaleMultiplier;
        float currentTranslationMultiplier;
        float targetTranslationMultiplier;
        bool      isAnchor    = false;
        bool      isHovered   = false;
        float     hoverAmount = 0.0f;   // animated 0→1, drives color lerp in shader
        glm::vec3 homePos     = glm::vec3(0.0f);
        float     meshMinX   = 0.0f, meshMaxX = 0.0f;
        float     meshMinZ   = 0.0f, meshMaxZ = 0.0f;
        // Spin
        float     spinAngle = 0.0f;
        float     spinRate  = 1.0f;   // radians/sec
        glm::vec3 spinAxis  = glm::vec3(0.0f, 1.0f, 0.0f);
    };

    std::vector<SceneObject> m_sceneObjects;

    // camera/view matrices
    glm::mat4 m_projection;
    glm::mat4 m_view;

    float m_rotation;
    wxStopWatch m_stopWatch;
    long m_lastTime;

    wxPoint m_mousePos;

    wxCursor m_cursorDefault;
    wxCursor m_cursorHover;
    wxCursor m_cursorDrag;

    ShapeTrainer* m_trainer = nullptr;

    wxDECLARE_EVENT_TABLE();
};
