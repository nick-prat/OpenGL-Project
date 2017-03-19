#include "dynamicmodel.hpp"

#include <glm/gtx/transform.hpp>

#include <Core/display.hpp>
#include <Core/camera.hpp>

DynamicModel::DynamicModel(const Model3D& model3D)
        : StaticModel(model3D, glm::mat4(1.0f))
        , m_visible(true)
        , m_translate({1.0f})
        , m_scale({1.0f})
        , m_rotate({1.0f}) {}

DynamicModel::DynamicModel(const StaticModel& model)
        : StaticModel(model)
        , m_visible(true)
        , m_translate({1.0f})
        , m_scale({1.0f})
        , m_rotate({1.0f}) {}

DynamicModel::DynamicModel(StaticModel&& model)
        : StaticModel(std::move(model))
        , m_visible(true)
        , m_translate({1.0f})
        , m_scale({1.0f})
        , m_rotate({1.0f}) {}

void DynamicModel::render(const Shader& shader, const Display& display) const {
    if(m_visible) {
        StaticModel::render(shader, display);
    }
}

glm::mat4 DynamicModel::generateMVP(const Display& display) const {
    return display.getProjectionMatrix() * display.getCamera()->getViewMatrix() * getWorld();
}

void DynamicModel::translate(const glm::vec3& trans) {
    m_translate = glm::translate(m_translate, trans);
}

void DynamicModel::rotate(const float& degrees, const glm::vec3& rotate) {
    m_rotate = glm::rotate(m_rotate, degrees, rotate);
}

void DynamicModel::scale(const glm::vec3& scale) {
    m_scale = glm::scale(m_scale, scale);
}

void DynamicModel::setVisible(const bool visible) {
    m_visible = visible;
}

bool DynamicModel::isVisible() const {
    return m_visible;
}

glm::mat4 DynamicModel::getWorld() const {
    return m_translate * m_rotate * m_scale;
}
