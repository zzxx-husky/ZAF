#include "zaf/actor_engine.hpp"
#include "zaf/zaf_exception.hpp"

#include "zmq.hpp"

namespace zaf {
Actor ActorEngine::spawn(ActorBehavior* new_actor) {
  new_actor->initialize_actor(forwarder->get_actor_system(), *this);
  this->inc_num_alive_actors();
  forwarder->send(executors[next_executor++ % num_executors], Executor::NewActor, new_actor);
  return {new_actor->get_local_actor_handle()};
}

void ActorEngine::init_scoped_actor(ActorBehavior& new_actor) {
  return forwarder->get_actor_system().init_scoped_actor(new_actor);
}

void ActorEngine::set_load_diff_ratio(double ratio) {
  this->load_diff_ratio = ratio;
}

void ActorEngine::set_load_rebalance_period(size_t period) {
  this->load_rebalance_period = period;
}

ActorEngine::ActorEngine(ActorSystem& actor_system, size_t num_executors) {
  initialize(actor_system, num_executors);
}

void ActorEngine::initialize(ActorSystem& actor_system, size_t num_executors) {
  if (this->num_executors != 0) {
    throw ZAFException("Attempt to initialize an active ActorEngine.");
  }
  this->num_executors = num_executors;
  this->forwarder = actor_system.create_scoped_actor();
  this->executors.resize(num_executors);
  for (size_t i = 0; i < num_executors; i++) {
    this->executors[i] = actor_system.spawn<Executor>(*this, i);
  }
  { // for load rebalance
    this->load_infos[0].executor_load_counts.resize(num_executors);
    this->load_infos[1].executor_load_counts.resize(num_executors);
    this->current_load_info_index = 0;
    this->load_infos[0].need_load_rebalance = false;
    this->executor_load_indices.resize(num_executors);
    for (size_t i = 0; i < num_executors; i++) {
      this->executor_load_indices[i] = i;
    }
  }
}

void ActorEngine::terminate() {
  for (auto& e : executors) {
    forwarder->send(e, Executor::Termination);
  }
  await_all_actors_done();
  this->forwarder = nullptr;
  executors.clear();
  num_executors = 0;
}

ActorSystem& ActorEngine::get_actor_system() {
  return forwarder->get_actor_system();
}

ActorEngine::~ActorEngine() {
  await_all_actors_done();
  for (auto& e : executors) {
    forwarder->send(e, Executor::Termination);
  }
  this->forwarder = nullptr;
  executors.clear();
  num_executors = 0;
}

// this function is invoked by executor[0]
void ActorEngine::Executor::load_rebalance() {
  auto load_info_index = engine.current_load_info_index;
  // the executors switch to the other load_info
  engine.current_load_info_index ^= 1;
  auto& load = engine.load_infos[load_info_index];
  
  std::sort(engine.executor_load_indices.begin(), engine.executor_load_indices.end(),
    [&](auto i, auto j) {
      return load.executor_load_counts[i] < load.executor_load_counts[j];
    });
  for (int i = 0, j = engine.num_executors - 1; i < j; i++, j--) {
    // diff controls the dist btw the fastest and the slowest executor
    // 1e-6 in case min count is 0
    auto idx_i = engine.executor_load_indices[i];
    auto idx_j = engine.executor_load_indices[j];
    auto diff = (load.executor_load_counts[idx_j] - load.executor_load_counts[idx_i]) / double(load.executor_load_counts[idx_i] + 1e-6);
    if (diff >= engine.load_diff_ratio) {
      // the distance is too large, send an actor from the slowest executor to the fastest one
      this->send(engine.executors[idx_i], Rebalance, engine.executors[idx_j]);
    } else {
      break;
    }
  }
  { // reset
    for (auto& i : load.executor_load_counts) {
      i = 0;
    }
    load.need_load_rebalance = false;
  }
}

ActorEngine::Executor::Executor(ActorEngine& engine, size_t eid):
  engine(engine),
  eid(eid) {
}

MessageHandlers ActorEngine::Executor::behavior() {
  return {
    NewActor - [=](ActorBehavior* new_actor) {
      new_actor->activate();
      new_actor->start();
      if (new_actor->is_activated()) {
        listen_to_actor(new_actor, new_actor->behavior());
      }
    },
    Rebalance - [=](const Actor& peer) {
      if (num_poll_items == 1) {
        return;
      }
      auto i = 1 + rand() % (num_poll_items - 1);
      if (i != num_poll_items - 1) {
        std::swap(actors[i], actors.back());
        std::swap(handlers[i], handlers.back());
        std::swap(poll_items[i], poll_items.back());
      }
      this->send(peer, ActorTransfer, actors.back(), std::move(handlers.back()));
      actors.pop_back();
      handlers.pop_back();
      poll_items.pop_back();
      num_poll_items--;
    },
    ActorTransfer - [=](ActorBehavior* new_actor, MessageHandlers&& handler) {
      listen_to_actor(new_actor, std::move(handler));
    },
    Termination - [=]() {
      this->deactivate();
    }
  };
}

void ActorEngine::Executor::listen_to_actor(ActorBehavior* new_actor, MessageHandlers&& handler) {
  actors.push_back(new_actor);
  handlers.emplace_back(std::move(handler));
  poll_items.emplace_back(zmq::pollitem_t{
    new_actor->get_recv_socket().handle(), 0, ZMQ_POLLIN, 0
  });
  ++num_poll_items;
}

void ActorEngine::Executor::launch() {
  thread::set_name(to_string("ZAF/E", this->get_actor_id()));
  listen_to_actor(this, this->behavior());
  this->activate();
  while (true) {
    // block until receive a message
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int npoll = zmq::poll(poll_items);
#pragma GCC diagnostic pop
    if (npoll == 0) {
      continue;
    }
    for (size_t i = 0; i < num_poll_items; i++) {
      if (poll_items[i].revents & ZMQ_POLLIN) {
        try {
          actors[i]->receive_once(handlers[i]);
        } catch (const std::exception& e) {
          std::cerr << "Exception caught when running an actor at " << __PRETTY_FUNCTION__ << std::endl;
          print_exception(e);
          actors[i]->deactivate();
        } catch (...) {
          std::cerr << "Unknown exception caught when running an actor at " << __PRETTY_FUNCTION__ << std::endl;
          actors[i]->deactivate();
        }

        if (!actors[i]->is_activated() && i != 0) {
          actors[i]->stop();
          engine.dec_num_alive_actors();
          delete actors[i];
          actors[i] = actors.back();
          actors.pop_back();
          // using `handlers[i] = std::move(handlers.back());` calls seg fault in malloc/new
          std::swap(handlers[i], handlers.back());
          handlers.pop_back();
          poll_items[i] = poll_items.back();
          poll_items.pop_back();
          num_poll_items--;
          i--;
        }
      }
    }
    if (!this->is_activated()) {
      break;
    }
    if (engine.load_rebalance_period != 0) {
      // one for loop above = one round
      // smaller round = slower, larger round = faster
      auto& load = engine.load_infos[engine.current_load_info_index];
      if (++load.executor_load_counts[this->eid] == engine.load_rebalance_period) {
        load.need_load_rebalance = true;
      }
      if (eid == 0 && load.need_load_rebalance) {
        load_rebalance();
      }
    }
  }
  // stop
  for (size_t i = 1, n = actors.size(); i < n; i++) {
    actors[i]->stop();
    engine.dec_num_alive_actors();
    delete actors[i];
  }
  actors.clear();
  poll_items.clear();
  handlers.clear();
  num_poll_items = 0;
}
} // namespace zaf
