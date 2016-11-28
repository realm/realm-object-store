/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#include <jni.h>

#include <cerrno>
#include <iostream>
#include <fstream>
#include <string>

#include <android/log.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"


#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))


void android_main(struct android_app* state)
{
    // Make sure glue isn't stripped.
    app_dummy();

    ANativeActivity* nativeActivity = state->activity;
    const char* externalDataPath = nativeActivity->externalDataPath;
    const char* internalDataPath = nativeActivity->internalDataPath;
    std::string inDataPath(internalDataPath);
    std::string exDataPath(externalDataPath);

    LOGI("Starting unit tests...");

    Catch::registerTestMethods();
    int result = Catch::Session().run(0, nullptr);

    LOGI("Done running unit tests... %d", result);

    ANativeActivity_finish(nativeActivity);
}
