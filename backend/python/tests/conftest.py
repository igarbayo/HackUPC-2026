"""
Shared fixtures and fake scheduler_cpp module.

scheduler_cpp is the compiled pybind11 extension. Tests must not require it,
so we install a pure-Python fake in sys.modules before any app code is imported.
"""
import collections
import os
import sys
import threading
import types
from datetime import datetime, timezone

import pytest

# --- sys.path: make backend/python importable ---
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


# ============================================================
# Fake C++ objects – mirror the structs in types.h
# ============================================================

class FakePosition:
    def __init__(self, x=0, y=1, z=1, side=1):
        self.x = x
        self.y = y
        self.z = z
        self.side = side


class FakeBoxSnap:
    def __init__(self, id="B1", family="ZARA", arrival_tick=0, position=None):
        self.id = id
        self.family = family
        self.arrival_tick = arrival_tick
        self.position = position or FakePosition()


class FakeShuttleSnap:
    def __init__(
        self,
        y_level=1,
        position=None,
        phase="Idle",
        is_carrying=False,
        carried_box_id="",
        carried_box_family="",
        floor_boxes=None,
    ):
        self.y_level = y_level
        self.position = position or FakePosition(x=-1, y=y_level)
        self.phase = phase
        self.is_carrying = is_carrying
        self.carried_box_id = carried_box_id
        self.carried_box_family = carried_box_family
        self.floor_boxes = floor_boxes if floor_boxes is not None else []


class FakeAisleSnap:
    def __init__(self, shuttles=None):
        self.shuttles = shuttles or []


class FakePalletSnap:
    def __init__(self, slot=0, family="ZARA", placed_count=0, reserved_count=0, boxes=None):
        self.slot = slot
        self.family = family
        self.placed_count = placed_count
        self.reserved_count = reserved_count
        self.boxes = boxes if boxes is not None else []


class FakeTickSnapshot:
    def __init__(self, tick=1, aisle=None, pallets=None):
        self.tick = tick
        self.aisle = aisle or FakeAisleSnap()
        self.pallets = pallets if pallets is not None else []


class FakeEvent:
    def __init__(self, type="box_arrived", logical_time=1, box_id="B1", pallet_id=-1, family="ZARA"):
        self.type = type
        self.logical_time = logical_time
        self.box_id = box_id
        self.pallet_id = pallet_id
        self.family = family


# ============================================================
# Rich default fake simulation data
# ============================================================

def _make_floor_boxes():
    return [
        FakeBoxSnap("BOX1", "ZARA",    5, FakePosition(3, 1, 1, 1)),
        FakeBoxSnap("BOX2", "BERSHKA", 8, FakePosition(1, 1, 2, 2)),
    ]


def _make_shuttles():
    return [
        FakeShuttleSnap(
            y_level=1,
            position=FakePosition(x=-1, y=1, z=1, side=1),
            phase="Idle",
            is_carrying=False,
            carried_box_id="",
            carried_box_family="",
            floor_boxes=_make_floor_boxes(),
        ),
        FakeShuttleSnap(
            y_level=2,
            position=FakePosition(x=3, y=2, z=1, side=1),
            phase="MovingToDrop",
            is_carrying=True,
            carried_box_id="BOX3",
            carried_box_family="MASSIMO_DUTTI",
            floor_boxes=[],
        ),
    ]


def _make_pallets():
    return [
        FakePalletSnap(
            slot=0,
            family="ZARA",
            placed_count=2,
            reserved_count=1,
            boxes=[
                FakeBoxSnap("PBOX1", "ZARA", 1),
                FakeBoxSnap("PBOX2", "ZARA", 2),
            ],
        )
    ]


def _make_snapshot(tick=42):
    return FakeTickSnapshot(
        tick=tick,
        aisle=FakeAisleSnap(shuttles=_make_shuttles()),
        pallets=_make_pallets(),
    )


def _make_events():
    return [
        FakeEvent("box_arrived",    10, "BOX1", -1, "ZARA"),
        FakeEvent("box_stored",     15, "BOX1", -1, "ZARA"),
        FakeEvent("box_on_pallet",  40, "BOX1",  0, "ZARA"),
        FakeEvent("done",           42, "",     -1, ""),
    ]


FAKE_RESULT = {
    "events":    _make_events(),
    "snapshots": [_make_snapshot(tick=42)],
}


# ============================================================
# Fake SnapshotQueue – Python equivalent of the C++ class
# ============================================================

class _FakeSnapshotQueue:
    def __init__(self):
        self._q = collections.deque()
        self._done = False
        self._lock = threading.Lock()
        self._cv = threading.Condition(self._lock)

    def push(self, snap):
        with self._cv:
            self._q.append(snap)
            self._cv.notify()

    def pop(self):
        with self._cv:
            self._cv.wait_for(lambda: bool(self._q) or self._done)
            if not self._q:
                return None
            return self._q.popleft()

    def markDone(self):
        with self._cv:
            self._done = True
            self._cv.notify_all()

    def isDone(self):
        with self._lock:
            return self._done


# ============================================================
# Fake scheduler_cpp module
# ============================================================

class _FakeParams:
    def __init__(self):
        self.boxes = []
        self.num_slots = 20
        self.num_y = 2
        self.num_sides = 1
        self.max_ticks = 10000


class _FakeBox:
    def __init__(self, id, family, arrival_tick=0):
        self.id = id
        self.family = family
        self.arrival_tick = arrival_tick


class _FakeBoxGeneratorParams:
    def __init__(self):
        self.num_boxes = 50
        self.num_destinations = 5
        self.weights = {}
        self.seed = 42
        self.mean_inter_arrival_ticks = 10.0
        self.std_inter_arrival_ticks = 3.0
        self.demand_profile = []


class _FakeDemandPeriod:
    def __init__(self):
        self.from_tick = 0
        self.to_tick = 0
        self.rate_multiplier = 1.0


def _fake_run_simulation_streaming(params, queue):
    """Pushes fake snapshots into the queue and returns fake events."""
    for snap in FAKE_RESULT["snapshots"]:
        queue.push(snap)
    # markDone() is called by scheduler.submit's run_cpp finally-block
    return list(FAKE_RESULT["events"])


_fake_cpp = types.ModuleType("scheduler_cpp")
_fake_cpp.Params = _FakeParams
_fake_cpp.Box = _FakeBox
_fake_cpp.BoxGeneratorParams = _FakeBoxGeneratorParams
_fake_cpp.DemandPeriod = _FakeDemandPeriod
_fake_cpp.SnapshotQueue = _FakeSnapshotQueue
_fake_cpp.run_simulation_streaming = _fake_run_simulation_streaming
_fake_cpp.available_destinations = lambda: ["ZARA", "BERSHKA", "MASSIMO_DUTTI"]
_fake_cpp.generate_boxes = lambda p: [
    _FakeBox(f"G{i}", "ZARA", i * 10) for i in range(p.num_boxes)
]

sys.modules["scheduler_cpp"] = _fake_cpp


# ============================================================
# Import store and converter after fake module is installed
# ============================================================

import store as _store          # noqa: E402
import scheduler as _sched      # noqa: E402


def _make_fake_tick_states():
    return [_sched._snapshot_to_tick_state(s) for s in FAKE_RESULT["snapshots"]]


def _make_fake_events():
    return [_sched._event_to_model(e) for e in FAKE_RESULT["events"]]


# ============================================================
# Fixtures
# ============================================================

@pytest.fixture(autouse=True)
def clear_store():
    """Wipe the in-memory store before and after every test."""
    _store.simulations.clear()
    _store.snapshots.clear()
    _store.events.clear()
    _store.done.clear()
    yield
    _store.simulations.clear()
    _store.snapshots.clear()
    _store.events.clear()
    _store.done.clear()


@pytest.fixture
def client(clear_store):
    from fastapi.testclient import TestClient
    from main import app
    with TestClient(app) as c:
        yield c


@pytest.fixture
def completed_sim(client):
    """
    Inserts a completed simulation directly into the store.
    Returns the sim_id.
    """
    from models import SimulationRecord, SimulationParams

    sim_id = "test-completed-sim"
    now = datetime.now(timezone.utc)
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="done",
        created_at=now,
        finished_at=now,
        params=SimulationParams(),
    )
    _store.snapshots[sim_id] = _make_fake_tick_states()
    _store.events[sim_id] = _make_fake_events()
    _store.done[sim_id] = True
    return sim_id


@pytest.fixture
def running_sim(client):
    """
    Inserts a running simulation (no result yet) into the store.
    Returns the sim_id.
    """
    from models import SimulationRecord, SimulationParams

    sim_id = "test-running-sim"
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="running",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    _store.snapshots[sim_id] = []
    _store.events[sim_id] = []
    _store.done[sim_id] = False
    return sim_id
