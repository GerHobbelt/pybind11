// Purpose: Test what avenues might be possible for creating instances in Python
// to then be owned in C++.

#include <cstddef>
#include <cmath>
#include <sstream>
#include <string>

#include <pybind11/cast.h>
#include <pybind11/embed.h>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace py::literals;
using namespace std;

class Test {
 public:
  Test(int value)
      : value_(value) {
    cout << "Test::Test(int)\n";
  }
  ~Test() {
    cout << "Test::~Test()\n";
  }
  int value() const { return value_; }
 private:
  int value_{};
};

unique_ptr<Test> check_creation(py::function create_obj) {
  py::object obj = create_obj();
  unique_ptr<Test> in = py::cast<unique_ptr<Test>>(std::move(obj));
  return in;
}

PYBIND11_MODULE(_move, m) {
  py::class_<Test>(m, "Test")
    .def(py::init<int>())
    .def("value", &Test::value);

  m.def("check_creation", &check_creation);
}

// Export this to get access as we desire.
void custom_init_move(py::module& m) {
  PYBIND11_CONCAT(pybind11_init_, _move)(m);
}

int main() {
  py::scoped_interpreter guard{};

  py::module m("_move");
  custom_init_move(m);

  py::dict globals = py::globals();
  globals["move"] = m;

  py::exec(R"(
def create_obj():
    return move.Test(10)

obj = move.check_creation(create_obj)
print(obj.value())
)", py::globals());

  cout << "Done" << endl;

  return 0;
}
