#pragma once
#include <glad/glad.h>
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

class ShapeTrainer;

class ExploreCanvas : public wxGLCanvas {
public:
    ExploreCanvas(wxWindow* parent, const wxGLAttributes& canvasAttrs, ShapeTrainer* trainer);
    virtual ~ExploreCanvas();

    void onModelUpdated();

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);
    void OnKillFocus(wxFocusEvent& event);

    void InitGL();
    void CreateSkybox();
    void CreatePointCloudResources();
    void CreateCursorResources();
    void CreateFramebuffer();
    void RenderSkybox();
    void RenderPointCloud();
    void RenderCursor();
    void RenderFramebufferToScreen();
    void UpdatePointCloud();
    GLuint CompileShader(const char* vertSrc, const char* fragSrc);
    GLuint LoadTexture(const std::string& path);

    wxGLContext* m_context;
    bool         m_glInitialized = false;
    ShapeTrainer* m_trainer;

    // Skybox
    GLuint m_skyboxVAO = 0, m_skyboxVBO = 0;
    GLuint m_skyboxTexture = 0;
    GLuint m_skyboxShader = 0;

    // Mesh faces
    GLuint m_pcVAO = 0, m_pcVBO = 0, m_pcEBO = 0;
    GLuint m_pcShader = 0;
    int    m_pcVertexCount = 0;
    int    m_pcIndexCount  = 0;

    // Cursor
    GLuint m_cursorVAO = 0, m_cursorVBO = 0;
    GLuint m_cursorShader = 0;
    GLuint m_cursorTexture = 0;

    // Pixelation framebuffer
    GLuint m_framebuffer = 0;
    GLuint m_framebufferTexture = 0;
    GLuint m_renderbuffer = 0;
    GLuint m_quadVAO = 0, m_quadVBO = 0;
    GLuint m_postProcessShader = 0;
    int    m_fbWidth = 0, m_fbHeight = 0;

    // View/projection
    glm::mat4 m_projection;
    glm::mat4 m_view;

    // Navigation
    float m_cursorX = 0.5f;
    float m_cursorY = 0.5f;
    bool  m_keyW = false, m_keyS = false;
    bool  m_keyA = false, m_keyD = false;

    // Last inferred mesh data
    std::vector<float> m_lastMeshData;
    bool               m_meshDirty = false;

    float m_rotation = 0.0f;
    wxStopWatch m_stopWatch;
    long  m_lastTime = 0;

    wxDECLARE_EVENT_TABLE();
};
