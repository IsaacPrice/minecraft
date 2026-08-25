#include "headers/Settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "headers/Controls.hpp"
#include "headers/Display.hpp"
#include "headers/Input.hpp"
#include "headers/Object.hpp"

namespace
{
    Settings current;

    std::string trim(const std::string& text)
    {
        size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";

        size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    bool parseBool(const std::string& value)
    {
        return value == "true" || value == "1" || value == "on";
    }

    // Anisotropy is a power of two, and the menu cycles through the powers, so a
    // value read from the file is snapped to one rather than left as whatever
    // was typed.
    //
    // Deliberately not clamped to the driver's limit here. This runs from
    // LoadSettings, which has to happen before the window exists so the window
    // can be created at the saved size -- and asking the driver anything before
    // there is a context calls through a null function pointer. The clamp to
    // what the hardware will actually honour is in ApplyGraphicsSettings, which
    // runs once there is one.
    int snapAnisotropy(int value, int limit)
    {
        int snapped = 1;
        while (snapped * 2 <= value && snapped * 2 <= limit)
            snapped *= 2;

        return snapped;
    }

    void clampAll()
    {
        current.fov = std::min(110.0f, std::max(30.0f, current.fov));
        current.mouseSensitivity = std::min(3.0f, std::max(0.1f, current.mouseSensitivity));

        current.renderDistance = std::min(96, std::max(4, current.renderDistance));

        current.foliageDistance = std::min(96.0f, std::max(0.0f, current.foliageDistance));
        current.anisotropy = snapAnisotropy(current.anisotropy, 16);

        // 0, or a tens value in range. Anything else came from a hand-edited
        // file and is pulled back to the nearest thing the menu can show.
        if (current.frameCap != 0)
            current.frameCap = std::min(240, std::max(30, (current.frameCap / 10) * 10));

        current.windowMode = std::min(2, std::max(0, current.windowMode));
        current.resolutionWidth = std::max(640, current.resolutionWidth);
        current.resolutionHeight = std::max(360, current.resolutionHeight);
    }
}

Settings& settings()
{
    return current;
}

void LoadSettings(const char* path)
{
    std::ifstream file(path);
    if (!file)
    {
        // No file yet is the ordinary first run, not a failure. Defaults stand,
        // and one will be written the first time the menu is left.
        clampAll();
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;

        size_t split = line.find('=');
        if (split == std::string::npos)
            continue;

        const std::string key = trim(line.substr(0, split));
        const std::string value = trim(line.substr(split + 1));

        if      (key == "fov")              current.fov = (float)atof(value.c_str());
        else if (key == "sensitivity")      current.mouseSensitivity = (float)atof(value.c_str());
        else if (key == "invertMouseY")     current.invertMouseY = parseBool(value);
        else if (key == "renderDistance")   current.renderDistance = atoi(value.c_str());
        else if (key == "foliageDistance")  current.foliageDistance = (float)atof(value.c_str());
        else if (key == "fancyGraphics")    current.fancyGraphics = parseBool(value);
        else if (key == "fxaa")             current.fxaa = parseBool(value);
        else if (key == "frameCap")         current.frameCap = atoi(value.c_str());
        else if (key == "windowMode")       current.windowMode = atoi(value.c_str());
        else if (key == "resolutionWidth")  current.resolutionWidth = atoi(value.c_str());
        else if (key == "resolutionHeight") current.resolutionHeight = atoi(value.c_str());
        else if (key == "vsync")            current.vsync = parseBool(value);
        else if (key == "anisotropy")       current.anisotropy = atoi(value.c_str());
        else if (key.compare(0, 4, "key.") == 0)
        {
            const std::string action = key.substr(4);
            for (int i = 0; i < Input::ACTION_COUNT; i++)
            {
                Input::Action which = static_cast<Input::Action>(i);
                if (action != Input::ActionKey(which))
                    continue;

                Input::Binding binding;
                if (Input::ParseBinding(value, binding))
                    Input::SetBinding(which, binding);

                break;
            }
        }
        // Anything else is from a newer build, or a typo. Ignored either way.
    }

    clampAll();
}

void SaveSettings(const char* path)
{
    std::ofstream file(path);
    if (!file)
    {
        printf("Could not write %s; settings will not persist.\n", path);
        return;
    }

    file << "# Written by the options menu. Delete this file to return to defaults.\n";
    file << "fov=" << current.fov << "\n";
    file << "sensitivity=" << current.mouseSensitivity << "\n";
    file << "invertMouseY=" << (current.invertMouseY ? "true" : "false") << "\n";
    file << "renderDistance=" << current.renderDistance << "\n";
    file << "foliageDistance=" << current.foliageDistance << "\n";
    file << "fancyGraphics=" << (current.fancyGraphics ? "true" : "false") << "\n";
    file << "fxaa=" << (current.fxaa ? "true" : "false") << "\n";
    file << "vsync=" << (current.vsync ? "true" : "false") << "\n";
    file << "anisotropy=" << current.anisotropy << "\n";
    file << "frameCap=" << current.frameCap << "\n";
    file << "windowMode=" << current.windowMode << "\n";
    file << "resolutionWidth=" << current.resolutionWidth << "\n";
    file << "resolutionHeight=" << current.resolutionHeight << "\n";

    for (int i = 0; i < Input::ACTION_COUNT; i++)
    {
        Input::Action action = static_cast<Input::Action>(i);
        file << "key." << Input::ActionKey(action) << "="
             << Input::BindingName(Input::GetBinding(action)) << "\n";
    }
}

void ApplyGeneralSettings()
{
    clampAll();

    setFieldOfView(current.fov);
    setMouseSensitivity(current.mouseSensitivity);
    setInvertMouseY(current.invertMouseY);
}

void ApplyGraphicsSettings()
{
    clampAll();

    // Now that there is a context, the level can be held to what this driver
    // reports. Showing 16x while quietly doing 4x would be a lie, so the setting
    // is corrected rather than only the call.
    current.anisotropy = snapAnisotropy(current.anisotropy, std::max(1, MaxTerrainAnisotropy()));

    Display::SetVSync(current.vsync);
    Display::SetFrameCap(current.frameCap);
    SetTerrainAnisotropy(current.anisotropy);
}

void ApplyDisplaySettings()
{
    clampAll();

    Display::Apply(static_cast<Display::Mode>(current.windowMode),
                   current.resolutionWidth, current.resolutionHeight);

    // Going fullscreen snaps to the nearest video mode the monitor actually has,
    // and borderless ignores the request entirely. Reading the size back means
    // the menu shows what is on screen rather than what was asked for.
    if (current.windowMode != (int)Display::Mode::Borderless)
    {
        current.resolutionWidth = Display::Width();
        current.resolutionHeight = Display::Height();
    }
}
