//
//  push_client.hpp
//  realm-object-store
//
//  Created by Lee Maguire on 06/05/2020.
//

#ifndef PUSH_CLIENT_HPP
#define PUSH_CLIENT_HPP

#include "sync/auth_request_client.hpp"
#include "sync/app_service_client.hpp"
#include <realm/util/optional.hpp>
#include <string>
#include <map>

namespace realm {
namespace app {

class PushClient {
public:
    PushClient(const std::string& service_name,
               const std::string& app_id,
               std::shared_ptr<AuthRequestClient> auth_request_client,
               std::shared_ptr<AppServiceClient> app_service_client) :
    m_service_name(service_name),
    m_app_id(app_id),
    m_auth_request_client(auth_request_client),
    m_app_service_client(app_service_client) { }
    
    ~PushClient() = default;
    PushClient(const PushClient&) = default;
    PushClient(PushClient&&) = default;
    PushClient& operator=(const PushClient&) = default;
    PushClient& operator=(PushClient&&) = default;
    
    void register_device(const std::string& registration_token,
                         std::shared_ptr<SyncUser> sync_user,
                         std::function<void(util::Optional<AppError>)> completion_block);
    
    void deregister_device(const std::string& registration_token,
                           std::shared_ptr<SyncUser> sync_user,
                           std::function<void(util::Optional<AppError>)> completion_block);
    
    struct SendMessageNotification {
            /**
         * The notification's title.
         */
        util::Optional<std::string> title;

        /**
         * The notification's body text.
         */
        util::Optional<std::string> body;

        /**
         * The sound to play when the device receives the notification.
         */
        util::Optional<std::string> sound;

        /**
         * The action associated with a user click on the notification.
         */
        util::Optional<std::string> click_action;

        /**
         * The the key to the body string in the app's string resources to use to localize the body
         * text to the user's current localization.
         */
        util::Optional<std::string> body_loc_key;

        /**
         * The variable string values to be used in place of the format specifiers in
         * bodyLocKey to use to localize the body text to the user's current localization.
         */
        util::Optional<std::string> body_loc_args;

        /**
         * The key to the title string in the app's string resources to use to localize the
         * title text to the user's current localization.
         */
        util::Optional<std::string> title_loc_key;

        /**
         * The variable string values to be used in place of the format specifiers in
         * titleLocKey to use to localize the title text to the user's current localization.
         */
        util::Optional<std::string> title_loc_args;

        /**
         * The notification's icon. Note: for messages to Android devices only.
         */
        util::Optional<std::string> icon;

        /**
         * The identifier used to replace existing notifications in the notification drawer.
         * Note: for messages to Android devices only.
         */
        util::Optional<std::string> tag;

        /**
         * The notification's icon color, expressed in #rrggbb format. Note: for messages to
         * Android devices only.
         */
        util::Optional<std::string> color;

        /**
         * The value of the badge on the home screen app icon. Note: for messages to iOS devices only.
         */
        util::Optional<std::string> badge;
    };
    
    struct SendMessageRequest {
        
        enum SendMessagePriority {
            normal,
            high
        };
        
        /**
         * The priority of the message.
         */
        SendMessagePriority priority;

        /**
         * The group of messages that can be collapsed.
         */
        util::Optional<std::string> collapse_key;

        /**
         * Whether or not to indicate to the client that content is available in order
         * to wake the device. Note: for messages to iOS devices only.
         */
        util::Optional<bool> content_available;

        /**
         * Whether or not the content in the message can be mutated. Note: for messages to
         * iOS devices only.
         */
        util::Optional<bool> mutable_content;

        /**
         * How long (in seconds) the message should be kept in  storage if the device is offline.
         */
        util::Optional<int64_t> time_to_live;

        /**
         * The custom data to send in the payload.
         */
        util::Optional<bson::BsonDocument> data;

        /**
         * The predefined, user-visible key-value pairs of the notification payload.
         */
        util::Optional<SendMessageNotification> notification;
    };
    
    struct SendMessageResultFailureDetail {
        /**
        * The index corresponding to the target.
        */
        int64_t index;
        
        /**
        * The error that occurred.
        */
        std::string error;
        
        /**
        * The user ID that could not be sent a message to, if applicable.
        */
        util::Optional<std::string> user_id;
    };
    
    struct SendMessageResult {
        /**
        * The number of messages successfully sent.
        */
        int64_t successes;
        
        /**
        * The number of messages successfully sent.
        */
        int64_t failures;
        
        /**
        * The details of each failure, if there were failures.
        */
        std::vector<SendMessageResultFailureDetail> failure_details;
    };
    
    void send_message(const std::string& target,
                      const SendMessageRequest& request,
                      std::function<void(util::Optional<AppError>,
                                         util::Optional<SendMessageResult>)> completion_block);
    
    void send_message_to_user_ids(std::vector<std::string> user_ids,
                                  const SendMessageRequest& request,
                                  std::function<void(util::Optional<AppError>,
                                                     util::Optional<SendMessageResult>)> completion_block);
    
    void send_message_to_registration_tokens(bson::BsonArray registration_tokens,
                                             const SendMessageRequest& request,
                                             std::function<void(util::Optional<AppError>,
                                                                util::Optional<SendMessageResult>)> completion_block);
    
private:
    friend class App;
    
    std::string m_service_name;
    std::string m_app_id;
    std::shared_ptr<AuthRequestClient> m_auth_request_client;
    std::shared_ptr<AppServiceClient> m_app_service_client;
    
    void send_message(const bson::BsonArray& args, std::function<void(util::Optional<AppError>,
                                                                      util::Optional<SendMessageResult>)> completion_block);
};

} // namespace app
} // namespace realm

#endif /* PUSH_CLIENT_HPP */
