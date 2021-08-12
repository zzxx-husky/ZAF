#pragma once

#include <exception>
#include <stdexcept>

#include "actor.hpp"
#include "actor_behavior.hpp"
#include "actor_group.hpp"
#include "actor_system.hpp"
#include "scoped_actor.hpp"

namespace zaf {
class ActorEngine : public ActorGroup {
public:
  ActorEngine() = default;

  ActorEngine(ActorSystem& actor_system, size_t num_executors);

  void initialize(ActorSystem& actor_system, size_t num_executors);

  using ActorGroup::spawn;
  Actor spawn(ActorBehavior* new_actor) override;
  void init_scoped_actor(ActorBehavior&) override;

  void set_load_diff_ratio(double ratio);
  void set_load_rebalance_period(size_t period);

  void terminate();

  ActorSystem& get_actor_system();

  ~ActorEngine();

private:
  class Executor : public ActorBehavior {
  public:
    inline const static Code NewActor{0};
    inline const static Code Termination{1};
    inline const static Code Rebalance{2};
    inline const static Code ActorTransfer{3};

    Executor(ActorEngine& engine, size_t eid);

    MessageHandlers behavior() override;

    void listen_to_actor(ActorBehavior* new_actor, MessageHandlers&& handler);

    void launch() override;

    void load_rebalance();

  private:
    ActorEngine& engine;
    std::vector<ActorBehavior*> actors;       // actors[0] will be `this`
    std::vector<MessageHandlers> handlers;    // handlers[0] will be `this->behave()`
    std::vector<zmq::pollitem_t> poll_items;  // poll_items[0] will be the poll item of this
    size_t num_poll_items = 0;
    const size_t eid;
  };

  size_t num_executors = 0, next_executor = 0;
  std::vector<Actor> executors;
  ScopedActor<ActorBehavior> forwarder;

  // for load rebalance
  struct {
    std::vector<size_t> executor_load_counts;
    bool need_load_rebalance = false;
  } load_infos[2];
  bool current_load_info_index = false;
  std::vector<size_t> executor_load_indices;
  double load_diff_ratio = 0.5;
  // larger period = longer time for a rebalance
  // smaller period = previous ActorTransfer may not take effect when a new ActorTransfer is issued.
  size_t load_rebalance_period = 0; // 0 means no load rebalance
};
} // namespace zaf
