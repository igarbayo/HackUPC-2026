#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "simulation.h"
#include "InputBelt.h"

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
        .def_readwrite("boxes",     &Params::boxes)
        .def_readwrite("num_slots", &Params::num_slots)
        .def_readwrite("num_y",     &Params::num_y)
        .def_readwrite("num_sides", &Params::num_sides)
        .def_readwrite("max_ticks", &Params::max_ticks);

    m.def("run_simulation", &run_simulation,
          py::arg("params"),
          py::call_guard<py::gil_scoped_release>(),
          "Run full simulation; returns list of Event objects");

    // --- box generation ---

    py::class_<DemandPeriod>(m, "DemandPeriod")
        .def(py::init<>())
        .def_readwrite("from_tick",        &DemandPeriod::from_tick)
        .def_readwrite("to_tick",          &DemandPeriod::to_tick)
        .def_readwrite("rate_multiplier",  &DemandPeriod::rate_multiplier);

    py::class_<BoxGeneratorParams>(m, "BoxGeneratorParams")
        .def(py::init<>())
        .def_readwrite("num_boxes",                 &BoxGeneratorParams::num_boxes)
        .def_readwrite("num_destinations",          &BoxGeneratorParams::num_destinations)
        .def_readwrite("weights",                   &BoxGeneratorParams::weights)
        .def_readwrite("seed",                      &BoxGeneratorParams::seed)
        .def_readwrite("mean_inter_arrival_ticks",  &BoxGeneratorParams::mean_inter_arrival_ticks)
        .def_readwrite("std_inter_arrival_ticks",   &BoxGeneratorParams::std_inter_arrival_ticks)
        .def_readwrite("demand_profile",            &BoxGeneratorParams::demand_profile);

    m.def("generate_boxes",
        [](const BoxGeneratorParams& p) {
            InputBelt belt = InputBelt::generate(p);
            std::vector<Box> out;
            out.reserve(static_cast<std::size_t>(p.num_boxes));
            while (auto b = belt.pop()) out.push_back(*b);
            return out;
        },
        py::arg("params"),
        py::call_guard<py::gil_scoped_release>());

    m.def("available_destinations", &InputBelt::availableDestinations);
}
