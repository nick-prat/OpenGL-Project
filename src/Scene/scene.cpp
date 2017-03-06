#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include "scene.hpp"

#include <iostream>

#include <Core/camera.hpp>
#include <Models/dynamicmodel.hpp>
#include <Utilities/exceptions.hpp>
#include <Utilities/log.hpp>
#include <Render/renderobject.hpp>
#include <Render/renderchain.hpp>
#include <Resources/resourcehandler.hpp>

using luabridge::LuaRef;
using json = nlohmann::json;

// TODO Implement some sort of multithreading in lua (coroutines?)
// TODO Check to see if main.lua exists (a scene doesn't need to have lua support)

// NOTE How slow is calling lua functions?
// NOTE What should lua be capable of doing?

class ShittySystem : public Scene::entity_manager_t::System<ShittySystem,CRender> {
public:
    ShittySystem(Scene::entity_manager_t* entMan)
    : System(entMan) {}

};

Scene::Scene()
        : m_scenename("null")
        , m_luaEnabled(false)
        , m_display(nullptr) {}

Scene::Scene(const Display* const display, std::string scenename)
        : m_luaEnabled(true)
        , m_display(display) {

    luaL_openlibs(m_luaState.get());

    auto entId = m_entityManager.createEntity();
    auto& ent = m_entityManager.getEntity(entId);
    ent.addComponent<CRender>();
    ShittySystem sys(&m_entityManager);
    sys.subscribe(ent);

    std::string script =  "Resources/Scenes/" + scenename + "/main.lua";
    std::string filename = "Resources/Scenes/" + scenename + "/scene.json";

    buildLuaNamespace();

    luaL_dofile(m_luaState.get(), script.c_str());

    registerLuaFunctions();

    std::string contents;
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
    } else {
        throw GenericException(filename + " doesn't exist");
    }

    try {
        const json scene = json::parse(contents);
        m_resourceHandler.loadResources(scene);

        if(scene.find("staticObjects") != scene.end()) {
            std::vector<json> objects = scene["staticObjects"];
            for(const json& object : objects) {
                try {
                    std::string type = object["type"];
                    IRenderObject* rObject = nullptr;

                    // NOTE Are there other types of render obects i might want to load?
                    if(type == "line") {
                        rObject = m_resourceHandler.generateLine(object);
                    } else if(type == "staticmodel") {
                        rObject = m_resourceHandler.generateModel(object, m_resourceHandler.getResource<Model3D>(object["resource"]));
                    } else if(type == "rawstaticmodel") {
                        rObject = m_resourceHandler.generateModel(object);
                    }

                    if(rObject != nullptr) {
                        m_renderObjects.push_back(std::unique_ptr<IRenderObject>(rObject));
                    }
                } catch (BadResource& error) {
                    error.printError();
                } catch (std::domain_error& error) {
                    Log::getErrorLog() << error.what() << '\n';
                }
            }
        }

        // TODO Implement actual dynamic model loading
        // NOTE What is "actual dynamic loading"?
        if(scene.find("dynamicObjects") != scene.end()) {
            std::vector<json> objects = scene["dynamicObjects"];
            for(const json& object : objects) {
                try {
                    auto name = object["name"];
                    //m_dynamicModels[name] = std::make_unique<DynamicModel>(m_resourceHandler.GetResource<Model3D>(object["resource"]));
                } catch (BadResource& error) {
                    error.printError();
                } catch (std::domain_error& error) {
                    Log::getErrorLog() << error.what() << '\n';
                }
            }
        }
    } catch(std::exception& error) {
        Log::getErrorLog() << "Parsing of " << filename << " failed: " << error.what() << '\n';
    }

    for(const auto& shader : m_resourceHandler.getShaders()) {
        m_renderChain.attachShader(shader.second.get());
    }
}

Scene::Scene(Scene&& scene)
        : m_renderObjects(std::move(scene.m_renderObjects))
        , m_entityManager(std::move(scene.m_entityManager))
        , m_renderChain(std::move(scene.m_renderChain))
        , m_resourceHandler(std::move(scene.m_resourceHandler))
        , m_scenename(scene.m_scenename)
        , m_luaState(std::move(scene.m_luaState))
        , m_luaKeyBinds(std::move(scene.m_luaKeyBinds))
        , m_startFunc(std::move(scene.m_startFunc))
        , m_renderFunc(std::move(scene.m_renderFunc))
        , m_luaEnabled(scene.m_luaEnabled)
        , m_display(scene.m_display) {
    scene.m_luaEnabled = false;
    scene.m_display = nullptr;
}

Scene::~Scene() {}

Scene& Scene::operator=(Scene&& scene) {
    m_renderObjects = std::move(scene.m_renderObjects);
    m_entityManager = std::move(scene.m_entityManager);
    m_renderChain = std::move(scene.m_renderChain);
    m_resourceHandler = std::move(scene.m_resourceHandler);
    m_scenename = std::move(scene.m_scenename);
    m_luaState = std::move(scene.m_luaState);
    m_luaKeyBinds = std::move(scene.m_luaKeyBinds);
    m_startFunc = std::move(scene.m_startFunc);
    m_renderFunc = std::move(scene.m_renderFunc);
    m_luaEnabled = scene.m_luaEnabled;
    m_display = scene.m_display;

    scene.m_luaEnabled = false;
    scene.m_display = nullptr;
    return *this;
}

// NOTE What other functions are necessary to expose to lua?
void Scene::buildLuaNamespace() {
    luabridge::getGlobalNamespace(m_luaState.get())
        .beginClass<glm::vec2>("vec2")
            .addConstructor<void(*)(float, float)>()
            .addData("x", &glm::vec2::x)
            .addData("y", &glm::vec2::y)
        .endClass()
        .beginClass<glm::vec3>("vec3")
            .addConstructor<void(*)(float, float, float)>()
            .addData("x", &glm::vec3::x)
            .addData("y", &glm::vec3::y)
            .addData("z", &glm::vec3::z)
        .endClass()
        .beginClass<glm::vec4>("vec4")
            .addConstructor<void(*)(float, float, float, float)>()
            .addData("x", &glm::vec4::x)
            .addData("y", &glm::vec4::y)
            .addData("z", &glm::vec4::z)
            .addData("w", &glm::vec4::w)
        .endClass()
        .beginNamespace("Game")
            .beginClass<Camera>("Camera")
                .addFunction("MoveCamera", &Camera::moveCamera)
                .addFunction("SetCamera", &Camera::setPosition)
                .addFunction("GetPosition", &Camera::getPosition)
                .addFunction("GetDirection", &Camera::getDirection)
            .endClass()
            .beginClass<Scene>("Scene")
                .addFunction("SetAmbientIntensity", &Scene::setAmbientIntensity)
                .addFunction("SetAmbientColor", &Scene::setAmbientColor)
                .addFunction("BindFunctionToKey", &Scene::bindFunctionToKey)
                .addFunction("GetCamera", &Scene::getCamera)
                .addFunction("GetEntity", &Scene::getEntity)
                .addFunction("GetEntityCount", &Scene::getEntityCount)
                .addFunction("CreateEntity", &Scene::createEntity)
            .endClass()
        .endNamespace();

    luabridge::setGlobal(m_luaState.get(), this, "Level");
}

// NOTE Are there other necessary function the engine might want to call?
void Scene::registerLuaFunctions() {
    m_startFunc = std::make_unique<LuaRef>(luabridge::getGlobal(m_luaState.get(), "Start"));
    if(!m_startFunc->isFunction()) {
        throw GenericException("Start function wasn't found");
    }

    m_renderFunc = std::make_unique<LuaRef>(luabridge::getGlobal(m_luaState.get(), "GameLoop"));
    if(!m_renderFunc->isFunction()) {
        throw GenericException("Render function wasn't found");
    }
}

// Is this the best way to do it?
void Scene::start() {
    (*m_startFunc)();
}

// NOTE Do I want to call the render func or perform a render first?
void Scene::gameLoop() {
    m_renderChain.render(m_display);
    (*m_renderFunc)();
}

// TODO Find some way to expose what keys are pressed to lua
void Scene::bindFunctionToKey(int ikey, LuaRef function, bool repeat) {
    if(!function.isFunction()) {
        throw GenericException("function wasn't found");
    }

    InputKey key = (InputKey)ikey;
    m_luaKeyBinds[key] = std::make_unique<LuaRef>(function);
    if(repeat) {
        m_display->getInputController()->registerWhileKeyPressed(key, [this](InputKey key) {
            (*m_luaKeyBinds[key])();
        });
    } else {
        m_display->getInputController()->registerOnKeyPressed(key, [this](InputKey key) {
            (*m_luaKeyBinds[key])();
        });
    }
}

void Scene::setAmbientIntensity(float intensity) {
    m_renderChain.setAmbientIntensity(intensity);
}

void Scene::setAmbientColor(const glm::vec3 &color) {
    m_renderChain.setAmbientColor(color);
}

// NOTE Do i want to be able to easily destroy an entity?
unsigned int Scene::createEntity() {
    return 0;
}

// TODO Expand on the capabilites of an entity, ie. how does the user interact with them?
Scene::entity_t* Scene::getEntity(unsigned int id) const {
    return nullptr;
}

int Scene::getEntityCount() const {
    return 0;
}
