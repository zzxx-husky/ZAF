namespace zaf {
template<typename ... ArgT>
void ActorBehavior::reply(const Message& msg, Code code, ArgT&& ... args) {
  if (msg.get_body().get_code() == DefaultCodes::Request) {
    this->send(msg.get_sender(), DefaultCodes::Response,
      get_request_id(msg), std::unique_ptr<MessageBody>(
        new_message(code, std::forward<ArgT>(args) ...)
      ));
  } else {
    this->send(msg.get_sender(), code, std::forward<ArgT>(args)...);
  }
}

template<typename ... ArgT>
void ActorBehavior::send(const Actor& receiver, Code code, ArgT&& ... args) {
  if (!receiver) {
    return; // ignore message sent to null actor
  }
  receiver.visit(overloaded {
    [&](const LocalActorHandle& r) {
      auto message = new_message(Actor{this->get_local_actor_handle()},
        code, std::forward<ArgT>(args)...);
      this->send(r, message);
    },
    [&](const RemoteActorHandle& r) {
      if constexpr (traits::all_serializable<ArgT ...>::value) {
        auto bytes = MessageBytes::make(this->get_local_actor_handle(),
          r.remote_actor, code, std::forward<ArgT>(args) ...);
        this->send(r.net_sender_info->net_sender,
          DefaultCodes::ForwardMessage, std::move(bytes));
      } else {
        throw ZAFException("Attempt to serialize non-serializable message data: ",
          traits::NonSerializableAnalyzer<ArgT ...>::to_string());
      }
    }
  });
}

// timeout == 0, non-blocking
// timeout == -1, block until receiving a message
// timeout > 0, wait for specified timeout or until receiving a message
template<typename Callback,
  typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>*>
bool ActorBehavior::receive_once(Callback&& callback, long timeout) {
  timeout = std::max(timeout, long(-1));
  switch (timeout) {
    case -1: { // block until receiving a message
      while (true) {
        auto delayed_timeout = this->remaining_time_to_next_delayed_message();
        if (delayed_timeout) {
          auto t = std::max(static_cast<long>(delayed_timeout->count()), long(0));
          auto ret = inner_receive_once(std::forward<Callback>(callback), t);
          this->flush_delayed_messages();
          if (ret) { // return until a message is received
            return ret;
          }
        } else {
          break;
        }
      }
      // no more delayed message, return until a message is received
      return inner_receive_once(std::forward<Callback>(callback), timeout);
    }
    case 0: { // return if no message
      // try to receive a message and flush delayed message if any is ready
      auto ret = inner_receive_once(std::forward<Callback>(callback), 0);
      this->flush_delayed_messages();
      return ret;
    }
    default: { // wait for speicified timeout
      auto e = std::chrono::steady_clock::now() + std::chrono::milliseconds{timeout};
      while (true) {
        auto delayed_timeout = this->remaining_time_to_next_delayed_message();
        if (delayed_timeout) {
          auto t = std::min(
            std::max(static_cast<long>(delayed_timeout->count()), long(0)),
            timeout);
          auto ret = inner_receive_once(std::forward<Callback>(callback), t);
          this->flush_delayed_messages();
          if (ret) { // return because one message is received
            return ret;
          }
          timeout = (e - std::chrono::steady_clock::now()).count();
          if (timeout <= 0) { // return because receive timeout is reached
            return ret;
          }
        } else {
          break;
        }
      }
      return inner_receive_once(std::forward<Callback>(callback), timeout);
    }
  }
}

template<typename Callback,
  typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>*>
bool ActorBehavior::inner_receive_once(Callback&& callback, long timeout) {
  if (!waiting_for_response && !pending_messages.empty()) {
    callback(pending_messages.front());
    pending_messages.pop_front();
    return true;
  }
  process_recv_poll_reqs();
  try {
    // if failed to receive or receive nothing, return
    if (int npoll = 0; !try_receive_guard([&]() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      npoll = zmq::poll(&recv_poll_items.front(), recv_poll_items.size(), timeout);
#pragma GCC diagnostic pop
    }) || npoll == 0) {
      return false;
    }
    if (recv_poll_items[0].revents & ZMQ_POLLIN) {
      zmq::message_t sender_routing_id;
      receive_guard([&]() {
        // receive current sender routing id
        if (!recv_socket.recv(sender_routing_id)) {
          throw ZAFException(
            "Expect to receive a message but actually received nothing.");
        }
      });
      zmq::message_t message_ptr;
      receive_guard([&]() {
        if (!recv_socket.recv(message_ptr)) {
          throw ZAFException(
            "Failed to receive a message after having received an routing id at ",
            __PRETTY_FUNCTION__
          );
        }
        callback(*reinterpret_cast<Message**>(message_ptr.data()));
      });
    }
    for (int i = 1, n = recv_poll_items.size(); i < n; i++) {
      if (recv_poll_items[i].revents & ZMQ_POLLIN) {
        recv_poll_callbacks[i]();
      }
    }
    return true;
  } catch (...) {
    std::throw_with_nested(ZAFException(
      "Exception caught in ", __PRETTY_FUNCTION__, " in actor ", this->actor_id
    ));
  }
}

template<typename Rep, typename Period, typename ... ArgT>
void ActorBehavior::delayed_send(const std::chrono::duration<Rep, Period>& delay,
  const Actor& receiver, Code code, ArgT&& ... args) {
  if (!receiver) {
    return;
  }
  auto now = std::chrono::steady_clock::now();
  auto send_time = now + delay;
  receiver.visit(overloaded{
    [&](const LocalActorHandle&) {
      auto m = new_message(Actor{this->get_local_actor_handle()},
        code, std::forward<ArgT>(args)...);
      delayed_messages.emplace(send_time, DelayedMessage(receiver, m));
    },
    [&](const RemoteActorHandle& r) {
      if constexpr (traits::all_serializable<ArgT ...>::value) {
        delayed_messages.emplace(send_time, DelayedMessage(
          receiver,
          MessageBytes::make(this->get_local_actor_handle(),
            r.remote_actor, code, std::forward<ArgT>(args) ...)
        ));
      } else {
        throw ZAFException("Attempt to serialize non-serializable data: ",
          traits::NonSerializableAnalyzer<ArgT ...>::to_string());
      }
    }
  });
}
} // namespace zaf
