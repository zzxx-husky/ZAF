ScopedActor<ActorBehavior> (ActorGroup::*ActorGroup_create_scoped_actor)() =
  &ActorGroup::create_scoped_actor;

Actor (ActorGroup::*ActorGroup_spawn)(ActorBehavior*) =
  &ActorGroup::spawn;

struct ActorGroupWrapper : ActorGroup, wrapper<ActorGroup> {
  Actor spawn(ActorBehavior* a) {
    return this->get_override("spawn")(a);
  }

  void init_scoped_actor(ActorBehavior& a) {
    this->get_override("init_scoped_actor")(a);
  }
};

class_<ActorGroupWrapper, boost::noncopyable>("ActorGroup")
  .def("create_scoped_actor", ActorGroup_create_scoped_actor)
  .def("spawn", pure_virtual(ActorGroup_spawn))
  .def("init_scoped_actor", pure_virtual(&ActorGroup::init_scoped_actor))
  .def("await_alive_actors_done", &ActorGroup::await_alive_actors_done)
  .def("await_all_actors_done", &ActorGroup::await_all_actors_done)
  ;
