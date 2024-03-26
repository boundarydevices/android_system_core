/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_HARDWARE_CONFIRMATIONUI_V1_0_TRUSTY_CONFIRMATIONUI_H
#define ANDROID_HARDWARE_CONFIRMATIONUI_V1_0_TRUSTY_CONFIRMATIONUI_H

#ifdef ENABLE_SECURE_DISPLAY
#include <aidl/android/hardware/graphics/composer3/IComposer.h>
#include <aidl/android/hardware/graphics/composer3/IComposerClient.h>
#include <android/hardware/graphics/composer3/ComposerClientWriter.h>
#include <android/hardware/graphics/composer/2.1/IComposer.h>
#include <android/hardware/graphics/composer/2.1/IComposerClient.h>
#endif

#include <aidl/android/hardware/confirmationui/BnConfirmationUI.h>
#include <aidl/android/hardware/confirmationui/IConfirmationResultCallback.h>
#include <aidl/android/hardware/confirmationui/UIOption.h>
#include <aidl/android/hardware/security/keymint/HardwareAuthToken.h>
#include <android/binder_manager.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <teeui/generic_messages.h>
#include <thread>

#include "TrustyApp.h"

namespace aidl::android::hardware::confirmationui {

#ifdef ENABLE_SECURE_DISPLAY
namespace V2_1 = ::android::hardware::graphics::composer::V2_1;
using V2_1::Layer;
using V2_1::Error;
using aidl::android::hardware::graphics::composer3::ComposerClientWriter;
using AidlFRect = aidl::android::hardware::graphics::common::FRect;
using AidlRect = aidl::android::hardware::graphics::common::Rect;
using aidl::android::hardware::graphics::composer3::CommandResultPayload;
using aidl::android::hardware::graphics::composer3::Composition;
#endif

using std::shared_ptr;
using std::string;
using std::vector;
using ::android::sp;

using ::aidl::android::hardware::security::keymint::HardwareAuthToken;
using ::android::trusty::confirmationui::TrustyApp;

class TrustyConfirmationUI : public BnConfirmationUI {
  public:
    TrustyConfirmationUI();
    virtual ~TrustyConfirmationUI();
    // Methods from ::aidl::android::hardware::confirmationui::IConfirmationUI
    // follow.
    ::ndk::ScopedAStatus
    promptUserConfirmation(const shared_ptr<IConfirmationResultCallback>& resultCB,
                           const vector<uint8_t>& promptText, const vector<uint8_t>& extraData,
                           const string& locale, const vector<UIOption>& uiOptions) override;
    ::ndk::ScopedAStatus
    deliverSecureInputEvent(const HardwareAuthToken& secureInputToken) override;

    ::ndk::ScopedAStatus abort() override;

  private:
    std::weak_ptr<TrustyApp> app_;
    std::thread callback_thread_;

    enum class ListenerState : uint32_t {
        None,
        Starting,
        SetupDone,
        Interactive,
        Terminating,
    };

    /*
     * listener_state is protected by listener_state_lock. It makes transitions between phases
     * of the confirmation operation atomic.
     * (See TrustyConfirmationUI.cpp#promptUserConfirmation_ for details about operation phases)
     */
    ListenerState listener_state_;
    /*
     * abort_called_ is also protected by listener_state_lock_ and indicates that the HAL user
     * called abort.
     */
    bool abort_called_;
    std::mutex listener_state_lock_;
    std::condition_variable listener_state_condv_;
    int prompt_result_;
    bool secureInputDelivered_;

    std::tuple<teeui::ResponseCode, teeui::MsgVector<uint8_t>, teeui::MsgVector<uint8_t>>
    promptUserConfirmation_(const teeui::MsgString& promptText,
                            const teeui::MsgVector<uint8_t>& extraData,
                            const teeui::MsgString& locale,
                            const teeui::MsgVector<teeui::UIOption>& uiOptions);
#ifdef ENABLE_SECURE_DISPLAY
    static constexpr int kMaxLayerBufferCount = 64;
    int64_t layer_id;
    int64_t primary_display_id;
    Error enable_secure_display(bool enable, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
    Error refresh_display(const std::shared_ptr<TrustyApp>& app);
#endif
};

}  // namespace aidl::android::hardware::confirmationui

#endif  // ANDROID_HARDWARE_CONFIRMATIONUI_V1_0_TRUSTY_CONFIRMATIONUI_H
