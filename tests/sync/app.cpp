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

#include "catch2/catch.hpp"

#include "sync/app.hpp"
#include "sync/app_credentials.hpp"

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

            curl_easy_setopt(curl, CURLOPT_HEADER, list);
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

TEST_CASE("app: login_with_credentials", "[sync]") {

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

        app->login_with_credentials(realm::AppCredentials::anonymous(),
                                    60,
                                    [&](std::shared_ptr<realm::SyncUser> user, realm::GenericNetworkError error) {
            CHECK(user);
            CHECK(!error.code);
            cv.notify_one();
            processed = true;
        });
        
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, ^{return processed;});
    }
}
