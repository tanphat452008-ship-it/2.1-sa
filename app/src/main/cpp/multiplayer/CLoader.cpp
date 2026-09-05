//
// Created on 24.01.2023.
//

#include <jni.h>
#include "CLoader.h"
#include "util/patch.h"
#include "crashlytics.h"
#include "CSettings.h"
#include "net/netgame.h"

// Chỉ giữ lại các header khung cơ bản (HUD, Chat, Dialog, Tab)
#include "java_systems/HUD.h"
#include "java_systems/Tab.h"
#include "java_systems/GuiWrapper.h"
#include "JavaGui.h"

void CLoader::loadBassLib()
{
   // LoadBassLibrary();
   // BASS_Init(-1, 44100, BASS_DEVICE_MONO | BASS_DEVICE_3D);
   // BASS_Set3DFactors(1, 0.15, 0);
   // BASS_Apply3D();
}

void CLoader::initCrashLytics()
{
    firebase::crashlytics::SetCustomKey("build data", __DATE__);
    firebase::crashlytics::SetCustomKey("build time", __TIME__);

    firebase::crashlytics::SetUserId(CSettings::m_Settings.szNickName);
    firebase::crashlytics::SetCustomKey("Nick", CSettings::m_Settings.szNickName);

    char str[100];

    sprintf(str, "0x%x", g_libGTASA);
    firebase::crashlytics::SetCustomKey("libGTASA.so", str);

    sprintf(str, "0x%x", g_libSAMP);
    firebase::crashlytics::SetCustomKey("libsamp.so", str);
}

void CLoader::loadSetting()
{
    CSettings::LoadSettings(nullptr);
}

// BỌC AN TOÀN: Tránh crash NewGlobalRef khi không tìm thấy class Java
jclass LinkJavaClass(jclass localObj) {
    auto env = CJavaWrapper::GetEnv();
    if (!env) return nullptr;

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (localObj == nullptr) {
        return nullptr;
    }

    auto globalRef = (jclass)env->NewGlobalRef(localObj);
    env->DeleteLocalRef(localObj);
    return globalRef;
}

void CLoader::initJavaClasses(JavaVM* pjvm) {
    JNIEnv* env = nullptr;
    if (pjvm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return;
    }

    // 1. Giao diện cơ bản (Khung Wrapper chứa HUD, Chat, Dialog)
    CJavaGui::clazz = LinkJavaClass(env->FindClass("com/russia/game/NewUiList"));

    // 2. Bảng Tab (Danh sách người chơi)
    CTab::clazz = LinkJavaClass(env->FindClass("com/russia/game/gui/tab/Tab"));
}
