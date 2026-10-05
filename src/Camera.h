#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

// Dual-mode camera.
//
//   Orbit   - swings around a target point. Best for inspecting the chamber
//             and for the final demo.
//   FreeFly - WASD + QE movement in the direction you are looking.
//
// Switching modes preserves the current viewpoint exactly, so there is no
// jump when you press C.
//
// This class deliberately knows nothing about GLFW; main.cpp translates input
// events into these calls.
enum class CameraMode
{
    Orbit,
    FreeFly
};

class Camera
{
public:
    Camera();

    void setMode(CameraMode mode);
    void toggleMode();
    CameraMode mode() const { return m_mode; }

    // Mouse drag. Interpretation depends on the mode.
    void look(float deltaX, float deltaY);

    // Scroll wheel: orbit distance in Orbit mode, field of view in FreeFly.
    void zoom(float scrollDelta);

    // FreeFly only. localDirection is (right, up, forward) in camera space.
    void move(const glm::vec3& localDirection, float deltaTime);

    void setTarget(const glm::vec3& target) { m_target = target; }
    void setDistance(float distance);
    void setSpeed(float speed) { m_speed = speed; }

    // Used by CameraDirector to ease the camera between shots.
    // First person drives the camera directly rather than orbiting.
    void setPosition(const glm::vec3& position) { m_position = position; }

    void setYaw(float yaw) { m_yaw = yaw; }
    void setPitch(float pitch);
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    float distance() const { return m_distance; }

    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;

    glm::vec3 position() const;
    glm::vec3 front() const;
    glm::vec3 target() const { return m_target; }
    float fov() const { return m_fov; }

private:
    CameraMode m_mode = CameraMode::Orbit;

    // Shared orientation. Degrees; yaw sweeps around Y, pitch is elevation.
    float m_yaw   = 45.0f;
    float m_pitch = 20.0f;

    // Orbit state
    glm::vec3 m_target{ 0.0f, 0.0f, 0.0f };
    float m_distance = 12.0f;

    // FreeFly state
    glm::vec3 m_position{ 0.0f, 0.0f, 0.0f };
    float m_speed = 6.0f;

    // Projection
    float m_fov       = 45.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane  = 200.0f;

    float m_lookSensitivity = 0.25f;

    // Unit vector for the current yaw/pitch.
    glm::vec3 orientationVector() const;
};

#endif // CAMERA_H
