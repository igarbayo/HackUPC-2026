#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "simulation.h"

namespace py = pybind11;

PYBIND11_MODULE(scheduler_cpp, m) {
    m.doc() = "C++ warehouse scheduler";

    py::class_<Box>(m, "Box")
        .def(py::init<BoxId, Family, Tick>(),
             py::arg("id"), py::arg("family"), py::arg("arrival_tick") = 0)
        .def_property_readonly("id",           &Box::id)
        .def_property_readonly("family",       &Box::family)
        .def_property_readonly("arrival_tick", &Box::arrivalTick);

    py::class_<Event>(m, "Event")
        .def_readonly("type",         &Event::type)
        .def_readonly("logical_time", &Event::logical_time)
        .def_readonly("box_id",       &Event::box_id)
        .def_readonly("pallet_id",    &Event::pallet_id)
        .def_readonly("family",       &Event::family);

    py::class_<Params>(m, "Params")
        .def(py::init<>())
        .def_readwrite("boxes",        &Params::boxes)
        .def_readwrite("num_slots",    &Params::num_slots)
        .def_readwrite("num_shuttles", &Params::num_shuttles)
        .def_readwrite("max_ticks",    &Params::max_ticks);

    m.def("run_simulation", &run_simulation,
          py::arg("params"),
          py::call_guard<py::gil_scoped_release>(),
          "Run full simulation; returns list of Event objects");
}
