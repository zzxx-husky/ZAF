class_<LocalActorHandle>("LocalActorHandle", "A handle to a local actor object", init<>())
  .def(init<ActorIdType, bool>())
  .def("__hash__", &LocalActorHandle::hash_code)
  .def("__eq__", &LocalActorHandle::operator==)
  .def_readwrite("local_actor_id", &LocalActorHandle::local_actor_id)
  .def_readwrite("use_swsr_msg_delivery", &LocalActorHandle::use_swsr_msg_delivery)
  ;

class_<NetSenderInfo>("NetSenderInfo", "Information of a sender in a NetGate", init<>())
  .def_readwrite("net_sender", &NetSenderInfo::net_sender)
  .def_readwrite("local_net_gate_url", &NetSenderInfo::local_net_gate_url)
  .def_readwrite("remote_net_gate_url", &NetSenderInfo::remote_net_gate_url)
  ;

class_<RemoteActorHandle>("RemoteActorHandle", "A handle to a remote actor object", init<>())
  .def(init<NetSenderInfo, LocalActorHandle>())
  .def("__hash__", &RemoteActorHandle::hash_code)
  .def("__eq__", &RemoteActorHandle::operator==)
  .add_property("net_sender_info", make_function(
    [](const RemoteActorHandle& r) -> const NetSenderInfo& {
      return *r.net_sender_info;
    },
    return_value_policy<copy_const_reference>(),
    // explicitly tells the return type and the arg types
    boost::mpl::vector<const NetSenderInfo&, const RemoteActorHandle&>()))
  .def_readwrite("remote_actor", &RemoteActorHandle::remote_actor)
  ;

using ActorHandle = std::variant<LocalActorHandle, RemoteActorHandle>;
struct ActorHandleToPyObj {
  static PyObject* convert(const ActorHandle& x) {
    return std::visit(
      [](const auto& l) {
        return to_python_value<decltype(l)>()(l);
      },
      x);
  }
};
to_python_converter<ActorHandle, ActorHandleToPyObj>();
implicitly_convertible<LocalActorHandle, ActorHandle>();
implicitly_convertible<RemoteActorHandle, ActorHandle>();

class_<Actor>("Actor", "A handle used to send messages to an actor object", init<>())
  .def(init<LocalActorHandle>())
  .def(init<RemoteActorHandle>())
  .def("__hash__", &Actor::hash_code)
  .def("__eq__", &Actor::operator==)
  .def("is_remote", &Actor::is_remote)
  .def("is_local", &Actor::is_local)
  .def_readwrite("handle", &Actor::handle)
  ;
