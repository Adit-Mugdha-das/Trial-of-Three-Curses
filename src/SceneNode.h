#ifndef SCENENODE_H
#define SCENENODE_H

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Material.h"
#include "Mesh.h"
#include "Shader.h"

// A node in the scene hierarchy.
//
//   world = parent.world * local
//   local = T(position) * T(pivot) * R(rotation) * T(-pivot) * S(scale)
//
// The pivot term is the composite-transform requirement built in: set a pivot
// and the node rotates about that offset point instead of its own origin.
// That is exactly what the Djinn lamp's lid needs to swing on its hinge rather
// than spin through the lamp body.
//
// Nodes own their children. Meshes are NOT owned - many nodes share one mesh
// (every snake segment is the same cylinder), so the Scene owns those.
class SceneNode
{
public:
    explicit SceneNode(std::string nodeName = {});

    SceneNode* createChild(std::string nodeName = {});
    SceneNode* find(const std::string& nodeName);

    glm::mat4 localMatrix() const;

    // Recomputes this node's world matrix and every descendant's.
    void updateWorld(const glm::mat4& parentWorld = glm::mat4(1.0f));

    // Draws the opaque nodes of this subtree. Hiding a node hides its
    // children too. Transparent nodes are skipped here - they have to be
    // depth-sorted, which a recursive walk cannot do.
    void draw(const Shader& shader) const;

    // Gathers the visible transparent nodes of this subtree so the Scene can
    // sort them back-to-front before drawing.
    void collectTransparent(std::vector<const SceneNode*>& out) const;

    // Issues this one node's draw call, ignoring children.
    void drawSelf(const Shader& shader) const;

    // Shadow pass. Uploads only uModel, because the depth shader has no other
    // uniforms - routing this through draw() would fire a warning for every
    // material uniform the depth program does not declare.
    void drawDepth(const Shader& shader) const;

    // Sets `petrifies` on this node and everything under it.
    void setPetrifiesRecursive(bool value);

    const glm::mat4& world() const { return m_world; }
    glm::vec3 worldPosition() const { return glm::vec3(m_world[3]); }
    const std::vector<std::unique_ptr<SceneNode>>& children() const { return m_children; }

    // Direction the node's local +Z points, in world space. Used to aim
    // Medusa's eye spotlights down her head's forward axis in Phase 9.
    glm::vec3 worldForward() const;

    std::string name;

    glm::vec3 position{ 0.0f, 0.0f, 0.0f };
    glm::vec3 rotation{ 0.0f, 0.0f, 0.0f };   // Euler degrees, applied Y then X then Z
    glm::vec3 pivot   { 0.0f, 0.0f, 0.0f };   // rotate about this local offset
    glm::vec3 scale   { 1.0f, 1.0f, 1.0f };

    Material material;

    const Mesh* mesh = nullptr;
    bool visible = true;

    // Opts this node into the shader's petrification blend. Only the
    // traveller's body sets it; everything else ignores the effect entirely.
    bool petrifies = false;

private:
    glm::mat4 m_world{ 1.0f };
    std::vector<std::unique_ptr<SceneNode>> m_children;
};

#endif // SCENENODE_H
