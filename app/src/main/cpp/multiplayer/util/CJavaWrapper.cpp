#include "CJavaWrapper.h"
#include "../main.h"

extern "C" JavaVM *javaVM;

#include "..//keyboard.h"
#include "..//CSettings.h"
#include "..//net/netgame.h"
#include "../game/game.h"
#include "java_systems/Tab.h"
#include "java_systems/HUD.h"
#include "../game/Entity/Ped/Ped.h"
#include "java_systems/SkinShop.h"
#include "java_systems/Registration.h"
#include "java_systems/BusStation.h"

#include "..//CDebugInfo.h"
#include "chatwindow.h"
#include "java_systems/Medic.h"
#include "java_systems/Speedometr.h"
#include "java_systems/casino/Dice.h"
#include "java_systems/AutoShop.h"
#include "java_systems/ChooseSpawn.h"
#include "java_systems/Authorization.h"
#include "java_systems/GuiWrapper.h"
#include "GuiWrapper.h"

extern CNetGame *pNetGame;

// Pointer toàn cục an toàn
CJavaWrapper *g_pJavaWrapper = nullptr;

// ============================================================================
// HELPER AN TOÀN TRÁNH CRASH JNI
// ============================================================================

// Kiểm tra và xóa ngoại lệ JNI đang treo (Tránh gây crash JVM ở các lệnh tiếp theo)
static bool CheckAndClearException(JNIEnv* env, const char* actionName) {
    if (!env) return false;
    if (env->ExceptionCheck()) {
        Log("JNI Exception trapped in [%s]", actionName ? actionName : "Unknown");
        env->ExceptionClear();
        return true;
    }
    return false;
}

// Lấy MethodID an toàn kèm kiểm tra Exception
static jmethodID SafeGetMethodID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    if (!env || !clazz || !name || !sig) return nullptr;
    
    jmethodID id = env->GetMethodID(clazz, name, sig);
    if (CheckAndClearException(env, name)) {
        Log("Failed to find JNI Method: %s with signature: %s", name, sig);
        return nullptr;
    }
    return id;
}

// ============================================================================
// CJAVAWAPPER IMPLEMENTATION
// ============================================================================

JNIEnv *CJavaWrapper::GetEnv() {
    if (!javaVM) {
        Log("GetEnv: javaVM is null");
        return nullptr;
    }

    JNIEnv *env = nullptr;
    int getEnvStat = javaVM->GetEnv((void **) &env, JNI_VERSION_1_6);

    if (getEnvStat == JNI_EDETACHED) {
        if (javaVM->AttachCurrentThread(&env, NULL) != 0) {
            Log("GetEnv: Failed to attach thread");
            return nullptr;
        }
    } else if (getEnvStat == JNI_EVERSION || getEnvStat == JNI_ERR) {
        Log("GetEnv: JNI Error status code: %d", getEnvStat);
        return nullptr;
    }

    return env;
}

typedef void* (*OSThreadFunction)(void*);
struct ThreadLaunchData {
    void* thread_struct;
    OSThreadFunction func;
    char thread_name[32];
};

void* CJavaWrapper::NVThreadSpawnProc(void* arg) {
    if (!arg) {
        Log("NVThreadSpawnProc: Critical Error - arg is null!");
        return nullptr;
    }

    std::unique_ptr<ThreadLaunchData> data(static_cast<ThreadLaunchData*>(arg));

    if (!data->func) {
        Log("NVThreadSpawnProc: Critical Error - data->func is null!");
        return nullptr;
    }

    bool attached = false;
    JNIEnv* env = nullptr;

    if (javaVM) {
        jint status = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            if (javaVM->AttachCurrentThread(&env, nullptr) == 0) {
                attached = true;
            } else {
                Log("NVThreadSpawnProc: Failed to attach thread");
            }
        }
    } else {
        Log("NVThreadSpawnProc: Warning - javaVM is null");
    }

    if (env && data->thread_name[0] != '\0') {
        jclass threadClass = env->FindClass("java/lang/Thread");
        if (threadClass) {
            jmethodID currentThread = SafeGetMethodID(env, threadClass, "currentThread", "()Ljava/lang/Thread;");
            jmethodID setName = SafeGetMethodID(env, threadClass, "setName", "(Ljava/lang/String;)V");

            if (currentThread && setName) {
                jobject threadObj = env->CallStaticObjectMethod(threadClass, currentThread);
                CheckAndClearException(env, "currentThread");

                if (threadObj) {
                    jstring nameStr = env->NewStringUTF(data->thread_name);
                    if (nameStr) {
                        env->CallVoidMethod(threadObj, setName, nameStr);
                        CheckAndClearException(env, "setName");
                        env->DeleteLocalRef(nameStr);
                    }
                    env->DeleteLocalRef(threadObj);
                }
            }
            env->DeleteLocalRef(threadClass);
        }
    }

    void* result = nullptr;
    if (data->func) {
        result = data->func(data->thread_struct);
    }

    if (attached && javaVM) {
        javaVM->DetachCurrentThread();
    }
    return result;
}

CJavaWrapper::CJavaWrapper(JNIEnv *env, jobject activity) {
    // Khởi tạo tất cả biến con trỏ về nullptr trước
    this->activity = nullptr;
    this->s_ShowClientSettings = nullptr;
    this->j_Vibrate = nullptr;
    this->s_setPauseState = nullptr;
    this->s_ExitGame = nullptr;

    if (!env || !activity) {
        Log("CJavaWrapper constructor: env or activity is null");
        return;
    }

    this->activity = env->NewGlobalRef(activity);
    if (!this->activity) {
        Log("CJavaWrapper constructor: NewGlobalRef failed");
        return;
    }

    jclass nvEventClass = env->GetObjectClass(activity);
    if (!nvEventClass) {
        Log("CJavaWrapper constructor: nvEventClass is null");
        CheckAndClearException(env, "GetObjectClass");
        return;
    }

    // Lấy MethodID an toàn, chống Crash nếu Java method không tồn tại
    s_ShowClientSettings = SafeGetMethodID(env, nvEventClass, "showClientSettings", "()V");
    j_Vibrate            = SafeGetMethodID(env, nvEventClass, "goVibrate", "(I)V");
    s_setPauseState      = SafeGetMethodID(env, nvEventClass, "setPauseState", "(Z)V");
    s_ExitGame           = SafeGetMethodID(env, nvEventClass, "exitGame", "()V");

    env->DeleteLocalRef(nvEventClass);
}

CJavaWrapper::~CJavaWrapper() {
    JNIEnv *pEnv = GetEnv();
    if (pEnv && this->activity) {
        pEnv->DeleteGlobalRef(this->activity);
        this->activity = nullptr;
    }
}

void CJavaWrapper::ShowClientSettings() {
    JNIEnv *env = GetEnv();
    if (!env || !this->activity || !s_ShowClientSettings) {
        Log("ShowClientSettings: Invalid environment, activity, or MethodID");
        return;
    }

    env->CallVoidMethod(this->activity, s_ShowClientSettings);
    CheckAndClearException(env, "ShowClientSettings");
}

void CJavaWrapper::Vibrate(int milliseconds) {
    JNIEnv *env = GetEnv();
    if (!env || !this->activity || !j_Vibrate) {
        Log("Vibrate: Invalid environment, activity, or MethodID");
        return;
    }

    env->CallVoidMethod(this->activity, this->j_Vibrate, milliseconds);
    CheckAndClearException(env, "Vibrate");
}

void CJavaWrapper::SetPauseState(bool a1) {
    JNIEnv *env = GetEnv();
    if (!env || !this->activity || !s_setPauseState) {
        Log("SetPauseState: Invalid environment, activity, or MethodID");
        return;
    }

    env->CallVoidMethod(this->activity, this->s_setPauseState, a1);
    CheckAndClearException(env, "SetPauseState");
}

void CJavaWrapper::hideLoadingScreen() {
    JNIEnv *env = GetEnv();
    if (!env || !this->activity) {
        Log("hideLoadingScreen: No env or activity null");
        return;
    }

    jclass clazz = env->GetObjectClass(this->activity);
    if (!clazz) {
        Log("hideLoadingScreen: Failed to get class from activity");
        CheckAndClearException(env, "GetObjectClass");
        return;
    }

    jmethodID method = SafeGetMethodID(env, clazz, "hideLoadingScreen", "()V");
    if (method) {
        env->CallVoidMethod(this->activity, method);
        CheckAndClearException(env, "hideLoadingScreen call");
    }

    env->DeleteLocalRef(clazz);
}

void CJavaWrapper::ExitGame() {
    JNIEnv *env = GetEnv();
    if (!env || !this->activity || !s_ExitGame) {
        Log("ExitGame: Invalid environment, activity, or MethodID");
        return;
    }

    env->CallVoidMethod(this->activity, this->s_ExitGame);
    CheckAndClearException(env, "ExitGame");
}

void CJavaWrapper::ClearScreen() {
    Log("ClearScreen");

    CSkinShop::Destroy();
    CHUD::hideTargetNotify();
    CAuthorization::Destroy();
    CChooseSpawn::Destroy();
    CRegistration::Destroy();
    CSpeedometr::Destroy();
    CAutoShop::toggle(false);
    CBusStation::Destroy();
    CHUD::toggleGps(false);
    CHUD::toggleGreenZone(false);
    CMedic::hide();
    CDice::Destroy();
}

void CJavaWrapper::SendBuffer(const std::string& text) const {
    if (text.empty()) return;

    JNIEnv *env = GetEnv();
    if (!env || !activity) return;

    jstring jstr = env->NewStringUTF(text.c_str());
    if (!jstr) {
        CheckAndClearException(env, "SendBuffer NewStringUTF");
        return;
    }

    jclass clazz = env->GetObjectClass(activity);
    if (clazz) {
        jmethodID method = SafeGetMethodID(env, clazz, "copyTextToBuffer", "(Ljava/lang/String;)V");
        if (method) {
            env->CallVoidMethod(activity, method, jstr);
            CheckAndClearException(env, "copyTextToBuffer call");
        }
        env->DeleteLocalRef(clazz);
    }
    env->DeleteLocalRef(jstr);
}

void CJavaWrapper::OpenUrl(const std::string& url) const {
    if (url.empty()) return;

    JNIEnv *env = GetEnv();
    if (!env || !activity) return;

    jstring jstr = env->NewStringUTF(url.c_str());
    if (!jstr) {
        CheckAndClearException(env, "OpenUrl NewStringUTF");
        return;
    }

    jclass clazz = env->GetObjectClass(activity);
    if (clazz) {
        jmethodID method = SafeGetMethodID(env, clazz, "openUrl", "(Ljava/lang/String;)V");
        if (method) {
            env->CallVoidMethod(activity, method, jstr);
            CheckAndClearException(env, "openUrl call");
        }
        env->DeleteLocalRef(clazz);
    }
    env->DeleteLocalRef(jstr);
}

// ============================================================================
// NATIVE JNI EXPORTS
// ============================================================================

extern "C"
{
JNIEXPORT void JNICALL
Java_com_nvidia_devtech_NvEventQueueActivity_togglePlayer(JNIEnv *pEnv, jobject thiz, jint toggle) {
    auto pPed = CLocalPlayer::GetPlayerPed();
    if (!pPed) {
        Log("togglePlayer: PlayerPed is null");
        return;
    }

    if (toggle)
        pPed->TogglePlayerControllable(false, true);
    else
        pPed->TogglePlayerControllable(true, true);
}

JNIEXPORT void JNICALL
Java_com_nvidia_devtech_NvEventQueueActivity_onSpeedTurnRightClick(JNIEnv *pEnv, jobject thiz, jint state) {
    if (pNetGame) pNetGame->SendSpeedTurnPacket(2, state);
}

JNIEXPORT void JNICALL
Java_com_nvidia_devtech_NvEventQueueActivity_onSpeedTurnCenterClick(JNIEnv *pEnv, jobject thiz, jint state) {
    if (pNetGame) pNetGame->SendSpeedTurnPacket(1, state);
}

JNIEXPORT void JNICALL
Java_com_nvidia_devtech_NvEventQueueActivity_onSpeedTurnLeftClick(JNIEnv *pEnv, jobject thiz, jint state) {
    if (pNetGame) pNetGame->SendSpeedTurnPacket(0, state);
}

JNIEXPORT void JNICALL
Java_com_nvidia_devtech_NvEventQueueActivity_onDeathInfoWait(JNIEnv *pEnv, jobject thiz) {
    if (pNetGame) pNetGame->SendCustomPacket(251, 48, 0);
}

JNIEXPORT void JNICALL
Java_com_nvidia_devtech_NvEventQueueActivity_onDeathInfoClick(JNIEnv *pEnv, jobject thiz) {
    if (pNetGame) pNetGame->SendCustomPacket(251, 48, 1);
}

JNIEXPORT void JNICALL
Java_com_nvidia_devtech_NvEventQueueActivity_onAuctionButtonClick(JNIEnv *pEnv, jobject thiz, jint btnid) {
    if (pNetGame) pNetGame->SendCustomPacket(251, 52, btnid);
}

JNIEXPORT void JNICALL
Java_com_russia_game_core_Samp_00024Companion_playUrlSound(JNIEnv *env, jobject clazz, jstring jurl) {
    if (!env || !jurl) return;

    const char *url = env->GetStringUTFChars(jurl, nullptr);
    if (!url) {
        CheckAndClearException(env, "playUrlSound GetStringUTFChars");
        return;
    }

    // Sao chép an toàn sang std::string
    std::string urlStr(url);
    env->ReleaseStringUTFChars(jurl, url);

    if (urlStr.empty()) return;

    // Chuyển string theo dạng Value (Copy) vào Lambda Thread để tránh Dangling Pointer hay Stack Overflow
    CAudioStreamPool::PostToAudioThread([urlStr]() {
        auto stream = BASS_StreamCreateURL(urlStr.c_str(), 0, BASS_STREAM_AUTOFREE | BASS_STREAM_BLOCK | BASS_STREAM_RESTRATE, nullptr, 0);
        if (stream) {
            BASS_ChannelPlay(stream, false);
        }
    });
}

JNIEXPORT void JNICALL
Java_com_russia_game_gui_Menu_nativeSendMenuButt(JNIEnv *env, jobject thiz, jint butt_id) {
    if (!pNetGame) {
        Log("nativeSendMenuButt: pNetGame is null");
        return;
    }

    switch (butt_id) {
        case 1:  pNetGame->SendChatCommand("/gps"); break;
        case 2:  pNetGame->SendChatCommand("/mm"); break;
        case 3:  pNetGame->SendChatCommand("/inv"); break;
        case 4:  pNetGame->SendChatCommand("/anim"); break;
        case 5:  pNetGame->SendChatCommand("/donat"); break;
        case 6:  pNetGame->SendChatCommand("/car"); break;
        case 7:  pNetGame->SendChatCommand("/report"); break;
        case 8:  pNetGame->SendChatCommand("/promo"); break;
        case 9:  CTab::Show(); break;
        case 10: pNetGame->SendChatCommand("/fammenu"); break;
        case 11: pNetGame->SendChatCommand("/achievements"); break;
        case 12: pNetGame->SendChatCommand("/livepass"); break;
        default: break;
    }
}
}
