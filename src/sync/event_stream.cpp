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

#include "event_stream.hpp"

#include <utility>

namespace realm {
namespace app {
EventStream::EventStream(Response response) : m_response(std::move(response)) {
}

bson::BsonDocument EventStream::get_next_event() {
    while(true){
        std::string line = m_response.read_body_line();
        m_watch_stream.feed_line(line);


        if (m_watch_stream.state() == WatchStream::HAVE_EVENT) {
            return m_watch_stream.next_event();
        }

        if (m_watch_stream.state() == WatchStream::HAVE_ERROR) {
            throw AppError(make_custom_error_code(1001), "Event stream has error");
        }
    }
}

void EventStream::close() {
    m_response.close();
}


void WatchStream::feed_buffer(std::string_view input) {
    REALM_ASSERT(m_state == NEED_DATA);
    m_buffer += input;
    advance_buffer_state();
}

void WatchStream::advance_buffer_state() {
    REALM_ASSERT(m_state == NEED_DATA);
    while (m_state == NEED_DATA) {
        if (m_buffer_offset == m_buffer.size()) {
            m_buffer.clear();
            m_buffer_offset = 0;
            return;
        }

        // NOTE not supporting CR-only newlines, just LF and CRLF.
        auto next_newline = m_buffer.find('\n', m_buffer_offset);
        if (next_newline == std::string::npos) {
            // We have a partial line.
            if (m_buffer_offset != 0) {
                // Slide the partial line down to the front of the buffer.
                m_buffer.assign(m_buffer.data() + m_buffer_offset, m_buffer.size() - m_buffer_offset);
                m_buffer_offset = 0;
            }
            return;
        }

        feed_line(std::string_view(m_buffer.data() + m_buffer_offset, next_newline - m_buffer_offset));
        m_buffer_offset = next_newline + 1; // Advance past this line, including its newline.
    }
}

void WatchStream::feed_line(std::string_view line) {
    REALM_ASSERT(m_state == NEED_DATA);
    // This is an implementation of the algorithm described at
    // https://html.spec.whatwg.org/multipage/server-sent-events.html#event-stream-interpretation.
    // Currently the server does not use id or retry lines, so that processing isn't implemented.

    // ignore trailing LF if not removed by SDK.
    if (!line.empty() && line.back() == '\n')
        line = line.substr(0, line.size() - 1);

    // ignore trailing CR from CRLF
    if (!line.empty() && line.back() == '\r')
        line = line.substr(0, line.size() - 1);

    if (line.empty()) {
        // This is the "dispatch the event" portion of the algorithm.
        if (m_data_buffer.empty()) {
            m_event_type.clear();
            return;
        }

        if (m_data_buffer.back() == '\n')
            m_data_buffer.pop_back();

        feed_sse({m_data_buffer, m_event_type});
        m_data_buffer.clear();
        m_event_type.clear();
    }

    if (line[0] == ':')
        return;

    const auto colon = line.find(':');
    const auto field = line.substr(0, colon);
    auto value = colon == std::string::npos ? std::string_view() : line.substr(colon + 1);
    if (!value.empty() && value[0] == ' ')
        value = value.substr(1);

    if (field == "event") {
        m_event_type = value;
    } else if (field == "data") {
        m_data_buffer += value;
        m_data_buffer += '\n';
    } else {
        // line is ignored (even if field is id or retry).
    }
}

void WatchStream::feed_sse(ServerSentEvent sse) {
    REALM_ASSERT(m_state == NEED_DATA);
    std::string buffer; // must outlast if-block since we bind sse.data to it.
    size_t first_percent = sse.data.find('%');
    if (first_percent != std::string::npos) {
        // For some reason, the stich server decided to add percent-encoding for '%', '\n', and '\r' to its
        // event-stream replies. But it isn't real urlencoding, since most characters pass through, so we can't use
        // uri_percent_decode() here.
        buffer.reserve(sse.data.size());
        size_t start = 0;
        while (true) {
            auto percent = start == 0 ? first_percent : sse.data.find('%', start);
            buffer += sse.data.substr(start, percent - start);
            if (percent == std::string::npos)
                break;

            auto encoded = sse.data.substr(percent, 3); // may be smaller than 3 if string ends with %
            if (encoded == "%25") {
                buffer += '%';
            } else if (encoded == "%0A") {
                buffer += '\x0A'; // '\n'
            } else if (encoded == "%0D") {
                buffer += '\x0D'; // '\r'
            } else {
                buffer += encoded; // propagate as-is
            }
            start = percent + encoded.size();
        }

        sse.data = buffer;
    }

    if (sse.eventType.empty() || sse.eventType == "message") {
        try {
            auto parsed = bson::parse(sse.data);
            if (parsed.type() == bson::Bson::Type::Document) {
                m_next_event = parsed.operator const bson::BsonDocument&();
                m_state = HAVE_EVENT;
                return;
            }
        } catch (...) {
            // fallthrough to same handling as for non-document value.
        }
        m_state = HAVE_ERROR;
        m_error.emplace(app::make_error_code(JSONErrorCode::bad_bson_parse),
                        "server returned malformed event: " + std::string(sse.data));
    } else if (sse.eventType == "error") {
        m_state = HAVE_ERROR;

        // default error message if we have issues parsing the reply.
        m_error.emplace(app::make_error_code(ServiceErrorCode::unknown), std::string(sse.data));
        try {
            auto parsed = bson::parse(sse.data);
            if (parsed.type() != bson::Bson::Type::Document) return;
            auto& obj = parsed.operator const bson::BsonDocument&();
            auto& code = obj.at("error_code");
            auto& msg = obj.at("error");
            if (code.type() != bson::Bson::Type::String) return;
            if (msg.type() != bson::Bson::Type::String) return;
            m_error.emplace(app::make_error_code(app::service_error_code_from_string(
                    code.operator const std::string&())),
                            msg.operator const std::string&());
        } catch(...) {
            return; // Use the default state.
        }
    } else {
        // Ignore other event types
    }
}
}
}