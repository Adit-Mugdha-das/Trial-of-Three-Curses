#ifndef HUD_H
#define HUD_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Shader.h"
#include "TrialState.h"

// A minimal screen-space overlay: a progress bar for the current state, a
// colour swatch identifying it, and a marker showing where the heart's weight
// sits relative to the balance tolerance.
//
// Deliberately not a text renderer. Bitmap fonts are a project of their own,
// and colour plus position communicates this much state perfectly well.
class Hud
{
public:
    bool init();
    void shutdown();

    // Everything the overlay shows. A struct rather than a parameter list,
    // because the escape added four more things worth reading at a glance.
    struct Info
    {
        TrialState state = TrialState::Waiting;
        float progress    = 0.0f;

        float heartWeight  = 0.5f;
        bool  wouldBalance = true;

        // During the escape the two tracks change meaning: how far to the
        // exit, and how close she is.
        bool  escapeActive = false;
        float exitProgress = 0.0f;   // 0 at the treasure, 1 at the doorway out
        float danger       = 0.0f;   // 0 far away, 1 about to be caught

        // Medusa's gaze: how full the petrification bar is, and whether an
        // attack is under way. The second matters even with the bar empty -
        // it is the warning.
        float exposure    = 0.0f;
        float gazeWarning = 0.0f;
        bool  gazeLive    = false;

        float charm = 0.0f;          // 0..1, the Djinn's charm

        // The sanctuary: the bottom bar becomes the time it has left.
        bool  sanctuaryActive = false;
        float sanctuaryLeft   = 0.0f;   // 1 just raised, 0 about to fall

        float time = 0.0f;           // for the danger pulse
    };

    void draw(const Info& info, int windowWidth, int windowHeight);

    static glm::vec3 colorFor(TrialState state);

private:
    Shader m_shader;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    // origin and size are in NDC: -1..1 across the screen.
    void rect(const glm::vec2& origin, const glm::vec2& size,
              const glm::vec4& color);
};

#endif // HUD_H
