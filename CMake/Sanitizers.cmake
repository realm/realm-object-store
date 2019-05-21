###########################################################################
#
# Copyright 2016 Realm Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
###########################################################################

option(SANITIZE_ADDRESS "build with ASan")
option(SANITIZE_THREAD "build with TSan")
option(SANITIZE_UNDEFINED "build with UBSan")
option(SANITIZE_MEMORY "build with MSan")

if(SANITIZE_ADDRESS)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=address")
    set(CORE_SANITIZER_FLAGS ${CORE_SANITIZER_FLAGS};-D;REALM_ASAN=ON)
endif()

if(SANITIZE_THREAD)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=thread")
    set(CORE_SANITIZER_FLAGS ${CORE_SANITIZER_FLAGS};-D;REALM_TSAN=ON)
endif()

if(SANITIZE_UNDEFINED)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=undefined")
    set(CORE_SANITIZER_FLAGS ${CORE_SANITIZER_FLAGS};-D;REALM_USAN=ON)
endif()

if(SANITIZE_MEMORY)
    set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=memory")
    set(CORE_SANITIZER_FLAGS ${CORE_SANITIZER_FLAGS};-D;REALM_MSAN=ON)
endif()

if(SANITIZE_ADDRESS OR SANITIZE_THREAD OR SANITIZE_UNDEFINED OR SANITIZE_MEMORY)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS} -fno-omit-frame-pointer")
endif()
