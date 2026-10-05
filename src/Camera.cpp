#include "Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace
{
    // Stops the view from flipping over at the poles.
    constexpr float kMaxPitch = 89.0f;

    const glm::vec3 kWorldUp(0.0f, 1.0f, 0.0f);
}

Camera::Camera()
{
    // Keep the FreeFly position consistent with the initial orbit viewpoint.
    m_position = m_target - orientationVector() * m_distance;
}

glm::vec3 Camera::orientationVector() const
{
    const float yaw   = glm::radians(m_yaw);
    const float pitch = glm::radians(m_pitch);

    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw)
    ));
}

glm::vec3 Camera::position() const
{
    if (m_mode == CameraMode::Orbit)
    {
        // Orbit sits on a sphere around the target, looking inward.
        return m_target + orientationVector() * m_distance;
    }

    return m_position;
}

glm::vec3 Camera::front() const
{
    if (m_mode == CameraMode::Orbit)
    {
        return -orientationVector();
    }

    return orientationVector();
}

void Camera::setMode(CameraMode mode)
{
    if (mode == m_mode)
    {
        return;
    }

    if (mode == CameraMode::FreeFly)
    {
        // Hand the current orbit viewpoint over to free-fly. The orbit camera
        // looks inward, so the outward yaw/pitch must be flipped to become a
        // forward-facing direction.
        m_position = position();
        m_yaw   += 180.0f;
        m_pitch  = -m_pitch;
    }
    else
    {
        // Drop an orbit target out in front of where we are looking, then
        // flip the orientation back to outward-facing.
        m_target = m_position + orientationVector() * m_distance;
        m_yaw   += 180.0f;
        m_pitch  = -m_pitch;
    }

    // Keep yaw in a sane range so it never drifts to huge values.
    m_yaw = std::fmod(m_yaw, 360.0f);

    m_mode = mode;
}

void Camera::toggleMode()
{
    setMode(m_mode == CameraMode::Orbit ? CameraMode::FreeFly : CameraMode::Orbit);
}

void Camera::look(float deltaX, float deltaY)
{
    const float dx = deltaX * m_lookSensitivity;
    const float dy = deltaY * m_lookSensitivity;

    if (m_mode == CameraMode::Orbit)
    {
        // Feels like grabbing the object and spinning it.
        m_yaw   -= dx;
        m_pitch += dy;
    }
    else
    {
        // Standard first-person look.
        m_yaw   += dx;
        m_pitch -= dy;
    }

    m_pitch = std::clamp(m_pitch, -kMaxPitch, kMaxPitch);
}

void Camera::setPitch(float pitch)
{
    m_pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);
}

void Camera::setDistance(float distance)
{
    m_distance = std::clamp(distance, 1.0f, 120.0f);
}

void Camera::zoom(float scrollDelta)
{
    if (m_mode == CameraMode::Orbit)
    {
        // Multiplicative so each notch feels the same at any distance.
        setDistance(m_distance * (1.0f - scrollDelta * 0.1f));
    }
    else
    {
        m_fov = std::clamp(m_fov - scrollDelta * 2.0f, 15.0f, 90.0f);
    }
}

void Camera::move(const glm::vec3& localDirection, float deltaTime)
{
    if (m_mode != CameraMode::FreeFly)
    {
        return;
    }

    if (glm::dot(localDirection, localDirection) < 1e-6f)
    {
        return;
    }

    const glm::vec3 forward = orientationVector();
    const glm::vec3 right   = glm::normalize(glm::cross(forward, kWorldUp));
    const glm::vec3 up      = glm::normalize(glm::cross(right, forward));

    const glm::vec3 direction = glm::normalize(
        right   * localDirection.x +
        up      * localDirection.y +
        forward * localDirection.z
    );

    m_position += direction * m_speed * deltaTime;
}

glm::mat4 Camera::view() const
{
    const glm::vec3 eye = position();

    if (m_mode == CameraMode::Orbit)
    {
        return glm::lookAt(eye, m_target, kWorldUp);
    }

    return glm::lookAt(eye, eye + front(), kWorldUp);
}

glm::mat4 Camera::projection(float aspect) const
{
    // Guard against a zero-width framebuffer when the window is minimised.
    if (aspect < 0.01f)
    {
        aspect = 0.01f;
    }

    return glm::perspective(glm::radians(m_fov), aspect, m_nearPlane, m_farPlane);
}
