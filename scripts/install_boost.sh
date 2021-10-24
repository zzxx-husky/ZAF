scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

BOOST_VERSION=1.70.0
VER=$(echo ${BOOST_VERSION} | tr '.' '_')

if [ ! -z "$1" ]; then
  cd $1
else
  cd ${scriptdir}
fi

if [ ! -d ./boost/install ]; then
  if [ ! -d ./boost ]; then
    wget http://boostorg.jfrog.io/ui/native/main/release/${BOOST_VERSION}/source/boost_${VER}.tar.gz
    tar xf boost_${VER}.tar.gz
    rm boost_${VER}.tar.gz
    mv boost_${VER} boost
  fi
  cd boost
  rm -rf $(pwd)/install/
  ./bootstrap.sh --prefix=$(pwd)/install/ --with-python=$(which python3)
  ./b2 --clean
  ./b2 threading=multi cxxflags="-fPIC" link=shared variant=release install
  cd ..
fi

if [ -z "$(cat ~/.bashrc | grep BOOST_ROOT)" ]; then
  echo "export BOOST_ROOT=$(pwd)/boost/install" >> ~/.bashrc
  echo "export LD_LIBRARY_PATH=\${BOOST_ROOT}/lib:\${LD_LIBRARY_PATH}" >> ~/.bashrc
  echo "export CMAKE_PREFIX_PATH=\${BOOST_ROOT}:\${CMAKE_PREFIX_PATH}" >> ~/.bashrc
  source ~/.bashrc
fi
