/*
  Created on 27.07.18.
*/

#include <menu/object/input/TextInput.hpp>

static settings::RVariable<rgba_t> color_border_active{ "zk.style.input.text.color.border.active", "ffd900ff" };
static settings::RVariable<rgba_t> color_border{ "zk.style.input.text.color.border.inactive", "ffaa00ff" };

static settings::RVariable<rgba_t> color_text_active{ "zk.style.input.text.color.text.active", "ffffff" };
static settings::RVariable<rgba_t> color_text{ "zk.style.input.text.color.text.inactive", "aaaaaa" };

zerokernel::TextInput::TextInput() : BaseMenuObject{}
{
    text_object.setParent(this);
    text_object.move(0, 0);
    text_object.bb.width.setFill();
    text_object.bb.height.setFill();
    text_object.bb.setPadding(0, 0, 5, 0);
}

bool zerokernel::TextInput::handleSdlEvent(SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_MOUSEBUTTONDOWN:
    {
        if (isHovered())
        {
            if (is_input_active)
                finishInput(event->button.button == SDL_BUTTON_LEFT);
            else
                startInput();
            return true;
        }
        else
        {
            if (is_input_active)
            {
                finishInput(false);
                return true;
            }
        }
    }
    case SDL_TEXTINPUT:
        if (is_input_active)
        {
            for (auto c : event->text.text)
            {
                if (!c)
                    break;
                if (current_text.length() == max_length)
                    break;
                current_text.push_back(c);
            }
            return true;
        }
    case SDL_KEYDOWN:
        if (is_input_active)
        {
            switch (event->key.keysym.sym)
            {
            case SDLK_BACKSPACE:
                if (!current_text.empty())
                    current_text.pop_back();
                return true;
            case SDLK_RETURN:
                finishInput(true);
                return true;
            case SDLK_ESCAPE:
                finishInput(false);
                return true;
            default:
                break;
            }
        }
    default:
        break;
    }

    return BaseMenuObject::handleSdlEvent(event);
}

void zerokernel::TextInput::render()
{
    if (draw_border)
        renderBorder(is_input_active ? *color_border_active : *color_border);

    if (is_input_active)
    {
        text_object.setColorText(&*color_text_active);
        text_object.set(current_text);
    }
    else
    {
        text_object.setColorText(&*color_text);
        float x, y;
        std::string t = getValue();
        resource::font::base.stringSize(t, &x, &y);
        if (x + 5 > bb.getBorderBox().width)
        {
            while (true)
            {
                resource::font::base.stringSize(t, &x, &y);
                if (x + 13 > bb.getBorderBox().width)
                {
                    t = t.substr(0, t.size() - 1);
                }
                else
                    break;
            }
            t.append("..");
        }
        text_object.set(t);
    }
    text_object.render();
}

void zerokernel::TextInput::startInput()
{
    is_input_active = true;
    current_text    = getValue();
}

void zerokernel::TextInput::finishInput(bool accept)
{
    is_input_active = false;
    setValue(current_text);
}

const std::string &zerokernel::TextInput::getValue()
{
    return text;
}

void zerokernel::TextInput::setValue(std::string value)
{
    text = std::move(value);
    emitValueChange();
}

void zerokernel::TextInput::emitValueChange()
{
    Message msg{ "ValueChange" };
    emit(msg, false);
}

void zerokernel::TextInput::onMove()
{
    BaseMenuObject::onMove();

    text_object.onParentMove();
}
