Actor (ActorSystem::*ActorSystem_spawn)(ActorBehavior*) =
  &ActorSystem::spawn;

class_<ActorSystem, bases<ActorGroup>, boost::noncopyable>("ActorSystem", "ActorSystem spawns actors.")
  .def("spawn", ActorSystem_spawn)
  ;
