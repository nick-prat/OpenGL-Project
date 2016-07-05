//
// Created by nprat on 7/4/16.
//

#ifndef _OPENGL_OPENGL_H
#define _OPENGL_OPENGL_H

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Model/renderchain.h>
#include <Display/display.h>
#include <Model/ShittyObject.h>

class OpenGL
{
public:
    OpenGL();
    ~OpenGL();

    bool InitOpenGL();
    void Destroy();

    void DisplayFunc();

    static bool CreateInstance();
    static void DestroyInstance();
    static std::shared_ptr<OpenGL> getInstance();

private:
    static std::shared_ptr<OpenGL> m_openGL;

private:
    std::shared_ptr<Display> m_display;
    std::shared_ptr<RenderChain> m_renderChain;
    std::shared_ptr<ShittyObject> m_shittyObject;
};


#endif //OPENGL_OPENGL_H
