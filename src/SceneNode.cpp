#include "SceneNode.h"

#include <glad/glad.h>

#include <utility>

#include <glm/gtc/matrix_transform.hpp>

SceneNode::SceneNode(std::string nodeName)
    : name(std::move(nodeName))
{
}

SceneNode* SceneNode::createChild(std::string nodeName)
{
    m_children.push_back(std::make_unique<SceneNode>(std::move(nodeName)));
    return m_children.back().get();
}

SceneNode* SceneNode::find(const std::string& nodeName)
{
    if (name == nodeName)
    {
        return this;
    }

    for (const std::unique_ptr<SceneNode>& child : m_children)
    {
        if (SceneNode* found = child->find(nodeName))
        {
            return found;
        }
    }

    return nullptr;
}

glm::mat4 SceneNode::localMatrix() const
{
    // Read bottom-up: scale first, then rotate about the pivot, then place.
    glm::mat4 m(1.0f);

    m = glm::translate(m, position + pivot);

    m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    m = glm::translate(m, -pivot);
    m = glm::scale(m, scale);

    return m;
}

void SceneNode::updateWorld(const glm::mat4& parentWorld)
{
    m_world = parentWorld * localMatrix();

    for (const std::unique_ptr<SceneNode>& child : m_children)
    {
        child->updateWorld(m_world);
    }
}

glm::vec3 SceneNode::worldForward() const
{
    return glm::normalize(glm::vec3(m_world * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));
}

void SceneNode::drawSelf(const Shader& shader) const
{
    if (mesh == nullptr)
    {
        return;
    }

    shader.setMat4("uModel", m_world);

    // Inverse-transpose keeps normals correct under non-uniform scale,
    // which this scene uses heavily (squashed lamp body, growing rings).
    shader.setMat3("uNormalMatrix",
                   glm::mat3(glm::transpose(glm::inverse(m_world))));

    shader.setInt("uPetrifyEnabled", petrifies ? 1 : 0);

    material.upload(shader);
    if (material.sky)
    {
        // The sky dome is seen from inside: draw both faces.
        const GLboolean culling = glIsEnabled(GL_CULL_FACE);
        glDisable(GL_CULL_FACE);
        mesh->draw();
        if (culling) { glEnable(GL_CULL_FACE); }
        return;
    }
    mesh->draw();
}

void SceneNode::draw(const Shader& shader) const
{
    if (!visible)
    {
        return;
    }

    if (!material.isTransparent())
    {
        drawSelf(shader);
    }

    for (const std::unique_ptr<SceneNode>& child : m_children)
    {
        child->draw(shader);
    }
}

void SceneNode::drawDepth(const Shader& shader) const
{
    if (!visible)
    {
        return;
    }

    // Transparent nodes are skipped: the energy rings are light itself and
    // must not cast a solid silhouette across the chamber.
    if (mesh != nullptr && !material.isTransparent() && !material.sky)
    {
        shader.setMat4("uModel", m_world);
        mesh->draw();
    }

    for (const std::unique_ptr<SceneNode>& child : m_children)
    {
        child->drawDepth(shader);
    }
}

void SceneNode::setPetrifiesRecursive(bool value)
{
    petrifies = value;

    for (const std::unique_ptr<SceneNode>& child : m_children)
    {
        child->setPetrifiesRecursive(value);
    }
}

void SceneNode::collectTransparent(std::vector<const SceneNode*>& out) const
{
    if (!visible)
    {
        return;
    }

    if (mesh != nullptr && material.isTransparent())
    {
        out.push_back(this);
    }

    for (const std::unique_ptr<SceneNode>& child : m_children)
    {
        child->collectTransparent(out);
    }
}
