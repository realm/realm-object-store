////////////////////////////////////////////////////////////////////////////
//
// Copyright 2020 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or utilied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_EVENT_STREAM_HPP
#define REALM_EVENT_STREAM_HPP

#include "generic_network_transport.hpp"
#include <json.hpp>
#include <string>
#include <vector>
#include <set>
#include <object-store/src/util/bson/bson.hpp>

namespace realm {
namespace app {
/**
 * Simplifies the handling the stream for collection.watch() API.
 *
 * General pattern for languages with pull-based async generators (preferred):
 *    auto request = app.make_streaming_request("watch", ...);
 *    auto reply = await doHttpRequestUsingNativeLibs(request);
 *    if (reply.error)
 *        throw reply.error;
 *    auto ws = WatchStream();
 *    for await (chunk : reply.body) {
 *        ws.feedBuffer(chunk);
 *        while (ws.state == WatchStream::HAVE_EVENT) {
 *            yield ws.nextEvent();
 *        }
 *        if (ws.state == WatchStream::HAVE_ERROR)
 *            throw ws.error;
 *    }
 *
 * General pattern for languages with only push-based streams:
 *    auto request = app.make_streaming_request("watch", ...);
 *    doHttpRequestUsingNativeLibs(request, {
 *        .onError = [downstream](error) { downstream.onError(error); },
 *        .onHeadersDone = [downstream](reply) {
 *            if (reply.error)
 *                downstream.onError(error);
 *        },
 *        .onBodyChunk = [downstream, ws = WatchStream()](chunk) {
 *            ws.feedBuffer(chunk);
 *            while (ws.state == WatchStream::HAVE_EVENT) {
 *                downstream.nextEvent(ws.nextEvent());
 *            }
 *            if (ws.state == WatchStream::HAVE_ERROR)
 *                downstream.onError(ws.error);
 *        }
 *    });
 */
struct WatchStream {
    // NOTE: this is a fully processed event, not a single "data: foo" line!
    struct ServerSentEvent {
        std::string_view data;
        std::string_view eventType = "message";
    };

    // Call these when you have data, in whatever shape is easiest for your SDK to get.
    // Pick one, mixing and matching on a single instance isn't supported.
    // These can only be called in NEED_DATA state, which is the initial state.
    void feed_buffer(std::string_view); // May have multiple and/or partial lines.
    void feed_line(std::string_view); // May include terminating CR and/or LF (not required).
    void feed_sse(ServerSentEvent); // Only interested in "message" and "error" events. Others are ignored.

    // Call state() to see what to do next.
    enum State {
        NEED_DATA, // Need to call one of the feed functions.
        HAVE_EVENT, // Call next_event() to consume an event.
        HAVE_ERROR, // Call error().
    };
    State state() const { return m_state; }

    // Consumes the returned event. If you used feed_buffer(), there may be another event or error after this one,
    // so you need to call state() again to see what to do next.
    bson::BsonDocument next_event() {
        REALM_ASSERT(m_state == HAVE_EVENT);
        auto out = std::move(m_next_event);
        m_state = NEED_DATA;
        advance_buffer_state();
        return out;
    }

    // Once this enters the error state, it stays that way. You should not feed any more data.
    const app::AppError& error() const {
        REALM_ASSERT(m_state == HAVE_ERROR);
        return *m_error;
    }

private:
    void advance_buffer_state();

    State m_state = NEED_DATA;
    util::Optional<app::AppError> m_error;
    bson::BsonDocument m_next_event;

    // Used by feed_buffer to construct lines
    std::string m_buffer;
    size_t m_buffer_offset = 0;

    // Used by feed_line for building the next SSE
    std::string m_event_type;
    std::string m_data_buffer;

};

class EventStream {
public:

    EventStream(Response response);

    EventStream(const EventStream&) = default;
    EventStream(EventStream&&) = default;
    EventStream& operator=(const EventStream&) = default;
    EventStream& operator=(EventStream&&) = default;
    ~EventStream() = default;

    /// Returns the next available event in the stream
    /// @param completion_block The result of performing the deletion. Returns the count of deleted objects
    /// @returns document representing the event
    bson::BsonDocument get_next_event();

    /// Closes the streamed network response
    void close();
private:
    Response m_response;
    WatchStream m_watch_stream;
};
}
}

#endif //REALM_EVENT_STREAM_HPP
