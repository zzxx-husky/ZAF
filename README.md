# ZAF: ZeroMQ-based Actor Framework

A ZMQ-based C++ actor framework.

1. Use ZMQ Router socket for communication among actors.
2. Due to 1, an actor can send messages to any other as long as it knows the actor id or obtains an Actor object of the other actor.
3. Each actor is either managed by a thread (default impl) or managed by an ActorEngine which uses multiple threads to process multiple actors.

## TODOs

1. Current implementation is limited to communication among local threads. Add I/O actors to support communication across machines.
2. Runtime type check on the message elements and the message handler arguments.
3. Enable task stealing in ActorEngine.

## Notes

1. During the message delivery, the types of the message elements are erased. Instead of storing the type information in the message, the types are recovered using the types of the arguments of the message handler. Therefore, message handler are not allowed to be a lambda with `auto` argument or a functor with multiple `operator()`. Also, the types of the message elements must match exactly with the types of the message handler arguments.
