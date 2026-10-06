#ifndef CAMERADIRECTOR_H
#define CAMERADIRECTOR_H

#include <glm/glm.hpp>

#include "Camera.h"
#include "TrialState.h"

// Frames the camera on whatever the current state is about: the scale while
// the verdict is being reached, the lamp for a blessing, Medusa and the
// traveller for a curse.
//
// It writes into the Camera rather than replacing it, so handing control back
// to the user is just a matter of switching it off - the view does not jump.
class CameraDirector
{
public:
    void update(Camera& camera, TrialState state, float deltaTime, float time);

    // Where the traveller is. The escape shot rides behind him, so the
    // director needs to be told; every other shot ignores it.
    void setFollowTarget(const glm::vec3& position) { m_follow = position; }

    // How hard he is running, 0..1. The chase camera leads further ahead and
    // pulls back as he speeds up, which is what makes a sprint read as fast
    // rather than just as movement.
    void setFollowSpeed(float speed01) { m_followSpeed = speed01; }

    // How close the pursuer is, 0..1. The chase camera climbs and looks
    // down as this rises, so she stops blocking the view forward.
    void setDanger(float danger01) { m_danger = danger01; }

    // 0..1: the chamber is coming apart overhead. The chase camera normally
    // looks down at the floor ahead, which puts the top of the walls just out
    // of frame - so it tilts up to watch the ceiling break, then settles.
    void setLookUp(float amount01) { m_lookUp = amount01; }

    // Seconds into the current state, for shots that change part way.
    void setStateTime(float seconds) { m_stateTime = seconds; }

    // Which way the traveller is facing, in degrees. The Caught shot swings
    // round to his face, and his face can be pointing anywhere.
    void setFollowHeading(float degrees) { m_followHeading = degrees; }

    void setEnabled(bool value) { m_enabled = value; }
    bool enabled() const { return m_enabled; }

private:
    struct Shot
    {
        glm::vec3 target;
        float distance;
        float yaw;      // degrees
        float pitch;    // degrees
    };

    bool m_enabled = true;
    glm::vec3 m_follow{ 0.0f, 0.0f, 0.0f };
    float m_followSpeed = 0.0f;
    float m_danger      = 0.0f;
    float m_lookUp      = 0.0f;
    float m_stateTime   = 0.0f;
    float m_followHeading = 180.0f;

    Shot shotFor(TrialState state) const;
};

#endif // CAMERADIRECTOR_H
