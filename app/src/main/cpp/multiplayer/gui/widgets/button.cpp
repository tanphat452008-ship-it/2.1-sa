#include "main.h"
#include "button.h"
#include "gui/gui.h"
//extern bool OpenButton;

//============== Default Button =================//
Button::Button(const std::string& caption, float font_size)
{
    m_callback = nullptr;

    m_label = new Label(caption, ImColor(1.0f, 1.0f, 1.0f), false, font_size);
    this->addChild(m_label);

    m_color = ImColor(25, 25, 25, 200);
    m_colorFocused = ImColor(55, 55, 55, 240);/*ImColor(119, 4, 4, 255);*/
}

void Button::performLayout()
{
    float padding = 15.0f;

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padding * 2, padding));

    m_label->setPosition((size() - m_label->size()) / 2);
}

void Button::draw(ImGuiRenderer* renderer)
{
    // Ø¨Û•ØªÙ†Û• Ø±Ø§ÙˆÙ†Ø¯ÛŒØ¯ Ú©Ø±Ø§ÙˆÛ• Ùˆ outline Ù„Ø§Ø¨Ø±Ø§ÙˆÛ•
    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            focused() ? m_colorFocused : m_color,
            true,
            20.0f // âœ… radius
    );

    Widget::draw(renderer);
}

void Button::touchPopEvent()
{
    if (m_callback) m_callback();
}


//============== Custom Button =================//
CButton::CButton(const std::string& caption, float font_size)
{
    m_callback = nullptr;

    m_label = new Label(caption, ImColor(1.0f, 1.0f, 1.0f), false, font_size);
    this->addChild(m_label);

    m_color =  ImColor(25, 25, 25, 200);
    m_colorFocused = ImColor(55, 55, 55, 240);
}

void CButton::performLayout()
{
    float padding = 15.0f;

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padding * 2, padding));

    m_label->setPosition((size() - m_label->size()) / 2);
}

void CButton::draw(ImGuiRenderer* renderer)
{
   // if (!OpenButton) return;

    // Ø±Ø§ÙˆÙ†Ø¯ÛŒØ¯ Ùˆ Ø¨ÛŽ outline
    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            focused() ? m_colorFocused : m_color,
            true,
            20.0f
    );

    Widget::draw(renderer);
}

void CButton::touchPopEvent()
{
   // if (!OpenButton) return;
    if (m_callback) m_callback();
}


//============== OButton =================//
OButton::OButton(const std::string& caption, float font_size)
{
    m_callback = nullptr;

    m_label = new Label(caption, ImColor(1.0f, 1.0f, 1.0f), false, font_size);
    this->addChild(m_label);

    m_color = ImColor(25, 25, 25, 200);
    m_colorFocused = ImColor(55, 55, 55, 240);
}

void OButton::performLayout()
{
    float padding = 15.0f;

    m_label->performLayout();
    this->setSize(m_label->size() + ImVec2(padding * 2, padding));

    m_label->setPosition((size() - m_label->size()) / 2);
}

void OButton::draw(ImGuiRenderer* renderer)
{
    /*
    if (OpenButton)
    {
        // Ú¯Ø±ØªÙ†ÛŒ Ù¾Û†Ø²ÛŒØ´Ù†ÛŒ Ù„Ø§Ø¯Ø±Ø§Ùˆ
        this->setPosition(ImVec2(-150.0f, -150.0f));
        return;
    }
     */

    // Ø±Ø§ÙˆÙ†Ø¯ÛŒØ¯ Ùˆ Ø¨ÛŽ outline
    renderer->drawRect(
            absolutePosition(),
            absolutePosition() + size(),
            focused() ? m_colorFocused : m_color,
            true,
            20.0f
    );

    Widget::draw(renderer);

    // Ø¯ÙˆÙˆØ¨Ø§Ø±Û• Ù¾Û†Ø²ÛŒØ´Ù†ÛŒ Ø¦Ø§Ø³Ø§ÛŒÛŒ
    this->setPosition(ImVec2(15.0f, 15.0f));
}

void OButton::touchPopEvent()
{
    if (m_callback) m_callback();
}
