#include "headers/Widgets.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

void Widget::SetBounds(float x, float y, float w, float h)
{
    _x = x;
    _y = y;
    _w = w;
    _h = h;
}

bool Widget::Contains(float x, float y) const
{
    return x >= _x && x <= _x + _w && y >= _y && y <= _y + _h;
}

void Widget::Update(const UIContext& context)
{
    _hovered = Contains(context.mouseX, context.mouseY);
}

void Widget::RenderFrame(UIRenderer& ui) const
{
    ui.DrawRect(_x, _y, _w, _h, _hovered ? UI::FILL_HOVER : UI::FILL);
    ui.DrawRectOutline(_x, _y, _w, _h, UI::BORDER_WIDTH,
                       _hovered ? UI::BORDER_HOVER : UI::BORDER);
}

// --- Label ---

Label::Label(std::string text, Colour colour, float scale)
    : _text(std::move(text)), _colour(colour), _scale(scale)
{
}

void Label::Render(UIRenderer& ui) const
{
    const float y = _y + (_h - ui.LineHeight(_scale)) * 0.5f;
    ui.DrawTextCentredShadowed(_x + _w * 0.5f, y, _scale, _colour, _text);
}

// --- Button ---

Button::Button(std::string label, std::function<void()> onClick)
    : _label(std::move(label)), _onClick(std::move(onClick))
{
}

void Button::Update(const UIContext& context)
{
    Widget::Update(context);

    if (!_enabled)
    {
        _hovered = false;
        return;
    }

    // Acted on the press rather than the release. A menu here has nothing that
    // needs taking back by dragging off the button before letting go, and the
    // press is the moment the click is felt.
    if (_hovered && context.mousePressed && _onClick)
        _onClick();
}

void Button::Render(UIRenderer& ui) const
{
    RenderFrame(ui);

    const float y = _y + (_h - ui.LineHeight(UI::TEXT_SCALE)) * 0.5f;
    ui.DrawTextCentredShadowed(_x + _w * 0.5f, y, UI::TEXT_SCALE,
                               _enabled ? UI::TEXT : UI::TEXT_DIM, _label);
}

// --- Slider ---

Slider::Slider(float value, float minimum, float maximum, float step,
               std::function<std::string(float)> format,
               std::function<void(float)> onChange)
    : _value(value), _minimum(minimum), _maximum(maximum), _step(step),
      _format(std::move(format)), _onChange(std::move(onChange))
{
}

void Slider::SetFromMouse(float mouseX)
{
    const float span = std::max(1.0f, _w);
    float fraction = (mouseX - _x) / span;
    fraction = std::min(1.0f, std::max(0.0f, fraction));

    float value = _minimum + fraction * (_maximum - _minimum);

    // Snapped to the step, so the label shows 70 rather than 69.8431 and the
    // value written into the settings is one the file can round-trip.
    if (_step > 0.0f)
        value = _minimum + std::floor((value - _minimum) / _step + 0.5f) * _step;

    value = std::min(_maximum, std::max(_minimum, value));

    if (value == _value)
        return;

    _value = value;
    if (_onChange)
        _onChange(_value);
}

void Slider::Update(const UIContext& context)
{
    Widget::Update(context);

    if (_hovered && context.mousePressed)
        _dragging = true;

    if (!context.mouseDown)
        _dragging = false;

    if (_dragging)
        SetFromMouse(context.mouseX);
}

void Slider::Render(UIRenderer& ui) const
{
    ui.DrawRect(_x, _y, _w, _h, UI::TRACK);

    const float span = std::max(0.0001f, _maximum - _minimum);
    const float fraction = std::min(1.0f, std::max(0.0f, (_value - _minimum) / span));

    // The filled part reads as how far along the value is at a glance, which the
    // knob alone does not once the track is this wide.
    ui.DrawRect(_x, _y, _w * fraction, _h, UI::TRACK_FILL);

    const float knobWidth = 8.0f;
    const float knobX = _x + (_w - knobWidth) * fraction;
    ui.DrawRect(knobX, _y, knobWidth, _h, _hovered || _dragging ? UI::FILL_HOVER : UI::FILL);
    ui.DrawRectOutline(knobX, _y, knobWidth, _h, UI::BORDER_WIDTH,
                       _hovered || _dragging ? UI::BORDER_HOVER : UI::BORDER);

    ui.DrawRectOutline(_x, _y, _w, _h, UI::BORDER_WIDTH, UI::BORDER);

    const float textY = _y + (_h - ui.LineHeight(UI::TEXT_SCALE)) * 0.5f;
    ui.DrawTextCentredShadowed(_x + _w * 0.5f, textY, UI::TEXT_SCALE, UI::TEXT,
                               _format ? _format(_value) : std::string());
}

// --- Toggle ---

Toggle::Toggle(std::string label, std::vector<std::string> options, int index,
               std::function<void(int)> onChange)
    : _label(std::move(label)), _options(std::move(options)),
      _index(index), _onChange(std::move(onChange))
{
    if (_options.empty())
        _options.push_back("");

    _index = std::min((int)_options.size() - 1, std::max(0, _index));
}

void Toggle::Update(const UIContext& context)
{
    Widget::Update(context);

    if (!_hovered || !context.mousePressed)
        return;

    _index = (_index + 1) % (int)_options.size();

    if (_onChange)
        _onChange(_index);
}

void Toggle::Render(UIRenderer& ui) const
{
    RenderFrame(ui);

    const float y = _y + (_h - ui.LineHeight(UI::TEXT_SCALE)) * 0.5f;
    ui.DrawTextCentredShadowed(_x + _w * 0.5f, y, UI::TEXT_SCALE, UI::TEXT,
                               _label + ": " + _options[_index]);
}
