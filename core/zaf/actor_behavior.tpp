namespace zaf {
template<typename ... ArgT>
void ActorBehavior::send(const Actor& receiver, size_t code, Message::Type type, ArgT&& ... args) {
  if (!receiver) {
    return; // ignore message sent to null actor
  }
  receiver.visit(overloaded {
    [&](const LocalActorHandle& r) {
      auto message = make_message(LocalActorHandle{this->get_actor_id()}, code, std::forward<ArgT>(args)...);
      message->set_type(type);
      this->send(r, message);
    },
    [&](const RemoteActorHandle& r) {
      if constexpr (traits::all_serializable<ArgT ...>::value) {
        std::vector<char> bytes;
        Serializer(bytes)
          .write(this->get_actor_id())
          .write(r.remote_actor_id)
          .write(code)
          .write(type)
          .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
          .write(std::forward<ArgT>(args) ...);
        // Have to copy the bytes here because we do not know how many bytes
        // are needed for serialization and we need to reserve more than necessary.
        this->send(r.net_sender_info->id, DefaultCodes::ForwardMessage, Message::Type::Normal,
          zmq::message_t{&bytes.front(), bytes.size()});
      } else {
        throw ZAFException("Attempt to serialize non-serializable message data.");
      }
    }
  });
}

// timeout == 0, non-blocking
// timeout == -1, block until receiving a message
// timeout > 0, wait for specified timeout or until receiving a message
template<typename Callback,
  typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>* = nullptr>
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
  typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>* = nullptr>
bool ActorBehavior::inner_receive_once(Callback&& callback, long timeout) {
  if (!waiting_for_response && !pending_messages.empty()) {
    callback(pending_messages.front());
    pending_messages.pop_front();
    return true;
  }
  zmq::pollitem_t item[1] = {
    {recv_socket.handle(), 0, ZMQ_POLLIN, 0}
  };
  try {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int npoll = zmq::poll(item, 1, timeout);
#pragma GCC diagnostic pop
    if (npoll == 0) {
      return false;
    }
    zmq::message_t message_ptr;
    // receive current sender routing id
    if (!recv_socket.recv(message_ptr)) {
      throw ZAFException("Expect to receive a message but actually receive nothing.");
    }
    if (!recv_socket.recv(message_ptr)) {
      throw ZAFException(
        "Failed to receive a message after receiving an routing id at ", __PRETTY_FUNCTION__
      );
    }
    callback(*reinterpret_cast<Message**>(message_ptr.data()));
    return true;
  } catch (...) {
    std::throw_with_nested(ZAFException(
      "Exception caught in ", __PRETTY_FUNCTION__, " in actor ", this->actor_id
    ));
  }
}

template<typename Rep, typename Period, typename ... ArgT>
void ActorBehavior::delayed_send(const std::chrono::duration<Rep, Period>& delay, const Actor& receiver,
  size_t code, Message::Type type, ArgT&& ... args) {
  if (!receiver) {
    return;
  }
  auto now = std::chrono::steady_clock::now();
  auto send_time = now + delay;
  receiver.visit(overloaded{
    [&](const LocalActorHandle& r) {
      auto m = make_message(LocalActorHandle{this->get_actor_id()}, code, std::forward<ArgT>(args)...);
      m->set_type(type);
      delayed_messages.emplace(send_time, DelayedMessage(receiver, m));
    },
    [&](const RemoteActorHandle& r) {
      if constexpr (traits::all_serializable<ArgT ...>::value) {
        std::vector<char> bytes;
        Serializer(bytes)
          .write(this->get_actor_id())
          .write(r.remote_actor_id)
          .write(code)
          .write(type)
          .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
          .write(std::forward<ArgT>(args) ...);
        delayed_messages.emplace(send_time, DelayedMessage(
          receiver,
          zmq::message_t{&bytes.front(), bytes.size()}
        ));
      } else {
        throw ZAFException("Attempt to serialize non-serializable data");
      }
    }
  });
}
} // namespace zaf
