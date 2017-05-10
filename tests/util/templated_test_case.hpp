////////////////////////////////////////////////////////////////////////////
//
// Copyright 2017 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

// Define a Catch test case templated on up to 10 types
// The current type is exposed inside the test as `TestType`
#define TEMPLATE_TEST_CASE(name, ...) \
    REALM_TEMPLATE_TEST_CASE(name, INTERNAL_CATCH_UNIQUE_NAME(REALM_TEMPLATE_TEST_), \
                             __VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1)

#define REALM_TEMPLATE_TEST_CASE(name, fn, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, n, ...) \
    template<typename> static void fn(); \
    TEST_CASE(name) { \
        REALM_TEMPLATE_TEST_CASE_##n(fn, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10) \
    } \
    template<typename TestType> static void fn()

#define REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    INTERNAL_CATCH_SECTION(#T, "") { fn<T>(); }

#define REALM_TEMPLATE_TEST_CASE_1(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \

#define REALM_TEMPLATE_TEST_CASE_2(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_1(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_3(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_2(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_4(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_3(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_5(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_4(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_6(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_5(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_7(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_6(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_8(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_7(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_9(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_8(fn, __VA_ARGS__)

#define REALM_TEMPLATE_TEST_CASE_10(fn, T, ...) \
    REALM_TEMPLATE_TEST_CASE_SECTION(fn, T) \
    REALM_TEMPLATE_TEST_CASE_9(fn, __VA_ARGS__)
