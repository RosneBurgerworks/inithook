/*
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <menu/object/input/ColorSelector.hpp>
#include <menu/Menu.hpp>
#include <menu/object/container/ModalColorSelect.hpp>

static settings::RVariable<int> default_width{ "zk.style.input.color.width", "36" };
static settings::RVariable<int> default_height{ "zk.style.input.color.height", "14" };

static settings::RVariable<rgba_t> border{ "zk.style.input.color.border", "ffaa00ff" };

namespace zerokernel
{

ColorSelector::ColorSelector() : BaseMenuObject{}
{
    resize(*default_width, *default_height);
    bb.setPadding(3, 3, 3, 3);
}

ColorSelector::ColorSelector(settings::Variable<rgba_t> &variable) : BaseMenuObject{}, variable(&variable)
{
    resize(*default_width, *default_height);
    bb.setPadding(3, 3, 3, 3);
}

void ColorSelector::render()
{
    renderBorder(variable ? *border : *style::colors::error);
    if (variable)
    {
        auto pb = bb.getContentBox();
        draw::Rectangle(pb.left(), pb.top(), pb.width, pb.height, **variable);
    }
}

bool ColorSelector::onLeftMouseClick()
{
    if (!variable)
        return false;
    auto modal = std::make_unique<ModalColorSelect>(*variable);
    modal->setParent(Menu::instance->wm.get());
    modal->move(bb.getBorderBox().right(), bb.getBorderBox().y);
    Menu::instance->addModalObject(std::move(modal));
    BaseMenuObject::onLeftMouseClick();
    return true;
}

void ColorSelector::loadFromXml(const tinyxml2::XMLElement *data)
{
    BaseMenuObject::loadFromXml(data);

    const char *target{ nullptr };
    if (tinyxml2::XML_SUCCESS == data->QueryStringAttribute("target", &target))
    {
        std::string str(target);
        auto opt = settings::Manager::instance().lookup(str);
        if (opt)
            variable = dynamic_cast<settings::Variable<rgba_t> *>(opt);
    }
}
} // namespace zerokernel
