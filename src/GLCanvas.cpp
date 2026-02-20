#include "GLCanvas.h"
#include "MeshBounds.h"
#include "stb_image.h"
#include <cmath>
#include <cfloat>
#include <algorithm>

wxBEGIN_EVENT_TABLE(GLCanvas, wxGLCanvas)
    EVT_PAINT(GLCanvas::OnPaint)
    EVT_SIZE(GLCanvas::OnSize)
    EVT_IDLE(GLCanvas::OnIdle)
    EVT_MOTION(GLCanvas::OnMouseMove)
    EVT_LEFT_DOWN(GLCanvas::OnMouseClick)
    EVT_LEFT_UP(GLCanvas::OnMouseRelease)
wxEND_EVENT_TABLE()

GLCanvas::GLCanvas(wxWindow* parent, const wxGLAttributes& canvasAttrs)
    : wxGLCanvas(parent, canvasAttrs, wxID_ANY, wxDefaultPosition, wxDefaultSize),
      m_context(nullptr),
      m_glInitialized(false),
      m_shaderProgram(0),
      m_skyboxShaderProgram(0),
      m_postProcessShader(0),
      m_toonShaderProgram(0),
      m_framebuffer(0),
      m_framebufferTexture(0),
      m_renderbuffer(0),
      m_quadVAO(0),
      m_quadVBO(0),
      m_fbWidth(0),
      m_fbHeight(0),
      m_skyboxVAO(0),
      m_skyboxVBO(0),
      m_skyboxTexture(0),
      m_borderShaderProgram(0),
      m_borderVAO(0),
      m_borderVBO(0),
      m_rotation(0.0f),
      m_lastTime(0),
      m_mousePos(0, 0) {

    m_stopWatch.Start();

    wxGLContextAttrs ctxAttrs;
    ctxAttrs.PlatformDefaults().CoreProfile().OGLVersion(3, 3).EndList();
    m_context = new wxGLContext(this, nullptr, &ctxAttrs);

    if (!m_context->IsOK()) {
        wxMessageBox("Failed to create OpenGL context", "Error", wxOK | wxICON_ERROR);
        delete m_context;
        m_context = nullptr;
    }
}

GLCanvas::~GLCanvas() {
    if (m_context) {
        SetCurrent(*m_context);

        for (auto& obj : m_sceneObjects) {
            if (obj.model) {
                obj.model->cleanup();
            }
        }
        if (m_shaderProgram) glDeleteProgram(m_shaderProgram);
        if (m_skyboxShaderProgram) glDeleteProgram(m_skyboxShaderProgram);
        if (m_postProcessShader) glDeleteProgram(m_postProcessShader);
        if (m_toonShaderProgram) glDeleteProgram(m_toonShaderProgram);
        if (m_skyboxVAO) glDeleteVertexArrays(1, &m_skyboxVAO);
        if (m_skyboxVBO) glDeleteBuffers(1, &m_skyboxVBO);
        if (m_skyboxTexture) glDeleteTextures(1, &m_skyboxTexture);
        if (m_framebuffer) glDeleteFramebuffers(1, &m_framebuffer);
        if (m_framebufferTexture) glDeleteTextures(1, &m_framebufferTexture);
        if (m_renderbuffer) glDeleteRenderbuffers(1, &m_renderbuffer);
        if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
        if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
        if (m_borderShaderProgram) glDeleteProgram(m_borderShaderProgram);
        if (m_borderVAO) glDeleteVertexArrays(1, &m_borderVAO);
        if (m_borderVBO) glDeleteBuffers(1, &m_borderVBO);
        delete m_context;
    }
}

void GLCanvas::OnPaint(wxPaintEvent& /*event*/) {
    wxPaintDC dc(this);

    if (!m_context) return;

    SetCurrent(*m_context);

    if (!m_glInitialized) {
        InitGL();
        m_glInitialized = true;
    }

    Render();
    SwapBuffers();
}

void GLCanvas::OnSize(wxSizeEvent& event) {
    if (!m_glInitialized) return;

    wxSize size = GetClientSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0) return;

    SetCurrent(*m_context);
    glViewport(0, 0, size.GetWidth(), size.GetHeight());

    float aspect = static_cast<float>(size.GetWidth()) / static_cast<float>(size.GetHeight());
    m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    event.Skip();
}

void GLCanvas::OnIdle(wxIdleEvent& event) {
    if (!m_glInitialized) return;

    if (m_context) {
        SetCurrent(*m_context);
    }

    long currentTime = m_stopWatch.Time();
    float deltaTime = (currentTime - m_lastTime) / 1000.0f;
    m_lastTime = currentTime;

    m_rotation += 0.015f * deltaTime;

    UpdateScaleAnimations(deltaTime);
    UpdateTranslationAnimations(deltaTime);

    for (size_t i = 1; i < m_sceneObjects.size(); ++i) {
        auto& obj = m_sceneObjects[i];
        if (obj.isAnchor) obj.spinAngle += obj.spinRate * deltaTime;
    }

    Refresh(false);
    event.RequestMore();
}

void GLCanvas::LoadCursors() {
    auto load = [](const char* path, int hotX, int hotY) -> wxCursor {
        wxImage img(path, wxBITMAP_TYPE_PNG);
        if (!img.IsOk()) return wxCursor(wxCURSOR_ARROW);
        img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, hotX);
        img.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, hotY);
        return wxCursor(img);
    };
    m_cursorDefault = load("assets/textures/cursor.png",       0, 0);
    m_cursorHover   = load("assets/textures/cursor_hover.png", 0, 0);
    m_cursorDrag    = load("assets/textures/cursor_drag.png",  0, 0);
    SetCursor(m_cursorDefault);
}

void GLCanvas::OnMouseMove(wxMouseEvent& event) {
    m_mousePos = event.GetPosition();
    glm::vec2 ndc = MouseToNDC(m_mousePos);

    // Move dragged object freely
    if (m_dragIndex >= 1 && m_dragIndex < (int)m_sceneObjects.size()) {
        auto& obj = m_sceneObjects[m_dragIndex];
        obj.position.x = ndc.x + m_dragOffset.x;
        obj.position.y = ndc.y + m_dragOffset.y;
        obj.targetPos  = obj.position;
    }

    // Hover detection: scale up + flag for shader brightening
    for (size_t i = 1; i < m_sceneObjects.size(); ++i) {
        auto& obj = m_sceneObjects[i];
        if (!obj.isAnchor) continue;

        bool hovering = ((int)i == m_dragIndex);
        if (!hovering) {
            float s = obj.scale.x * obj.currentScaleMultiplier;
            hovering = ndc.x >= obj.position.x + obj.meshMinX * s &&
                       ndc.x <= obj.position.x + obj.meshMaxX * s &&
                       ndc.y >= obj.position.y + obj.meshMinZ * s &&
                       ndc.y <= obj.position.y + obj.meshMaxZ * s;
        }
        obj.isHovered             = hovering;
        obj.targetScaleMultiplier = hovering ? 1.3f : 1.0f;
    }

    // Update cursor
    if (m_dragIndex >= 1) {
        SetCursor(m_cursorDrag);
    } else {
        bool anyHovered = false;
        for (size_t i = 1; i < m_sceneObjects.size(); ++i)
            if (m_sceneObjects[i].isHovered) { anyHovered = true; break; }
        SetCursor(anyHovered ? m_cursorHover : m_cursorDefault);
    }

    event.Skip();
}

void GLCanvas::OnMouseClick(wxMouseEvent& event) {
    glm::vec2 ndc = MouseToNDC(event.GetPosition());

    for (size_t i = 1; i < m_sceneObjects.size(); ++i) {
        auto& obj = m_sceneObjects[i];
        if (!obj.isAnchor) continue;

        float s = obj.scale.x * obj.currentScaleMultiplier;
        if (ndc.x >= obj.position.x + obj.meshMinX * s &&
            ndc.x <= obj.position.x + obj.meshMaxX * s &&
            ndc.y >= obj.position.y + obj.meshMinZ * s &&
            ndc.y <= obj.position.y + obj.meshMaxZ * s) {
            m_dragIndex  = (int)i;
            m_dragOffset = glm::vec2(obj.position.x - ndc.x,
                                     obj.position.y - ndc.y);
            SetCursor(m_cursorDrag);
            break;
        }
    }
    event.Skip();
}

void GLCanvas::OnMouseRelease(wxMouseEvent& event) {
    if (m_dragIndex >= 1 && m_dragIndex < (int)m_sceneObjects.size()) {
        auto& obj = m_sceneObjects[m_dragIndex];

        // If dropped in menu area (outside map) → snap home
        bool inMap = obj.position.x >= m_mapBounds.minX &&
                     obj.position.x <= m_mapBounds.maxX &&
                     obj.position.y >= m_mapBounds.minY &&
                     obj.position.y <= m_mapBounds.maxY;
        if (!inMap)
            obj.targetPos = obj.homePos;
    }
    m_dragIndex = -1;
    SetCursor(m_cursorDefault);
    event.Skip();
}

void GLCanvas::InitGL() {
    // Load OpenGL functions with GLAD
    if (!gladLoadGL()) {
        wxMessageBox("Failed to initialize GLAD", "Error", wxOK | wxICON_ERROR);
        return;
    }

    wxLogStatus("OpenGL %d.%d", GLVersion.major, GLVersion.minor);

    glEnable(GL_DEPTH_TEST);

    // Enable transparency/alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);  // White background

    CreateShaders();
    CreateToonShader();
    CreateSkybox();
    CreateFramebuffer();
    CreateBorderShader();
    LoadCursors();

    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(0.0f, 0.0f, -4.0f));  // Move back from the model
    view = glm::rotate(view, glm::radians(20.0f), glm::vec3(1.0f, 0.0f, 0.0f));  // Look down 20 degrees
    view = glm::translate(view, glm::vec3(0.0f, -0.5f, 0.0f));  // Raise camera
    m_view = view;

    wxSize size = GetClientSize();
    float aspect = static_cast<float>(size.GetWidth()) / static_cast<float>(size.GetHeight());
    m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    SceneObject menu;
    menu.model = std::make_unique<ModelLoader>();
    if (menu.model->loadModel("assets/meshes/menu.obj")){
        menu.position = glm::vec3(0.0f, 0.0f, 0.0f);
        menu.targetPos = glm::vec3(0.0f, 0.0f, 0.0f);
        menu.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        menu.scale = glm::vec3(0.6804f, 0.6f, 0.7851f);
        menu.currentScaleMultiplier = 1.0f;
        menu.targetScaleMultiplier = 1.0f;
        menu.currentTranslationMultiplier = 1.0f;
        menu.targetTranslationMultiplier = 1.0f;
        // std::move is SO lit
        m_sceneObjects.push_back(std::move(menu));
    } else {
        wxLogError("Failed to load assets/meshes/menu.obj");
    }

    InitAnchorObjects();
}

void GLCanvas::CreateShaders() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoords;

        out vec3 FragPos;
        out vec3 Normal;
        out vec2 TexCoords;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        void main() {
            FragPos = vec3(model * vec4(aPos, 1.0));
            Normal = mat3(transpose(inverse(model))) * aNormal;
            TexCoords = aTexCoords;
            gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec3 FragPos;
        in vec3 Normal;
        in vec2 TexCoords;
        out vec4 FragColor;

        uniform sampler2D textureDiffuse;
        uniform bool uNDCMode;

        void main() {
            // Get texture color with alpha
            vec4 texColor = texture(textureDiffuse, TexCoords);

            // Discard fully transparent pixels
            if (texColor.a < 0.01) {
                discard;
            }

            // If texture is black/missing, use a default color
            vec3 objectColor = texColor.rgb;
            if (length(objectColor) < 0.01) {
                objectColor = vec3(0.8, 0.8, 0.8);
            }

            vec3 result;
            if (uNDCMode) {
                // NDC menu: skip lighting entirely, always fully visible
                result = objectColor;
            } else {
                // Simple lighting - light positioned above camera
                vec3 lightPos = vec3(0.0, 3.0, 4.0);
                vec3 lightDir = normalize(lightPos - FragPos);
                vec3 norm = normalize(Normal);

                float diff = max(dot(norm, lightDir), 0.0);

                vec3 cameraDir = vec3(0.0, 0.0, 1.0);
                float frontalLight = max(dot(norm, cameraDir), 0.0);

                vec3 ambient = 0.4 * objectColor;
                vec3 diffuse = 0.7 * diff * objectColor;
                vec3 frontal = 0.2 * frontalLight * objectColor;
                result = ambient + diffuse + frontal;
            }

            FragColor = vec4(result, texColor.a);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        wxLogError("Vertex shader compilation failed: %s", infoLog);
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        wxLogError("Fragment shader compilation failed: %s", infoLog);
    }

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vertexShader);
    glAttachShader(m_shaderProgram, fragmentShader);
    glLinkProgram(m_shaderProgram);

    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, infoLog);
        wxLogError("Shader program linking failed: %s", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void GLCanvas::CreateToonShader() {
    const char* toonVertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoords;

        out float vLightIntensity;

        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;

        void main() {
            vec3 fragPos = vec3(model * vec4(aPos, 1.0));
            vec3 normal = normalize(mat3(transpose(inverse(model))) * aNormal);

            vec3 lightPos = vec3(0.0, 3.0, 4.0);
            vec3 lightDir = normalize(lightPos - fragPos);

            vLightIntensity = max(dot(normal, lightDir), 0.0);

            gl_Position = projection * view * vec4(fragPos, 1.0);
        }
    )";

    const char* toonFragmentShaderSource = R"(
        #version 330 core
        in float vLightIntensity;
        out vec4 FragColor;

        uniform float uHover;
        uniform float uTime;

        vec3 hsv2rgb(vec3 c) {
            vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
            vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
            return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
        }

        void main() {
            float intensity = vLightIntensity * 0.5;

            vec3 color;
            float band;
            if (intensity > 0.45) {
                color = vec3(1.0, 1.0, 1.0);      // white (highlight)
                band = 0.0;
            } else if (intensity > 0.2) {
                color = vec3(0.88, 0.98, 0.88);   // super light green
                band = 0.25;
            } else if (intensity > 0.1) {
                color = vec3(0.72, 0.92, 1.00);   // light blue (shadow)
                band = 0.5;
            } else {
                color = vec3(0.25, 0.62, 0.92);   // medium blue (deep shadow)
                band = 0.75;
            }

            if (uHover > 0.001) {
                float t = floor(uTime * 60.0);
                float hue = fract(sin(t * 127.1 + band * 311.7) * 43758.5453);
                vec3 hoverColor = hsv2rgb(vec3(hue, 1.0, 1.0));
                color = mix(color, hoverColor, uHover);
            }

            FragColor = vec4(color, 1.0);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &toonVertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        wxLogError("Toon vertex shader compilation failed: %s", infoLog);
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &toonFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        wxLogError("Toon fragment shader compilation failed: %s", infoLog);
    }

    m_toonShaderProgram = glCreateProgram();
    glAttachShader(m_toonShaderProgram, vertexShader);
    glAttachShader(m_toonShaderProgram, fragmentShader);
    glLinkProgram(m_toonShaderProgram);

    glGetProgramiv(m_toonShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_toonShaderProgram, 512, nullptr, infoLog);
        wxLogError("Toon shader program linking failed: %s", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void GLCanvas::Render() {
    RenderToFramebuffer();
    RenderFramebufferToScreen();

    // Draw debug overlays directly to screen — bypasses post-process pixelation
    if (m_glInitialized)
        RenderBorder(m_mapBounds, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
}

void GLCanvas::RenderToFramebuffer() {
    // Bind framebuffer and render scene to texture
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderSkybox();

    if (m_sceneObjects.empty()) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glm::mat4 identity = glm::mat4(1.0f);

    // --- Menu (index 0): textured shader, NDC mode ---
    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "view"),       1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniform1i(glGetUniformLocation(m_shaderProgram, "uNDCMode"), GL_TRUE);

    {
        auto& obj = m_sceneObjects[0];
        if (obj.model) {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, obj.position);
            modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix = glm::scale(modelMatrix, GetScaledSize(0));
            glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
            obj.model->draw(m_shaderProgram);
        }
    }

    // --- Anchor objects (indices 1+): toon shader, NDC mode, always on top ---
    if (m_sceneObjects.size() > 1) {
        glDepthFunc(GL_ALWAYS);  // render on top of menu regardless of Z

        glUseProgram(m_toonShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(m_toonShaderProgram, "view"),       1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(glGetUniformLocation(m_toonShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(identity));

        float currentTime = static_cast<float>(m_stopWatch.TimeInMicro().ToLong()) / 1000000.0f;
        glUniform1f(glGetUniformLocation(m_toonShaderProgram, "uTime"), currentTime);

        for (size_t i = 1; i < m_sceneObjects.size(); ++i) {
            auto& obj = m_sceneObjects[i];
            if (!obj.model) continue;

            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, obj.position);
            modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            modelMatrix = glm::rotate(modelMatrix, obj.spinAngle, obj.spinAxis);
            modelMatrix = glm::scale(modelMatrix, GetScaledSize(i));
            glUniformMatrix4fv(glGetUniformLocation(m_toonShaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelMatrix));
            glUniform1f(glGetUniformLocation(m_toonShaderProgram, "uHover"), obj.hoverAmount);
            obj.model->draw(m_toonShaderProgram);
        }

        glDepthFunc(GL_LESS);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GLCanvas::CreateFramebuffer() {
    wxSize size = GetClientSize();
    double scale = GetContentScaleFactor();
    m_fbWidth = size.GetWidth() * scale;
    m_fbHeight = size.GetHeight() * scale;

    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

    // create empty texture to render camera to
    glGenTextures(1, &m_framebufferTexture);
    glBindTexture(GL_TEXTURE_2D, m_framebufferTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_fbWidth, m_fbHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_framebufferTexture, 0);

    // the render buffer allows for resolution effects
    glGenRenderbuffers(1, &m_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_fbWidth, m_fbHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        wxLogError("Framebuffer is not complete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // fullscreen quad
    float quadVertices[] = {
        // positions   // texCoords
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

    const char* quadVertShader = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoords;
        out vec2 TexCoords;
        void main() {
            TexCoords = aTexCoords;
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
        }
    )";

    const char* pixelationFragShader = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoords;

        uniform sampler2D screenTexture;
        uniform vec2 resolution;
        uniform float pixelSize;

        void main() {
            vec2 pixelCoord = floor(TexCoords * resolution / pixelSize) * pixelSize / resolution;
            vec4 color = texture(screenTexture, pixelCoord);
            FragColor = color;
        }
    )";

    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &quadVertShader, nullptr);
    glCompileShader(vertShader);

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &pixelationFragShader, nullptr);
    glCompileShader(fragShader);

    m_postProcessShader = glCreateProgram();
    glAttachShader(m_postProcessShader, vertShader);
    glAttachShader(m_postProcessShader, fragShader);
    glLinkProgram(m_postProcessShader);

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
}

void GLCanvas::RenderFramebufferToScreen() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    wxSize size = GetClientSize();
    double scale = GetContentScaleFactor();
    int w = size.GetWidth() * scale;
    int h = size.GetHeight() * scale;

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

    // re-enable for next frame
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
}

void GLCanvas::CreateSkybox() {
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_skyboxVAO);
    glGenBuffers(1, &m_skyboxVBO);
    glBindVertexArray(m_skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    m_skyboxTexture = LoadTexture("assets/textures/skybox.JPG");

    const char* skyboxVertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;

        out vec3 TexCoords;

        uniform mat4 projection;
        uniform mat4 view;

        void main() {
            TexCoords = aPos;
            vec4 pos = projection * view * vec4(aPos, 1.0);
            gl_Position = pos.xyww;  // Trick to make skybox depth = 1.0
        }
    )";

    const char* skyboxFragmentShaderSource = R"(
        #version 330 core
        in vec3 TexCoords;
        out vec4 FragColor;

        uniform sampler2D skyboxTexture;

        void main() {
            // Map 3D cube coordinates to 2D texture coordinates with tiling
            vec3 absCoords = abs(TexCoords);
            vec2 uv;

            float tileScale = 2.0;

            if (absCoords.x >= absCoords.y && absCoords.x >= absCoords.z) {
                uv = vec2(TexCoords.z / absCoords.x, TexCoords.y / absCoords.x);
            } else if (absCoords.y >= absCoords.x && absCoords.y >= absCoords.z) {
                uv = vec2(TexCoords.x / absCoords.y, TexCoords.z / absCoords.y);
            } else {
                uv = vec2(TexCoords.x / absCoords.z, TexCoords.y / absCoords.z);
            }

            uv = uv * tileScale * 0.5 + 0.5;
            FragColor = texture(skyboxTexture, uv);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &skyboxVertexShaderSource, nullptr);
    glCompileShader(vertexShader);

    GLint success;
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
        wxLogError("Skybox vertex shader compilation failed: %s", infoLog);
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &skyboxFragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
        wxLogError("Skybox fragment shader compilation failed: %s", infoLog);
    }

    m_skyboxShaderProgram = glCreateProgram();
    glAttachShader(m_skyboxShaderProgram, vertexShader);
    glAttachShader(m_skyboxShaderProgram, fragmentShader);
    glLinkProgram(m_skyboxShaderProgram);

    glGetProgramiv(m_skyboxShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_skyboxShaderProgram, 512, nullptr, infoLog);
        wxLogError("Skybox shader program linking failed: %s", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void GLCanvas::RenderSkybox() {
    glDepthFunc(GL_LEQUAL);
    glUseProgram(m_skyboxShaderProgram);

    // remove translation from view matrix for skybox, then apply multi-axis rotation
    glm::mat4 view = glm::mat4(glm::mat3(m_view));
    view = glm::rotate(view, m_rotation,          glm::vec3(0.0f, 1.0f, 0.0f));
    view = glm::rotate(view, m_rotation * 0.37f,  glm::vec3(1.0f, 0.0f, 0.0f));
    view = glm::rotate(view, m_rotation * 0.21f,  glm::vec3(0.0f, 0.0f, 1.0f));

    glUniformMatrix4fv(glGetUniformLocation(m_skyboxShaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_skyboxShaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(m_projection));

    glBindVertexArray(m_skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_skyboxTexture);
    glUniform1i(glGetUniformLocation(m_skyboxShaderProgram, "skyboxTexture"), 0);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}

GLuint GLCanvas::LoadTexture(const std::string& path) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

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
        wxLogStatus("Loaded texture: %s", path.c_str());
    } else {
        wxLogError("Failed to load texture: %s", path.c_str());
        stbi_image_free(data);
    }

    return textureID;
}

void GLCanvas::UpdateTranslationAnimations(float deltaTime) {
    float animationSpeed = 5.0f;

    for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
        auto& obj = m_sceneObjects[i];

        glm::vec3 diff = obj.targetPos - obj.position;
        float distance = glm::length(diff);

        if (distance > 0.001f) {
            obj.position += diff * animationSpeed * deltaTime;
        } else {
            obj.position = obj.targetPos;
        }
    }
}

void GLCanvas::UpdateScaleAnimations(float deltaTime) {
    float animationSpeed = 8.0f;

    for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
        auto& obj = m_sceneObjects[i];

        float scaleDiff = obj.targetScaleMultiplier - obj.currentScaleMultiplier;
        if (fabs(scaleDiff) > 0.001f)
            obj.currentScaleMultiplier += scaleDiff * animationSpeed * deltaTime;
        else
            obj.currentScaleMultiplier = obj.targetScaleMultiplier;

        float targetHover = obj.isHovered ? 1.0f : 0.0f;
        float hoverDiff   = targetHover - obj.hoverAmount;
        if (fabs(hoverDiff) > 0.001f)
            obj.hoverAmount += hoverDiff * animationSpeed * deltaTime;
        else
            obj.hoverAmount = targetHover;
    }
}

glm::vec3 GLCanvas::GetScaledSize(size_t objectIndex) {
    if (objectIndex >= m_sceneObjects.size()) {
        return glm::vec3(1.0f);
    }

    const auto& obj = m_sceneObjects[objectIndex];
    return obj.scale * obj.currentScaleMultiplier;
}

// ---------------------------------------------------------------------------
// Anchor plane helpers
// ---------------------------------------------------------------------------

// Loads the four anchor meshes and places them inside the L-shaped menu.
//
// Menu NDC transform (identity view/proj, rotate -90° X, then scale):
//   NDC.x = menu.pos.x + v.x * menu.scale.x
//   NDC.y = menu.pos.y + v.z * menu.scale.z   (mesh Z maps to screen Y)
//
// The L has a vertical arm on the left and a horizontal base at the bottom.
// We find the inner corner at runtime by sampling vertex extents in each half.
void GLCanvas::InitAnchorObjects() {
    if (m_sceneObjects.empty()) return;

    const auto& menu = m_sceneObjects[0];

    // --- Compute L-shape geometry via MeshBounds ---
    auto xzb    = MeshBounds::ComputeXZBounds(*menu.model);
    auto ndc    = MeshBounds::ToNDCRect(xzb, menu.position, menu.scale);
    auto corner = MeshBounds::FindLCorner(*menu.model, xzb);

    float ndcMinX   = ndc.minX;
    float ndcMaxX   = ndc.maxX;
    float ndcMinY   = ndc.minY;
    float ndcMaxY   = ndc.maxY;
    float armRightX = menu.position.x + corner.armRightX * menu.scale.x;
    float baseTopY  = menu.position.y + corner.baseTopZ  * menu.scale.z;

    // Map = the NDC region NOT occupied by the L-shape (upper-right cutout).
    // Right and top edges align with the actual screen edges, so pin them to ±1.
    m_mapBounds = { armRightX, 1.0f, baseTopY, 1.0f };

    // Arm (left column): NDC X ∈ [ndcMinX, armRightX], full Y height
    float armCX = (ndcMinX + armRightX) * 0.5f;
    // Base (bottom row): full X width, NDC Y ∈ [ndcMinY, baseTopY]
    float baseCY = (ndcMinY + baseTopY) * 0.5f;

    // Upper arm (above base junction) — 2 slots stacked
    float upperH  = ndcMaxY - baseTopY;
    float armY0   = baseTopY + upperH * 0.67f;  // upper slot centre
    float armY1   = baseTopY + upperH * 0.33f;  // lower slot centre

    // Right base (right of arm junction) — 2 slots side by side
    float rightW  = ndcMaxX - armRightX;
    float baseX0  = armRightX + rightW * 0.33f;  // left slot centre
    float baseX1  = armRightX + rightW * 0.67f;  // right slot centre

    const glm::vec2 positions[4] = {
        { armCX,  armY0  },   // upper vertical arm
        { armCX,  armY1  },   // lower vertical arm
        { baseX0, baseCY },   // left horizontal base
        { baseX1, baseCY },   // right horizontal base
    };

    // Scale: fit each object's largest extent into its slot
    // Arm slot size ≈ armWidth × upperH/2; base slot ≈ rightW/2 × baseHeight
    float armWidth   = armRightX - ndcMinX;
    float baseHeight = baseTopY  - ndcMinY;
    float slotDim    = std::min({ armWidth, upperH * 0.5f, rightW * 0.5f, baseHeight });
    float targetHalfSize = slotDim * 0.5f * 0.75f;

    const char* meshPaths[4] = {
        "assets/meshes/rndy.obj",
        "assets/meshes/spky.obj",
        "assets/meshes/strchy.obj",
        "assets/meshes/Gatomon.obj"
    };

    for (int i = 0; i < 4; ++i) {
        SceneObject obj;
        obj.model = std::make_unique<ModelLoader>();
        if (!obj.model->loadModel(meshPaths[i])) {
            wxLogError("Failed to load anchor mesh: %s", meshPaths[i]);
            continue;
        }

        // XZ footprint: after -90°X rotation, model X→NDC X, model Z→NDC Y
        float mnX =  FLT_MAX, mxX = -FLT_MAX;
        float mnZ =  FLT_MAX, mxZ = -FLT_MAX;
        for (const auto& mesh : obj.model->getMeshes()) {
            for (const auto& v : mesh.vertices) {
                mnX = std::min(mnX, v.position.x); mxX = std::max(mxX, v.position.x);
                mnZ = std::min(mnZ, v.position.z); mxZ = std::max(mxZ, v.position.z);
            }
        }

        float maxExtent = std::max(mxX - mnX, mxZ - mnZ);
        float s = (maxExtent > 0.0f) ? (2.0f * targetHalfSize) / maxExtent : 1.0f;

        // Center the object's mesh on its target position
        float meshCX = (mnX + mxX) * 0.5f;
        float meshCZ = (mnZ + mxZ) * 0.5f;

        obj.position  = glm::vec3(positions[i].x - meshCX * s,
                                  positions[i].y - meshCZ * s, 0.0f);
        obj.homePos   = obj.position;  // snap-back target for drag & drop
        obj.targetPos = obj.position;
        obj.scale     = glm::vec3(s);
        obj.rotation  = glm::vec3(0.0f);
        obj.currentScaleMultiplier      = 1.0f;
        obj.targetScaleMultiplier       = 1.0f;
        obj.currentTranslationMultiplier = 1.0f;
        obj.targetTranslationMultiplier  = 1.0f;
        obj.isAnchor  = true;
        obj.meshMinX  = mnX; obj.meshMaxX = mxX;
        obj.meshMinZ  = mnZ; obj.meshMaxZ = mxZ;

        constexpr float spinRates[4] = { 0.6f, 1.1f, 0.4f, 0.85f };
        const glm::vec3 spinAxes[4]  = {
            glm::normalize(glm::vec3(0.0f, 1.0f, 0.3f)),
            glm::normalize(glm::vec3(0.2f, 1.0f, 0.2f)),
            glm::normalize(glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::normalize(glm::vec3(0.4f, 1.0f, 0.0f)),
        };
        obj.spinRate  = spinRates[i];
        obj.spinAxis  = spinAxes[i];
        obj.spinAngle = 0.0f;

        m_sceneObjects.push_back(std::move(obj));
    }
}

glm::vec2 GLCanvas::MouseToNDC(wxPoint mousePos) const {
    wxSize size = GetClientSize();
    float ndcX =  (static_cast<float>(mousePos.x) / size.GetWidth())  * 2.0f - 1.0f;
    float ndcY = -(static_cast<float>(mousePos.y) / size.GetHeight()) * 2.0f + 1.0f;
    return { ndcX, ndcY };
}

void GLCanvas::CreateBorderShader() {
    const char* vertSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* fragSrc = R"(
        #version 330 core
        out vec4 FragColor;
        uniform vec4 uColor;
        void main() {
            FragColor = uColor;
        }
    )";

    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertSrc, nullptr);
    glCompileShader(vert);

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragSrc, nullptr);
    glCompileShader(frag);

    m_borderShaderProgram = glCreateProgram();
    glAttachShader(m_borderShaderProgram, vert);
    glAttachShader(m_borderShaderProgram, frag);
    glLinkProgram(m_borderShaderProgram);

    glDeleteShader(vert);
    glDeleteShader(frag);

    glGenVertexArrays(1, &m_borderVAO);
    glGenBuffers(1, &m_borderVBO);
    glBindVertexArray(m_borderVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_borderVBO);
    glBufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void GLCanvas::RenderBorder(const MeshBounds::NDCRect& rect, glm::vec4 color) {
    if (!m_borderShaderProgram || !m_borderVAO) return;

    float verts[8] = {
        rect.minX, rect.minY,
        rect.maxX, rect.minY,
        rect.maxX, rect.maxY,
        rect.minX, rect.maxY,
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_borderVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    glUseProgram(m_borderShaderProgram);
    glUniform4f(glGetUniformLocation(m_borderShaderProgram, "uColor"),
                color.r, color.g, color.b, color.a);

    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);
    glBindVertexArray(m_borderVAO);
    glDrawArrays(GL_LINE_LOOP, 0, 4);
    glBindVertexArray(0);
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}
