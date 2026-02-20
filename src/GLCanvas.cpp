#include "GLCanvas.h"
#include "stb_image.h"
#include <cmath>

wxBEGIN_EVENT_TABLE(GLCanvas, wxGLCanvas)
    EVT_PAINT(GLCanvas::OnPaint)
    EVT_SIZE(GLCanvas::OnSize)
    EVT_IDLE(GLCanvas::OnIdle)
    EVT_MOTION(GLCanvas::OnMouseMove)
    EVT_LEFT_DOWN(GLCanvas::OnMouseClick)
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

    m_rotation += 0.1745f * deltaTime;

    UpdateScaleAnimations(deltaTime);
    UpdateTranslationAnimations(deltaTime);

    Refresh(false);
    event.RequestMore();
}

void GLCanvas::OnMouseMove(wxMouseEvent& event) {
    m_mousePos = event.GetPosition();
}

void GLCanvas::OnMouseClick(wxMouseEvent& event) {
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

    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(0.0f, 0.0f, -4.0f));  // Move back from the model
    view = glm::rotate(view, glm::radians(20.0f), glm::vec3(1.0f, 0.0f, 0.0f));  // Look down 20 degrees
    view = glm::translate(view, glm::vec3(0.0f, -0.5f, 0.0f));  // Raise camera
    m_view = view;

    wxSize size = GetClientSize();
    float aspect = static_cast<float>(size.GetWidth()) / static_cast<float>(size.GetHeight());
    m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
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

            // Simple lighting - light positioned above camera
            vec3 lightPos = vec3(0.0, 3.0, 4.0);
            vec3 lightDir = normalize(lightPos - FragPos);
            vec3 norm = normalize(Normal);

            // Calculate diffuse from main light
            float diff = max(dot(norm, lightDir), 0.0);

            // Add frontal lighting for camera-facing surfaces
            vec3 cameraDir = vec3(0.0, 0.0, 1.0);
            float frontalLight = max(dot(norm, cameraDir), 0.0);

            vec3 ambient = 0.4 * objectColor;
            vec3 diffuse = 0.7 * diff * objectColor;
            vec3 frontal = 0.2 * frontalLight * objectColor;

            vec3 result = ambient + diffuse + frontal;
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

        void main() {
            float intensity = vLightIntensity * 0.5;

            if (intensity > 0.45) {
                FragColor = vec4(0.96, 0.96, 0.96, 1.0);
            } else if (intensity > 0.2) {
                FragColor = vec4(0.47, 0.27, 0.87, 1.0);
            } else if (intensity > 0.15) {
                FragColor = vec4(0.41, 0.91, 0.61, 1.0);
            } else {
                FragColor = vec4(0.95, 0.2, 0.3, 1.0);
            }
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

    glUseProgram(m_shaderProgram);

    GLint modelLoc = glGetUniformLocation(m_shaderProgram, "model");
    GLint viewLoc = glGetUniformLocation(m_shaderProgram, "view");
    GLint projLoc = glGetUniformLocation(m_shaderProgram, "projection");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));

    for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
        auto& obj = m_sceneObjects[i];
        if (!obj.model) continue;

        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, obj.position);
        modelMatrix = glm::rotate(modelMatrix, m_rotation, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::vec3 finalScale = GetScaledSize(i);
        modelMatrix = glm::scale(modelMatrix, finalScale);

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
        obj.model->draw(m_shaderProgram);
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

            float tileScale = 8.0;

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

    // remove translation from view matrix for skybox
    glm::mat4 view = glm::mat4(glm::mat3(m_view));

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
        float diff = obj.targetScaleMultiplier - obj.currentScaleMultiplier;

        if (fabs(diff) > 0.001f) {
            obj.currentScaleMultiplier += diff * animationSpeed * deltaTime;
        } else {
            obj.currentScaleMultiplier = obj.targetScaleMultiplier;
        }
    }
}

glm::vec3 GLCanvas::GetScaledSize(size_t objectIndex) {
    if (objectIndex >= m_sceneObjects.size()) {
        return glm::vec3(1.0f);
    }

    const auto& obj = m_sceneObjects[objectIndex];
    return obj.scale * obj.currentScaleMultiplier;
}
