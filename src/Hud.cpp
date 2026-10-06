#include "Hud.h"

#include <cmath>

#include <iostream>

#include "EscapeTuning.h"

bool Hud::init()
{
    if (!m_shader.load("shaders/hud.vert", "shaders/hud.frag"))
    {
        return false;
    }

    // A unit quad as a triangle strip: (0,0) (1,0) (0,1) (1,1).
    const float quad[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    return true;
}

void Hud::shutdown()
{
    if (m_vbo != 0) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao != 0) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
}

glm::vec3 Hud::colorFor(TrialState state)
{
    switch (state)
    {
        case TrialState::Waiting:  return { 0.55f, 0.52f, 0.60f };  // neutral
        case TrialState::Placing:  return { 0.88f, 0.30f, 0.34f };  // the heart
        case TrialState::Weighing: return { 0.90f, 0.74f, 0.30f };  // gold
        case TrialState::Balanced: return { 0.28f, 0.86f, 0.98f };  // Djinn
        case TrialState::Cursed:   return { 0.45f, 0.88f, 0.36f };  // Medusa
        case TrialState::TreasureRevealed: return { 1.00f, 0.80f, 0.30f };  // the prize
        case TrialState::Escape:   return { 0.95f, 0.45f, 0.20f };  // alarm
        case TrialState::Sanctuary: return { 0.35f, 0.90f, 1.00f }; // the charm's light
        case TrialState::Garden:   return { 0.45f, 0.95f, 0.55f };  // living green
        case TrialState::Escaped:  return { 0.55f, 0.95f, 0.65f };  // relief
        case TrialState::Caught:   return { 0.60f, 0.62f, 0.66f };  // stone
        case TrialState::Reset:    return { 0.42f, 0.40f, 0.46f };
    }
    return { 1.0f, 1.0f, 1.0f };
}

void Hud::rect(const glm::vec2& origin, const glm::vec2& size,
               const glm::vec4& color)
{
    m_shader.setVec2("uOrigin", origin);
    m_shader.setVec2("uSize", size);
    m_shader.setVec4("uColor", color);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Hud::draw(const Info& info, int windowWidth, int windowHeight)
{
    const TrialState state = info.state;
    if (m_vao == 0 || !m_shader.valid())
    {
        return;
    }

    // The overlay must sit on top of everything and must not write depth.
    // Culling state is remembered rather than assumed - the B key can have
    // turned it off, and restoring it blindly would override the user.
    GLboolean wasCulling = GL_FALSE;
    glGetBooleanv(GL_CULL_FACE, &wasCulling);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader.use();
    glBindVertexArray(m_vao);

    const glm::vec3 accent = colorFor(state);

    // Keep the bar a constant pixel height regardless of window size.
    const float pixelY = (windowHeight > 0) ? 2.0f / windowHeight : 0.002f;
    const float pixelX = (windowWidth  > 0) ? 2.0f / windowWidth  : 0.002f;

    const float barHeight = 10.0f * pixelY;
    const float margin    = 24.0f * pixelX;
    const float bottom    = -1.0f + 24.0f * pixelY;
    const float barWidth  = 2.0f - margin * 2.0f;

    // --- state progress bar -------------------------------------------------
    rect({ -1.0f + margin, bottom }, { barWidth, barHeight },
         { 0.10f, 0.09f, 0.13f, 0.85f });

    // During the escape the bottom bar becomes the one number that matters:
    // how much corridor is left. Otherwise it is the state's own progress,
    // and the open-ended states show a full bar rather than an empty one
    // that would look like the program had stalled.
    const bool openEnded = (state == TrialState::Waiting)
                        || (state == TrialState::TreasureRevealed)
                        || (state == TrialState::Escape);

    const float fill = info.sanctuaryActive ? info.sanctuaryLeft
                     : info.escapeActive    ? info.exitProgress
                     : openEnded            ? 1.0f
                                            : info.progress;

    // In its last seconds the sanctuary's bar blinks, as the dome does.
    float barAlpha = 0.95f;
    const float warnAt = Tuning::kSanctuaryWarning / Tuning::kSanctuaryDuration;
    if (info.sanctuaryActive && info.sanctuaryLeft < warnAt)
    {
        barAlpha = 0.45f + 0.5f * (0.5f + 0.5f * std::sin(info.time * 18.0f));
    }

    rect({ -1.0f + margin, bottom }, { barWidth * fill, barHeight },
         { accent.r, accent.g, accent.b, barAlpha });

    // A tick at the far end, so the goal is a place rather than a feeling.
    if (info.escapeActive)
    {
        rect({ 1.0f - margin - 3.0f * pixelX, bottom - 2.0f * pixelY },
             { 3.0f * pixelX, barHeight + 4.0f * pixelY },
             { 0.85f, 0.95f, 1.0f, 0.9f });
    }

    // --- the second track ----------------------------------------------------
    const float trackBottom = bottom + barHeight + 8.0f * pixelY;
    const float trackHeight = 8.0f * pixelY;
    const float trackWidth  = barWidth * 0.45f;
    const float trackLeft   = -trackWidth * 0.5f;

    rect({ trackLeft, trackBottom }, { trackWidth, trackHeight },
         { 0.10f, 0.09f, 0.13f, 0.85f });

    // --- the charm ------------------------------------------------------------
    // A gold square with a cyan gem in it, left of the track, once the Djinn
    // has given it. It grows in as the charm flies to him.
    if (info.charm > 0.01f)
    {
        const float size = 18.0f * info.charm;
        const float gem  = 10.0f * info.charm;
        const float cx = trackLeft - 30.0f * pixelX;
        const float cy = trackBottom + trackHeight * 0.5f;
        const float pulse = 0.8f + 0.2f * std::sin(info.time * 5.0f);

        rect({ cx - size * 0.5f * pixelX, cy - size * 0.5f * pixelY },
             { size * pixelX, size * pixelY }, { 0.95f, 0.75f, 0.25f, 0.95f });
        rect({ cx - gem * 0.5f * pixelX, cy - gem * 0.5f * pixelY },
             { gem * pixelX, gem * pixelY },
             { 0.35f * pulse, 0.95f * pulse, 1.0f * pulse, 1.0f });
    }

    if (info.escapeActive)
    {
        // How close she is. Pulsing at the top end, because by then a number
        // creeping up is less use than something demanding attention.
        const float pulse = (info.danger > 0.65f)
                          ? 0.75f + 0.25f * std::sin(info.time * 14.0f)
                          : 1.0f;

        const glm::vec4 dangerColor(0.95f * pulse,
                                    (0.55f - 0.45f * info.danger) * pulse,
                                    (0.35f - 0.30f * info.danger) * pulse,
                                    0.95f);

        rect({ trackLeft, trackBottom },
             { trackWidth * info.danger, trackHeight }, dangerColor);
    }
    else if (!info.sanctuaryActive)
    {
        // The lit band in the middle is the tolerance window; the marker is
        // the heart's current weight.
        rect({ trackLeft + trackWidth * 0.38f, trackBottom },
             { trackWidth * 0.24f, trackHeight },
             { 0.30f, 0.55f, 0.35f, 0.85f });

        const glm::vec4 markerColor = info.wouldBalance
                                    ? glm::vec4(0.55f, 1.00f, 0.55f, 1.0f)
                                    : glm::vec4(1.00f, 0.42f, 0.34f, 1.0f);

        const float markerWidth = 4.0f * pixelX;
        rect({ trackLeft + trackWidth * info.heartWeight - markerWidth * 0.5f,
               trackBottom - 3.0f * pixelY },
             { markerWidth, trackHeight + 6.0f * pixelY },
             markerColor);
    }

    // --- Medusa's gaze: the petrification bar -----------------------------------
    // Stacked above the danger bar and a little taller, because during an
    // attack this is the one that matters.
    if (info.escapeActive)
    {
        const float barBottom = trackBottom + trackHeight + 8.0f * pixelY;
        const float barH      = 12.0f * pixelY;
        const float ex        = glm::clamp(info.exposure, 0.0f, 1.0f);

        rect({ trackLeft, barBottom }, { trackWidth, barH },
             { 0.10f, 0.09f, 0.13f, 0.85f });

        // Living green turning to grey stone as it fills.
        const glm::vec3 flesh(0.45f, 0.85f, 0.35f);
        const glm::vec3 stone(0.66f, 0.68f, 0.72f);
        const glm::vec3 c = glm::mix(flesh, stone, ex);

        rect({ trackLeft, barBottom }, { trackWidth * ex, barH },
             { c.r, c.g, c.b, 0.95f });

        // Warning caps either side. They blink through the telegraph and burn
        // steadily once the beam is live, so there is a cue even in first
        // person, where Medusa is behind the camera and unseen.
        if (info.gazeWarning > 0.0f)
        {
            const float blink = info.gazeLive
                              ? 1.0f
                              : 0.45f + 0.55f * std::sin(info.time * 20.0f);
            const glm::vec4 cap(0.55f, 1.0f, 0.35f, glm::clamp(blink, 0.0f, 1.0f));

            const float capW = 10.0f * pixelX;
            rect({ trackLeft - capW - 6.0f * pixelX, barBottom - 2.0f * pixelY },
                 { capW, barH + 4.0f * pixelY }, cap);
            rect({ trackLeft + trackWidth + 6.0f * pixelX, barBottom - 2.0f * pixelY },
                 { capW, barH + 4.0f * pixelY }, cap);
        }
    }

    glBindVertexArray(0);

    glDisable(GL_BLEND);
    if (wasCulling == GL_TRUE)
    {
        glEnable(GL_CULL_FACE);
    }
    glEnable(GL_DEPTH_TEST);
}
