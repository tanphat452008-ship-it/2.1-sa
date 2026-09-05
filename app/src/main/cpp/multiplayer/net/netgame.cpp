#include "../main.h"
#include "../game/game.h"
#include "../gui/gui.h"
#include "..//util/CJavaWrapper.h"
#include "netgame.h"
#include "java_systems/HUD.h"
#include "CSettings.h"
#include "../game/Entity/Ped/Ped.h"
#include <thread>
#include <chrono>

#include "../chatwindow.h"

#include "..//CLocalisation.h"
#include "../vendor/hash/sha256.h"
#include "crashlytics.h"

#define NETGAME_VERSION 4057
#define AUTH_BS "E02262CF28BC542486C558D4BE9EFB716592AFAF8B"

extern CGUI *pGUI;

int iVehiclePoolProcessFlag = 0;
int iPickupPoolProcessFlag = 0;

void RegisterRPCs(RakClientInterface *pRakClient);
void UnRegisterRPCs(RakClientInterface *pRakClient);
void RegisterScriptRPCs(RakClientInterface *pRakClient);
void UnRegisterScriptRPCs(RakClientInterface *pRakClient);

unsigned char GetPacketID(Packet *p) {
    if (p == 0) return 255;

    if ((unsigned char) p->data[0] == ID_TIMESTAMP)
        return (unsigned char) p->data[sizeof(unsigned char) + sizeof(unsigned long)];
    else
        return (unsigned char) p->data[0];
}

CNetGame::CNetGame(const char *szHostOrIp, int iPort, const char *szPlayerName,
                   const char *szPass) {
    strcpy(m_szHostName, "San Andreas Multiplayer");
    strncpy(m_szHostOrIp, szHostOrIp, sizeof(m_szHostOrIp));
    m_iPort = iPort;

    CVehiclePool::Init();
    CTextDrawPool::Init();
    CAudioStreamPool::Init();
    CPlayerPool::SetLocalPlayerName(szPlayerName);

    g_pWidgetManager = new CWidgetManager();

    m_pRakClient = RakNetworkFactory::GetRakClientInterface();
    RegisterRPCs(m_pRakClient);
    RegisterScriptRPCs(m_pRakClient);

    m_pRakClient->SetPassword(szPass);

    m_dwLastConnectAttempt = GetTickCount();
    SetGameState(eNetworkState::WAIT_CONNECT);

    m_GreenZoneState = false;
    m_iSpawnsAvailable = 0;
    m_byteWorldMinute = 0;
    m_byteWorldTime = 12;
    m_byteWeather = 10;
    m_fGravity = (float) 0.008000000;
    m_bUseCJWalk = false;
    m_bDisableEnterExits = false;
    m_fNameTagDrawDistance = 60.0f;
    m_bZoneNames = false;
    m_bInstagib = false;
    m_iDeathDropMoney = 0;
    m_bNameTagLOS = false;

    for (int i = 0; i < 100; i++)
        m_dwMapIcons[i] = 0;

    CGame::EnableClock(false);
    CGame::EnableZoneNames(false);
    CGame::DisableRaceCheckpoint();

    Log("CNetGame created obj");
}

#include "../voice/Network.h"
CNetGame::~CNetGame() {
    Log("CNetGame::~CNetGame()");

    m_pRakClient->Disconnect(0);
    UnRegisterRPCs(m_pRakClient);
    UnRegisterScriptRPCs(m_pRakClient);
    RakNetworkFactory::DestroyRakClientInterface(m_pRakClient);
    m_pRakClient = nullptr;

    CAudioStreamPool::Free();
    CText3DLabelsPool::Free();
    CPlayerPool::Free();
    CGangZonePool::Free();
    CVehiclePool::Free();
    CPickupPool::Free();

    if (g_pWidgetManager) {
        delete g_pWidgetManager;
        g_pWidgetManager = nullptr;
    }

    if (!CGame::bIsGameExiting) g_pJavaWrapper->ClearScreen();
}

#include "game/IdleCam.h"
#include "voice/Network.h"
#include "game/Snow.h"
#include "game/Camera.h"

void CNetGame::Process() {
    UpdateNetwork();

    // need all frame
    if (CGame::m_bCheckpointsEnabled) {
        CPedSamp *pPlayerDed = CLocalPlayer::GetPlayerPed();
        if (pPlayerDed) {
            ScriptCommand(&is_actor_near_point_3d, pPlayerDed->m_dwGTAId,
                          CGame::m_vecCheckpointPos.x,
                          CGame::m_vecCheckpointPos.y,
                          CGame::m_vecCheckpointPos.z,
                          CGame::m_vecCheckpointExtent.x,
                          CGame::m_vecCheckpointExtent.y,
                          CGame::m_vecCheckpointExtent.z,
                          1);
        }
    }

    // 30 fps limited
    auto curTick = GetTickCount();
    static uint32 last_process_cnetgame = 0;
    if (curTick - last_process_cnetgame >= 33) {
        last_process_cnetgame = curTick;
    } else {
        return;
    }

    if (GetGameState() == eNetworkState::CONNECTED) {
        // pool process
        CPlayerPool::Process();
        CObjectPool::Process();
        CVehiclePool::Process();
        return;

    } else {
        CPedSamp *pPlayer = CGame::FindPlayerPed();

        if (pPlayer) {
            if (pPlayer->m_pPed->IsInVehicle())
                pPlayer->m_pPed->RemoveFromVehicleAndPutAt({314.0f, 160.0f, 39.0f});
            else
                pPlayer->m_pPed->Teleport(CVector(314.0f, 160.0f, 39.0f), false);

            CCamera::SetPosition(-314.0f, 160.0f, 39.0f, 0.0f, 0.0f, 0.0f);
            CCamera::LookAtPoint(-310.0f, 157.0f, 39.0f, 2);
            CHUD::toggleAll(false);
            CGame::SetWorldWeather(m_byteWeather);
        }
    }

    if (GetGameState() == eNetworkState::WAIT_CONNECT &&
        (GetTickCount() - m_dwLastConnectAttempt) > 3000) {
        CChatWindow::AddMessage(CLocalisation::GetMessage(E_MSG::CONNECTING));

        m_pRakClient->Connect(m_szHostOrIp, m_iPort, 0, 0, 5);
        Voice::Network::OnRaknetConnect(m_szHostOrIp, m_iPort);
        m_dwLastConnectAttempt = GetTickCount();
        SetGameState(eNetworkState::CONNECTING);
    }
}

void CNetGame::UpdateNetwork()
{
    while (auto pkt = m_pRakClient->Receive())
    {
        switch (GetPacketID(pkt)) {
            case ID_AUTH_KEY:
                packetAuthKey(pkt);
                break;

            case ID_CONNECTION_ATTEMPT_FAILED:
                CChatWindow::AddMessage(CLocalisation::GetMessage(E_MSG::CONNECTION_ATTEMPT_FAILED));
                SetGameState(eNetworkState::WAIT_CONNECT);
                break;

            case ID_NO_FREE_INCOMING_CONNECTIONS:
                CChatWindow::AddMessage("Server Full...");
                SetGameState(eNetworkState::WAIT_CONNECT);
                break;

            case ID_DISCONNECTION_NOTIFICATION:
                Packet_DisconnectionNotification(pkt);
                break;

            case ID_CONNECTION_LOST:
                Packet_ConnectionLost(pkt);
                break;

            case ID_CONNECTION_REQUEST_ACCEPTED:
                Packet_ConnectionSucceeded(pkt);
                break;

            case ID_FAILED_INITIALIZE_ENCRIPTION:
                CChatWindow::AddMessage("Failed to initialize encryption.");
                break;

            case ID_CONNECTION_BANNED:
                CChatWindow::AddMessage(CLocalisation::GetMessage(E_MSG::BANNED));
                break;

            case ID_INVALID_PASSWORD:
                CChatWindow::AddMessage("Wrong Password!");
                m_pRakClient->Disconnect(0);
                break;

            case ID_VEHICLE_SYNC:
                Packet_VehicleSync(pkt);
                break;

            case ID_AIM_SYNC:
                Packet_AimSync(pkt);
                break;

            case ID_BULLET_SYNC:
                Packet_BulletSync(pkt);
                break;

            case ID_PLAYER_SYNC:
                Packet_PlayerSync(pkt);
                break;

            case ID_MARKERS_SYNC:
                Packet_MarkersSync(pkt);
                break;

            case ID_TRAILER_SYNC:
                Packet_TrailerSync(pkt);
                break;

            case ID_PASSENGER_SYNC:
                Packet_PassengerSync(pkt);
                break;

            case ID_VOICE:
                Voice::Network::OnRaknetReceive(pkt);
                break;

            case ID_CUSTOM_RPC:
                Packet_CustomRPC(pkt);
                break;

            case PACKET_AUTH:
                Packet_AuthRPC(pkt);
                break;

            case PACKET_SPECIALCUSTOM:
                Packet_SpecialCustomRPC(pkt);
                break;
        }

        m_pRakClient->DeallocatePacket(pkt);
    }
}

void CNetGame::Packet_TrailerSync(Packet *p)
{
    RakNet::BitStream data((unsigned char *) p->data, p->length, false);

    BYTE bytePacketID = 0;
    BYTE bytePlayerID = 0;
    TRAILER_SYNC_DATA trSync;

    data.Read(bytePacketID);
    data.Read(bytePlayerID);
    data.Read((char *) &trSync, sizeof(TRAILER_SYNC_DATA));

    auto pPlayer = CPlayerPool::GetSpawnedPlayer(bytePlayerID);

    if (pPlayer) {
        pPlayer->StoreTrailerFullSyncData(&trSync);
    }
}

#include "..//game/CCustomPlateManager.h"
#include "str_obfuscator.hpp"
#include "game/WaterCannons.h"
#include "game/World.h"

void CNetGame::Packet_AuthRPC(Packet *p) {
    // Custom Auth UI đã loại bỏ
}

void CNetGame::Packet_SpecialCustomRPC(Packet *p) {
    // Custom Special UI đã loại bỏ
}

void CNetGame::Packet_CustomRPC(Packet *p) {

    RakNet::BitStream bs((unsigned char *) p->data, p->length, false);
    bs.IgnoreBits(8); // skip packet id

    uint32_t rpcID;
    bs.Read(rpcID);

    LOGRPC("CUSTOM Id = %d", rpcID);

    switch (rpcID) {
        case RPC_PUT_ACTOR_IN_VEH: {
            PLAYERID    actorId;
            uint16      vehicleId;
            uint8       setatId;

            bs.Read(actorId);
            bs.Read(vehicleId);
            bs.Read(setatId);

            auto pActor = CActorPool::GetAt(actorId);
            auto pVehicle = CVehiclePool::GetAt(vehicleId);
            if(!pVehicle || !pActor )
                return;

            if(pActor->m_pPed->IsInVehicle())
                return;

            ScriptCommand(&put_actor_in_car2, pActor->m_dwGTAId, pVehicle->m_dwGTAId, setatId);
            break;
        }
        case RPC_REMOVE_ACTOR_FROM_VEH: {
            PLAYERID    actorId;
            uint16      vehicleId;
            uint8       force;

            bs.Read(actorId);
            bs.Read(vehicleId);
            bs.Read(force);

            auto pActor = CActorPool::GetAt(actorId);
            auto pVehicle = CVehiclePool::GetAt(vehicleId);
            if(!pVehicle || !pActor)
                return;

            if(!pActor->m_pPed->IsInVehicle())
                return;

            if(force)
                pActor->m_pPed->RemoveFromVehicle();
            else
                ScriptCommand(&TASK_LEAVE_ANY_CAR, pActor->m_dwGTAId);
            break;
        }
        case RPC_ACTOR_ENTER_CAR_PASS: {
            PLAYERID    actorId;
            uint16      vehicleId;
            uint8       setatId;

            bs.Read(actorId);
            bs.Read(vehicleId);
            bs.Read(setatId);

            auto pActor = CActorPool::GetAt(actorId);
            auto pVehicle = CVehiclePool::GetAt(vehicleId);
            if(!pVehicle || !pActor)
                return;

            if(pActor->m_pPed->IsInVehicle())
                return;

            ScriptCommand(&send_actor_to_car_passenger, pActor->m_dwGTAId, pVehicle->m_dwGTAId, 3000, -1);
            break;
        }
        case RPC_SET_ACTOR_ARMOUR: {
            PLAYERID    actorId;
            float       value;

            bs.Read(actorId);
            bs.Read(value);

            CActorPool::SetArmour(actorId, value);
            break;
        }
        case RPC_SET_ACTOR_NAME: {
            PLAYERID    actorId;
            uint8_t     len;

            bs.Read(actorId);
            bs.Read(len);
            char nick[len + 1];
            bs.Read(&nick[0], len);

            nick[len] = '\0';

            CActorPool::SetName(actorId, nick);
            break;
        }
        case RPC_ACTOR_SHOOT_TO: {
            PLAYERID    actorId;
            CVector     pos;
            uint32      time;

            bs.Read(actorId);
            bs.Read(pos.x);
            bs.Read(pos.y);
            bs.Read(pos.z);
            bs.Read(time);

            auto pActor = CActorPool::GetAt(actorId);

            if(pActor && pActor->m_pPed) {
                ScriptCommand(&TASK_SHOOT_AT_COORD, pActor->m_dwGTAId, pos.x, pos.y, pos.z, time);
            }
            break;
        }
        case RPC_SET_ACTOR_POS_FINDZ: {
            PLAYERID actorId;
            CVector pos;

            bs.Read(actorId);
            bs.Read(pos.x);
            bs.Read(pos.y);
            bs.Read(pos.z);

            auto pActor = CActorPool::GetAt(actorId);

            if(pActor && pActor->m_pPed) {
                pos.z = CGame::FindGroundZForCoord(pos.x, pos.y, pos.z);
                pos.z += 1.f;

                pActor->m_pPed->Teleport(pos, false);
            }
            break;
        }
        case RPC_ACTOR_GOTO: {
            PLAYERID actorId;
            float x, y, z, a;

            bs.Read(actorId);
            bs.Read(x);
            bs.Read(y);
            bs.Read(z);
            bs.Read(a);

            auto pActor = CActorPool::GetAt(actorId);

            if(pActor && pActor->m_pPed)
                ScriptCommand(&TASK_CHAR_SLIDE_TO_COORD, pActor->m_dwGTAId, x, y, z, a, 10.f);
            break;
        }
        case RPC_GIVE_ACTOR_WEAPON: {
            CNetGame::GiveActorWeapon(p);
            break;
        }
        case PACKET_LINE_OF_SIGHT: {
            uint16_t id;
            bs.Read(id);

            CVector pos;
            bs.Read(pos);

            CVector target;
            bs.Read(target);

            CColPoint colPoint;
            CEntity* hitEntity;

            RakNet::BitStream sendData;
            sendData.Write((uint8)  ID_CUSTOM_RPC);
            sendData.Write((uint8)  PACKET_LINE_OF_SIGHT);
            sendData.Write((uint16) id);

            if(!CWorld::ProcessLineOfSight(&pos, &target, &colPoint, &hitEntity, true, true, true, true, true, false, false, true)) {
                sendData.Write((uint8)  0);
            }
            else {
                sendData.Write((uint8)  1);
                sendData.Write(colPoint.m_vecPoint);
                sendData.Write((uint8) hitEntity->m_nType);
            }
            pNetGame->GetRakClient()->Send(&sendData, HIGH_PRIORITY, RELIABLE, 0);
            break;
        }
        case RPC_ACTOR_ATTACK: {
            CNetGame::ActorAttack(p);
            break;
        }
        case RPC_PLAYER_TAG: {
            CNetGame::SetPlayerTag(p);
            break;
        }
        case RPC_SET_MONEY: {
            uint32_t money;
            bs.Read(money);

            CHUD::iLocalMoney = money;
            CHUD::UpdateMoney();
            break;
        }
        case RPC_ADMIN_RECON: {
            packetAdminRecon(p);
            break;
        }
        case RPC_SPAWN_AT: {
            CVector pos;
            float rotation;

            bs.Read(pos.x);
            bs.Read(pos.y);
            bs.Read(pos.z);
            bs.Read(rotation);

            CLocalPlayer::Spawn(pos, rotation);
            break;
        }
        case RPC_SEND_BUFFER: {
            std::string text;
            bs.Read(text);
            g_pJavaWrapper->SendBuffer( cp1251_to_utf8(text) );
            break;
        }
        case RPC_SHOW_TARGET_LABEL: {
            uint8_t value;
            char str[256];
            uint8_t len;
            bs.Read(len);
            bs.Read(&str[0], len);
            bs.Read(value);

            char text[256];
            cp1251_to_utf8(text, str, len);
            CHUD::showUpdateTargetNotify(value, (char *) text);

            break;
        }
        case RPC_PRE_DEATH: {
            packetPreDeath(p);
            break;
        }
        case RPC_CHECK_CLIENT: {
            uint16_t recievKey_len;
            bs.Read(recievKey_len);
            char recievKey[recievKey_len + 1];
            bs.Read(recievKey, recievKey_len);

            recievKey[recievKey_len] = '\0';

            CRYPTEDSTRING(preKey, "Yhjkkkkqur");
            CRYPTEDSTRING(postKey, "1147Yorag");

            char key_with_salt[999];
            strcpy(key_with_salt, preKey.decode().c_str());
            strcpy(key_with_salt, recievKey);
            strcat(key_with_salt, postKey.decode().c_str());

            SendCheckClientPacket(sha256(key_with_salt));
            break;

        }
        case RPC_CUSTOM_SET_LEVEL: {
            uint8_t current;
            uint8_t max;
            uint8_t level;
            bs.Read(current);
            bs.Read(max);
            bs.Read(level);

            CHUD::updateLevelInfo(level, current, max);
            break;
        }
        case RPC_TOGGLE_GPS_INFO: {
            uint8_t value;
            bs.Read(value);
            if (value == 1) {
                CHUD::toggleGps(true);
            } else if (value == 0) {
                CHUD::toggleGps(false);
            }
            break;
        }
        case RPC_TOGGLE_GREENZONE: {
            uint8_t value;
            bs.Read(value);

            if (value == 1) {
                CHUD::toggleGreenZone(true);
                m_GreenZoneState = true;
            } else if (value == 0) {
                CHUD::toggleGreenZone(false);
                m_GreenZoneState = false;
            }
            break;
        }
        case RPC_VIBRATE: {
            uint32_t value;
            bs.Read(value);

            g_pJavaWrapper->Vibrate(value);
            break;
        }
        case RPC_CUSTOM_VISUALS: {
            uint16_t vehId;
            bs.Read(vehId);

            auto pVeh = CVehiclePool::GetAt(vehId);
            if (!pVeh) {
                return;
            }

            bs.Read(pVeh->lightColor.r);
            bs.Read(pVeh->lightColor.g);
            bs.Read(pVeh->lightColor.b);

            float fWheelWidth;
            bs.Read(fWheelWidth);
            pVeh->SetWheelWidth(fWheelWidth);

            float fWheelAngleFront, fWheelAngleBack;
            bs.Read(fWheelAngleFront);
            bs.Read(fWheelAngleBack);

            pVeh->SetWheelAngle(true, fWheelAngleFront);
            pVeh->SetWheelAngle(false, fWheelAngleBack);

            float wheelOffsetX, wheelOffsetY;
            bs.Read(wheelOffsetX);
            bs.Read(wheelOffsetY);

            pVeh->SetWheelOffset(true, wheelOffsetX);
            pVeh->SetWheelOffset(false, wheelOffsetY);

            bs.Read(pVeh->mainColor.r);
            bs.Read(pVeh->mainColor.g);
            bs.Read(pVeh->mainColor.b);

            bs.Read(pVeh->secondColor.r);
            bs.Read(pVeh->secondColor.g);
            bs.Read(pVeh->secondColor.b);

            bs.Read(pVeh->wheelColor.r);
            bs.Read(pVeh->wheelColor.g);
            bs.Read(pVeh->wheelColor.b);

            bs.Read(pVeh->tonerColor.r);
            bs.Read(pVeh->tonerColor.g);
            bs.Read(pVeh->tonerColor.b);
            bs.Read(pVeh->tonerColor.a);

            int16_t vinyl;
            bs.Read(vinyl);
            pVeh->ChangeVinylTo(vinyl);

            uint8_t bPlateType;
            std::string szText;
            std::string szRegion;

            bs.Read(bPlateType);

            uint8_t strob;
            bs.Read(strob);

            pVeh->SetStrob(static_cast<eStobsStatus>(strob));

            bs.ReadStr8(szText);
            bs.ReadStr8(szRegion);
            pVeh->setPlate(static_cast<ePlateType>(bPlateType), szText, szRegion);

            bs.Read(pVeh->neon.neonColor.r);
            bs.Read(pVeh->neon.neonColor.g);
            bs.Read(pVeh->neon.neonColor.b);

            bs.Read(pVeh->neon.neonType);
            break;
        }
        case RPC_CUSTOM_HANDLING: {
            VEHICLEID vehId;
            bs.Read(vehId);

            auto pVeh = CVehiclePool::GetAt(vehId);
            if (!pVeh){
                return;
            }

            pVeh->m_msLastSyncHandling.clear();

            for (uint8_t i = 0; i < E_HANDLING_PARAMS::hpCount; i++) {
                uint8_t id;
                float fvalue;
                bs.Read(id);
                bs.Read(fvalue);
                pVeh->m_msLastSyncHandling.emplace_back(id, fvalue);
            }

            pVeh->SetHandlingData();
            break;
        }
        case RPC_STREAM_CREATE: {
            std::string link;
            uint16_t id, interior;
            CVector pos;
            float fDistance;
            uint8 needRepeat;
            uint32 creationTime;

            bs.Read(id);
            bs.Read(pos.x);
            bs.Read(pos.y);
            bs.Read(pos.z);
            bs.Read(fDistance);
            bs.Read(interior);
            bs.Read(link);
            bs.Read(needRepeat);
            bs.Read(creationTime);

            CAudioStreamPool::AddStream(id, &pos, interior, fDistance, cp1251_to_utf8(link).c_str(), needRepeat, creationTime);
            break;
        }
        case RPC_PLAY_STREAM_CORD: {
            std::string link;
            uint8_t repeat;
            CVector pos;
            float dist;

            bs.Read(link);

            bs.Read(pos.x);
            bs.Read(pos.y);
            bs.Read(pos.z);
            bs.Read(dist);
            bs.Read(repeat);

            auto localDist = CGame::FindPlayerPed()->m_pPed->GetDistanceFromPoint(pos.x, pos.y, pos.z);

            if(localDist < dist) {
                CAudioStreamPool::PlayIndividualStreamWithPos(link, &pos, dist, repeat);
            }
            break;
        }
        case RPC_STREAM_INDIVIDUAL: {
            std::string link;
            uint8_t repeat;

            bs.Read(link);
            bs.Read(repeat);

            if(link.size() <= 3) {
                CAudioStreamPool::StopIndividualStream();
                return;
            }

            CAudioStreamPool::PlayIndividualStream(link, repeat);
            break;
        }

        case RPC_STREAM_DESTROY: {
            uint32_t id;
            bs.Read(id);
            CAudioStreamPool::DeleteStreamByID(id);
            break;
        }

        case RPC_STREAM_VOLUME:
        {
            uint16_t id;
            float fVolume;
            bs.Read(id);
            bs.Read(fVolume);
            CAudioStreamPool::SetVolume(id, fVolume);
            break;
        }

        case RPC_STREAM_ATTACH: {
            uint16  streamId;
            uint8   type;
            uint16  toId;

            bs.Read(streamId);
            bs.Read(type);
            bs.Read(toId);

            CAudioStreamPool::AttachTo(streamId, static_cast<eSoundAttachedTo>(type), toId);
            break;
        }

        case RPC_OPEN_LINK: {
            std::string url;
            bs.Read(url);

            g_pJavaWrapper->OpenUrl( cp1251_to_utf8(url) );
            break;
        }
    }
}

void CNetGame::Reset() {
    Log("CNetGame::Reset()");

    CPlayerPool::Free();

    CLocalPlayer::ResetAllSyncAttributes();
    CLocalPlayer::ToggleSpectating(false);

    CVehiclePool::Free();
    CObjectPool::Free();
    CPickupPool::Free();
    CGangZonePool::Free();
    CText3DLabelsPool::Free();
    CActorPool::Free();
    CAudioStreamPool::Free();
    g_pJavaWrapper->ClearScreen();
    Voice::Network::OnRaknetDisconnect();

    CPedSamp *pPlayerPed = CLocalPlayer::GetPlayerPed();
    if (pPlayerPed) {
        pPlayerPed->m_pPed->SetInterior(0, true);
        pPlayerPed->SetArmour(0.0f);
    }

    CHUD::iLocalMoney = 0;
    GameResetRadarColors();
}

void CNetGame::SendCheckClientPacket(std::string password) {
    uint8_t packet = ID_CUSTOM_RPC;
    uint8_t RPC = RPC_CHECK_CLIENT;

    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write((uint16_t) password.size());
    bsSend.Write(password.c_str(), password.size());
    GetRakClient()->Send(&bsSend, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
}

void CNetGame::SendSpeedTurnPacket(uint8_t turnId, uint8_t state) {
    uint8_t packet = ID_CUSTOM_RPC;
    uint8_t RPC = 47;
    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write(turnId);
    bsSend.Write(state);
    GetRakClient()->Send(&bsSend, SYSTEM_PRIORITY, UNRELIABLE_SEQUENCED, 0);
}

void CNetGame::SendCustomPacket(uint8_t packet, uint8_t RPC, uint8_t Quantity) {
    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write(Quantity);
    GetRakClient()->Send(&bsSend, HIGH_PRIORITY, RELIABLE, 0);
}

void CNetGame::SendRegisterSkinPacket(uint32_t skinId) {
    uint8_t packet = PACKET_AUTH;
    uint8_t RPC = 3;
    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write(skinId);
    GetRakClient()->Send(&bsSend, SYSTEM_PRIORITY, RELIABLE, 0);
}

void CNetGame::SendCustomPacketFuelData(uint8_t packet, uint8_t RPC, uint8_t fueltype, uint32_t fuel) {
    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write(fueltype);
    bsSend.Write(fuel);
    GetRakClient()->Send(&bsSend, HIGH_PRIORITY, RELIABLE, 0);
}

void CNetGame::SendRegisterPacket(char *password, char *mail, uint8_t sex, uint8_t skin) {
    uint8_t packet = PACKET_AUTH;
    uint8_t RPC = RPC_TOGGLE_REGISTER;
    uint8_t bytePasswordLen = strlen(password);
    uint8_t byteMailLen = strlen(mail);
    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write(bytePasswordLen);
    bsSend.Write(byteMailLen);
    bsSend.Write(password, bytePasswordLen);
    bsSend.Write(mail, byteMailLen);
    bsSend.Write(sex);
    bsSend.Write(skin);
    GetRakClient()->Send(&bsSend, HIGH_PRIORITY, RELIABLE, 0);
}

void CNetGame::SendChatMessage(const char *szMsg) {
    if (GetGameState() != eNetworkState::CONNECTED) return;

    RakNet::BitStream bsSend;
    uint8_t byteTextLen = strlen(szMsg);

    bsSend.Write(byteTextLen);
    bsSend.Write(szMsg, byteTextLen);

    m_pRakClient->RPC(&RPC_Chat, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL);
}

void CNetGame::SendChatCommand(const char *szCommand) {
    if (GetGameState() != eNetworkState::CONNECTED) return;

    RakNet::BitStream bsParams;
    int iStrlen = strlen(szCommand);

    bsParams.Write(iStrlen);
    bsParams.Write(szCommand, iStrlen);
    m_pRakClient->RPC(&RPC_ServerCommand, &bsParams, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL);
}

void CNetGame::SetMapIcon(uint8_t byteIndex, float fX, float fY, float fZ, uint8_t byteIcon, int iColor, int style) {
    if (byteIndex >= 100) return;
    if (m_dwMapIcons[byteIndex]) DisableMapIcon(byteIndex);

    m_dwMapIcons[byteIndex] = CGame::CreateRadarMarkerIcon(byteIcon, fX, fY, fZ, iColor, style);
}

void CNetGame::DisableMapIcon(uint8_t byteIndex) {
    if (byteIndex >= 100) return;
    ScriptCommand(&disable_marker, m_dwMapIcons[byteIndex]);
    m_dwMapIcons[byteIndex] = 0;
}

void CNetGame::UpdatePlayerScoresAndPings() {
    static uint32_t dwLastUpdateTick = 0;

    if ((GetTickCount() - dwLastUpdateTick) >= 3000) {
        dwLastUpdateTick = GetTickCount();
        RakNet::BitStream bsParams;
        m_pRakClient->RPC(&RPC_UpdateScoresPingsIPs, &bsParams, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, NULL);
    }
}

const char* findKey(const char* sendValue);

void CNetGame::packetAuthKey(Packet *pkt) {
    RakNet::BitStream bsAuth((unsigned char *) pkt->data, pkt->length, false);

    uint8_t byteAuthLen;
    char szAuth[260];

    bsAuth.IgnoreBits(8);
    bsAuth.Read(byteAuthLen);
    bsAuth.Read(szAuth, byteAuthLen);
    szAuth[byteAuthLen] = '\0';

    auto szAuthKey = findKey(szAuth);

    RakNet::BitStream bsKey;
    uint8_t byteAuthKeyLen = (uint8_t) strlen(szAuthKey);

    bsKey.Write((uint8_t) ID_AUTH_KEY);
    bsKey.Write((uint8_t) byteAuthKeyLen);
    bsKey.Write(szAuthKey, byteAuthKeyLen);
    m_pRakClient->Send(&bsKey, SYSTEM_PRIORITY, RELIABLE, 0);
}

void CNetGame::Packet_DisconnectionNotification(Packet *pkt) {
    Log("Packet_DisconnectionNotification");
    CChatWindow::AddMessage(CLocalisation::GetMessage(E_MSG::SERVER_CLOSED_CONNECTION));
    m_pRakClient->Disconnect(2000);
}

void CNetGame::Packet_ConnectionLost(Packet *pkt) {
    if (m_pRakClient)
        m_pRakClient->Disconnect(0);

    CChatWindow::AddMessage(CLocalisation::GetMessage(E_MSG::CONNECTION_LOST));
    SetGameState(eNetworkState::WAIT_CONNECT);
}

#include <sstream>

void CNetGame::Packet_ConnectionSucceeded(Packet *pkt) {
    CChatWindow::AddMessage(CLocalisation::GetMessage(E_MSG::CONNECTED));
    SetGameState(eNetworkState::AWAIT_JOIN);

    RakNet::BitStream bsSuccAuth((unsigned char *) pkt->data, pkt->length, false);
    unsigned int uiChallenge;

    uint16_t playerid;
    bsSuccAuth.IgnoreBits(8);
    bsSuccAuth.IgnoreBits(32);
    bsSuccAuth.IgnoreBits(16);
    bsSuccAuth.Read(playerid);
    bsSuccAuth.Read(uiChallenge);
    char ip[0x7F];
    strncpy(ip, m_szHostOrIp, sizeof(ip));

    std::vector<std::string> strings;
    std::istringstream f((const char *) &ip[0]);
    std::string s;
    int sum = 0;
    while (getline(f, s, '.')) {
        sum += std::atoi(s.c_str());
    }

    CPlayerPool::SetLocalPlayerID(playerid);

    int iVersion = NETGAME_VERSION;
    char byteMod = 0x01;
    unsigned int uiClientChallengeResponse = uiChallenge ^ iVersion;

    char byteAuthBSLen = (char) strlen(AUTH_BS);
    char byteNameLen = (char) strlen(CPlayerPool::GetLocalPlayerName());
    char byteClientverLen = (char) strlen(SAMP_VERSION);

    RakNet::BitStream bsSend;
    bsSend.Write(iVersion);
    bsSend.Write(byteMod);
    bsSend.Write(byteNameLen);
    bsSend.Write(CPlayerPool::GetLocalPlayerName(), byteNameLen);
    bsSend.Write(uiClientChallengeResponse);
    bsSend.Write(byteAuthBSLen);
    bsSend.Write(AUTH_BS, byteAuthBSLen);
    bsSend.Write(byteClientverLen);
    bsSend.Write(SAMP_VERSION, byteClientverLen);

    Voice::Network::OnRaknetRpc(RPC_ClientJoin, bsSend);

    m_pRakClient->RPC(&RPC_ClientJoin, &bsSend, HIGH_PRIORITY, RELIABLE, 0, false, UNASSIGNED_NETWORK_ID, nullptr);
    Log("Packet_ConnectionSucceeded");
}

void CNetGame::Packet_PlayerSync(Packet *pkt) {
    CRemotePlayer *pPlayer;
    RakNet::BitStream bsPlayerSync((unsigned char *) pkt->data, pkt->length, false);

    static ONFOOT_SYNC_DATA ofSync;
    memset(&ofSync, 0, sizeof(ONFOOT_SYNC_DATA));

    uint8_t bytePacketID = 0;
    PLAYERID playerId;

    bool bHasLR, bHasUD;
    bool bHasVehicleSurfingInfo;

    bsPlayerSync.Read(bytePacketID);
    bsPlayerSync.Read(playerId);

    bsPlayerSync.Read(bHasLR);
    if (bHasLR) bsPlayerSync.Read(ofSync.lrAnalog);

    bsPlayerSync.Read(bHasUD);
    if (bHasUD) bsPlayerSync.Read(ofSync.udAnalog);

    bsPlayerSync.Read(ofSync.wKeys);

    bsPlayerSync.Read((char *) &ofSync.vecPos, sizeof(CVector));

    float tw, tx, ty, tz;
    bsPlayerSync.ReadNormQuat(tw, tx, ty, tz);
    ofSync.quat.w = tw;
    ofSync.quat.x = tx;
    ofSync.quat.y = ty;
    ofSync.quat.z = tz;

    uint8_t byteHealthArmour;
    uint8_t byteArmTemp = 0, byteHlTemp = 0;

    bsPlayerSync.Read(byteHealthArmour);
    byteArmTemp = (byteHealthArmour & 0x0F);
    byteHlTemp = (byteHealthArmour >> 4);

    if (byteArmTemp == 0xF) ofSync.byteArmour = 100;
    else if (byteArmTemp == 0) ofSync.byteArmour = 0;
    else ofSync.byteArmour = byteArmTemp * 7;

    if (byteHlTemp == 0xF) ofSync.byteHealth = 100;
    else if (byteHlTemp == 0) ofSync.byteHealth = 0;
    else ofSync.byteHealth = byteHlTemp * 7;

    bsPlayerSync.Read(ofSync.byteCurrentWeapon);
    bsPlayerSync.Read(ofSync.byteSpecialAction);

    bsPlayerSync.ReadVector(tx, ty, tz);
    ofSync.vecMoveSpeed.x = tx;
    ofSync.vecMoveSpeed.y = ty;
    ofSync.vecMoveSpeed.z = tz;

    bsPlayerSync.Read(bHasVehicleSurfingInfo);
    if (bHasVehicleSurfingInfo) {
        bsPlayerSync.Read(ofSync.wSurfInfo);
        bsPlayerSync.Read(ofSync.vecSurfOffsets.x);
        bsPlayerSync.Read(ofSync.vecSurfOffsets.y);
        bsPlayerSync.Read(ofSync.vecSurfOffsets.z);
    } else
        ofSync.wSurfInfo = INVALID_VEHICLE_ID;

    pPlayer = CPlayerPool::GetSpawnedPlayer(playerId);
    if (pPlayer)
        pPlayer->StoreOnFootFullSyncData(&ofSync);
}

void CNetGame::Packet_VehicleSync(Packet *pkt) {
    RakNet::BitStream bsSync((unsigned char *) pkt->data, pkt->length, false);

    uint8_t bytePacketID = 0;
    PLAYERID playerId;

    static INCAR_SYNC_DATA icSync;
    memset(&icSync, 0, sizeof(INCAR_SYNC_DATA));

    bsSync.Read(bytePacketID);
    bsSync.Read(playerId);
    bsSync.Read(icSync.VehicleID);

    bsSync.Read(icSync.lrAnalog);
    bsSync.Read(icSync.udAnalog);
    bsSync.Read(icSync.wKeys);

    bsSync.ReadNormQuat(
            icSync.quat.w,
            icSync.quat.x,
            icSync.quat.y,
            icSync.quat.z);

    bsSync.Read((char *) &icSync.vecPos, sizeof(CVector));

    bsSync.ReadVector(
            icSync.vecMoveSpeed.x,
            icSync.vecMoveSpeed.y,
            icSync.vecMoveSpeed.z);

    uint16_t wTempVehicleHealth;
    bsSync.Read(wTempVehicleHealth);
    icSync.fCarHealth = (float) wTempVehicleHealth;

    uint8_t byteHealthArmour;
    uint8_t byteArmTemp = 0, byteHlTemp = 0;

    bsSync.Read(byteHealthArmour);
    byteArmTemp = (byteHealthArmour & 0x0F);
    byteHlTemp = (byteHealthArmour >> 4);

    if (byteArmTemp == 0xF) icSync.bytePlayerArmour = 100;
    else if (byteArmTemp == 0) icSync.bytePlayerArmour = 0;
    else icSync.bytePlayerArmour = byteArmTemp * 7;

    if (byteHlTemp == 0xF) icSync.bytePlayerHealth = 100;
    else if (byteHlTemp == 0) icSync.bytePlayerHealth = 0;
    else icSync.bytePlayerHealth = byteHlTemp * 7;

    uint8_t byteTempWeapon;
    bsSync.Read(byteTempWeapon);
    icSync.byteCurrentWeapon ^= (byteTempWeapon ^ icSync.byteCurrentWeapon) & 0x3F;

    bool bCheck;

    bsSync.Read(bCheck);
    if (bCheck) icSync.byteSirenOn = 1;
    bsSync.Read(bCheck);
    if (bCheck) icSync.byteLandingGearState = 1;

    bsSync.Read(bCheck);
    if (bCheck) bsSync.Read(icSync.HydraThrustAngle);

    bsSync.Read(bCheck);
    if (bCheck) bsSync.Read(icSync.TrailerID);

    auto pPlayer = CPlayerPool::GetSpawnedPlayer(playerId);
    if (pPlayer) {
        pPlayer->StoreInCarFullSyncData(&icSync);
    }
}

void CNetGame::Packet_PassengerSync(Packet *pkt) {
    uint8_t bytePacketID;
    PLAYERID playerId;

    static PASSENGER_SYNC_DATA psSync;

    RakNet::BitStream bsPassengerSync((unsigned char *) pkt->data, pkt->length, false);
    bsPassengerSync.Read(bytePacketID);
    bsPassengerSync.Read(playerId);
    bsPassengerSync.Read((char *) &psSync, sizeof(PASSENGER_SYNC_DATA));

    auto pPlayer = CPlayerPool::GetSpawnedPlayer(playerId);
    if (pPlayer)
        pPlayer->StorePassengerFullSyncData(&psSync);
}

void CNetGame::Packet_MarkersSync(Packet *pkt) {
    CRemotePlayer *pPlayer;
    int iNumberOfPlayers = 0;
    PLAYERID playerId;
    short sPos[3];
    bool bIsPlayerActive;
    uint8_t unk0 = 0;

    if (GetGameState() != eNetworkState::CONNECTED) return;

    RakNet::BitStream bsMarkersSync((unsigned char *) pkt->data, pkt->length, false);
    bsMarkersSync.Read(unk0);
    bsMarkersSync.Read(iNumberOfPlayers);

    if (iNumberOfPlayers) {
        for (int i = 0; i < iNumberOfPlayers; i++) {
            bsMarkersSync.Read(playerId);
            bsMarkersSync.ReadCompressed(bIsPlayerActive);

            if (bIsPlayerActive) {
                bsMarkersSync.Read(sPos[0]);
                bsMarkersSync.Read(sPos[1]);
                bsMarkersSync.Read(sPos[2]);
            }

            if (playerId < MAX_PLAYERS && CPlayerPool::GetAt(playerId)) {
                pPlayer = CPlayerPool::GetAt(playerId);
                if (pPlayer) {
                    if (bIsPlayerActive)
                        pPlayer->ShowGlobalMarker(sPos[0], sPos[1], sPos[2]);
                    else
                        pPlayer->HideGlobalMarker();
                }
            }
        }
    }
}

void CNetGame::Packet_BulletSync(Packet *p) {
    uint8_t bytePacketID;
    uint16_t PlayerID;
    BULLET_SYNC btSync;
    RakNet::BitStream bsBulletSync((unsigned char *) p->data, p->length, false);

    bsBulletSync.Read(bytePacketID);
    bsBulletSync.Read(PlayerID);
    bsBulletSync.Read((char *) &btSync, sizeof(BULLET_SYNC));

    CRemotePlayer *pRemotePlayer = CPlayerPool::GetSpawnedPlayer(PlayerID);
    if (pRemotePlayer) {
        pRemotePlayer->StoreBulletSyncData(&btSync);
    }
}

void CNetGame::Packet_AimSync(Packet *p) {
    CRemotePlayer *pPlayer;
    RakNet::BitStream bsAimSync((unsigned char *) p->data, p->length, false);
    static AIM_SYNC_DATA aimSync;

    uint8_t bytePacketID = 0;
    uint16_t bytePlayerID = 0;

    bsAimSync.Read(bytePacketID);
    bsAimSync.Read(bytePlayerID);
    bsAimSync.Read((char *) &aimSync, sizeof(AIM_SYNC_DATA));

    pPlayer = CPlayerPool::GetSpawnedPlayer(bytePlayerID);

    if (pPlayer) {
        pPlayer->UpdateAimFromSyncData(&aimSync);
    }
}

void CNetGame::ActorTakeDamage(PLAYERID actorId, eWeaponType weaponId, float ammount, int bodyPart) {
    RakNet::BitStream bs;

    bs.Write((bool)  true);
    bs.Write((uint16_t) actorId);
    bs.Write((float)    ammount);
    bs.Write((uint32_t) weaponId);
    bs.Write((uint32_t) bodyPart);

    pNetGame->GetRakClient()->RPC(&RPC_GiveActorDamage, &bs, HIGH_PRIORITY, RELIABLE_SEQUENCED, 0, false, UNASSIGNED_NETWORK_ID, 0);
}

void CNetGame::SendExtinguishPointWithWater(const CVector* point, const float* fRadius, const float* fWaterStrength) {
    static auto lastSend = GetTickCount();

    if( (GetTickCount() - lastSend) < 1000 )
        return;

    lastSend = GetTickCount();

    RakNet::BitStream bs;
    bs.Write((uint8_t)  ID_CUSTOM_RPC);
    bs.Write((uint8_t)  RPC_EXTINGUISH_POINT_WITH_WATER);
    bs.Write((float)    point->x);
    bs.Write((float)    point->y);
    bs.Write((float)    point->z);
    bs.Write((float)    *fRadius);
    bs.Write((float)    *fWaterStrength);

    pNetGame->GetRakClient()->Send(&bs, HIGH_PRIORITY, RELIABLE, 0);
}

void CNetGame::sendTakeDamage(PLAYERID attacker, eWeaponType weaponId, float ammount, int bodyPart) {
    RakNet::BitStream bs;

    bs.Write((uint8_t) ID_CUSTOM_RPC);
    bs.Write((uint8_t) RPC_TAKE_DAMAGE);
    bs.Write((uint16_t) attacker);
    bs.Write((float) ammount);
    bs.Write((uint16_t) weaponId);
    bs.Write((uint16_t) bodyPart);

    pNetGame->GetRakClient()->Send(&bs, HIGH_PRIORITY, RELIABLE, 0);

    CLocalPlayer::lastDamageId = attacker;
    CLocalPlayer::lastDamageWeap = weaponId;
}

void CNetGame::sendGiveDamage(PLAYERID taker, int weaponId, float ammount, int bodyPart) {
    RakNet::BitStream bs;

    bs.Write((uint8_t) ID_CUSTOM_RPC);
    bs.Write((uint8_t) RPC_GIVE_DAMAGE);
    bs.Write((uint16_t) taker);
    bs.Write((float) ammount);
    bs.Write((uint16_t) weaponId);
    bs.Write((uint16_t) bodyPart);

    pNetGame->GetRakClient()->Send(&bs, HIGH_PRIORITY, RELIABLE, 0);
}

void CNetGame::GiveActorWeapon(Packet *p) {
    RakNet::BitStream bs((unsigned char*)p->data, p->length, false);

    bs.IgnoreBits(40);

    PLAYERID    actorId;
    uint32      weaponId;

    bs.Read(actorId);
    bs.Read(weaponId);

    auto pActor = CActorPool::GetAt(actorId);
    if(pActor && pActor->m_pPed)
        pActor->m_pPed->GiveWeapon(weaponId, 9999);
}

void CNetGame::ActorAttack(Packet *p) {
    RakNet::BitStream bs((unsigned char*)p->data, p->length, false);

    bs.IgnoreBits(40);

    PLAYERID    actorId;
    PLAYERID    attackId;

    bs.Read(actorId);
    bs.Read(attackId);

    auto pActor = CActorPool::GetAt(actorId);
    auto pTargetActor = CPlayerPool::GetSpawnedPlayer(attackId);
    if(pActor && pActor->m_pPed && pTargetActor)
        ScriptCommand(&TASK_SHOOT_AT_CHAR, pActor->m_dwGTAId, pTargetActor->GetPlayerPed()->m_pPed, 99999);
}

void CNetGame::SetPlayerTag(Packet *p) {
    RakNet::BitStream bs((unsigned char*)p->data, p->length, false);

    bs.IgnoreBits(40);

    PLAYERID    playerId;
    uint8       tagType;

    bs.Read(playerId);
    bs.Read(tagType);

    auto pPlayer = CPlayerPool::GetAt(playerId);
    if(pPlayer)
        pPlayer->m_nTag = (CRemotePlayer::eTags)tagType;
}

void CNetGame::SetGameState(eNetworkState newState) {
    Log("SetGameState, old = %d, new = %d", m_iGameState, newState);
    if(m_iGameState == eNetworkState::CONNECTED && newState != eNetworkState::CONNECTED)
        Reset();

    m_iGameState = newState;
}
