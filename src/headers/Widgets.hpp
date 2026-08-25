#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "UIRenderer.hpp"

// Where the mouse is and what it did this frame, in UI units. Built once per
// frame by the screen stack and handed down, so no widget talks to GLFW and
// none of them can disagree about where the pointer is.
struct UIContext
{
    float mouseX = 0.0f;
    float mouseY = 0.0f;

    bool mouseDown = false;     // held, which is what a drag needs
    bool mousePressed = false;  // went down this frame
    bool mouseReleased = false; // came up this frame
};

// The menu palette, in one place so a screen never picks a colour out of the
// air and two screens cannot drift apart.
namespace UI
{
    const Colour TEXT           = { 255, 255, 255, 255 };
    const Colour TEXT_DIM       = { 168, 168, 176, 255 };
    const Colour TEXT_WARNING   = { 232, 112, 112, 255 };
    const Colour PANEL          = {   0,   0,   0, 150 };
    const Colour FILL           = {  38,  38,  46, 225 };
    const Colour FILL_HOVER     = {  74,  74,  90, 235 };
    const Colour BORDER         = { 112, 112, 124, 255 };
    const Colour BORDER_HOVER   = { 214, 214, 224, 255 };
    const Colour TRACK          = {  24,  24,  30, 235 };
    const Colour TRACK_FILL     = {  92, 124,  92, 235 };

    // The canvas is 540 units tall, so these are roughly half what they would be
    // in pixels at 1080p.
    const float ROW_HEIGHT   = 26.0f;
    const float ROW_GAP      = 7.0f;
    const float WIDE_WIDTH   = 260.0f;
    const float HALF_WIDTH   = 126.0f; // two of these plus a gap make a wide one
    const float BORDER_WIDTH = 1.0f;

    const float TEXT_SCALE   = 1.5f;
    const float TITLE_SCALE  = 3.0f;
}

class Widget
{
public:
    virtual ~Widget() = default;

    void SetBounds(float x, float y, float w, float h);

    virtual void Update(const UIContext& context);
    virtual void Render(UIRenderer& ui) const = 0;

    bool Contains(float x, float y) const;

    float X() const { return _x; }
    float Y() const { return _y; }
    float Width() const { return _w; }
    float Height() const { return _h; }

protected:
    // Drawn by everything with a box around it, so the hover treatment is the
    // same on a button, a toggle and a keybind row without each repeating it.
    void RenderFrame(UIRenderer& ui) const;

    float _x = 0.0f;
    float _y = 0.0f;
    float _w = 0.0f;
    float _h = 0.0f;

    bool _hovered = false;
};

class Label : public Widget
{
public:
    Label(std::string text, Colour colour = UI::TEXT_DIM, float scale = UI::TEXT_SCALE);

    void SetText(std::string text) { _text = std::move(text); }
    void Render(UIRenderer& ui) const override;

private:
    std::string _text;
    Colour _colour;
    float _scale;
};

class Button : public Widget
{
public:
    Button(std::string label, std::function<void()> onClick);

    void SetLabel(std::string label) { _label = std::move(label); }
    void SetEnabled(bool enabled) { _enabled = enabled; }

    void Update(const UIContext& context) override;
    void Render(UIRenderer& ui) const override;

private:
    std::string _label;
    std::function<void()> _onClick;
    bool _enabled = true;
};

// A value dragged along a track. The label carries the value, because a bare
// track tells you nothing about what seventy per cent of the way along means.
class Slider : public Widget
{
public:
    // The formatter turns the value into the whole label, so "FOV: 70" and
    // "Render Distance: 32 chunks" are the same widget with different text.
    Slider(float value, float minimum, float maximum, float step,
           std::function<std::string(float)> format,
           std::function<void(float)> onChange);

    void Update(const UIContext& context) override;
    void Render(UIRenderer& ui) const override;

    float Value() const { return _value; }

private:
    void SetFromMouse(float mouseX);

    float _value;
    float _minimum;
    float _maximum;
    float _step;

    std::function<std::string(float)> _format;
    std::function<void(float)> _onChange;

    // Held across frames: once a drag starts inside the track it keeps following
    // the mouse even when the pointer leaves the widget, which is how a slider
    // is expected to behave and what a per-frame hit test alone would not give.
    bool _dragging = false;
};

// Cycles a list of options on click. Covers on/off and the anisotropy levels
// alike, because two options is just a short list.
class Toggle : public Widget
{
public:
    Toggle(std::string label, std::vector<std::string> options, int index,
           std::function<void(int)> onChange);

    void Update(const UIContext& context) override;
    void Render(UIRenderer& ui) const override;

    int Index() const { return _index; }

private:
    std::string _label;
    std::vector<std::string> _options;
    int _index;
    std::function<void(int)> _onChange;
};
