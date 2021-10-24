#include "zaf/actor.hpp"
#include "zaf/actor_behavior.hpp"
#include "zaf/actor_group.hpp"
#include "zaf/actor_system.hpp"

#include "boost/python.hpp"

namespace zaf {

BOOST_PYTHON_MODULE(pyzaf) {

using namespace boost::python;

#include "python/actor.hpp"
#include "python/actor_behavior.hpp"
#include "python/actor_group.hpp"
#include "python/actor_system.hpp"

}

} // namespace zaf

