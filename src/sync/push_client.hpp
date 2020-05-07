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
               AuthRequestClient& auth_request_client,
               AppServiceClient&& app_service_client) :
    m_service_name(service_name),
    m_app_id(app_id),
    m_auth_request_client(auth_request_client),
    m_app_service_client(std::move(app_service_client)) { }
    
    void register_device(const std::string& registration_token,
                         std::shared_ptr<SyncUser> sync_user,
                         std::function<void(util::Optional<AppError>)> completion_block);
    
    void deregister_device(const std::string& registration_token,
                           std::shared_ptr<SyncUser> sync_user,
                           std::function<void(util::Optional<AppError>)> completion_block);
    
    struct FCMSendMessageNotification {
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
    
    struct FCMSendMessageRequest {
        
        enum FCMSendMessagePriority {
            normal,
            high
        };
        
        /**
         * The priority of the message.
         */
        FCMSendMessagePriority priority;

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
         * How long (in seconds) the message should be kept in FCM storage if the device is offline.
         */
        util::Optional<uint64_t> time_to_live;

        /**
         * The custom data to send in the payload.
         */
        util::Optional<bson::BsonDocument> data;

        /**
         * The predefined, user-visible key-value pairs of the notification payload.
         */
        util::Optional<FCMSendMessageNotification> notification;
    };
    
    struct FCMSendMessageResult {
        
    };
    
    void send_message(const std::string& target,
                      const FCMSendMessageRequest& request,
                      std::function<void(util::Optional<AppError>,
                                         util::Optional<FCMSendMessageResult>)> completion_block);
    
    void send_message_to_user_ids(std::vector<std::string> user_ids,
                                  const FCMSendMessageRequest& request,
                                  std::function<void(util::Optional<AppError>,
                                                     util::Optional<FCMSendMessageResult>)> completion_block);
    
    void send_message_to_registration_tokens(std::vector<std::string> user_ids,
                                             const FCMSendMessageRequest& request,
                                             std::function<void(util::Optional<AppError>,
                                                                util::Optional<FCMSendMessageResult>)> completion_block);
    
private:
    std::string m_service_name;
    std::string m_app_id;
    AuthRequestClient& m_auth_request_client;
    AppServiceClient m_app_service_client;
};

} // namespace app
} // namespace realm

#endif /* PUSH_CLIENT_HPP */
