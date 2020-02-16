#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "condition.hpp"
#include "session.hpp"
#include "string.hpp"
#include "traits.hpp"

namespace dolores {
    template <typename E, typename = enable_if_derived_from_user_event_t<E>>
    class Handler {
    public:
        explicit Handler(std::function<void(Session<E> &session)> impl, std::shared_ptr<Condition> condition = nullptr)
            : _impl(std::move(impl)), _condition(std::move(condition)) {
        }

        bool check_condition(const E &event) const {
            if (!_condition) return true;
            return (*_condition)(event);
        }

        void run(Session<E> &session) const {
            if (!_impl) return;
            _impl(session);
        }

    private:
        std::shared_ptr<Condition> _condition;
        std::function<void(Session<E> &session)> _impl;
    };

    struct _HandlerMapWrapper {
        template <typename E, typename = enable_if_derived_from_user_event_t<E>>
        using HandlerMap = std::map<std::string, std::shared_ptr<Handler<E>>>;

        template <typename E, typename = enable_if_derived_from_user_event_t<E>>
        static auto &handlers() {
            if constexpr (std::is_base_of_v<cq::MessageEvent, E>) {
                static HandlerMap<cq::MessageEvent> message_handlers;
                return message_handlers;
            } else if constexpr (std::is_base_of_v<cq::NoticeEvent, E>) {
                static HandlerMap<cq::NoticeEvent> notice_handlers;
                return notice_handlers;
            } else { // std::is_base_of_v<cq::RequestEvent, E>
                static HandlerMap<cq::RequestEvent> request_handlers;
                return request_handlers;
            }
        }
    };

    template <typename E, typename = enable_if_derived_from_user_event_t<E>>
    inline bool add_handler(const std::string &name, const std::shared_ptr<Handler<E>> &handler) {
        auto &handlers = _HandlerMapWrapper::handlers<E>();
        handlers[name] = handler;
        return true;
    }

    template <typename E, typename = enable_if_derived_from_user_event_t<E>>
    inline void run_handlers(const E &event) {
        const auto &handlers = _HandlerMapWrapper::handlers<E>();
        for (const auto &[name, handler] : handlers) {
            if (string::startswith(name, "_")) continue;
            if (handler->check_condition(event)) {
                if constexpr (std::is_base_of_v<cq::MessageEvent, E>) {
                    MessageSession session(event);
                    handler->run(session);
                } else if constexpr (std::is_base_of_v<cq::NoticeEvent, E>) {
                    NoticeSession session(event);
                    handler->run(session);
                } else { // std::is_base_of_v<cq::RequestEvent, E>
                    RequestSession session(event);
                    handler->run(session);
                }
            }
        }
    }
} // namespace dolores

#define dolores_on_message(Name, ...)                                                                                 \
    static void __dummy_message_handler_##Name(dolores::MessageSession &);                                            \
    static const auto __dummy_message_handler_##Name##_res =                                                          \
        dolores::add_handler(#Name,                                                                                   \
                             std::make_shared<dolores::Handler<cq::MessageEvent>>(                                    \
                                 __dummy_message_handler_##Name, std::make_shared<dolores::cond::All>(__VA_ARGS__))); \
    static void __dummy_message_handler_##Name(dolores::MessageSession &session)

#define dolores_on_notice(Name, ...)                                                                                 \
    static void __dummy_notice_handler_##Name(dolores::NoticeSession &);                                             \
    static const auto __dummy_notice_handler_##Name##_res =                                                          \
        dolores::add_handler(#Name,                                                                                  \
                             std::make_shared<dolores::Handler<cq::NoticeEvent>>(                                    \
                                 __dummy_notice_handler_##Name, std::make_shared<dolores::cond::All>(__VA_ARGS__))); \
    static void __dummy_notice_handler_##Name(dolores::NoticeSession &session)

#define dolores_on_request(Name, ...)                                                                                 \
    static void __dummy_request_handler_##Name(dolores::RequestSession &);                                            \
    static const auto __dummy_request_handler_##Name##_res =                                                          \
        dolores::add_handler(#Name,                                                                                   \
                             std::make_shared<dolores::Handler<cq::RequestEvent>>(                                    \
                                 __dummy_request_handler_##Name, std::make_shared<dolores::cond::All>(__VA_ARGS__))); \
    static void __dummy_request_handler_##Name(dolores::RequestSession &session)
