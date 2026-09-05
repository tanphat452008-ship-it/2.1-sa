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
#include "java_systems/Authorization.h"
#include "java_systems/Registration.h"
void CLoader::loadBassLib()
{
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

// BỌC AN TOÀN: Kiểm tra Exception và NULL để không bao giờ bị crash NewGlobalRef
jclass LinkJavaClass(jclass localObj) {
    auto env = CJavaWrapper::GetEnv();
    
    // Nếu phía Java bị lỗi không tìm thấy Class (ClassNotFoundException) -> Xóa lỗi
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    // Nếu con trỏ localObj bị NULL -> Trả về nullptr thay vì gọi NewGlobalRef gây crash
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
    CJavaGui::clazz = LinkJavaClass(env->FindClass("com/game/russia/NewUiList"));

    // 2. Bảng Tab danh sách người chơi
    CTab::clazz = LinkJavaClass(env->FindClass("com/game/russia/gui/tab/Tab"));

    // 3. Hệ thống Chat / Đăng nhập cơ bản (nếu APK của bạn dùng các class này)
    CAuthorization::clazz = LinkJavaClass(env->FindClass("com/game/russia/gui/Authorization"));
    CRegistration::clazz = LinkJavaClass(env->FindClass("com/game/russia/gui/Registration"));

}
