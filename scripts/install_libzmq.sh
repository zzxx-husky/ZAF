scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

ZMQ_VERSION=v4.3.4
CPPZMQ_VERSION=v4.7.1

cd ${scriptdir}

if [ ! -d ./libzmq/install ]; then
  if [ ! -d ./libzmq ]; then
    git clone --depth 1 http://github.com/zeromq/libzmq --branch ${ZMQ_VERSION}
  fi
  cd libzmq
  mkdir build
  cd build
  cmake .. -DWITH_PERF_TOOL=ON -DZMQ_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(pwd)/../install && make -j4 install
  cd ../..
fi
if [ ! -d ./cppzmq ]; then
  git clone --depth 1 http://github.com/zeromq/cppzmq --branch ${CPPZMQ_VERSION}
  cp cppzmq/zmq.hpp libzmq/install/include
  cp cppzmq/zmq_addon.hpp libzmq/install/include
fi
if [ -z "$(cat ~/.bashrc | grep ZMQ_ROOT)" ]; then
  echo "export ZMQ_ROOT=$(pwd)/libzmq/install" >> ~/.bashrc
  echo "export LD_LIBRARY_PATH=\${ZMQ_ROOT}/lib:\${LD_LIBRARY_PATH}" >> ~/.bashrc
  echo "export CMAKE_PREFIX_PATH=\${ZMQ_ROOT}:\${CMAKE_PREFIX_PATH}" >> ~/.bashrc
  source ~/.bashrc
fi


