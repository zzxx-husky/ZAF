#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "actor.hpp"
#include "actor_behavior.hpp"
#include "actor_group.hpp"
#include "scoped_actor.hpp"
#include "thread_utils.hpp"

#include "zmq.hpp"

namespace zaf {
class ActorSystem : public ActorGroup {
public:
  ActorSystem() = default;

  ActorSystem(const ActorSystem&) = delete;
  ActorSystem& operator=(const ActorSystem&) = delete;

  // The reason ActorSystem is not movable is becuz ActorBehavior keeps a reference to ActorSystem.
  // The reference in ActorBehavior will not be updated if ActorSystem is moved.
  // ActorSystem(ActorSystem&&);
  // ActorSystem& operator=(ActorSystem&&);

  void init_scoped_actor(ActorBehavior&) override;

  using ActorGroup::spawn;
  Actor spawn(ActorBehavior* new_actor) override;

  template<typename Callable,
    typename Signature = traits::is_callable<Callable>,
    typename std::enable_if_t<Signature::value>* = nullptr,
    typename std::enable_if_t<Signature::args_t::size == 1>* = nullptr,
    typename ActorType = typename Signature::args_t::template decay_arg_t<0>,
    typename std::enable_if_t<std::is_base_of_v<ActorBehavior, ActorType>>* = nullptr>
  Actor spawn(Callable&& callable) {
    auto new_actor = new ActorType();
    new_actor->initialize_actor(*this, *this);
    std::thread([run = std::forward<Callable>(callable), new_actor]() mutable {
      thread::set_name(new_actor->get_name());
      try {
        run(*new_actor);
      } catch (const std::exception& e) {
        std::cerr << "Exception caught when running an actor at " << __PRETTY_FUNCTION__ << std::endl;
        print_exception(e);
      } catch (...) {
        std::cerr << "Unknown exception caught when running an actor at " << __PRETTY_FUNCTION__ << std::endl;
      }
      delete new_actor;
    }).detach();
    return {new_actor->get_local_actor_handle()};
  }

  zmq::context_t& get_zmq_context();

  ActorIdType get_next_available_actor_id();

  void set_identifier(const std::string&);

  const std::string& get_identifier() const;

  ~ActorSystem();

private:
  std::atomic<size_t> num_alive_actors{0};
  std::condition_variable all_actors_done_cv;
  std::mutex all_actors_done_mutex;

  // Available actor id starts from 1. Actor id 0 means null actor.
  std::atomic<ActorIdType> next_available_actor_id{1};

  // identifier should be different when communicating with other ActorSystems
  std::string identifier = "zaf";

  zmq::context_t zmq_context;
};
} // namespace zaf
