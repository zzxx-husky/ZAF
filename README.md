# ZAF: ZeroMQ-based Actor Framework

[![CMake](https://github.com/zzxx-husky/ZAF/actions/workflows/cmake.yml/badge.svg?branch=master)](https://github.com/zzxx-husky/ZAF/actions/workflows/cmake.yml)
[![CodeQL](https://github.com/zzxx-husky/ZAF/actions/workflows/codeql-analysis.yml/badge.svg?branch=master)](https://github.com/zzxx-husky/ZAF/actions/workflows/codeql-analysis.yml)

A ZMQ-based C++ actor framework.

ZAF is designed to support the developement of distributed data processing systems for high performance.

1. ZAF uses ZeroMQ for underlying message delivery, i.e., Router socket for in-memory communication and ZMQ PUSH/PULL sockets for network communication among actors.

2. The design of ZAF is flexible so that one can customize the parts that are critical to the performance.
For examples, one can customize how to actors are run (e.g., one thread for one actor like `ActorSystem`, or multiple actors with a fixed number of threads like `ActorEngine`),
or customize the NetGate to control the messages delivery throught network.

Please visit [ZAF wiki](https://github.com/zzxx-husky/ZAF/wiki) for detailed usage.
