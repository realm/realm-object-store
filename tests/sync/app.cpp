////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
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

#include <curl/curl.h>
#include <json.hpp>
#include <sstream>
#include "util/test_utils.hpp"
#include "util/test_file.hpp"
#include "catch2/catch.hpp"

#include "sync/app.hpp"
#include "sync/app_credentials.hpp"

#pragma mark - Integration Tests

// temporarily disable these tests for now,
// but allow opt-in by building with REALM_ENABLE_AUTH_TESTS=1
#ifndef REALM_ENABLE_AUTH_TESTS
#define REALM_ENABLE_AUTH_TESTS 0
#endif

#if REALM_ENABLE_AUTH_TESTS

class IntTestTransport : public realm::GenericNetworkTransport {
    static ssize_t write(void *ptr, size_t size, size_t nmemb, std::vector<char>* data) {
        for (size_t i = 0; i < nmemb; i++)
            data->push_back(((char*)ptr)[i]);

        return size * nmemb;
    }

    void send_request_to_server(std::string url,
                                std::string httpMethod,
                                std::map<std::string, std::string> headers,
                                std::vector<char> data,
                                int timeout,
                                std::function<void (std::vector<char>, realm::GenericNetworkError)> completion_block) override {
        CURL *curl;
        CURLcode res;
        std::vector<char> *s = new std::vector<char>();

        curl_global_init(CURL_GLOBAL_ALL);
        /* get a curl handle */
        curl = curl_easy_init();

        struct curl_slist *list = NULL;

        if (curl) {
            /* First set the URL that is about to receive our POST. This URL can
             just as well be a https:// URL if that is what should receive the
             data. */
            curl_easy_setopt(curl, CURLOPT_URL, url.data());

            /* Now specify the POST data */
            if (httpMethod == "POST") {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.data());
            }
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

            for (auto header : headers)
            {
                std::stringstream h;
                h << header.first << ": " << header.second;
                list = curl_slist_append(list, h.str().data());
            }

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, s);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            double cl;
            curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
            /* Check for errors */
            if(res != CURLE_OK)
                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                        curl_easy_strerror(res));

            // erase headers
            s->erase(s->begin(), s->begin() + s->size() - cl);

            /* always cleanup */
            curl_easy_cleanup(curl);
            curl_slist_free_all(list); /* free the list again */

            completion_block(*s, realm::GenericNetworkError{});
        }
        curl_global_cleanup();
    }
};

TEST_CASE("app: login_with_credentials integration", "[sync]") {

    SECTION("login") {
        std::unique_ptr<realm::GenericNetworkTransport> (*factory)() = []{
            return std::unique_ptr<realm::GenericNetworkTransport>(new IntTestTransport);
        };
        realm::GenericNetworkTransport::set_network_transport_factory(factory);

        // TODO: create dummy app using Stitch CLI instead of hardcording
        auto app = realm::RealmApp::app("translate-utwuv", realm::util::none);

        std::condition_variable cv;
        std::mutex m;
        bool processed = false;

        static const std::string base_path = realm::tmp_dir();

        auto tsm = TestSyncManager(base_path);

        app->login_with_credentials(realm::AppCredentials::anonymous(),
                                    60,
                                    [&](std::shared_ptr<realm::SyncUser> user, realm::GenericNetworkError error) {
            CHECK(user);
            CHECK(error.code == realm::GenericNetworkError::GenericNetworkErrorCode::NONE);
            cv.notify_one();
            processed = true;
        });

        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, ^{return processed;});
    }
}

#pragma mark - Unit Tests

//{"access_token":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE1ODE1MDc3OTYsImlhdCI6MTU4MTUwNTk5NiwiaXNzIjoiNWU0M2RkY2M2MzZlZTEwNmVhYTEyYmRjIiwic3RpdGNoX2RldklkIjoiMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwIiwic3RpdGNoX2RvbWFpbklkIjoiNWUxNDk5MTNjOTBiNGFmMGViZTkzNTI3Iiwic3ViIjoiNWU0M2RkY2M2MzZlZTEwNmVhYTEyYmRhIiwidHlwIjoiYWNjZXNzIn0.0q3y9KpFxEnbmRwahvjWU1v9y1T1s3r2eozu93vMc3s","refresh_token":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE1ODY2ODk5OTYsImlhdCI6MTU4MTUwNTk5Niwic3RpdGNoX2RhdGEiOm51bGwsInN0aXRjaF9kZXZJZCI6IjAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwMCIsInN0aXRjaF9kb21haW5JZCI6IjVlMTQ5OTEzYzkwYjRhZjBlYmU5MzUyNyIsInN0aXRjaF9pZCI6IjVlNDNkZGNjNjM2ZWUxMDZlYWExMmJkYyIsInN0aXRjaF9pZGVudCI6eyJpZCI6IjVlNDNkZGNjNjM2ZWUxMDZlYWExMmJkNy15Z2lyaWxkamtxYmJlZHJidmlzcWJqc3YiLCJwcm92aWRlcl90eXBlIjoiYW5vbi11c2VyIiwicHJvdmlkZXJfaWQiOiI1ZTE0YWYzNTQyZTlmNGQwNjlmOWU0MDkifSwic3ViIjoiNWU0M2RkY2M2MzZlZTEwNmVhYTEyYmRhIiwidHlwIjoicmVmcmVzaCJ9.CZ846t1pD-nJRRRyjoGaOL44AU32HaIs5QMEdYQgMQs","user_id":"5e43ddcc636ee106eaa12bda","device_id":"000000000000000000000000"}

//{"user_id":"5e43ddcc636ee106eaa12bda","domain_id":"5e149913c90b4af0ebe93527","identities":[{"id":"5e43ddcc636ee106eaa12bd7-ygirildjkqbbedrbvisqbjsv","provider_type":"anon-user","provider_id":"5e14af3542e9f4d069f9e409"}],"data":{},"type":"normal"}

class UnitTestTransport : public realm::GenericNetworkTransport {
public:
    static std::string access_token;
    static const std::string user_id;
    static const std::string identity_0_id;
    static const std::string identity_1_id;
    static const nlohmann::json profile_0;
    static const nlohmann::json profile_1;

private:
    void handle_profile(std::string,
                        std::string httpMethod,
                        std::map<std::string, std::string> headers,
                        std::vector<char> data,
                        int timeout,
                        std::function<void (std::vector<char>, realm::GenericNetworkError)> completion_block)
    {
        CHECK(httpMethod == "GET");
        CHECK(headers["Content-Type"] == "application/json;charset=utf-8");
        CHECK(headers["Authorization"] == "Bearer " + access_token);
        CHECK(data.empty());
        CHECK(timeout == 60);

        std::string response = nlohmann::json({
            {"user_id", user_id},
            {"identities", {
                {
                    {"id", identity_0_id},
                    {"provider_type", "anon-user"},
                    {"provider_id", "lol"}
                },
                {
                    {"id", identity_1_id},
                    {"provider_type", "lol_wut"},
                    {"provider_id", "nah_dawg"}
                }
            }},
            {"data", profile_0}
        }).dump();

        completion_block(std::vector<char>(response.begin(), response.end()), realm::GenericNetworkError{});
    }

    void handle_login(std::string,
                      std::string httpMethod,
                      std::map<std::string, std::string> headers,
                      std::vector<char> data,
                      int timeout,
                      std::function<void (std::vector<char>, realm::GenericNetworkError)> completion_block)
    {
        CHECK(httpMethod == "POST");
        CHECK(headers["Content-Type"] == "application/json;charset=utf-8");

        CHECK(nlohmann::json::parse(data.begin(), data.end()) == nlohmann::json({{"provider", "anon-user"}}));
        CHECK(timeout == 60);

        std::string response = nlohmann::json({
            {"access_token", access_token},
            {"refresh_token", access_token},
            {"user_id", "Brown Bear"},
            {"device_id", "Panda Bear"}}).dump();

        completion_block(std::vector<char>(response.begin(), response.end()), realm::GenericNetworkError{});
    }

public:
    void send_request_to_server(std::string url,
                                std::string httpMethod,
                                std::map<std::string, std::string> headers,
                                std::vector<char> data,
                                int timeout,
                                std::function<void (std::vector<char>, realm::GenericNetworkError)> completion_block)
    override {
        if (url.find("/login") != std::string::npos) {
            handle_login(url, httpMethod, headers, data, timeout, completion_block);
        } else if (url.find("/profile") != std::string::npos) {
            handle_profile(url, httpMethod, headers, data, timeout, completion_block);
        }
    }
};

static const std::string good_access_token =  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJleHAiOjE1ODE1MDc3OTYsImlhdCI6MTU4MTUwNTk5NiwiaXNzIjoiNWU0M2RkY2M2MzZlZTEwNmVhYTEyYmRjIiwic3RpdGNoX2RldklkIjoiMDAwMDAwMDAwMDAwMDAwMDAwMDAwMDAwIiwic3RpdGNoX2RvbWFpbklkIjoiNWUxNDk5MTNjOTBiNGFmMGViZTkzNTI3Iiwic3ViIjoiNWU0M2RkY2M2MzZlZTEwNmVhYTEyYmRhIiwidHlwIjoiYWNjZXNzIn0.0q3y9KpFxEnbmRwahvjWU1v9y1T1s3r2eozu93vMc3s";

std::string UnitTestTransport::access_token = good_access_token;

static const std::string bad_access_token = "lolwut";

const std::string UnitTestTransport::user_id = "Ailuropoda melanoleuca";
const std::string UnitTestTransport::identity_0_id = "Ursus arctos isabellinus";
const std::string UnitTestTransport::identity_1_id = "Ursus arctos horribilis";

static const std::string profile_0_first_name = "Ursus americanus";
static const std::string profile_0_last_name = "Ursus boeckhi";
static const std::string profile_0_email = "Ursus ursinus";
static const std::string profile_0_picture_url = "Ursus malayanus";
static const std::string profile_0_gender = "Ursus thibetanus";
static const std::string profile_0_birthday = "Ursus americanus";
static const std::string profile_0_min_age = "Ursus maritimus";
static const std::string profile_0_max_age = "Ursus arctos";

const nlohmann::json UnitTestTransport::profile_0 = {
    {"first_name", profile_0_first_name},
    {"last_name", profile_0_last_name},
    {"email", profile_0_email},
    {"picture_url", profile_0_picture_url},
    {"gender", profile_0_gender},
    {"birthday", profile_0_birthday},
    {"min_age", profile_0_min_age},
    {"max_age", profile_0_max_age}
};

const nlohmann::json UnitTestTransport::profile_1 = {
};

TEST_CASE("app: login_with_credentials unit_tests", "[sync]") {
    std::unique_ptr<realm::GenericNetworkTransport> (*factory)() = []{
        return std::unique_ptr<realm::GenericNetworkTransport>(new UnitTestTransport);
    };
    realm::GenericNetworkTransport::set_network_transport_factory(factory);

    auto app = realm::RealmApp::app("<>", realm::util::none);

    SECTION("login_anonymous good") {
        UnitTestTransport::access_token = good_access_token;

        std::condition_variable cv;
        std::mutex m;
        bool processed = false;

        app->login_with_credentials(realm::AppCredentials::anonymous(),
                                    60,
                                    [&](std::shared_ptr<realm::SyncUser> user, realm::GenericNetworkError error) {
            CHECK(user);
            CHECK(error.code == realm::GenericNetworkError::GenericNetworkErrorCode::NONE);

            CHECK(user->identities().size() == 2);
            CHECK(user->identities()[0].id == UnitTestTransport::identity_0_id);
            CHECK(user->identities()[1].id == UnitTestTransport::identity_1_id);
            CHECK(user->user_profile()->first_name() == profile_0_first_name);
            CHECK(user->user_profile()->last_name() == profile_0_last_name);
            CHECK(user->user_profile()->email() == profile_0_email);
            CHECK(user->user_profile()->picture_url() == profile_0_picture_url);
            CHECK(user->user_profile()->gender() == profile_0_gender);
            CHECK(user->user_profile()->birthday() == profile_0_birthday);
            CHECK(user->user_profile()->min_age() == profile_0_min_age);
            CHECK(user->user_profile()->max_age() == profile_0_max_age);

            cv.notify_one();
            processed = true;
        });

        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, ^{return processed;});
    }

    SECTION("login_anonymous bad") {
        UnitTestTransport::access_token = bad_access_token;

        std::condition_variable cv;
        std::mutex m;
        bool processed = false;

        app->login_with_credentials(realm::AppCredentials::anonymous(),
                                    60,
                                    [&](std::shared_ptr<realm::SyncUser> user, realm::GenericNetworkError error) {
            CHECK(!user);
            CHECK(error.code == realm::GenericNetworkError::GenericNetworkErrorCode::INVALID_TOKEN);

            cv.notify_one();
            processed = true;
        });

        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, ^{return processed;});
    }
}


#endif // REALM_ENABLE_AUTH_TESTS
