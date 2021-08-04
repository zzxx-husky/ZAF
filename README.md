# ZAF: ZeroMQ-based Actor Framework

A ZMQ-based C++ actor framework.

1. Use ZMQ Router socket for communication among actors.
2. Due to 1, an actor can send messages to any other as long as it knows the actor id or obtains an Actor object of the other actor.
3. Each actor is either managed by a thread (default impl) or managed by an ActorEngine which uses multiple threads to process multiple actors.

## Usage

The design concepts of ActorSystem, Actor, MessageHandlers are borrowed from [CAF](https://github.com/actor-framework/actor-framework).

1. Construct an `ActorSystem`, where an `ActorSystem` contains a `zmq::context`:
```c++
zaf::ActorSystem actor_sys;
```

2. (Optional) Construct an `ActorEngine`, which creates multiple threads to manage multiple actors:
```c++
// Specify the actor system and the number of threads
zaf::ActorEngine engine{actor_sys, 4};
```

3. Define and create actors:

We can define an actor by inherting `zaf::ActorBehavior`.

```c++
struct A : public zaf::ActorBehavior {
  // `behavior()` returns a collection of message handlers
  zaf::MessageHandlers behavior() override {
    return {
      // We define multiple message handlers here.
      // Each message handler is a `code -> message content handler` mapping by `operator -`.
      // The message codes of different message handlers must be different.
      // The message content handler can be a lambda, a functor or a function
      zaf::Code{1} - [](std::string& ping) {
        // The message code is 1 and the handler receives a `std::string` "ping".
        // We reply the "pong" back to the sender
        this->reply(zaf::Code{10}, std::string("pong"));
        // Or we can send the same message to the current sender actor.
        // this->send(this->get_current_sender_actor(), 10, std::string("pong"));

        // Call `deactivate` to stop this actor if no more message to receive
        this->deactivate();
      }
    };
  }

  void start() override {
    // Actions to take after the construction of the actor and before receiving any message, automatically called by ZAF
  }

  void stop() override {
    // Actions to take after calling `deactivate` before the destruction of the actor, automatically called by ZAF
  }
};

int main() {
  // We spawn the actor of type `A` by an actor system
  // The new actor will run immediately using an isolated thread.
  // `spawn` returns a actor handler which can be used to send messages to the new actor
  zaf::Actor a = actor_sys.spawn<A>();

  // We spawn the actor of type `A` by an actor engine
  // The new actor will be managed by the threads inside the engine.
  zaf::Actor a = engine.spawn<A>();
}
```

We can also create an actor by giving a lambda that processes an `zaf::ActorBehavior` object.
```c++
zaf::Actor x = actor_sys.spawn<A>();

// An isolated thread is created to execute the lambda.
// Lambda-based actor creation is available in ActorSystem only.
actor_sys.spawn([&](zaf::ActorBehavior& actor) {
  // Send a `std::string` "ping" to actor `x`
  // When sending a message, the message content must match exactly with the arguments of the message handler in the receiver
  actor.send(x, 1, std::string("ping"));
  // Receive one message from any actor
  actor.receive_once({
    zaf::Code{2} - [](const std::string& pong) { ...  }
  });
  // Receive multiple messages until `actor.deactivate()` is called.
  actor.receive({
    zaf::Code{101} - [](...) { ... },
    zaf::Code{102} - [](...) { ... },
    ...
  });
  // The actor stops when this lambda finishes.
});
```

We can choose to wait for all actors to be done before taking any action.
The `await_all_actors_done` function is called inside the destructor of ActorSystem and ActorEngine.
```c++
actor_sys.await_all_actors_done();
engine.await_all_actors_done();
```

## TODOs

1. Current implementation is limited to communication among local threads. Add I/O actors to support communication across machines.
2. Runtime type check on the message elements and the message handler arguments.
3. Add necessary info (in this README or in wiki) to introduce how to use this framework.

## Notes

1. During the message delivery, the types of the message elements are erased. Instead of storing the type information in the message, the types are recovered using the types of the arguments of the message handler. Therefore, message handler are not allowed to be a lambda with `auto` argument or a functor with multiple `operator()`. Also, the types of the message elements must match exactly with the types of the message handler arguments.
