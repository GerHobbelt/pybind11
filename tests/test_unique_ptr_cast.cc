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
  virtual ~Test() {
    cout << "Test::~Test()\n";
  }
  virtual int value() const {
    cout << "Test::value()\n";
    return value_;
  }
 private:
  int value_{};
};


// TODO(eric.cousineau): Add converter for `is_base<T, trampoline<T>>`, only for
// `cast` (C++ to Python) to handle swapping lifetime control.

// Trampoline class.
class PyTest : public py::trampoline<Test> {
 public:
  typedef py::trampoline<Test> Base;
  using Base::Base;

  int value() const override {
    PYBIND11_OVERLOAD(int, Test, value);
  }
};

unique_ptr<Test> check_creation(py::function create_obj) {
  // Test getting a pointer.
//  Test* in_test = py::cast<Test*>(obj);

  // Test a terminal pointer.
  cout << "---\n";
  unique_ptr<Test> fin = py::cast<unique_ptr<Test>>(create_obj());
  fin.reset();
  cout << "---\n";

  // Test pass-through.
  py::object obj = create_obj();
  unique_ptr<Test> in = py::cast<unique_ptr<Test>>(std::move(obj));
  return in;
}

PYBIND11_MODULE(_move, m) {
  py::class_<Test, PyTest>(m, "Test")
    .def(py::init<int>())
    .def("value", &Test::value);

  m.def("check_creation", &check_creation);
}

// Export this to get access as we desire.
void custom_init_move(py::module& m) {
  PYBIND11_CONCAT(pybind11_init_, _move)(m);
}

void check_pure_cpp() {
  py::exec(R"(
def create_obj():
    return move.Test(10)
obj = move.check_creation(create_obj)
print(obj.value())
)");
}

void check_py_child() {
  py::exec(R"(
class Child(move.Test):
    def __init__(self, value):
        move.Test.__init__(self, value)
        print("py.Child.Child")
    def __del__(self):
        print("py.Child.__del__")
    def value(self):
        print("py.Child.value")
        return move.Test.value(self)

def create_obj():
    return Child(20)
obj = move.check_creation(create_obj)
print(obj.value())
)");
}

int main() {
  {
    py::scoped_interpreter guard{};

    py::module m("_move");
    custom_init_move(m);
    py::globals()["move"] = m;

//    check_pure_cpp();
    check_py_child();
  }

  cout << "[ Done ]" << endl;

  return 0;
}
