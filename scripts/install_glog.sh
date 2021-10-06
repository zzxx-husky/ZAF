scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

GLOG_VERSION=v0.4.0

if [ ! -z "$1" ]; then
  cd $1
else
  cd ${scriptdir}
fi

if [ ! -d ./glog/install ]; then
  if [ ! -d ./glog ]; then
    git clone --depth 1 http://github.com/google/glog --branch ${GLOG_VERSION}
  fi
  cd glog
  # require autoconf && libtool
  ./autogen.sh && ./configure --prefix=$(pwd)/install && make install -j4
  cd ..
fi
if [ -z "$(cat ~/.bashrc | grep GLOG_ROOT)" ]; then
  echo "export GLOG_ROOT=$(pwd)/glog/install" >> ~/.bashrc
  echo "export LD_LIBRARY_PATH=\${GLOG_ROOT}/lib:\${LD_LIBRARY_PATH}" >> ~/.bashrc
  echo "export CMAKE_PREFIX_PATH=\${GLOG_ROOT}:\${CMAKE_PREFIX_PATH}" >> ~/.bashrc
fi
