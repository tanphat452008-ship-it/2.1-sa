#include "gui/gui.h"
#include "game/game.h"
#include "net/netgame.h"

extern CNetGame* pNetGame;

Spawn::Spawn()
        : Layout(Orientation::HORIZONTAL)
{
    m_buttonPrev = new Button(std::string((const char*)u8"<<"), 30.0f/ 2);
    m_buttonSpawn = new Button(std::string((const char*)u8"Spawn"), 30.0f / 2);
    m_buttonNext = new Button(std::string((const char*)u8">>"), 30.0f / 2);

    m_buttonPrev->setCallback([]() { if (pNetGame) CLocalPlayer::SendPrevClass(); });
    m_buttonSpawn->setCallback([]() { if (pNetGame) CLocalPlayer::SendSpawn(); });
    m_buttonNext->setCallback([]() { if (pNetGame) CLocalPlayer::SendNextClass(); });

    this->addChild(m_buttonPrev);
    this->addChild(m_buttonSpawn);
    this->addChild(m_buttonNext);
}