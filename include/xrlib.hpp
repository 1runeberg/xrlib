/* 
 * Copyright 2024,2025 Copyright Rune Berg 
 * https://github.com/1runeberg | http://runeberg.io | https://runeberg.social | https://www.youtube.com/@1RuneBerg
 * Licensed under Apache 2.0: https://www.apache.org/licenses/LICENSE-2.0
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This work is the next iteration of OpenXRProvider (v1, v2)
 * OpenXRProvider (v1): Released 2021 -  https://github.com/1runeberg/OpenXRProvider
 * OpenXRProvider (v2): Released 2022 - https://github.com/1runeberg/OpenXRProvider_v2/
 * v1 & v2 licensed under MIT: https://opensource.org/license/mit
*/


#pragma once

#include <xrlib/session.hpp>
#include <xrlib/input.hpp>
#include <xrlib/vulkan.hpp>

namespace xrlib
{
    #ifdef XR_USE_PLATFORM_ANDROID
    static void ExitApp( struct android_app *pAndroidApp)
    {
        if ( pAndroidApp && pAndroidApp->activity && pAndroidApp->activity->vm && pAndroidApp->activity->clazz )
        {
            JNIEnv *env = nullptr;
            pAndroidApp->activity->vm->AttachCurrentThread( &env, nullptr );
            jclass activityClass = env->GetObjectClass( pAndroidApp->activity->clazz );
            jmethodID finishMethod = env->GetMethodID( activityClass, "finish", "()V" );
            env->CallVoidMethod( pAndroidApp->activity->clazz, finishMethod );
            pAndroidApp->activity->vm->DetachCurrentThread();
        }
    }
    #else
    static int ExitApp( int result )
        {
            std::cout << "\n\nPress enter to end.";
            std::cin.get();

            return result;
        }
    #endif
} // namespace xrlib
