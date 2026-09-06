//
// Created on 28.10.2022.
//

#include "HUD.h"
#include <jni.h>
#include <vector>

#include "main.h"

#include "../game/game.h"
#include "net/netgame.h"
#include "gui/gui.h"
#include "keyboard.h"
#include "CSettings.h"
#include "chatwindow.h"
#include "Speedometr.h"
#include "util/patch.h"
#include "../game/Entity/Ped/Ped.h"
#include "game/Widgets/TouchInterface.h"

extern CJavaWrapper *g_pJavaWrapper;
extern CGUI* pGUI;

bool        CHUD::bIsOnlyVisualOff = true;
bool        CHUD::bIsShow = false;
bool        CHUD::bIsShowEnterExitButt = false;
bool        CHUD::bIsShowLockButt = false;
bool        CHUD::bIsShowChat = true;
int         CHUD::iLocalMoney = 0;
int         CHUD::iWantedLevel = 0;
bool        CHUD::bIsShowMafiaWar = false;
float       CHUD::fLastGiveDamage = 0.0f;
bool        CHUD::bIsTouchCameraButt = false;
bool        CHUD::bIsCamEditGui = false;
int         CHUD::iSatiety = 0;
PLAYERID    CHUD::lastGiveDamagePlayerId = INVALID_PLAYER_ID;

CVector2D   CHUD::radarBgPos1;
CVector2D   CHUD::radarBgPos2;

jobject CHUD::thiz = nullptr;

jmethodID jUpdateHudInfo;

void CHUD::ChangeChatTextSize(int size) {
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if(!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "ChangeChatFontSize", "(I)V");
    if (method) {
        env->CallVoidMethod(thiz, method, size);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_HudManager_HudInit(JNIEnv *env, jobject thiz) {
    if (!env || !thiz) return;

    if (CHUD::thiz) {
        env->DeleteGlobalRef(CHUD::thiz);
    }

    CHUD::thiz = env->NewGlobalRef(thiz);
    jclass clazz = env->GetObjectClass(thiz);
    if (clazz) {
        jUpdateHudInfo = env->GetMethodID(clazz, "updateAmmo", "(III)V");
    }

    CHUD::ToggleProgressTexts(CSettings::m_Settings.iHPArmourText);
    CHUD::ChangeChatTextSize(CSettings::m_Settings.iChatFontSize);
}

void CHUD::toggleAll(bool toggle, bool chat, bool onlyVisual)
{
    if(toggle == bIsShow) return;

    bIsShow = toggle;
    bIsOnlyVisualOff = onlyVisual;

    CGame::ToggleHUDElement(HUD_ELEMENT_FPS, toggle);

    JNIEnv *env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "toggleAll", "(ZZ)V");
    if (method) {
        env->CallVoidMethod(thiz, method, toggle, chat);
    }

    if (g_libGTASA) {
        *(uint8_t*)(g_libGTASA + (VER_x32 ? 0x00819D88 : 0x9FF3A8)) = toggle ? 1 : 0;
    }
}

void CHUD::toggleLockButton(bool toggle)
{
    bIsShowLockButt = toggle;

    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID ToggleLockVehicleButton = env->GetMethodID(clazz, "toggleLockButton", "(Z)V");
    if (ToggleLockVehicleButton) {
        env->CallVoidMethod(thiz, ToggleLockVehicleButton, toggle);
    }
}

bool CHUD::NeededRenderPassengerButton() {
    CPedSamp* pPed = CGame::FindPlayerPed();
    if (!pPed || !pPed->m_pPed) return false;

    if (bIsShowEnterExitButt && !pPed->m_pPed->IsInVehicle())
        return true;

    return false;
}

void CHUD::updateAmmo()
{
}

void CHUD::updateBars() {
}

void CHUD::UpdateWanted()
{
}

void CHUD::updateLevelInfo(int level, int currentexp, int maxexp)
{
}

void CHUD::showUpdateTargetNotify(int type, char *text)
{
    if (!text) return;

    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "showUpdateTargetNotify", "(ILjava/lang/String;)V");
    if (!method) return;

    jclass strClass = env->FindClass("java/lang/String");
    if (!strClass) return;

    jmethodID ctorID = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jstring encoding = env->NewStringUTF("UTF-8");

    size_t len = strlen(text);
    jbyteArray bytes = env->NewByteArray(len);
    if (!bytes) {
        env->DeleteLocalRef(encoding);
        return;
    }

    env->SetByteArrayRegion(bytes, 0, len, (jbyte*)text);
    jstring jtext = (jstring) env->NewObject(strClass, ctorID, bytes, encoding);
    
    if (jtext) {
        env->CallVoidMethod(thiz, method, type, jtext);
        env->DeleteLocalRef(jtext);
    }

    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(encoding);
}

void CHUD::hideTargetNotify()
{
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "hideTargetNotify", "()V");
    if (method) {
        env->CallVoidMethod(thiz, method);
    }
}

void CHUD::toggleGps(bool toggle)
{
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "toggleGps", "(Z)V");
    if (method) {
        env->CallVoidMethod(thiz, method, toggle);
    }
}

void CHUD::toggleServerLogo(bool toggle)
{
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "toggleServerLogo", "(Z)V");
    if (method) {
        env->CallVoidMethod(thiz, method, toggle);
    }
}

void CHUD::toggleGreenZone(bool toggle)
{
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "toggleGreenZone", "(Z)V");
    if (method) {
        env->CallVoidMethod(thiz, method, toggle);
    }
}

void CHUD::UpdateMoney()
{
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_HudManager_ClickLockVehicleButton(JNIEnv *env, jobject thiz) {
    if (pNetGame) {
        pNetGame->SendChatCommand("/lock");
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_Speedometer_ClickSpedometr(JNIEnv *env, jobject thiz, jint turn_id,
                                                      jboolean toggle) {
    if (!pNetGame || !pNetGame->GetRakClient()) return;

    uint8_t packet = ID_CUSTOM_RPC;
    uint8_t RPC = RPC_TURN_SIGNAL;
    uint8_t button = turn_id;
    uint8_t toggle_ = toggle;

    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write(button);
    bsSend.Write(toggle_);
    pNetGame->GetRakClient()->Send(&bsSend, SYSTEM_PRIORITY, RELIABLE_SEQUENCED, 0);
}

void CNetGame::packetSalary(Packet* p)
{
    if (!p || !p->data) return;

    RakNet::BitStream bs((unsigned char*)p->data, p->length, false);
    uint8_t packetID;
    uint32_t rpcID;
    uint32_t salary;
    uint32_t lvl;
    float exp;

    bs.Read(packetID);
    bs.Read(rpcID);
    bs.Read(salary);
    bs.Read(lvl);
    bs.Read(exp);

    CHUD::UpdateSalary(salary, lvl, exp);
}

void CHUD::UpdateSalary(int salary, int lvl, float exp)
{
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID UpdateSalary = env->GetMethodID(clazz, "updateSalary", "(IIF)V");
    if (UpdateSalary) {
        env->CallVoidMethod(thiz, UpdateSalary, salary, lvl, exp);
    }
}

void CHUD::SetChatInput(const char ch[])
{
    if (!ch) return;

    size_t len = strlen(ch) * 3 + 1;
    std::vector<char> msg_utf(len);
    cp1251_to_utf8(msg_utf.data(), ch);

    JNIEnv* env = CJavaWrapper::GetEnv();
    if (!env || !thiz) return;

    jstring jch = env->NewStringUTF(msg_utf.data());
    if (!jch) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (clazz) {
        jmethodID AddToChatInput = env->GetMethodID(clazz, "AddToChatInput", "(Ljava/lang/String;)V");
        if (AddToChatInput) {
            env->CallVoidMethod(thiz, AddToChatInput, jch);
        }
    }
    env->DeleteLocalRef(jch);
}

void CHUD::AddChatMessage(const char msg[])
{
    if(!thiz || !msg) return;

    size_t len = strlen(msg) * 3 + 1;
    std::vector<char> msg_utf(len);
    cp1251_to_utf8(msg_utf.data(), msg);

    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env) return;

    jstring jmsg = env->NewStringUTF(msg_utf.data());
    if (!jmsg) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (clazz) {
        jmethodID AddChatMessage = env->GetMethodID(clazz, "AddChatMessage", "(Ljava/lang/String;)V");
        if (AddChatMessage) {
            env->CallVoidMethod(thiz, AddChatMessage, jmsg);
        }
    }
    env->DeleteLocalRef(jmsg);
}

void CHUD::addGiveDamageNotify(PLAYERID id, int weaponId, float damage, ePedPieceTypes bodypart)
{
    if(!CSettings::m_Settings.iIsEnableDamageInformer) return;

    if(damage > 200) damage = 200.0f;

    if(lastGiveDamagePlayerId == id) {
        fLastGiveDamage += damage;
    }
    else {
        lastGiveDamagePlayerId = id;
        fLastGiveDamage = damage;
    }

    JNIEnv* env = CJavaWrapper::GetEnv();
    if (!env || !thiz) return;

    jstring jnick = nullptr;

    if(CActorPool::GetAt(id))
        jnick = env->NewStringUTF( CActorPool::GetName(id) );
    else
        jnick = env->NewStringUTF( CPlayerPool::GetPlayerName(id) );

    if (!jnick) return;

    jstring jweap = env->NewStringUTF( CUtil::GetWeaponName(weaponId) );
    jstring jbodypartname = env->NewStringUTF(pedPieceNames[bodypart].c_str());

    jclass clazz = env->GetObjectClass(thiz);
    if (clazz) {
        jmethodID method = env->GetMethodID(clazz, "addGiveDamageNotify", "(Ljava/lang/String;Ljava/lang/String;FLjava/lang/String;)V");
        if (method) {
            env->CallVoidMethod(thiz, method, jnick, jweap, fLastGiveDamage, jbodypartname);
        }
    }

    env->DeleteLocalRef(jnick);
    if (jweap) env->DeleteLocalRef(jweap);
    if (jbodypartname) env->DeleteLocalRef(jbodypartname);
}

void CHUD::addTakeDamageNotify(char nick[], int weaponId, float damage)
{
    if(!CSettings::m_Settings.iIsEnableDamageInformer || !nick) return;

    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    if(damage > 200) damage = 200.0f;
    jstring jnick = env->NewStringUTF( nick );
    if (!jnick) return;

    jstring jweap = env->NewStringUTF( CUtil::GetWeaponName(weaponId) );

    jclass clazz = env->GetObjectClass(thiz);
    if (clazz) {
        jmethodID method = env->GetMethodID(clazz, "addTakeDamageNotify", "(Ljava/lang/String;Ljava/lang/String;F)V");
        if (method) {
            env->CallVoidMethod(thiz, method, jnick, jweap, damage);
        }
    }

    env->DeleteLocalRef(jnick);
    if (jweap) env->DeleteLocalRef(jweap);
}

void CHUD::ToggleProgressTexts(bool toggle)
{
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "toggleProgressTexts", "(Z)V");
    if (method) {
        env->CallVoidMethod(thiz, method, toggle);
    }
}

void CHUD::ToggleChat(bool toggle){
    bIsShowChat = toggle;

    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID ToggleChat = env->GetMethodID(clazz, "ToggleChat" , "(Z)V");
    if (ToggleChat) {
        env->CallVoidMethod(thiz, ToggleChat, toggle);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_HudManager_SetRadarBgPos(JNIEnv *env, jobject thiz, jfloat x1, jfloat y1,
                                                    jfloat x2, jfloat y2) {
    CHUD::radarBgPos1.x = x1;
    CHUD::radarBgPos1.y = y1;

    CHUD::radarBgPos2.x = x2;
    CHUD::radarBgPos2.y = y2;
}

void CHUD::ToggleChatInput(bool toggle)
{
    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !thiz) return;

    jclass clazz = env->GetObjectClass(thiz);
    if (!clazz) return;

    jmethodID ToggleChatInput = env->GetMethodID(clazz, "ToggleChatInput", "(Z)V");
    if (ToggleChatInput) {
        env->CallVoidMethod(thiz, ToggleChatInput, toggle);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_Chat_SendChatMessage(JNIEnv *env, jobject thiz, jbyteArray str) {
    if (!str) return;

    // Chặn gửi chat nếu pNetGame chưa khởi tạo hoặc nhân vật chưa spawn (tránh crash lúc đăng nhập)
    if (!pNetGame || !CLocalPlayer::GetPlayerPed()) return;

    jbyte* pMsg = env->GetByteArrayElements(str, nullptr);
    if (!pMsg) return;

    jsize length = env->GetArrayLength(str);
    if (length <= 0) {
        env->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
        return;
    }

    std::string szStr((char*)pMsg, length);

    CGame::PostToMainThread([szStr]{
        CKeyBoard::m_sInput = szStr;
        CKeyBoard::Send();
    });

    env->ReleaseByteArrayElements(str, pMsg, JNI_ABORT);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_HudManager_clickCameraMode(JNIEnv *env, jobject thiz) {
    CPedSamp *pPed = CLocalPlayer::GetPlayerPed();
    if(!pPed || !pPed->m_pPed) return;

    if(CLocalPlayer::IsSpectating())
        return;

    if(pPed->m_pPed->IsInVehicle()) {
        CHUD::bIsTouchCameraButt = true;
    }
    else {
        CFirstPersonCamera::Toggle();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_HudManager_clickMultText(JNIEnv *env, jobject thiz) {
    if (pNetGame) {
        pNetGame->SendChatCommand("/action");
    }
}

void CNetGame::packetUpdateSatiety(Packet* p)
{
    if (!p || !p->data) return;

    RakNet::BitStream bs((unsigned char*)p->data, p->length, false);
    bs.IgnoreBits(40); // skip packet and rpc id

    uint8_t value;
    bs.Read(value);

    CHUD::iSatiety = value;
    CHUD::updateBars();
}

void CNetGame::packetTorpedoButt(Packet* p)
{
    if (!p || !p->data) return;

    RakNet::BitStream bs((unsigned char*)p->data, p->length, false);
    bs.IgnoreBits(40); // skip packet and rpc id

    uint8_t value;
    bs.Read(value);

    JNIEnv* env = g_pJavaWrapper ? g_pJavaWrapper->GetEnv() : nullptr;
    if (!env || !CHUD::thiz) return;

    jclass clazz = env->GetObjectClass(CHUD::thiz);
    if (!clazz) return;

    jmethodID method = env->GetMethodID(clazz, "toggleTorpedoButt", "(Z)V");
    if (method) {
        env->CallVoidMethod(CHUD::thiz, method, value);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_HudManager_sendTorpedo(JNIEnv *env, jobject thiz) {
    if (!pNetGame || !pNetGame->GetRakClient()) return;

    uint8_t packet = ID_CUSTOM_RPC;
    uint8_t RPC = 80;

    RakNet::BitStream bsSend;
    bsSend.Write(packet);
    bsSend.Write(RPC);
    bsSend.Write(1);
    pNetGame->GetRakClient()->Send(&bsSend, SYSTEM_PRIORITY, RELIABLE_SEQUENCED, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_russia_game_gui_hud_HudManager_nativeSetRadarPos(JNIEnv *env, jobject thiz, jfloat x1,
                                                            jfloat y1, jfloat width,
                                                            jfloat height) {
    CHUD::radarPos.x = x1;
    CHUD::radarPos.y = y1;
    CHUD::radarSize.x = width;
    CHUD::radarSize.y = height;
}
