scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

GTEST_VERSION=release-1.10.0

if [ ! -z "$1" ]; then
  cd $1
else
  cd ${scriptdir}
fi

if [ ! -d ./googletest/googletest/build ]; then
  if [ ! -d ./googletest ]; then
    git clone --depth 1 http://github.com/google/googletest --branch ${GTEST_VERSION}
  fi
  cd googletest/googletest
  mkdir build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=11 && make all -j4 && cd ..
  mkdir lib
  if [ -d ./build/lib ]; then
    cp build/lib/*.a ./lib
  else
    cp build/*.a ./lib
  fi
  cd ../..
fi
if [ -z "$(cat ~/.bashrc | grep GTEST_ROOT)" ]; then
  echo "export GTEST_ROOT=$(pwd)/googletest/googletest" >> ~/.bashrc
  echo "export LD_LIBRARY_PATH=\${GTEST_ROOT}/lib:\${LD_LIBRARY_PATH}" >> ~/.bashrc
  echo "export CMAKE_PREFIX_PATH=\${GTEST_ROOT}:\${CMAKE_PREFIX_PATH}" >> ~/.bashrc
fi
