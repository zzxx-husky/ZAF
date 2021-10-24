void (ActorBehavior::*ActorBehavior_send)(const LocalActorHandle&, Message*) =
  &ActorBehavior::send;

class_<ActorBehavior, boost::noncopyable>("ActorBehavior")
  .def("get_actor_system", &ActorBehavior::get_actor_system,
    return_value_policy<reference_existing_object>())
  .def("initialize_actor", &ActorBehavior::initialize_actor)
  .def("send", ActorBehavior_send)
  ;
