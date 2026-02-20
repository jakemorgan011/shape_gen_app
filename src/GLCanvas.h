#pragma once

// IMPORTANT: Include GLAD before any other OpenGL headers (including wx/glcanvas.h)
#include <glad/glad.h>

#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ModelLoader.h"
#include <memory>

class GLCanvas : public wxGLCanvas {
public:
    GLCanvas(wxWindow* parent, const wxGLAttributes& canvasAttrs);
    virtual ~GLCanvas();

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

    struct SceneObject {
        std::unique_ptr<ModelLoader> model;
        glm::vec3 position;
        glm::vec3 targetPos;
        glm::vec3 rotation;
        glm::vec3 scale;
        float currentScaleMultiplier;
        float targetScaleMultiplier;
    };

    std::vector<SceneObject> m_sceneObjects;

    // camera/view matrices
    glm::mat4 m_projection;
    glm::mat4 m_view;

    float m_rotation;
    wxStopWatch m_stopWatch;
    long m_lastTime;

    wxPoint m_mousePos;

    wxDECLARE_EVENT_TABLE();
};
