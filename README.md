# ZAF: ZeroMQ-based Actor Framework

[![CMake](https://github.com/zzxx-husky/ZAF/actions/workflows/cmake.yml/badge.svg?branch=master)](https://github.com/zzxx-husky/ZAF/actions/workflows/cmake.yml)
[![CodeQL](https://github.com/zzxx-husky/ZAF/actions/workflows/codeql-analysis.yml/badge.svg?branch=master)](https://github.com/zzxx-husky/ZAF/actions/workflows/codeql-analysis.yml)

A ZMQ-based C++ actor framework.

ZAF uses ZMQ Router socket for in-memory communication and ZMQ PUSH/PULL sockets for network communication among actors.
The purpose of ZAF is to support the developement of distributed data processing systems.
The design of ZAF is rather flexible. For examples, one can customize how to run the actors (e.g., one thread for one actor like `ActorSystem`, or multiple actors with a fixed number of threads like `ActorEngine`, or else),
or customize how messages are delivered throught network.

Please check [ZAF wiki](https://github.com/zzxx-husky/ZAF/wiki) for detailed usage.
