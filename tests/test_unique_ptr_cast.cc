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

class Child : public Test {
 public:
  Child(int value)
     : Test(value) {}
  ~Child() {
    cout << "Child::~Child()\n";
  }
  int value() const override {
    cout << "Child::value()\n";
    return 10 * Test::value();
  }
};

// TODO(eric.cousineau): Add converter for `is_base<T, trampoline<T>>`, only for
// `cast` (C++ to Python) to handle swapping lifetime control.

// Trampoline class.
class PyTest : public py::trampoline<Test> {
 public:
  typedef py::trampoline<Test> Base;
  using Base::Base;
  ~PyTest() {
    cout << "PyTest::~PyTest()" << endl;
  }
  int value() const override {
    PYBIND11_OVERLOAD(int, Test, value);
  }
};
class PyCppChild : public py::trampoline<Child> {
 public:
  typedef py::trampoline<Child> Base;
  using Base::Base;
  ~PyCppChild() {
    cout << "PyCppChild::~PyCppChild()" << endl;
  }
  int value() const override {
    PYBIND11_OVERLOAD(int, Child, value);
  }
};

unique_ptr<Test> check_creation(py::function create_obj) {
  // Test getting a pointer.
//  Test* in_test = py::cast<Test*>(obj);

  // Test a terminal pointer.
  // NOTE: This yields a different destructor order.
  // However, the trampoline class destructors should NOT interfere with nominal
  // Python destruction.
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

  py::class_<Child, PyCppChild, Test>(m, "Child")
      .def(py::init<int>())
      .def("value", &Child::value);

  m.def("check_creation", &check_creation);

  auto mdict = m.attr("__dict__");
  py::exec(R"(
class PyExtTest(Test):
    def __init__(self, value):
        Test.__init__(self, value)
        print("PyExtTest.PyExtTest")
    def __del__(self):
        print("PyExtTest.__del__")
    def value(self):
        print("PyExtTest.value")
        return Test.value(self)

class PyExtChild(Child):
    def __init__(self, value):
        Child.__init__(self, value)
        print("PyExtChild.PyExtChild")
    def __del__(self):
        print("PyExtChild.__del__")
    def value(self):
        print("PyExtChild.value")
        return Child.value(self)
)", mdict, mdict);
}

// Export this to get access as we desire.
void custom_init_move(py::module& m) {
  PYBIND11_CONCAT(pybind11_init_, _move)(m);
}

void check_pure_cpp() {
  cout << "\n[ check_pure_cpp ]\n";
  py::exec(R"(
def create_obj():
    return move.Test(10)
obj = move.check_creation(create_obj)
print(obj.value())
)");
}

void check_py_child() {
  // Check ownership for a Python-extended C++ class.
  cout << "\n[ check_py_child ]\n";
  py::exec(R"(
def create_obj():
    return move.PyExtTest(20)
obj = move.check_creation(create_obj)
print(obj.value())
)");
}

void check_casting() {
  // Check a class which, in C++, derives from the direct type, but not the alias.
  cout << "\n[ check_casting ]\n";
  py::exec(R"(
def create_obj():
    return move.PyExtChild(30)
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
//    check_py_child();
    check_casting();
  }

  cout << "[ Done ]" << endl;

  return 0;
}
