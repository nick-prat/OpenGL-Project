#include "display.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Util/exceptions.hh>
#include <Util/log.hh>
#include <Core/camera.hh>

Opal::Display::Display()
        : m_projMatrix({1.0f}) {}

Opal::Display::Display(unsigned int width, unsigned int height, unsigned int major, unsigned int minor, const std::string &title)
        : m_width(width)
        , m_height(height)
        , m_projMatrix(glm::mat4(1.0f)) {

    if(!glfwInit()) {
        throw GenericException("Couldn't initialize GLFW3\n");
    }

    glfwSetErrorCallback([](int error, const char* desc) {
        Log::getErrorLog<SyncLogger>() << "ERROR: " << "(" << error << ")" << " " << desc << '\n';
    });

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if(!m_window) {
        throw GenericException("Couldn't create window\n");
    }

    glfwMakeContextCurrent(m_window);

    glfwSetWindowUserPointer(m_window, this);

    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    });

    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
        Display* display = reinterpret_cast<Display*>(glfwGetWindowUserPointer(window));
        if(display == nullptr) {
            return;
        }

        if(action == GLFW_PRESS) {
            display->updateKey(key, true);
        } else if(action == GLFW_RELEASE) {
            display->updateKey(key, false);
        }
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int /*mods*/) {
        Display* display = reinterpret_cast<Display*>(glfwGetWindowUserPointer(window));
        if(display == nullptr) {
            return;
        }
        if(action == GLFW_PRESS) {
            display->updateKey(button, true);
        } else if(action == GLFW_RELEASE) {
            display->updateKey(button, false);
        }
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
        Display* display = reinterpret_cast<Display*>(glfwGetWindowUserPointer(window));
        if(display == nullptr) {
            return;
        }
        display->updateCursorPosition(xpos, ypos);
    });

    m_projMatrix = glm::perspective(glm::radians(60.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
}

Opal::Display::Display(Display &&display)
        : m_window(display.m_window)
        , m_camera(std::move(display.m_camera))
        , m_width(display.m_width)
        , m_height(display.m_height)
        , m_projMatrix(std::move(display.m_projMatrix)) {
    display.m_window = nullptr;
    display.m_width = 0;
    display.m_height = 0;

    glfwTerminate();
}

Opal::Display::~Display() {
    glfwDestroyWindow(m_window);
}

Opal::Display &Opal::Display::operator=(Display &&display) {
    m_window = display.m_window;
    m_width = display.m_width;
    m_height = display.m_height;
    m_camera = std::move(display.m_camera);
    m_projMatrix = std::move(display.m_projMatrix);

    display.m_window = nullptr;
    display.m_width = 0;
    display.m_height = 0;

    if(m_window != nullptr) {
        glfwSetWindowUserPointer(m_window, this);
    }

    return *this;
}

void Opal::Display::resize(int width, int height) {
    glfwSetWindowSize(m_window, width, height);
}

void Opal::Display::update() {
    double time = glfwGetTime();
    m_timeScale = (time - m_prevTime) * 60;
    m_prevTime = time;
    callKeyLambdas();
    glfwSwapBuffers(m_window);
    glfwPollEvents();
}

Opal::Camera &Opal::Display::getCamera() {
    return m_camera;
}

const Opal::Camera &Opal::Display::getCamera() const {
    return m_camera;
}

glm::mat4 Opal::Display::getProjectionMatrix() const {
    return m_projMatrix;
}

unsigned int Opal::Display::getWidth() const {
    return m_width;
}

unsigned int Opal::Display::getHeight() const {
    return m_height;
}

bool Opal::Display::windowShouldClose() const {
    return glfwWindowShouldClose(m_window);
}

void Opal::Display::setMouseCapture(bool capture) {
    if(capture) {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Opal::Display::setClearColor(const glm::vec4 &color) {
    glClearColor(color.x, color.y, color.z, color.w);
}

void Opal::Display::setVsync(bool enabled) {
    if(enabled) {
        glfwSwapInterval(1);
    } else {
        glfwSwapInterval(0);
    }
}

// TODO Implement set cursor position
void Opal::Display::setCursorPosition(const glm::vec2 &pos) {
    if(m_window != nullptr) {
        glfwSetCursorPos(m_window, glm::clamp(pos.x, 0.0f, 1.0f), glm::clamp(pos.y, 0.0f, 1.0f));
    }
}

// TODO Implement set cursor visible
void Opal::Display::setCursorVisible(bool visible) {

}

void Opal::Display::setWireFrame(bool wireframe) {
    if(wireframe) {
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
        glEnable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void Opal::Display::clearWhileKeyPressed() {
    m_whileKeyPressed.clear();
}

void Opal::Display::deregisterWhileKeyPressed(const InputKey key) {
    m_whileKeyPressed.erase(key);
}

void Opal::Display::registerWhileKeyPressed(const InputKey key, const std::function<void(InputKey)> &lambda) {
    m_whileKeyPressed[key] = lambda;
}

void Opal::Display::clearOnKeyPressed() {
    m_onKeyPressed.clear();
}

void Opal::Display::deregisterOnKeyPressed(const InputKey key) {
    m_onKeyPressed.erase(key);
}

void Opal::Display::registerOnKeyPressed(const InputKey key, const std::function<void(InputKey)> &lambda) {
    m_onKeyPressed[key] = lambda;
}

void Opal::Display::centerCursor() {

}

void Opal::Display::bindCursorUpdate(std::function<void(int, int)> func) {
    m_cursorFunc = func;
}

void Opal::Display::callKeyLambdas() {
    for(auto &[key, repeat] : m_pressedKeys) {
        auto lambda = m_whileKeyPressed.find(key);
        if(lambda != m_whileKeyPressed.end()) {
            lambda->second(key);
        }

        if(!repeat) {
            repeat = true;
            if(auto lambda = m_onKeyPressed.find(key); lambda != m_onKeyPressed.end()) {
                lambda->second(key);
            }
        }
    }
}

void Opal::Display::updateCursorPosition(const int xpos, const int ypos) {
    m_cursorPos.first = xpos;
    m_cursorPos.second = ypos;
    if(m_cursorFunc) {
        m_cursorFunc(xpos, ypos);
    }
}

const std::pair<int, int> &Opal::Display::getCursorPosition() const {
    return m_cursorPos;
}

bool Opal::Display::isKeyPressed(const InputKey key) const {
    return m_pressedKeys.find(key) != m_pressedKeys.end();
}

void Opal::Display::updateKey(const int key, const bool pressed) {
    InputKey ikey = static_cast<InputKey>(key);

    if(pressed  &&m_pressedKeys.find(ikey) == m_pressedKeys.end()) {
        m_pressedKeys[ikey] = false;
    } else {
        m_pressedKeys.erase(ikey);
    }
}