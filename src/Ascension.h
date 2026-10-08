#ifndef ASCENSION_H
#define ASCENSION_H

#include <vector>

#include <glm/glm.hpp>

// The last twist: the traveller was a soul all along. When dawn has come to
// the garden, he too turns to light and rises - over the temple, through the
// clouds, along a river of souls to Ra's sun boat, and finally into the night
// sky, where the stars draw his journey and he becomes the last star.
//
// One timeline, in seconds from begin():
//
//    0 -  5   he turns to light      the charm cracks; gold climbs his body; he lifts
//    5 - 14.5 the temple from above  he rises fast; the temple below; through the clouds
//   14.5- 26  the river of souls     above the clouds he joins a river of light to the boat
//   26 - 44   he becomes a star      night; constellations of his journey; his star
//
// No GL here: the scene, main and the tests all read the same answers.
class Ascension
{
public:
    static constexpr float kEnd = 44.0f;

    void reset();
    void begin(const glm::vec3& feet);   // where he is standing
    void update(float deltaTime);

    bool  active() const { return m_active; }
    bool  finished() const { return m_active && m_t >= kEnd; }
    float time() const { return m_t; }

    // --- him -----------------------------------------------------------------
    float     bodyGlow() const;        // 0..1 gold climbing from his feet
    float     bodyFade() const;        // 0..1 his body gone, only light left
    glm::vec3 soulPosition() const;    // his feet while in a body; the orb after
    float     soulOrb() const;         // 0..1 the bright orb he becomes

    // --- the world ---------------------------------------------------------------
    float clouds() const;              // 0..1 the sea of clouds
    float skyGold() const;             // 0..1 the golden sky above the clouds
    float skyNight() const;            // 0..1 night falling for the stars

    // --- the river of souls --------------------------------------------------------
    static constexpr int kRiver = 150;
    glm::vec3 riverPosition(int i) const;
    float     riverGlow(int i) const;

    // --- Ra's sun boat -----------------------------------------------------------
    float     boat() const;            // 0..1 visible
    glm::vec3 boatPosition() const;
    float     oarAngle(int oar) const; // degrees: the stroke
    glm::vec3 skySunPosition() const;
    float     skySun() const;          // 0..1 the sun the boat sails toward

    // --- the constellations ---------------------------------------------------------
    int       starCount() const { return static_cast<int>(m_stars.size()); }
    glm::vec3 starPosition(int i) const { return m_stars[i].position; }
    float     starGlow(int i) const;
    float     starSize(int i) const;
    glm::vec3 heartStar() const { return m_heart; }   // where he ends up
    float     heartGlow() const;

    // --- the camera ------------------------------------------------------------------
    struct Shot { glm::vec3 eye; glm::vec3 look; };
    Shot camera() const;

private:
    struct Star
    {
        glm::vec3 position{ 0.0f };
        float appearAt = 0.0f;
        float size = 0.3f;
        bool  vertex = true;     // a star, or a dot along a line between stars
        float twinkle = 0.0f;
        int   figure = 0;        // 4 = the traveller himself
    };

    bool  m_active = false;
    float m_t = 0.0f;
    glm::vec3 m_feet{ 0.0f };
    glm::vec3 m_heart{ 0.0f };
    glm::vec3 m_skyCentre{ 0.0f };
    std::vector<Star> m_stars;

    void buildConstellations();
    glm::vec3 seat(float t) const;                 // his seat in the boat
    glm::vec3 boatAt(float t) const;
    Shot shotAt(int which, float t) const;
};

#endif // ASCENSION_H
