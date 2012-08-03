/*
 * fluorescence is a free, customizable Ultima Online client.
 * Copyright (C) 2011-2012, http://fluorescence-client.org

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */



#include "xmlloader.hpp"

#include <ClanLib/Display/Image/pixel_buffer.h>

#include <boost/bind.hpp>
#include <boost/filesystem/operations.hpp>

#include <platform.hpp>
#include <misc/log.hpp>
#include <client.hpp>

#include "manager.hpp"
#include "texture.hpp"
#include "components/button.hpp"
#include "components/scrollarea.hpp"
#include "components/scrollbar.hpp"
#include "components/lineedit.hpp"
#include "components/label.hpp"
#include "components/clicklabel.hpp"
#include "components/worldview.hpp"
#include "components/propertylabel.hpp"
#include "components/paperdollview.hpp"
#include "components/containerview.hpp"
#include "components/image.hpp"
#include "components/background.hpp"
#include "components/checkbox.hpp"
#include "components/sysloglabel.hpp"
#include "components/tbackground.hpp"
#include "components/warmodebutton.hpp"
#include "components/radiobutton.hpp"

namespace fluo {
namespace ui {

XmlLoader* XmlLoader::singleton_ = NULL;

XmlLoader* XmlLoader::getSingleton() {
    if (!singleton_) {
        singleton_ = new XmlLoader();
    }

    return singleton_;
}

XmlLoader::XmlLoader() {
    functionTable_["image"] = boost::bind(&XmlLoader::parseImage, this, _1, _2, _3, _4);
    functionTable_["button"] = boost::bind(&XmlLoader::parseButton, this, _1, _2, _3, _4);
    functionTable_["page"] = boost::bind(&XmlLoader::parsePage, this, _1, _2, _3, _4);
    functionTable_["background"] = boost::bind(&XmlLoader::parseBackground, this, _1, _2, _3, _4);
    functionTable_["checkbox"] = boost::bind(&XmlLoader::parseCheckbox, this, _1, _2, _3, _4);
    functionTable_["radiobutton"] = boost::bind(&XmlLoader::parseRadioButton, this, _1, _2, _3, _4);
    functionTable_["label"] = boost::bind(&XmlLoader::parseLabel, this, _1, _2, _3, _4);
    functionTable_["propertylabel"] = boost::bind(&XmlLoader::parsePropertyLabel, this, _1, _2, _3, _4);
    functionTable_["sysloglabel"] = boost::bind(&XmlLoader::parseSysLogLabel, this, _1, _2, _3, _4);
    functionTable_["clicklabel"] = boost::bind(&XmlLoader::parseClickLabel, this, _1, _2, _3, _4);
    functionTable_["lineedit"] = boost::bind(&XmlLoader::parseLineEdit, this, _1, _2, _3, _4);
    functionTable_["scrollarea"] = boost::bind(&XmlLoader::parseScrollArea, this, _1, _2, _3, _4);
    functionTable_["repeat"] = boost::bind(&XmlLoader::parseRepeat, this, _1, _2, _3, _4);

    functionTable_["worldview"] = boost::bind(&XmlLoader::parseWorldView, this, _1, _2, _3, _4);
    functionTable_["paperdoll"] = boost::bind(&XmlLoader::parsePaperdoll, this, _1, _2, _3, _4);
    functionTable_["container"] = boost::bind(&XmlLoader::parseContainer, this, _1, _2, _3, _4);
    functionTable_["warmodebutton"] = boost::bind(&XmlLoader::parseWarModeButton, this, _1, _2, _3, _4);


    functionTable_["tcombobox"] = boost::bind(&XmlLoader::parseTComboBox, this, _1, _2, _3, _4);
    functionTable_["tgroupbox"] = boost::bind(&XmlLoader::parseTGroupBox, this, _1, _2, _3, _4);
    functionTable_["tspin"] = boost::bind(&XmlLoader::parseTSpin, this, _1, _2, _3, _4);
    functionTable_["ttabs"] = boost::bind(&XmlLoader::parseTTabs, this, _1, _2, _3, _4);
    functionTable_["tslider"] = boost::bind(&XmlLoader::parseTSlider, this, _1, _2, _3, _4);
    functionTable_["ttextedit"] = boost::bind(&XmlLoader::parseTTextEdit, this, _1, _2, _3, _4);
    functionTable_["tbackground"] = boost::bind(&XmlLoader::parseTBackground, this, _1, _2, _3, _4);
}


void XmlLoader::addRepeatContext(const UnicodeString& name, const RepeatContext& context) {
    getSingleton()->repeatContexts_[name] = context;
}

void XmlLoader::removeRepeatContext(const UnicodeString& name) {
    getSingleton()->repeatContexts_.erase(name);
}

void XmlLoader::clearRepeatContexts() {
    getSingleton()->repeatContexts_.clear();
}


bool XmlLoader::readTemplateFile(const boost::filesystem::path& themePath) {
    boost::filesystem::path path = themePath / "templates.xml";

    if (!boost::filesystem::exists(path)) {
        LOG_ERROR << "Unable to load templates.xml for theme " << themePath << ", file not found" << std::endl;
        return false;
    }

    LOG_DEBUG << "Parsing templates.xml for theme " << themePath << std::endl;

    pugi::xml_parse_result result = getSingleton()->templateDocument_.load_file(path.string().c_str());

    if (result) {
        getSingleton()->setTemplates();

        return true;
    } else {
        LOG_ERROR << "Error parsing templates.xml file at offset " << result.offset << ": " << result.description() << std::endl;
        return false;
    }
}

void XmlLoader::setTemplates() {
    templateMap_.clear();

    pugi::xml_node rootNode = templateDocument_.child("templates");

    pugi::xml_node_iterator iter = rootNode.begin();
    pugi::xml_node_iterator iterEnd = rootNode.end();

    for (; iter != iterEnd; ++iter) {
        pugi::xml_node componentNode = iter->first_child();

        if (functionTable_.find(componentNode.name()) != functionTable_.end()) {
            // register template by name, if it has one
            if (iter->attribute("name")) {
                templateMap_[iter->attribute("name").value()] = componentNode;
            }

            // if default, register by component name
            if (iter->attribute("default").as_bool()) {
                defaultTemplateMap_[componentNode.name()] = componentNode;
            }
        } else {
            LOG_WARN << "Unknown gump tag " << componentNode.name() << " in templates file" << std::endl;
        }
    }
}

pugi::xml_node XmlLoader::getTemplate(const UnicodeString& templateName) {
    std::map<UnicodeString, pugi::xml_node>::iterator iter = templateMap_.find(templateName);
    if (iter != templateMap_.end()) {
        return iter->second;
    } else {
        LOG_WARN << "Trying to access unknown gump component template " << templateName << std::endl;
        static pugi::xml_node ret;
        return ret;
    }
}

pugi::xml_attribute XmlLoader::getAttribute(const char* name, pugi::xml_node& node, pugi::xml_node& defaultNode) {
    pugi::xml_attribute ret = node.attribute(name);
    if (!ret) {
        ret = defaultNode.attribute(name);
    }
    return ret;
}

pugi::xml_node XmlLoader::getNode(const char* name, pugi::xml_node& node, pugi::xml_node& defaultNode) {
    pugi::xml_node ret = node.child(name);
    if (!ret) {
        ret = defaultNode.child(name);
    }
    return ret;
}


GumpMenu* XmlLoader::fromXmlFile(const UnicodeString& name) {
    boost::filesystem::path path = "gumps";
    std::string utf8FileName = StringConverter::toUtf8String(name) + ".xml";
    path = path / utf8FileName;

    path = data::Manager::getShardFilePath(path);

    if (!boost::filesystem::exists(path)) {
        LOG_ERROR << "Unable to load gump xml, file not found: " << utf8FileName << std::endl;
        return nullptr;
    }

    LOG_DEBUG << "Parsing xml gump file: " << path << std::endl;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(path.string().c_str());

    if (result) {
        GumpMenu* ret = getSingleton()->fromXml(doc);

        // save transformed document (debug)
        //LOG_DEBUG << "Saving result: " << doc.save_file("out.xml") << std::endl;
        //LOG_DEBUG << "Transformed xml:" << std::endl;
        //doc.print(std::cout);

        if (ret) {
            ret->setName(name);
        }

        return ret;
    } else {
        LOG_ERROR << "Error parsing gump xml file at offset " << result.offset << ": " << result.description() << std::endl;
        return nullptr;
    }
}

GumpMenu* XmlLoader::fromXmlString(const UnicodeString& str) {
    pugi::xml_document doc;
    std::string utf8String = StringConverter::toUtf8String(str);
    pugi::xml_parse_result result = doc.load_buffer(utf8String.c_str(), utf8String.length());

    if (result) {
        GumpMenu* ret = getSingleton()->fromXml(doc);

        return ret;
    } else {
        LOG_ERROR << "Error parsing gump xml string at offset " << result.offset << ": " << result.description() << std::endl;
        return nullptr;
    }
}

GumpMenu* XmlLoader::fromXml(pugi::xml_document& doc) {
    pugi::xml_node rootNode = doc.child("gump");

    pugi::xml_node dummy;
    CL_Rect bounds = getBoundsFromNode(rootNode, dummy);
    bounds.set_width(1);
    bounds.set_height(1);

    bool closable = rootNode.attribute("closable").as_bool();
    bool draggable = rootNode.attribute("draggable").as_bool();
    UnicodeString action = StringConverter::fromUtf8(rootNode.attribute("action").value());
    UnicodeString cancelAction = StringConverter::fromUtf8(rootNode.attribute("cancelaction").value());
    bool inbackground = rootNode.attribute("inbackground").as_bool();

    CL_GUITopLevelDescription desc(bounds, false);
    desc.set_decorations(false);
    desc.set_in_background(inbackground);


    GumpMenu* ret = new GumpMenu(desc);
    ret->setClosable(closable);
    ret->setDraggable(draggable);

    if (action.length() > 0) {
        ret->setAction(action);
    }

    if (cancelAction.length() > 0) {
        ret->setCancelAction(cancelAction);
    }

    parseChildren(rootNode, ret, ret);

    ret->fitSizeToChildren();
    ret->activateFirstPage();

    return ret;
}

bool XmlLoader::parseChildren(pugi::xml_node& rootNode, CL_GUIComponent* parent, GumpMenu* top) {
    pugi::xml_node_iterator iter = rootNode.begin();
    pugi::xml_node_iterator iterEnd = rootNode.end();

    bool ret = true;

    for (; iter != iterEnd && ret; ++iter) {
        std::map<UnicodeString, XmlParseFunction>::iterator function = functionTable_.find(iter->name());

        LOG_DEBUG << "Gump Component: " << iter->name() << std::endl;

        if (function != functionTable_.end()) {
            pugi::xml_node defaultNode;
            if (iter->attribute("template")) {
                defaultNode = getTemplate(iter->attribute("template").value());
            }

            if (!defaultNode) {
                defaultNode = defaultTemplateMap_[iter->name()];
            }

            ret = (function->second)(*iter, defaultNode, parent, top);
        } else {
            LOG_WARN << "Unknown gump tag " << iter->name() << std::endl;
        }
    }

    return ret;
}


CL_Rect XmlLoader::getBoundsFromNode(pugi::xml_node& node, pugi::xml_node& defaultNode) {
    unsigned int width = getAttribute("width", node, defaultNode).as_uint();
    unsigned int height = getAttribute("height", node, defaultNode).as_uint();
    int locX = getAttribute("x", node, defaultNode).as_int();
    int locY = getAttribute("y", node, defaultNode).as_int();

    CL_Rect bounds(locX, locY, CL_Size(width, height));
    return bounds;
}

bool XmlLoader::parseId(pugi::xml_node& node, CL_GUIComponent* component) {
    std::string cssid = node.attribute("name").value();
    if (cssid.length() > 0) {
        component->set_id_name(cssid);
    }

    return true;
}

boost::shared_ptr<ui::Texture> XmlLoader::parseTexture(pugi::xml_node& node, pugi::xml_node& defaultNode) {
    UnicodeString imgSource = StringConverter::fromUtf8(getAttribute("source", node, defaultNode).value());
    UnicodeString imgId = StringConverter::fromUtf8(getAttribute("id", node, defaultNode).value());

    boost::shared_ptr<ui::Texture> texture = data::Manager::getTexture(imgSource, imgId);
    texture->setUsage(ui::Texture::USAGE_GUMP);

    return texture;
}

bool XmlLoader::parseMultiTextureImage(pugi::xml_node& node, pugi::xml_node& defaultNode, components::MultiTextureImage* img, unsigned int index) {
    boost::shared_ptr<ui::Texture> texture = parseTexture(node, defaultNode);

    if (!texture) {
        LOG_ERROR << "Unable to parse gump image, source=" << getAttribute("source", node, defaultNode) << " id=" << getAttribute("id", node, defaultNode) << std::endl;
        return false;
    }

    unsigned int hue = getAttribute("hue", node, defaultNode).as_uint();
    std::string rgba = getAttribute("color", node, defaultNode).value();
    float alpha = getAttribute("alpha", node, defaultNode).as_float();

    bool tiled = getAttribute("tiled", node, defaultNode).as_bool();

    img->addTexture(index, texture, hue, rgba, alpha, tiled);

    return true;
}

bool XmlLoader::parseButtonText(pugi::xml_node& node, pugi::xml_node& defaultNode, components::Button* button, unsigned int index) {
    unsigned int hue = getAttribute("font-hue", node, defaultNode).as_uint();
    std::string rgba = getAttribute("font-color", node, defaultNode).value();
    UnicodeString text = getAttribute("text", node, defaultNode).value();
    CL_Colorf color(rgba);

    if (text.length() > 0) {
        button->setText(index, text);
    }

    // if the node has its own color or hue property, don't use the template values
    if (node.attribute("font-color")) {
        button->setFontColor(index, color);
    } else if (node.attribute("font-hue")) {
        button->setFontHue(index, hue);
    } else if (rgba.length() > 0) {
        button->setFontColor(index, color);
    } else {
        button->setFontHue(index, hue);
    }

    return true;
}

bool XmlLoader::parseScrollBarTextures(boost::shared_ptr<ui::Texture>* textures, CL_Colorf* colors, CL_Vec3f* hueInfos, pugi::xml_node& node, pugi::xml_node& defaultNode) {
    pugi::xml_node normalNode = node.child("normal");
    pugi::xml_node mouseOverNode = node.child("mouseover");
    pugi::xml_node mouseDownNode = node.child("mousedown");

    pugi::xml_node defaultNormalNode = defaultNode.child("normal");
    pugi::xml_node defaultMouseOverNode = defaultNode.child("mouseover");
    pugi::xml_node defaultMouseDownNode = defaultNode.child("mousedown");

    if ((normalNode || defaultNormalNode) && (mouseOverNode || defaultMouseOverNode) && (mouseDownNode || defaultMouseDownNode)) {
        textures[components::ScrollBar::TEX_INDEX_UP] = parseTexture(normalNode, defaultNormalNode);
        hueInfos[components::ScrollBar::TEX_INDEX_UP][1u] = getAttribute("hue", normalNode, defaultNormalNode).as_uint();
        std::string rgba = getAttribute("color", normalNode, defaultNormalNode).value();
        if (rgba.length() > 0) {
            colors[components::ScrollBar::TEX_INDEX_UP] = CL_Colorf(rgba);
        }

        textures[components::ScrollBar::TEX_INDEX_MOUSEOVER] = parseTexture(mouseOverNode, defaultMouseOverNode);
        hueInfos[components::ScrollBar::TEX_INDEX_MOUSEOVER][1u] = getAttribute("hue", mouseOverNode, defaultMouseOverNode).as_uint();
        rgba = getAttribute("color", mouseOverNode, defaultMouseOverNode).value();
        if (rgba.length() > 0) {
            colors[components::ScrollBar::TEX_INDEX_MOUSEOVER] = CL_Colorf(rgba);
        }

        textures[components::ScrollBar::TEX_INDEX_DOWN] = parseTexture(mouseDownNode, defaultMouseDownNode);
        hueInfos[components::ScrollBar::TEX_INDEX_DOWN][1u] = getAttribute("hue", mouseDownNode, defaultMouseDownNode).as_uint();
        rgba = getAttribute("color", mouseDownNode, defaultMouseDownNode).value();
        if (rgba.length() > 0) {
            colors[components::ScrollBar::TEX_INDEX_DOWN] = CL_Colorf(rgba);
        }
    } else {
        LOG_ERROR << "Scrollbar needs definitions for normal, mouseover and mousedown textures for all components" << std::endl;
        return false;
    }

    if (!textures[components::ScrollBar::TEX_INDEX_UP] || !textures[components::ScrollBar::TEX_INDEX_MOUSEOVER] || !textures[components::ScrollBar::TEX_INDEX_DOWN]) {
        LOG_ERROR << "Invalid scrollbar texture (texture not found)" << std::endl;
        return false;
    }

    return true;
}

bool XmlLoader::parseScrollBar(components::ScrollBar* bar, components::ScrollArea* parent, bool vertical, pugi::xml_node node, pugi::xml_node defaultNode) {
    static std::string visibilityAlways("always");
    static std::string visibilityNever("never");
    static std::string visibilityOnDemand("ondemand");

    std::string visibility = getAttribute("visible", node, defaultNode).value();
    if (visibility.length() > 0) {
        if (visibility == visibilityAlways) {
            bar->setVisibility(components::ScrollBar::VISIBLE_ALWAYS);
        } else if (visibility == visibilityNever) {
            bar->setVisibility(components::ScrollBar::VISIBLE_NEVER);
        } else if (visibility == visibilityOnDemand) {
            bar->setVisibility(components::ScrollBar::VISIBLE_ON_DEMAND);
        } else {
            LOG_ERROR << "Unknown scrollbar visibility: " << visibility << ". Possible values: always/never/ondemand" << std::endl;
            return false;
        }
    }

    // don't bother parsing if it is not visible anyway
    if (bar->getVisibility() == components::ScrollBar::VISIBLE_NEVER) {
        return true;
    }

    if (vertical) {
        unsigned int width = getAttribute("width", node, defaultNode).as_uint();
        if (width == 0) {
            LOG_ERROR << "Vertical scrollbar without width is not possible" << std::endl;
            return false;
        }
        // only need to set the final width here, position and height is calculated in scrollarea
        bar->set_geometry(CL_Rectf(0, 0, CL_Sizef(width, 1)));
    } else {
        unsigned int height = getAttribute("height", node, defaultNode).as_uint();
        if (height == 0) {
            LOG_ERROR << "Horizontal scrollbar without height is not possible" << std::endl;
            return false;
        }
        // only need to set the final height here, position and width is calculated in scrollarea
        bar->set_geometry(CL_Rectf(0, 0, CL_Sizef(1, height)));
    }

    pugi::xml_node thumbNode = node.child("thumb");
    pugi::xml_node defaultThumbNode = defaultNode.child("thumb");
    if (thumbNode || defaultThumbNode) {
        if (!parseScrollBarTextures(bar->thumbTextures_, bar->thumbColors_, bar->thumbHueInfos_, thumbNode, defaultThumbNode)) {
            LOG_ERROR << "Error parsing scrollbar thumb" << std::endl;
            return false;
        }
    } else {
        LOG_ERROR << "No thumb node given for scrollbar" << std::endl;
        return false;
    }

    pugi::xml_node incNode = node.child("increment");
    pugi::xml_node defaultIncNode = defaultNode.child("increment");
    if (incNode || defaultIncNode) {
        if (!parseScrollBarTextures(bar->incrementTextures_, bar->incrementColors_, bar->incrementHueInfos_, incNode, defaultIncNode)) {
            LOG_ERROR << "Error parsing scrollbar increment" << std::endl;
            return false;
        }
    } else {
        LOG_ERROR << "No increment node given for scrollbar" << std::endl;
        return false;
    }

    pugi::xml_node decNode = node.child("decrement");
    pugi::xml_node defaultDecNode = defaultNode.child("decrement");
    if (decNode || defaultDecNode) {
        if (!parseScrollBarTextures(bar->decrementTextures_, bar->decrementColors_, bar->decrementHueInfos_, decNode, defaultDecNode)) {
            LOG_ERROR << "Error parsing scrollbar decrement" << std::endl;
            return false;
        }
    } else {
        LOG_ERROR << "No decrement node given for scrollbar" << std::endl;
        return false;
    }

    pugi::xml_node trackNode = node.child("track");
    pugi::xml_node defaultTrackNode = defaultNode.child("track");
    if (trackNode || defaultTrackNode) {
        bar->trackTexture_ = parseTexture(trackNode, defaultTrackNode);
        bar->trackHueInfo_[1u] = getAttribute("hue", trackNode, defaultTrackNode).as_uint();
        std::string rgba = getAttribute("color", trackNode, defaultTrackNode).value();
        if (rgba.length() > 0) {
            bar->trackColor_ = CL_Colorf(rgba);
        }
    } else {
        LOG_ERROR << "No track node given for scrollbar" << std::endl;
        return false;
    }


    return true;
}


bool XmlLoader::parseImage(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    unsigned int hue = getAttribute("hue", node, defaultNode).as_uint();
    std::string rgba = getAttribute("color", node, defaultNode).value();
    float alpha = getAttribute("alpha", node, defaultNode).as_float();

    bool tiled = getAttribute("tiled", node, defaultNode).as_bool();

    components::Image* img = new components::Image(parent);
    parseId(node, img);

    boost::shared_ptr<ui::Texture> texture = parseTexture(node, defaultNode);
    if (!texture) {
        LOG_ERROR << "Unable to parse gump image, source=" << getAttribute("source", node, defaultNode) << " id=" << getAttribute("id", node, defaultNode) << std::endl;
        return false;
    }

    if (bounds.get_width() == 0 || bounds.get_height() == 0) {
        if (texture->getWidth() != 1) {
            bounds.set_width(texture->getWidth());
            bounds.set_height(texture->getHeight());
        } else {
            img->setAutoResize(true);
            bounds.set_width(1);
            bounds.set_height(1);
        }
    } else if (tiled) {
        img->setTiled(true);
    }

    img->setTexture(texture);
    img->set_geometry(bounds);

    img->setHue(hue);
    if (rgba.length() > 0) {
        img->setColorRGBA(CL_Colorf(rgba));
    }

    if (alpha) {
        img->setAlpha(alpha);
    }

    top->addToCurrentPage(img);
    return true;
}

bool XmlLoader::parseButton(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    unsigned int buttonId = getAttribute("buttonid", node, defaultNode).as_uint();
    unsigned int pageId = getAttribute("page", node, defaultNode).as_uint();
    UnicodeString action = StringConverter::fromUtf8(getAttribute("action", node, defaultNode).value());
    UnicodeString param = StringConverter::fromUtf8(getAttribute("param", node, defaultNode).value());
    UnicodeString param2 = StringConverter::fromUtf8(getAttribute("param2", node, defaultNode).value());
    UnicodeString param3 = StringConverter::fromUtf8(getAttribute("param3", node, defaultNode).value());
    UnicodeString param4 = StringConverter::fromUtf8(getAttribute("param4", node, defaultNode).value());
    UnicodeString param5 = StringConverter::fromUtf8(getAttribute("param5", node, defaultNode).value());

    std::string align = getAttribute("font-align", node, defaultNode).value();
    UnicodeString fontName = getAttribute("font", node, defaultNode).value();
    unsigned int fontHeight = getAttribute("font-height", node, defaultNode).as_uint();
    UnicodeString text = getAttribute("text", node, defaultNode).value();

    components::Button* button = new components::Button(parent);
    if (action.length() > 0) {
        button->setLocalButton(action);
        button->setParameter(param, 0);
        button->setParameter(param2, 1);
        button->setParameter(param3, 2);
        button->setParameter(param4, 3);
        button->setParameter(param5, 4);
    } else if (!getAttribute("buttonid", node, defaultNode).empty()) {
        button->setServerButton(buttonId);
    } else if (!getAttribute("page", node, defaultNode).empty()) {
        button->setPageButton(pageId);
    } else {
        LOG_WARN << "Button without action, id or page" << std::endl;
        return false;
    }

    if (align == "left") {
        button->setFontAlignment(components::Label::Alignment::LEFT);
    } else if (align == "right") {
        button->setFontAlignment(components::Label::Alignment::RIGHT);
    } else if (align.length() == 0 || align == "center") {
        button->setFontAlignment(components::Label::Alignment::CENTER);
    } else {
        LOG_WARN << "Unknown button align: " << align << std::endl;
        return false;
    }
    button->setFont(fontName, fontHeight);

    if (text.length() > 0) {
        button->setText(0, text);
        button->setText(1, text);
        button->setText(2, text);
    }

    parseId(node, button);

    pugi::xml_node normalNode = node.child("normal");
    pugi::xml_node mouseOverNode = node.child("mouseover");
    pugi::xml_node mouseDownNode = node.child("mousedown");

    pugi::xml_node defaultNormalNode = defaultNode.child("normal");
    pugi::xml_node defaultMouseOverNode = defaultNode.child("mouseover");
    pugi::xml_node defaultMouseDownNode = defaultNode.child("mousedown");

    if (normalNode || defaultNormalNode) {
        parseMultiTextureImage(normalNode, defaultNormalNode, button, components::Button::TEX_INDEX_UP);
        parseButtonText(normalNode, defaultNormalNode, button, components::Button::TEX_INDEX_UP);
    } else {
        LOG_ERROR << "Normal image for uo button not defined" << std::endl;
        node.print(std::cout);
        return false;
    }

    if (mouseOverNode || defaultMouseOverNode) {
        parseMultiTextureImage(mouseOverNode, defaultMouseOverNode, button, components::Button::TEX_INDEX_MOUSEOVER);
        parseButtonText(mouseOverNode, defaultMouseOverNode, button, components::Button::TEX_INDEX_MOUSEOVER);
    }
    if (mouseDownNode || defaultMouseDownNode) {
        parseMultiTextureImage(mouseDownNode, defaultMouseDownNode, button, components::Button::TEX_INDEX_DOWN);
        parseButtonText(mouseDownNode, defaultMouseDownNode, button, components::Button::TEX_INDEX_DOWN);
    }

    if (bounds.get_width() == 0 || bounds.get_height() == 0) {
        button->setAutoResize(true);
        bounds.set_width(1);
        bounds.set_height(1);
    }

    button->updateTexture();

    button->set_geometry(bounds);

    top->addToCurrentPage(button);

    return true;
}

bool XmlLoader::parseBackground(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    unsigned int hue = getAttribute("hue", node, defaultNode).as_uint();
    std::string rgba = getAttribute("color", node, defaultNode).value();
    float alpha = getAttribute("alpha", node, defaultNode).as_float();

    unsigned int gumpId = getAttribute("gumpid", node, defaultNode).as_uint();

    if (!gumpId) {
        LOG_ERROR << "Unable to parse background, gumpid not found, not a number or zero" << std::endl;
        return false;
    }

    components::Background* img = new components::Background(parent);
    parseId(node, img);

    img->set_geometry(bounds);

    img->setBaseId(gumpId);

    img->setHue(hue);
    if (rgba.length() > 0) {
        img->setColorRGBA(CL_Colorf(rgba));
    }

    if (alpha) {
        img->setAlpha(alpha);
    }

    top->addToCurrentPage(img);
    return true;
}

bool XmlLoader::parsePage(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    unsigned int number = getAttribute("number", node, defaultNode).as_uint();

    if (top->getActivePageId() != 0) {
        // check that we add pages only at the top level hierarchy
        // adding a page inside another page
        LOG_ERROR << "Adding page " << top->getActivePageId() << " inside of page " << number << std::endl;
        return false;
    }

    top->addPage(number);

    bool ret = parseChildren(node, parent, top);

    top->activatePage(0);

    return ret;
}

bool XmlLoader::parseCheckbox(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    unsigned int switchId = getAttribute("switchid", node, defaultNode).as_uint();
    bool isChecked = getAttribute("checked", node, defaultNode).as_bool();

    components::Checkbox* cb = new components::Checkbox(parent);
    cb->setSwitchId(switchId);

    parseId(node, cb);

    pugi::xml_node uncheckedNode = node.child("unchecked");
    pugi::xml_node uncheckedMoNode = node.child("unchecked-mouseover");
    pugi::xml_node checkedNode = node.child("checked");
    pugi::xml_node checkedMoNode = node.child("checked-mouseover");

    pugi::xml_node defUncheckedNode = defaultNode.child("unchecked");
    pugi::xml_node defUncheckedMoNode = defaultNode.child("unchecked-mouseover");
    pugi::xml_node defCheckedNode = defaultNode.child("checked");
    pugi::xml_node defCheckedMoNode = defaultNode.child("checked-mouseover");

    if ((checkedNode || defCheckedNode) && (uncheckedNode || defUncheckedNode)) {
        parseMultiTextureImage(uncheckedNode, defUncheckedNode, cb, components::Checkbox::TEX_INDEX_UNCHECKED);
        parseMultiTextureImage(checkedNode, defCheckedNode, cb, components::Checkbox::TEX_INDEX_CHECKED);
    } else {
        LOG_ERROR << "Checkbox needs to have checked and unchecked image node" << std::endl;
        return false;
    }

    if (uncheckedMoNode || defUncheckedMoNode) {
        parseMultiTextureImage(uncheckedMoNode, defUncheckedMoNode, cb, components::Checkbox::TEX_INDEX_UNCHECKED_MOUSEOVER);
    }
    if (checkedMoNode || defCheckedMoNode) {
        parseMultiTextureImage(checkedMoNode, defCheckedMoNode, cb, components::Checkbox::TEX_INDEX_CHECKED_MOUSEOVER);
    }

    if (bounds.get_width() == 0 || bounds.get_height() == 0) {
        cb->setAutoResize(true);
        bounds.set_width(1);
        bounds.set_height(1);
    }

    cb->setChecked(isChecked);

    cb->set_geometry(bounds);

    top->addToCurrentPage(cb);
    return true;
}

bool XmlLoader::parseRadioButton(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    unsigned int switchId = getAttribute("switchid", node, defaultNode).as_uint();
    bool isChecked = getAttribute("checked", node, defaultNode).as_bool();
    unsigned int groupId = getAttribute("group", node, defaultNode).as_uint();

    components::RadioButton* rb = new components::RadioButton(parent);
    rb->setSwitchId(switchId);
    rb->setRadioGroupId(groupId);

    parseId(node, rb);

    pugi::xml_node uncheckedNode = node.child("unchecked");
    pugi::xml_node uncheckedMoNode = node.child("unchecked-mouseover");
    pugi::xml_node checkedNode = node.child("checked");
    pugi::xml_node checkedMoNode = node.child("checked-mouseover");

    pugi::xml_node defUncheckedNode = defaultNode.child("unchecked");
    pugi::xml_node defUncheckedMoNode = defaultNode.child("unchecked-mouseover");
    pugi::xml_node defCheckedNode = defaultNode.child("checked");
    pugi::xml_node defCheckedMoNode = defaultNode.child("checked-mouseover");

    if ((checkedNode || defCheckedNode) && (uncheckedNode || defUncheckedNode)) {
        parseMultiTextureImage(uncheckedNode, defUncheckedNode, rb, components::Checkbox::TEX_INDEX_UNCHECKED);
        parseMultiTextureImage(checkedNode, defCheckedNode, rb, components::Checkbox::TEX_INDEX_CHECKED);
    } else {
        LOG_ERROR << "Checkbox needs to have checked and unchecked image node" << std::endl;
        return false;
    }

    if (uncheckedMoNode || defUncheckedMoNode) {
        parseMultiTextureImage(uncheckedMoNode, defUncheckedMoNode, rb, components::Checkbox::TEX_INDEX_UNCHECKED_MOUSEOVER);
    }
    if (checkedMoNode || defCheckedMoNode) {
        parseMultiTextureImage(checkedMoNode, defCheckedMoNode, rb, components::Checkbox::TEX_INDEX_CHECKED_MOUSEOVER);
    }

    if (bounds.get_width() == 0 || bounds.get_height() == 0) {
        rb->setAutoResize(true);
        bounds.set_width(1);
        bounds.set_height(1);
    }

    rb->setChecked(isChecked);

    rb->set_geometry(bounds);

    top->addToCurrentPage(rb);
    return true;
}

bool XmlLoader::parseLabelHelper(components::Label* label, pugi::xml_node& node, pugi::xml_node& defaultNode) {
    std::string align = getAttribute("align", node, defaultNode).value();
    UnicodeString fontName = getAttribute("font", node, defaultNode).value();
    unsigned int fontHeight = getAttribute("font-height", node, defaultNode).as_uint();
    unsigned int hue = getAttribute("hue", node, defaultNode).as_uint();
    std::string rgba = getAttribute("color", node, defaultNode).value();
    CL_Colorf color(rgba);

    parseId(node, label);

    if (align.length() == 0 || align == "left") {
        label->setAlignment(components::Label::Alignment::LEFT);
    } else if (align == "right") {
        label->setAlignment(components::Label::Alignment::RIGHT);
    } else if (align == "center") {
        label->setAlignment(components::Label::Alignment::CENTER);
    } else {
        LOG_WARN << "Unknown label align: " << align << std::endl;
        return false;
    }

    label->setFontName(fontName);
    label->setFontHeight(fontHeight);

    // if the node has its own color or hue property, don't use the template values
    if (node.attribute("color")) {
        label->setColor(color);
    } else if (node.attribute("hue")) {
        label->setHue(hue);
    } else if (rgba.length() > 0) {
        label->setColor(color);
    } else {
        label->setHue(hue);
    }

    return true;
}

bool XmlLoader::parseLabel(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    UnicodeString text = getAttribute("text", node, defaultNode).value();

    components::Label* label = new components::Label(parent);
    if (!parseLabelHelper(label, node, defaultNode)) {
        return false;
    }
    label->setText(text);
    label->set_geometry(bounds);

    top->addToCurrentPage(label);
    return true;
}

bool XmlLoader::parsePropertyLabel(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    UnicodeString link = StringConverter::fromUtf8(node.attribute("link").value());

    if (link.length() == 0) {
        LOG_WARN << "PropertyLabel without link" << std::endl;
        return false;
    }

    components::PropertyLabel* label = new components::PropertyLabel(parent, link);
    if (!parseLabelHelper(label, node, defaultNode)) {
        return false;
    }
    label->set_geometry(bounds);

    top->addToCurrentPage(label);
    return true;
}

bool XmlLoader::parseSysLogLabel(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    components::SysLogLabel* label = new components::SysLogLabel(top);
    label->setMaxGeometry(bounds);
    if (!parseLabelHelper(label, node, defaultNode)) {
        return false;
    }

    top->addToCurrentPage(label);
    return true;
}

bool XmlLoader::parseClickLabel(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    UnicodeString text = getAttribute("text", node, defaultNode).value();

    unsigned int buttonId = node.attribute("buttonid").as_uint();
    unsigned int pageId = node.attribute("page").as_uint();
    UnicodeString action = StringConverter::fromUtf8(node.attribute("action").value());
    UnicodeString param = StringConverter::fromUtf8(node.attribute("param").value());
    UnicodeString param2 = StringConverter::fromUtf8(node.attribute("param2").value());
    UnicodeString param3 = StringConverter::fromUtf8(node.attribute("param3").value());
    UnicodeString param4 = StringConverter::fromUtf8(node.attribute("param4").value());
    UnicodeString param5 = StringConverter::fromUtf8(node.attribute("param5").value());

    components::ClickLabel* label = new components::ClickLabel(parent);
    if (!parseLabelHelper(label, node, defaultNode)) {
        return false;
    }

    if (action.length() > 0) {
        label->setLocalButton(action);
        label->setParameter(param, 0);
        label->setParameter(param2, 1);
        label->setParameter(param3, 2);
        label->setParameter(param4, 3);
        label->setParameter(param5, 4);
    } else if (!node.attribute("buttonid").empty()) {
        label->setServerButton(buttonId);
    } else if (!node.attribute("page").empty()) {
        label->setPageButton(pageId);
    } else {
        LOG_WARN << "ClickLabel without action, id or page" << std::endl;
        return false;
    }

    label->setText(text);
    label->set_geometry(bounds);

    top->addToCurrentPage(label);
    return true;
}

bool XmlLoader::parseLineEdit(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    UnicodeString text = StringConverter::fromUtf8(getAttribute("text", node, defaultNode).value());
    int numeric = getAttribute("numeric", node, defaultNode).as_int();
    int password = getAttribute("password", node, defaultNode).as_int();
    unsigned int maxlength = getAttribute("maxlength", node, defaultNode).as_uint();
    UnicodeString action = StringConverter::fromUtf8(getAttribute("action", node, defaultNode).value());
    UnicodeString fontName = StringConverter::fromUtf8(getAttribute("font", node, defaultNode).value());
    unsigned int fontHeight = getAttribute("font-height", node, defaultNode).as_uint();
    std::string rgba = getAttribute("color", node, defaultNode).value();
    unsigned int hue = getAttribute("hue", node, defaultNode).as_uint();
    CL_Colorf color(rgba);

    components::LineEdit* edit = new components::LineEdit(parent);
    parseId(node, edit);
    edit->setFont(fontName, fontHeight);
    edit->setText(text);
    edit->set_geometry(bounds);

    if (password) {
        edit->setPasswordMode(true);
    }
    if (numeric) {
        edit->setNumericMode(true);
    }
    if (maxlength) {
        edit->setMaxLength(maxlength);
    }

    if (action.length() > 0) {
        edit->setAction(action);
    }

    // if the node has its own color or hue property, don't use the template values
    if (node.attribute("color")) {
        edit->setFontColor(color);
    } else if (node.attribute("hue")) {
        edit->setFontHue(hue);
    } else if (rgba.length() > 0) {
        edit->setFontColor(color);
    } else {
        edit->setFontHue(hue);
    }

    top->addToCurrentPage(edit);
    return true;
}

bool XmlLoader::parseScrollArea(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    components::ScrollArea* area = new components::ScrollArea(parent);
    parseId(node, area);
    area->set_geometry(bounds);

    parseScrollBar(area->getVerticalScrollBar(), area, true, node.child("vscroll"), defaultNode.child("vscroll"));
    parseScrollBar(area->getHorizontalScrollBar(), area, false, node.child("hscroll"), defaultNode.child("hscroll"));

    unsigned int hLineStep = node.attribute("hstep").as_uint();
    unsigned int hPageStep = node.attribute("hpage").as_uint();
    unsigned int vLineStep = node.attribute("vstep").as_uint();
    unsigned int vPageStep = node.attribute("vpage").as_uint();
    unsigned int marginLeft = node.attribute("marginleft").as_uint();
    unsigned int marginBottom = node.attribute("marginbottom").as_uint();

    top->addToCurrentPage(area);

    pugi::xml_node contentNode = node.child("content");
    if (contentNode) {
        parseChildren(contentNode, area->getClientArea(), top);
    } else {
        LOG_ERROR << "No content node in scrollarea" << std::endl;
    }

    area->updateScrollbars(area->getVerticalScrollBar()->getVisibility(), area->getHorizontalScrollBar()->getVisibility(),
            vPageStep, hPageStep, vLineStep, hLineStep, marginLeft, marginBottom);
    area->setupResizeHandler();

    return true;
}


bool XmlLoader::parseWorldView(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    ui::components::WorldView* worldView = new ui::components::WorldView(parent, bounds);
    parseId(node, worldView);
    top->addToCurrentPage(worldView);

    return true;
}

bool XmlLoader::parsePaperdoll(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    ui::components::PaperdollView* pdView = new ui::components::PaperdollView(parent, bounds);
    parseId(node, pdView);
    top->addToCurrentPage(pdView);

    return true;
}

bool XmlLoader::parseContainer(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    ui::components::ContainerView* contView = new ui::components::ContainerView(parent, bounds);
    parseId(node, contView);
    top->addToCurrentPage(contView);

    return true;
}

bool XmlLoader::parseWarModeButton(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    components::WarModeButton* button = new components::WarModeButton(parent);
    parseId(node, button);

    pugi::xml_node normalNode = node.child("normal");
    pugi::xml_node mouseOverNode = node.child("mouseover");
    pugi::xml_node mouseDownNode = node.child("mousedown");

    pugi::xml_node defaultNormalNode = defaultNode.child("normal");
    pugi::xml_node defaultMouseOverNode = defaultNode.child("mouseover");
    pugi::xml_node defaultMouseDownNode = defaultNode.child("mousedown");

    if (normalNode || defaultNormalNode) {
        parseMultiTextureImage(normalNode, defaultNormalNode, button, components::WarModeButton::TEX_INDEX_UP);
    } else {
        LOG_ERROR << "Normal image for warmode button not defined" << std::endl;
        return false;
    }

    if (mouseOverNode || defaultMouseOverNode) {
        parseMultiTextureImage(mouseOverNode, defaultMouseOverNode, button, components::WarModeButton::TEX_INDEX_MOUSEOVER);
    }
    if (mouseDownNode || defaultMouseDownNode) {
        parseMultiTextureImage(mouseDownNode, defaultMouseDownNode, button, components::WarModeButton::TEX_INDEX_DOWN);
    }


    pugi::xml_node warmodeNormalNode = node.child("warmode-normal");
    pugi::xml_node warmodeMouseOverNode = node.child("warmode-mouseover");
    pugi::xml_node warmodeMouseDownNode = node.child("warmode-mousedown");

    pugi::xml_node warmodeDefaultNormalNode = defaultNode.child("warmode-normal");
    pugi::xml_node warmodeDefaultMouseOverNode = defaultNode.child("warmode-mouseover");
    pugi::xml_node warmodeDefaultMouseDownNode = defaultNode.child("warmode-mousedown");

    if (warmodeNormalNode || warmodeDefaultNormalNode) {
        parseMultiTextureImage(warmodeNormalNode, warmodeDefaultNormalNode, button, components::WarModeButton::WARMODE_TEX_INDEX_UP);
    }
    if (warmodeMouseOverNode || warmodeDefaultMouseOverNode) {
        parseMultiTextureImage(warmodeMouseOverNode, warmodeDefaultMouseOverNode, button, components::WarModeButton::WARMODE_TEX_INDEX_MOUSEOVER);
    }
    if (warmodeMouseDownNode || warmodeDefaultMouseDownNode) {
        parseMultiTextureImage(warmodeMouseDownNode, warmodeDefaultMouseDownNode, button, components::WarModeButton::WARMODE_TEX_INDEX_DOWN);
    }

    if (bounds.get_width() == 0 || bounds.get_height() == 0) {
        button->setAutoResize(true);
        bounds.set_width(1);
        bounds.set_height(1);
    }

    button->updateTexture();

    button->set_geometry(bounds);

    top->addToCurrentPage(button);
    return true;
}




bool XmlLoader::parseRepeat(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    UnicodeString name(node.attribute("name").value());

    if (repeatContexts_.count(name) == 0) {
        LOG_ERROR << "Trying to access unknown repeat context " << name << std::endl;
        return false;
    }

    const RepeatContext& context = repeatContexts_[name];

    int xIncrease = node.attribute("xincrease").as_int();
    int yIncrease = node.attribute("yincrease").as_int();
    unsigned int xLimit = node.attribute("xlimit").as_uint();
    unsigned int yLimit = node.attribute("ylimit").as_uint();

    for (unsigned int index = 0; index < context.repeatCount_; ++index) {
        insertRepeatNodes(node.begin(), node.end(), node.parent(), context, index,
                xIncrease, yIncrease, xLimit, yLimit);
    }

    return true;
}

void XmlLoader::insertRepeatNodes(pugi::xml_node::iterator begin, pugi::xml_node::iterator end, pugi::xml_node dst,
            const RepeatContext& context, unsigned int index,
            int xIncrease, int yIncrease, unsigned int xLimit, unsigned int yLimit) {
    for (pugi::xml_node::iterator iter = begin; iter != end; ++iter) {
        pugi::xml_node newNode = dst.insert_copy_after(*iter, dst.last_child());

        replaceRepeatKeywords(newNode, context, index,
                xIncrease, yIncrease, xLimit, yLimit);

        checkRepeatIf(newNode, index, xLimit, yLimit);
    }
}

void XmlLoader::checkRepeatIf(pugi::xml_node& node, unsigned int index, unsigned int xLimit, unsigned int yLimit) {
    unsigned int xIndex = xLimit > 0 ? index % xLimit : index;
    unsigned int yIndex = yLimit > 0 ? index % yLimit : index;

    bool removeNode = false;
    if (strcmp(node.name(), "repeatif") == 0) {
        pugi::xml_node::attribute_iterator attrIter = node.attributes_begin();
        pugi::xml_node::attribute_iterator attrEnd = node.attributes_end();

        for (; attrIter != attrEnd; ++attrIter) {
            if (strcmp(attrIter->name(), "xindex") == 0) {
                if (attrIter->as_uint() != xIndex) {
                    removeNode = true;
                    break;
                }
            } else if (strcmp(attrIter->name(), "yindex") == 0) {
                if (attrIter->as_uint() != yIndex) {
                    removeNode = true;
                    break;
                }
            } else if (strcmp(attrIter->name(), "test") == 0) {
                if (attrIter->as_uint() != 1) {
                    removeNode = true;
                    break;
                }
            }
        }

        if (!removeNode) {
            // move children to parent node
            pugi::xml_node childIter = node.last_child();
            while (childIter) {
                pugi::xml_node newChild = node.parent().insert_copy_after(childIter, node);
                checkRepeatIf(newChild, index, xLimit, yLimit);
                childIter = childIter.previous_sibling();
            }
        }

        // remove repeatif node
        node.parent().remove_child(node);
    } else {
        pugi::xml_node childIter = node.first_child();
        while (childIter) {
            checkRepeatIf(childIter, index, xLimit, yLimit);
            childIter = childIter.next_sibling();
        }
    }
}

void XmlLoader::replaceRepeatKeywords(pugi::xml_node& node, const RepeatContext& context, unsigned int index,
            int xIncrease, int yIncrease, unsigned int xLimit, unsigned int yLimit) {

    static std::string attrNameX("x");
    static std::string attrNameY("y");

    pugi::xml_node::attribute_iterator attrIter = node.attributes_begin();
    pugi::xml_node::attribute_iterator attrEnd = node.attributes_end();

    for (; attrIter != attrEnd; ++attrIter) {
        bool contextHit = false;

        std::map<UnicodeString, std::vector<UnicodeString> >::const_iterator contextIter = context.keywordReplacements_.begin();
        std::map<UnicodeString, std::vector<UnicodeString> >::const_iterator contextEnd = context.keywordReplacements_.end();

        for (; contextIter != contextEnd; ++contextIter) {
            if (contextIter->first == attrIter->value()) {
                contextHit = true;

                attrIter->set_value(StringConverter::toUtf8String(contextIter->second[index]).c_str());
                break;
            }
        }

        if (!contextHit) {
            // increase and y values, if found
            if (attrNameX == attrIter->name()) {
                int baseX = attrIter->as_int();
                unsigned int xIndex = xLimit > 0 ? index % xLimit : index;
                int curXIncrease = xIncrease * xIndex;
                int curX = baseX + curXIncrease;
                attrIter->set_value(curX);
            } else if (attrNameY == attrIter->name()) {
                int baseY = attrIter->as_int();
                unsigned int yIndex = yLimit > 0 ? index % yLimit : index;
                int curYIncrease = yIncrease * yIndex;
                int curY = baseY + curYIncrease;
                attrIter->set_value(curY);
            }
        }
    }

    // also apply keyword replacements to child nodes
    pugi::xml_node childIter = node.first_child();
    while (childIter) {
        replaceRepeatKeywords(childIter, context, index,
                xIncrease, yIncrease, xLimit, yLimit);
        childIter = childIter.next_sibling();
    }
}





bool XmlLoader::parseTComboBox(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    CL_ComboBox* box = new CL_ComboBox(parent);
    parseId(node, box);
    box->set_geometry(bounds);

    CL_PopupMenu menu;

    pugi::xml_node_iterator iter = node.begin();
    pugi::xml_node_iterator iterEnd = node.end();

    unsigned int selected = 0;

    for (unsigned int index = 0; iter != iterEnd; ++iter, ++index) {
        if (strcmp(iter->name(), "option") != 0) {
            LOG_WARN << "Something different than option in combobox: " << iter->name() << std::endl;
            return false;
        } else {
            std::string text = iter->attribute("text").value();
            int isSelected = iter->attribute("selected").as_int();

            menu.insert_item(text);
            if (isSelected) {
                selected = index;
            }
        }
    }

    box->set_popup_menu(menu);
    box->set_selected_item(selected);

    top->addToCurrentPage(box);
    return true;
}

bool XmlLoader::parseTGroupBox(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    CL_GroupBox* box = new CL_GroupBox(parent);
    parseId(node, box);
    box->set_geometry(bounds);

    parseChildren(node, box, top);

    top->addToCurrentPage(box);
    return true;
}

bool XmlLoader::parseTSpin(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    std::string type = node.attribute("type").value();

    CL_Spin* spin = new CL_Spin(parent);
    parseId(node, spin);

    if (type == "int") {
        int min = node.attribute("min").as_int();
        int max = node.attribute("max").as_int();
        unsigned int stepsize = node.attribute("stepsize").as_uint();
        int value = node.attribute("value").as_int();

        spin->set_floating_point_mode(false);
        spin->set_ranges(min, max);
        spin->set_step_size(stepsize);
        spin->set_value(value);
    } else if (type == "float") {
        float min = node.attribute("min").as_float();
        float max = node.attribute("max").as_float();
        float stepsize = node.attribute("stepsize").as_float();
        float value = node.attribute("value").as_float();

        spin->set_floating_point_mode(true);
        spin->set_ranges_float(min, max);
        spin->set_step_size_float(stepsize);
        spin->set_value_float(value);
    } else {
        LOG_WARN << "Unknown spin type: " << type << std::endl;
        return false;
    }

    spin->set_geometry(bounds);

    top->addToCurrentPage(spin);
    return true;
}

bool XmlLoader::parseTTabs(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    CL_Tab* tabs = new CL_Tab(parent);
    parseId(node, tabs);
    tabs->set_geometry(bounds);

    pugi::xml_node_iterator iter = node.begin();
    pugi::xml_node_iterator iterEnd = node.end();

    for (; iter != iterEnd; ++iter) {
        if (strcmp(iter->name(), "ttabpage") != 0) {
            LOG_WARN << "Something different than ttabpage in ttabs: " << iter->name() << std::endl;
            return false;
        } else {
            std::string tabTitle = iter->attribute("text").value();
            CL_TabPage* newpage = tabs->add_page(tabTitle);
            parseId(*iter, newpage);

            parseChildren(*iter, newpage, top);
        }
    }

    top->addToCurrentPage(tabs);
    return true;
}

bool XmlLoader::parseTSlider(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    std::string type = node.attribute("type").value();
    int min = node.attribute("min").as_int();
    int max = node.attribute("max").as_int();
    unsigned int pagestep = node.attribute("pagestep").as_uint();
    int value = node.attribute("value").as_int();


    CL_Slider* slider = new CL_Slider(parent);
    parseId(node, slider);

    if (type == "vertical") {
        slider->set_vertical(true);
    } else if (type == "horizontal") {
        slider->set_horizontal(true);
    } else {
        LOG_WARN << "Unknown slider type: " << type << std::endl;
        return false;
    }

    slider->set_min(min);
    slider->set_max(max);
    slider->set_page_step(pagestep);
    slider->set_position(value);
    slider->set_lock_to_ticks(false);
    slider->set_geometry(bounds);

    top->addToCurrentPage(slider);
    return true;
}

bool XmlLoader::parseTTextEdit(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    // does not seem to work currently (clanlib problem)

    //CL_Rect bounds = getBoundsFromNode(node, defaultNode);
    //std::string text = node.attribute("text").value();

    //CL_GUIComponentDescription desc;
    //std::string cssid = node.attribute("cssid").value();
    //desc.set_type_name("TextEdit");
    //if (cssid.length() > 0) {
        //desc.set_id_name(cssid);
    //}
    //CL_TextEdit* edit = new CL_TextEdit(desc, parent);
    //edit->set_geometry(bounds);

    //return true;

    LOG_WARN << "TextEdit is currently not supported, sorry!" << std::endl;
    return false;
}

bool XmlLoader::parseTBackground(pugi::xml_node& node, pugi::xml_node& defaultNode, CL_GUIComponent* parent, GumpMenu* top) {
    CL_Rect bounds = getBoundsFromNode(node, defaultNode);

    components::TBackground* bg = new components::TBackground(parent);
    parseId(node, bg);
    bg->set_geometry(bounds);

    top->addToCurrentPage(bg);
    return true;
}

}
}