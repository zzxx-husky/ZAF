scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

PHMAP_VERSION=1.30

cd ${scriptdir}

if [ ! -d ./parallel-hashmap ]; then
  git clone --depth 1 http://github.com/greg7mdp/parallel-hashmap --branch ${PHMAP_VERSION}
fi
if [ -z "$(cat ~/.bashrc | grep PHMAP_ROOT)" ]; then
  echo "export PHMAP_ROOT=$(pwd)/parallel-hashmap" >> ~/.bashrc
  echo "export CMAKE_PREFIX_PATH=\${PHMAP_ROOT}:\${CMAKE_PREFIX_PATH}" >> ~/.bashrc
fi
