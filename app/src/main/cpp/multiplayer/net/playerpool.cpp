#include "playerpool.h"
#include "../main.h"
#include "../game/game.h"
#include "netgame.h"
#include "../game/Entity/Ped/Ped.h"
#include "../voice/Plugin.h"
#include "../CSettings.h"
#include "../game/Camera.h"

int g_iStatusDriftChanged = 0;

void CPlayerPool::Free()
{
    auto ids = CNetPool<CRemotePlayer*>::GetAllIds();
    for (auto& id : ids) {
        Delete(id, 0);
    }
}

#include "..//chatwindow.h"

void CPlayerPool::ApplyCollisionChecking()
{
    for(auto & pair : spawnedPlayers) {
        auto pPed = pair.second->GetPlayerPed()->m_pPed;
        if(!pPed->IsInVehicle())
        {
            m_bCollisionChecking[pair.first] = pPed->m_bCollisionProcessed;
            pPed->SetCollisionChecking(true);
        }
    }
}

void CPlayerPool::ResetCollisionChecking()
{
    for(auto & pair : spawnedPlayers) {
        auto pPed = pair.second->GetPlayerPed()->m_pPed;
        if(!pPed->IsInVehicle())
        {
            m_bCollisionChecking[pair.first] = pPed->m_bCollisionProcessed;
            pPed->SetCollisionChecking(false);
        }
    }
}

void CPlayerPool::UpdateScore(PLAYERID playerId, int iScore)
{
	if (playerId == m_LocalPlayerID)
	{
		m_iLocalPlayerScore = iScore;
	}
	else
	{
		if (playerId > MAX_PLAYERS - 1) { return; }
		m_iPlayerScores[playerId] = iScore;
	}
}

void CPlayerPool::UpdatePing(PLAYERID playerId, uint32_t dwPing)
{
	if (playerId == m_LocalPlayerID)
	{
		m_dwLocalPlayerPing = dwPing;
	}
	else
	{
		if (playerId > MAX_PLAYERS - 1) { return; }
		m_dwPlayerPings[playerId] = dwPing;
	}
}

bool CPlayerPool::Process()
{
	for(auto & pair : spawnedPlayers) {
        auto pPlayer = pair.second;
        pPlayer->Process();
	}

	CLocalPlayer::Process();

	if(pNetGame)
	{
		if(g_iStatusDriftChanged != CSettings::Get().szTimeStamp)
		{
			g_iStatusDriftChanged = CSettings::Get().szTimeStamp;

			RakNet::BitStream bs;
			bs.Write((uint8_t)ID_CUSTOM_PACKET_SYSTEM);
			bs.Write((uint8_t)2);
			bs.Write((uint8_t)g_iStatusDriftChanged);
			pNetGame->GetRakClient()->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
		}
	}

	if(CLocalPlayer::IsSpectating())
	{
		CVector vecPos = CCamera::Get().GetPosition();
		CGame::RefreshStreamingAt(vecPos.x, vecPos.y);

		BASS_Set3DPosition(
			reinterpret_cast<const BASS_3DVECTOR*>(&CCamera::Get().GetPosition()), nullptr,
			reinterpret_cast<const BASS_3DVECTOR*>(&CCamera::Get().GetMatrix().GetUp()),
			reinterpret_cast<const BASS_3DVECTOR*>(&CCamera::Get().GetMatrix().GetForward())
		);
		BASS_Apply3D();
	}

	Voice::CVoicePlugin::MainLoop();

	return true;
}

bool CPlayerPool::New(PLAYERID playerId, char *szPlayerName, bool IsNPC)
{
    if(CNetPool<CRemotePlayer*>::GetAt(playerId))
        Delete( playerId, 0 );

	auto newPlayer = CNetPool<CRemotePlayer*>::list[playerId] = new CRemotePlayer();

    strcpy(m_szPlayerNames[playerId], szPlayerName);
    newPlayer->SetID(playerId);
    newPlayer->SetNPC(IsNPC);
    return true;

}

bool CPlayerPool::Delete(PLAYERID playerId, uint8_t byteReason)
{
	if(!CNetPool<CRemotePlayer*>::GetAt(playerId))
		return false;

	if(CLocalPlayer::IsSpectating() && CLocalPlayer::m_SpectateID == playerId)
        CLocalPlayer::ToggleSpectating(false);

	delete CNetPool<CRemotePlayer*>::list[playerId];

	CNetPool<CRemotePlayer*>::list.erase(playerId);

	return true;
}

PLAYERID CPlayerPool::FindRemotePlayerIDFromGtaPtr(CEntity * pActor)
{
	for(auto & pair : spawnedPlayers) {
		auto pPed = pair.second->GetPlayerPed()->m_pPed;
		if (pPed == pActor)
			return pair.first;
	}
	return INVALID_PLAYER_ID;
}
