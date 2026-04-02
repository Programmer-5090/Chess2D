#ifndef INPUT_H
#define INPUT_H

#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <array>
#include <unordered_set>
#include <utility>

class Input {
public:
    Input() : quit(false), mousePos({0, 0}), mouseDelta({0, 0})
    {
        m_mouseDown.fill(false);
        m_mouseUp.fill(false);
        m_mouseHeld.fill(false);
    }

    void resetStates()
    {
        keysDown.clear();
        keysUp.clear();
        events.clear();
        m_mouseDown.fill(false);
        m_mouseUp.fill(false);
        mouseDelta = {0, 0};
    }

    void update()
    {
        resetStates();
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            event = e;
            events.push_back(e);

            switch (e.type)
            {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN: {
                std::string key = SDL_GetKeyName(e.key.key);
                if (keysHeld.insert(key).second)   // only if not already held
                    keysDown.insert(key);
                break;
            }
            case SDL_EVENT_KEY_UP: {
                std::string key = SDL_GetKeyName(e.key.key);
                keysUp.insert(key);
                keysHeld.erase(key);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                updateMouse(e.button.button, true);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                updateMouse(e.button.button, false);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                mousePos   = { (int)e.motion.x,   (int)e.motion.y   };
                mouseDelta = { (int)e.motion.xrel, (int)e.motion.yrel };
                break;
            }
        }
    }

    bool shouldQuit() const { return quit; }

    bool keyDown(const std::string& k) const { return keysDown.count(k) > 0; }
    bool keyUp  (const std::string& k) const { return keysUp.count(k)   > 0; }
    bool keyHeld(const std::string& k) const { return keysHeld.count(k) > 0; }

    const std::unordered_set<std::string>& getKeysDown() const { return keysDown; }

    std::pair<int, int> getMousePos()   const { return mousePos; }
    std::pair<int, int> getMouseDelta() const { return mouseDelta; }
    int getMouseX()      const { return mousePos.first; }
    int getMouseY()      const { return mousePos.second; }
    int getMouseDeltaX() const { return mouseDelta.first; }
    int getMouseDeltaY() const { return mouseDelta.second; }

    bool isMouseButtonDown    (int b) const { return inRange(b) && m_mouseDown[b]; }
    bool isMouseButtonReleased(int b) const { return inRange(b) && m_mouseUp[b];   }
    bool isMouseButtonHeld    (int b) const { return inRange(b) && m_mouseHeld[b]; }

    bool isMousePressed() const { return isMouseButtonHeld(SDL_BUTTON_LEFT); }

    const SDL_Event&              getCurrentEvent() const { return event; }
    const std::vector<SDL_Event>& getEvents()       const { return events; }
    void setCurrentEvent(const SDL_Event& e) { event = e; }

private:
    static constexpr int k_maxButtons = 16;

    SDL_Event event{};
    bool quit = false;

    std::unordered_set<std::string> keysDown;
    std::unordered_set<std::string> keysUp;
    std::unordered_set<std::string> keysHeld;

    // Per-button state indexed by SDL button number (SDL_BUTTON_LEFT = 1, etc.)
    std::array<bool, k_maxButtons> m_mouseDown{};
    std::array<bool, k_maxButtons> m_mouseUp{};
    std::array<bool, k_maxButtons> m_mouseHeld{};

    std::pair<int, int> mousePos;
    std::pair<int, int> mouseDelta;
    std::vector<SDL_Event> events;

    static bool inRange(int b) { return b >= 0 && b < k_maxButtons; }

    void updateMouse(int b, bool down)
    {
        if (!inRange(b)) return;
        if (down) {
            m_mouseDown[b] = true;
            m_mouseHeld[b] = true;
            m_mouseUp[b]   = false;
        } else {
            m_mouseUp[b]   = true;
            m_mouseHeld[b] = false;
            m_mouseDown[b] = false;
        }
    }
};

#endif // INPUT_H
