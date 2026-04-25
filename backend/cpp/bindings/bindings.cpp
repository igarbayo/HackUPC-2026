#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "simulation.h"
#include "SnapshotQueue.h"
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

    // --- per-tick state snapshot types ---

    py::class_<Position>(m, "Position")
        .def_readonly("x",    &Position::x)
        .def_readonly("y",    &Position::y)
        .def_readonly("z",    &Position::z)
        .def_readonly("side", &Position::side);

    py::class_<BoxSnap>(m, "BoxSnap")
        .def_readonly("id",          &BoxSnap::id)
        .def_readonly("family",      &BoxSnap::family)
        .def_readonly("arrival_tick",&BoxSnap::arrival_tick)
        .def_readonly("position",    &BoxSnap::position);

    py::class_<ShuttleSnap>(m, "ShuttleSnap")
        .def_readonly("y_level",            &ShuttleSnap::y_level)
        .def_readonly("position",           &ShuttleSnap::position)
        .def_readonly("phase",              &ShuttleSnap::phase)
        .def_readonly("is_carrying",        &ShuttleSnap::is_carrying)
        .def_readonly("carried_box_id",     &ShuttleSnap::carried_box_id)
        .def_readonly("carried_box_family", &ShuttleSnap::carried_box_family)
        .def_readonly("floor_boxes",        &ShuttleSnap::floor_boxes);

    py::class_<AisleSnap>(m, "AisleSnap")
        .def_readonly("shuttles", &AisleSnap::shuttles);

    py::class_<PalletSnap>(m, "PalletSnap")
        .def_readonly("slot",           &PalletSnap::slot)
        .def_readonly("family",         &PalletSnap::family)
        .def_readonly("placed_count",   &PalletSnap::placed_count)
        .def_readonly("reserved_count", &PalletSnap::reserved_count)
        .def_readonly("boxes",          &PalletSnap::boxes);

    py::class_<TickSnapshot>(m, "TickSnapshot")
        .def_readonly("tick",    &TickSnapshot::tick)
        .def_readonly("aisle",   &TickSnapshot::aisle)
        .def_readonly("pallets", &TickSnapshot::pallets)
        .def_readonly("events",  &TickSnapshot::events);

    m.def("run_simulation_with_state",
        [](const Params& p) -> py::dict {
            py::gil_scoped_release release;
            auto r = run_simulation_with_state(p);
            py::gil_scoped_acquire acquire;
            py::dict d;
            d["events"]    = r.events;
            d["snapshots"] = r.snapshots;
            return d;
        },
        py::arg("params"),
        "Run full simulation; returns dict with 'events' and 'snapshots' per tick");

    py::class_<SnapshotQueue>(m, "SnapshotQueue")
        .def(py::init<>())
        .def("push", &SnapshotQueue::push)
        .def("pop",
            [](SnapshotQueue& q) -> py::object {
                py::gil_scoped_release release;
                auto r = q.pop();
                py::gil_scoped_acquire acquire;
                if (!r) return py::none();
                return py::cast(std::move(*r));
            })
        .def("markDone", &SnapshotQueue::markDone)
        .def("isDone", &SnapshotQueue::isDone,
            py::call_guard<py::gil_scoped_release>());

    m.def("run_simulation_streaming",
        [](const Params& p, SnapshotQueue& q) {
            py::gil_scoped_release release;
            return run_simulation_streaming(p, q);
        },
        py::arg("params"), py::arg("queue"),
        "Run simulation streaming; pushes TickSnapshot per tick into queue, returns events");

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
