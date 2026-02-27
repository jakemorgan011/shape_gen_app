#include "ExploreCanvas.h"
#include "ShapeTrainer.h"
// stb_image.h implementation already defined in ModelLoader.cpp
#include "stb_image.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

wxBEGIN_EVENT_TABLE(ExploreCanvas, wxGLCanvas)
    EVT_PAINT(ExploreCanvas::OnPaint)
    EVT_SIZE(ExploreCanvas::OnSize)
    EVT_IDLE(ExploreCanvas::OnIdle)
    EVT_KEY_DOWN(ExploreCanvas::OnKeyDown)
    EVT_KEY_UP(ExploreCanvas::OnKeyUp)
    EVT_KILL_FOCUS(ExploreCanvas::OnKillFocus)
wxEND_EVENT_TABLE()

ExploreCanvas::ExploreCanvas(wxWindow* parent,
                              const wxGLAttributes& canvasAttrs,
                              ShapeTrainer* trainer)
    : wxGLCanvas(parent, canvasAttrs, wxID_ANY,
                 wxDefaultPosition, wxDefaultSize,
                 wxWANTS_CHARS)
    , m_trainer(trainer)
{
    m_stopWatch.Start();

    wxGLContextAttrs ctxAttrs;
    ctxAttrs.PlatformDefaults().CoreProfile().OGLVersion(3, 3).EndList();
    m_context = new wxGLContext(this, nullptr, &ctxAttrs);

    if (!m_context->IsOK()) {
        wxMessageBox("ExploreCanvas: Failed to create OpenGL context",
                     "Error", wxOK | wxICON_ERROR);
        delete m_context;
        m_context = nullptr;
    }

    SetFocus();
}

ExploreCanvas::~ExploreCanvas() {
    if (m_context) {
        SetCurrent(*m_context);
        if (m_skyboxVAO)    glDeleteVertexArrays(1, &m_skyboxVAO);
        if (m_skyboxVBO)    glDeleteBuffers(1, &m_skyboxVBO);
        if (m_skyboxTexture) glDeleteTextures(1, &m_skyboxTexture);
        if (m_skyboxShader) glDeleteProgram(m_skyboxShader);
        if (m_pcVAO)        glDeleteVertexArrays(1, &m_pcVAO);
        if (m_pcVBO)        glDeleteBuffers(1, &m_pcVBO);
        if (m_pcEBO)        glDeleteBuffers(1, &m_pcEBO);
        if (m_pcShader)     glDeleteProgram(m_pcShader);
        if (m_cursorVAO)          glDeleteVertexArrays(1, &m_cursorVAO);
        if (m_cursorVBO)          glDeleteBuffers(1, &m_cursorVBO);
        if (m_cursorShader)       glDeleteProgram(m_cursorShader);
        if (m_cursorTexture)      glDeleteTextures(1, &m_cursorTexture);
        if (m_framebuffer)        glDeleteFramebuffers(1, &m_framebuffer);
        if (m_framebufferTexture) glDeleteTextures(1, &m_framebufferTexture);
        if (m_renderbuffer)       glDeleteRenderbuffers(1, &m_renderbuffer);
        if (m_quadVAO)            glDeleteVertexArrays(1, &m_quadVAO);
        if (m_quadVBO)            glDeleteBuffers(1, &m_quadVBO);
        if (m_postProcessShader)  glDeleteProgram(m_postProcessShader);
        delete m_context;
    }
}

void ExploreCanvas::OnPaint(wxPaintEvent& /*event*/) {
    wxPaintDC dc(this);
    if (!m_context) return;

    SetCurrent(*m_context);

    if (!m_glInitialized) {
        InitGL();
        m_glInitialized = true;
    }

    // Render scene to framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderSkybox();
    RenderPointCloud();

    // Blit through pixelation shader
    RenderFramebufferToScreen();

    // Cursor drawn directly to screen (bypasses pixelation)
    RenderCursor();

    SwapBuffers();
}

void ExploreCanvas::OnSize(wxSizeEvent& event) {
    if (!m_glInitialized) { event.Skip(); return; }
    wxSize size = GetClientSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0) { event.Skip(); return; }
    SetCurrent(*m_context);
    double scale = GetContentScaleFactor();
    glViewport(0, 0,
               static_cast<int>(size.GetWidth()  * scale),
               static_cast<int>(size.GetHeight() * scale));
    float aspect = static_cast<float>(size.GetWidth()) / static_cast<float>(size.GetHeight());
    m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    event.Skip();
}

void ExploreCanvas::OnIdle(wxIdleEvent& event) {
    if (!m_glInitialized || !m_context) return;

    SetCurrent(*m_context);

    long now = m_stopWatch.Time();
    float dt = std::clamp((now - m_lastTime) / 1000.0f, 0.0f, 0.1f);
    m_lastTime = now;

    m_rotation += 0.045f * dt;

    const float SPEED = 0.4f;
    bool moved = false;
    if (m_keyW) { m_cursorY = std::min(m_cursorY + SPEED * dt, 1.0f); moved = true; }
    if (m_keyS) { m_cursorY = std::max(m_cursorY - SPEED * dt, 0.0f); moved = true; }
    if (m_keyA) { m_cursorX = std::max(m_cursorX - SPEED * dt, 0.0f); moved = true; }
    if (m_keyD) { m_cursorX = std::min(m_cursorX + SPEED * dt, 1.0f); moved = true; }

    if (moved || m_meshDirty) {
        m_meshDirty = false;
        UpdatePointCloud();
    }

    Refresh(false);
    event.RequestMore();
}

void ExploreCanvas::OnKeyDown(wxKeyEvent& event) {
    int key = event.GetKeyCode();
    if (key == 'W' || key == 'w') m_keyW = true;
    else if (key == 'S' || key == 's') m_keyS = true;
    else if (key == 'A' || key == 'a') m_keyA = true;
    else if (key == 'D' || key == 'd') m_keyD = true;
    event.Skip();
}

void ExploreCanvas::OnKeyUp(wxKeyEvent& event) {
    int key = event.GetKeyCode();
    if (key == 'W' || key == 'w') m_keyW = false;
    else if (key == 'S' || key == 's') m_keyS = false;
    else if (key == 'A' || key == 'a') m_keyA = false;
    else if (key == 'D' || key == 'd') m_keyD = false;
    event.Skip();
}

void ExploreCanvas::OnKillFocus(wxFocusEvent& event) {
    m_keyW = m_keyS = m_keyA = m_keyD = false;
    event.Skip();
}

void ExploreCanvas::onModelUpdated() {
    m_meshDirty = true;
}

void ExploreCanvas::UpdatePointCloud() {
    if (!m_trainer || !m_trainer->isModelReady()) return;

    std::vector<float> verts = m_trainer->infer(m_cursorX, m_cursorY);
    if (verts.empty()) return;

    m_lastMeshData  = verts;
    m_pcVertexCount = static_cast<int>(verts.size()) / 3;

    glBindBuffer(GL_ARRAY_BUFFER, m_pcVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const auto& indices = m_trainer->getMeshIndices();
    m_pcIndexCount = static_cast<int>(indices.size());
    if (m_pcIndexCount > 0) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pcEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                     indices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

// ---------------------------------------------------------------------------
// GL initialisation
// ---------------------------------------------------------------------------

GLuint ExploreCanvas::CompileShader(const char* vertSrc, const char* fragSrc) {
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertSrc, nullptr);
    glCompileShader(vert);
    GLint ok;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(vert, 512, nullptr, log);
        wxLogError("ExploreCanvas vert shader: %s", log);
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragSrc, nullptr);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetShaderInfoLog(frag, 512, nullptr, log);
        wxLogError("ExploreCanvas frag shader: %s", log);
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(prog, 512, nullptr, log);
        wxLogError("ExploreCanvas shader link: %s", log);
    }
    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

GLuint ExploreCanvas::LoadTexture(const std::string& path) {
    GLuint texID;
    glGenTextures(1, &texID);
    int w, h, ch;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : (ch == 1 ? GL_RED : GL_RGB);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        wxLogError("ExploreCanvas: Failed to load texture: %s", path.c_str());
        stbi_image_free(data);
    }
    return texID;
}

void ExploreCanvas::InitGL() {
    if (!gladLoadGL()) {
        wxMessageBox("ExploreCanvas: Failed to initialize GLAD",
                     "Error", wxOK | wxICON_ERROR);
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);

    CreateSkybox();
    CreatePointCloudResources();
    CreateCursorResources();
    CreateFramebuffer();

    // Camera — same as GLCanvas
    m_view = glm::mat4(1.0f);
    m_view = glm::translate(m_view, glm::vec3(0.0f, 0.0f, -4.0f));
    m_view = glm::rotate(m_view, glm::radians(20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    m_view = glm::translate(m_view, glm::vec3(0.0f, -0.5f, 0.0f));

    wxSize sz = GetClientSize();
    float aspect = (sz.GetHeight() > 0)
        ? static_cast<float>(sz.GetWidth()) / static_cast<float>(sz.GetHeight())
        : 1.0f;
    m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

void ExploreCanvas::CreateFramebuffer() {
    wxSize size = GetClientSize();
    double scale = GetContentScaleFactor();
    m_fbWidth  = static_cast<int>(size.GetWidth()  * scale);
    m_fbHeight = static_cast<int>(size.GetHeight() * scale);

    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    glGenTextures(1, &m_framebufferTexture);
    glBindTexture(GL_TEXTURE_2D, m_framebufferTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_fbWidth, m_fbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_framebufferTexture, 0);

    glGenRenderbuffers(1, &m_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_fbWidth, m_fbHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        wxLogError("ExploreCanvas: Framebuffer not complete!");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Fullscreen quad
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    const char* quadVert = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoords;
        out vec2 TexCoords;
        void main() {
            TexCoords = aTexCoords;
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
        }
    )";
    const char* pixelFrag = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoords;
        uniform sampler2D screenTexture;
        uniform vec2 resolution;
        uniform float pixelSize;
        void main() {
            vec2 pixelCoord = floor(TexCoords * resolution / pixelSize) * pixelSize / resolution;
            FragColor = texture(screenTexture, pixelCoord);
        }
    )";
    m_postProcessShader = CompileShader(quadVert, pixelFrag);
}

void ExploreCanvas::RenderFramebufferToScreen() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    wxSize size = GetClientSize();
    double scale = GetContentScaleFactor();
    int w = static_cast<int>(size.GetWidth()  * scale);
    int h = static_cast<int>(size.GetHeight() * scale);

    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(m_postProcessShader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_framebufferTexture);
    glUniform1i(glGetUniformLocation(m_postProcessShader, "screenTexture"), 0);
    glUniform2f(glGetUniformLocation(m_postProcessShader, "resolution"),
                static_cast<float>(m_fbWidth), static_cast<float>(m_fbHeight));
    glUniform1f(glGetUniformLocation(m_postProcessShader, "pixelSize"), 4.7f);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
}

void ExploreCanvas::CreateSkybox() {
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_skyboxVAO);
    glGenBuffers(1, &m_skyboxVBO);
    glBindVertexArray(m_skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    m_skyboxTexture = LoadTexture("assets/textures/skybox.JPG");

    const char* vertSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        out vec3 TexCoords;
        uniform mat4 projection;
        uniform mat4 view;
        void main() {
            TexCoords = aPos;
            vec4 pos = projection * view * vec4(aPos, 1.0);
            gl_Position = pos.xyww;
        }
    )";
    const char* fragSrc = R"(
        #version 330 core
        in vec3 TexCoords;
        out vec4 FragColor;
        uniform sampler2D skyboxTexture;
        void main() {
            vec3 absCoords = abs(TexCoords);
            vec2 uv;
            float tileScale = 2.0;
            if (absCoords.x >= absCoords.y && absCoords.x >= absCoords.z)
                uv = vec2(TexCoords.z / absCoords.x, TexCoords.y / absCoords.x);
            else if (absCoords.y >= absCoords.x && absCoords.y >= absCoords.z)
                uv = vec2(TexCoords.x / absCoords.y, TexCoords.z / absCoords.y);
            else
                uv = vec2(TexCoords.x / absCoords.z, TexCoords.y / absCoords.z);
            uv = uv * tileScale * 0.5 + 0.5;
            FragColor = texture(skyboxTexture, uv);
        }
    )";
    m_skyboxShader = CompileShader(vertSrc, fragSrc);
}

void ExploreCanvas::CreatePointCloudResources() {
    glGenVertexArrays(1, &m_pcVAO);
    glGenBuffers(1, &m_pcVBO);
    glGenBuffers(1, &m_pcEBO);

    glBindVertexArray(m_pcVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_pcVBO);
    glBufferData(GL_ARRAY_BUFFER, 2000 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pcEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 10000 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

    glBindVertexArray(0);

    const char* vertSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        out vec3 vWorldPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            vWorldPos   = vec3(model * vec4(aPos, 1.0));
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";
    const char* fragSrc = R"(
        #version 330 core
        in vec3 vWorldPos;
        out vec4 FragColor;

        void main() {
            // Compute face normal from screen-space derivatives
            vec3 dx     = dFdx(vWorldPos);
            vec3 dy     = dFdy(vWorldPos);
            vec3 normal = normalize(cross(dx, dy));

            vec3 lightPos = vec3(0.0, 3.0, 4.0);
            vec3 lightDir = normalize(lightPos - vWorldPos);
            float intensity = max(dot(normal, lightDir), 0.0) * 0.5;

            // Same 4-band toon colours as anchor objects
            vec3 color;
            if (intensity > 0.45)
                color = vec3(1.0,  1.0,  1.0);    // white highlight
            else if (intensity > 0.2)
                color = vec3(0.88, 0.98, 0.88);   // light green
            else if (intensity > 0.1)
                color = vec3(0.72, 0.92, 1.00);   // light blue
            else
                color = vec3(0.25, 0.62, 0.92);   // medium blue

            FragColor = vec4(color, 1.0);
        }
    )";
    m_pcShader = CompileShader(vertSrc, fragSrc);
}

void ExploreCanvas::CreateCursorResources() {
    // 2D quad ±0.05 NDC units with UV coords, 6 vertices
    float quadVerts[] = {
        // pos (2)   uv (2)
        -0.05f, -0.05f,  0.0f, 0.0f,
         0.05f, -0.05f,  1.0f, 0.0f,
         0.05f,  0.05f,  1.0f, 1.0f,

        -0.05f, -0.05f,  0.0f, 0.0f,
         0.05f,  0.05f,  1.0f, 1.0f,
        -0.05f,  0.05f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &m_cursorVAO);
    glGenBuffers(1, &m_cursorVBO);
    glBindVertexArray(m_cursorVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cursorVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    const char* vertSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aUV;
        out vec2 vUV;
        uniform vec2 uNDCPos;
        void main() {
            vUV = aUV;
            gl_Position = vec4(aPos + uNDCPos, 0.0, 1.0);
        }
    )";
    const char* fragSrc = R"(
        #version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uTex;
        void main() {
            vec4 c = texture(uTex, vUV);
            if (c.a < 0.01) discard;
            FragColor = c;
        }
    )";
    m_cursorShader = CompileShader(vertSrc, fragSrc);
    m_cursorTexture = LoadTexture("assets/textures/cursor.png");
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void ExploreCanvas::RenderSkybox() {
    glDepthFunc(GL_LEQUAL);
    glUseProgram(m_skyboxShader);

    // Strip translation from view, apply multi-axis rotation
    glm::mat4 view = glm::mat4(glm::mat3(m_view));
    view = glm::rotate(view, m_rotation,           glm::vec3(0.0f, 1.0f, 0.0f));
    view = glm::rotate(view, m_rotation * 0.37f,   glm::vec3(1.0f, 0.0f, 0.0f));
    view = glm::rotate(view, m_rotation * 0.21f,   glm::vec3(0.0f, 0.0f, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(m_skyboxShader, "view"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_skyboxShader, "projection"),
                       1, GL_FALSE, glm::value_ptr(m_projection));

    glBindVertexArray(m_skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_skyboxTexture);
    glUniform1i(glGetUniformLocation(m_skyboxShader, "skyboxTexture"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

void ExploreCanvas::RenderPointCloud() {
    if (m_pcVertexCount == 0 || m_lastMeshData.empty()) return;

    glUseProgram(m_pcShader);

    glm::mat4 model = glm::rotate(glm::mat4(1.0f), m_rotation, glm::vec3(0.0f, 1.0f, 0.0f));
    glUniformMatrix4fv(glGetUniformLocation(m_pcShader, "model"),
                       1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(m_pcShader, "view"),
                       1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(glGetUniformLocation(m_pcShader, "projection"),
                       1, GL_FALSE, glm::value_ptr(m_projection));

    glBindVertexArray(m_pcVAO);
    if (m_pcIndexCount > 0) {
        glDrawElements(GL_TRIANGLES, m_pcIndexCount, GL_UNSIGNED_INT, nullptr);
    } else {
        // Fallback: no indices yet, draw points
        glDrawArrays(GL_POINTS, 0, m_pcVertexCount);
    }
    glBindVertexArray(0);
}

void ExploreCanvas::RenderCursor() {
    glDisable(GL_DEPTH_TEST);

    glUseProgram(m_cursorShader);

    // Convert [0,1] cursor coords to NDC [-1,1]
    float ndcX = m_cursorX * 2.0f - 1.0f;
    float ndcY = m_cursorY * 2.0f - 1.0f;
    glUniform2f(glGetUniformLocation(m_cursorShader, "uNDCPos"), ndcX, ndcY);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_cursorTexture);
    glUniform1i(glGetUniformLocation(m_cursorShader, "uTex"), 0);

    glBindVertexArray(m_cursorVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}
