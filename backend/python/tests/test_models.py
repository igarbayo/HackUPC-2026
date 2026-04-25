"""Pydantic model validation: correct fields, types, optional handling, defaults."""
import pytest
from pydantic import ValidationError

from models import (
    AisleStateModel,
    BoxModel,
    EventModel,
    GeneratorParams,
    PalletStateModel,
    PositionModel,
    ShuttleStateModel,
    SimulationParams,
    SimulationRecord,
    TickStateModel,
    BoxInput,
)
from datetime import datetime, timezone


# ── PositionModel ───────────────────���──────────────────────────────────────────

def test_position_all_fields():
    p = PositionModel(x=3, y=2, z=1, side=2)
    assert p.x == 3
    assert p.y == 2
    assert p.z == 1
    assert p.side == 2


def test_position_requires_all_fields():
    with pytest.raises(ValidationError):
        PositionModel(x=1, y=1)  # missing z and side


# ── BoxModel ────────────────────────────────────────────────────────────────��──

def test_box_model_with_position():
    pos = PositionModel(x=2, y=1, z=1, side=1)
    b = BoxModel(id="B1", family="ZARA", arrival_tick=10, position=pos)
    assert b.id == "B1"
    assert b.family == "ZARA"
    assert b.arrival_tick == 10
    assert b.position == pos


def test_box_model_position_optional():
    b = BoxModel(id="B2", family="BERSHKA", arrival_tick=5)
    assert b.position is None


def test_box_model_requires_id_family_tick():
    with pytest.raises(ValidationError):
        BoxModel(family="ZARA", arrival_tick=0)  # missing id


# ── ShuttleStateModel ────────────────────────────────────────���─────────────────

def _make_position(**kw):
    defaults = dict(x=0, y=1, z=1, side=1)
    return PositionModel(**{**defaults, **kw})


def test_shuttle_state_idle():
    s = ShuttleStateModel(
        y_level=1,
        position=_make_position(x=-1),
        phase="Idle",
        is_carrying=False,
        carried_box_id="",
        carried_box_family="",
        floor_boxes=[],
    )
    assert s.y_level == 1
    assert s.phase == "Idle"
    assert s.is_carrying is False
    assert s.floor_boxes == []


def test_shuttle_state_carrying():
    box = BoxModel(id="B9", family="ZARA", arrival_tick=3, position=_make_position(x=2))
    s = ShuttleStateModel(
        y_level=2,
        position=_make_position(x=3, y=2),
        phase="MovingToDrop",
        is_carrying=True,
        carried_box_id="B9",
        carried_box_family="ZARA",
        floor_boxes=[box],
    )
    assert s.is_carrying is True
    assert s.carried_box_id == "B9"
    assert s.carried_box_family == "ZARA"
    assert len(s.floor_boxes) == 1
    assert s.floor_boxes[0].id == "B9"


# ── AisleStateModel ────────────────────────────────���───────────────────────────

def test_aisle_state_empty_shuttles():
    a = AisleStateModel(shuttles=[])
    assert a.shuttles == []


def test_aisle_state_multiple_shuttles():
    shuttles = [
        ShuttleStateModel(
            y_level=i + 1,
            position=_make_position(y=i + 1),
            phase="Idle",
            is_carrying=False,
            carried_box_id="",
            carried_box_family="",
            floor_boxes=[],
        )
        for i in range(4)
    ]
    a = AisleStateModel(shuttles=shuttles)
    assert len(a.shuttles) == 4
    assert [s.y_level for s in a.shuttles] == [1, 2, 3, 4]


# ── PalletStateModel ───────────────────────────────────────────────────────────

def test_pallet_state_fields():
    boxes = [BoxModel(id=f"PB{i}", family="ZARA", arrival_tick=i) for i in range(3)]
    p = PalletStateModel(slot=1, family="ZARA", placed_count=3, reserved_count=2, boxes=boxes)
    assert p.slot == 1
    assert p.family == "ZARA"
    assert p.placed_count == 3
    assert p.reserved_count == 2
    assert len(p.boxes) == 3


def test_pallet_state_empty_boxes():
    p = PalletStateModel(slot=0, family="MASSIMO_DUTTI", placed_count=0, reserved_count=0, boxes=[])
    assert p.boxes == []


# ── TickStateModel ─────────────────────────────────────────────────────────────

def test_tick_state_structure():
    aisle = AisleStateModel(shuttles=[])
    pallet = PalletStateModel(slot=0, family="ZARA", placed_count=1, reserved_count=0, boxes=[])
    t = TickStateModel(tick=42, aisle=aisle, pallets=[pallet])
    assert t.tick == 42
    assert isinstance(t.aisle, AisleStateModel)
    assert len(t.pallets) == 1


def test_tick_state_no_pallets():
    t = TickStateModel(tick=1, aisle=AisleStateModel(shuttles=[]), pallets=[])
    assert t.pallets == []


# ── EventModel ───────────────────────────────���────────────────────────────���────

def test_event_model_box_arrived():
    e = EventModel(type="box_arrived", logical_time=10, box_id="B1", pallet_id=-1, family="ZARA")
    assert e.type == "box_arrived"
    assert e.logical_time == 10
    assert e.box_id == "B1"
    assert e.pallet_id == -1
    assert e.family == "ZARA"


def test_event_model_pallet_dispatched():
    e = EventModel(type="pallet_dispatched", logical_time=99, box_id="", pallet_id=2, family="BERSHKA")
    assert e.pallet_id == 2


def test_event_model_done():
    e = EventModel(type="done", logical_time=200, box_id="", pallet_id=-1, family="")
    assert e.type == "done"


# ── SimulationParams ──────────────────────────────────���────────────────────────

def test_simulation_params_defaults():
    p = SimulationParams()
    assert p.num_slots == 20
    assert p.num_y == 2
    assert p.num_sides == 1
    assert p.max_ticks == 10000
    assert p.boxes is None
    assert p.generator is None


def test_simulation_params_with_boxes():
    p = SimulationParams(
        num_slots=10,
        num_y=4,
        num_sides=2,
        max_ticks=500,
        boxes=[BoxInput(id="X1", family="ZARA", arrival_tick=0)],
    )
    assert p.num_slots == 10
    assert len(p.boxes) == 1
    assert p.boxes[0].id == "X1"


def test_simulation_params_with_generator():
    p = SimulationParams(
        generator={"num_boxes": 30, "num_destinations": 3, "seed": 7}
    )
    assert p.generator.num_boxes == 30
    assert p.generator.seed == 7


# ── GeneratorParams ─────────────────────────────────────��──────────────────────

def test_generator_params_defaults():
    g = GeneratorParams()
    assert g.num_boxes == 50
    assert g.num_destinations == 5
    assert g.seed == 42
    assert g.mean_inter_arrival_ticks == 10.0
    assert g.demand_profile == []


# ── SimulationRecord ──────────────────────────────��────────────────────────────

def test_simulation_record_running():
    r = SimulationRecord(
        sim_id="abc",
        status="running",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    assert r.sim_id == "abc"
    assert r.status == "running"
    assert r.finished_at is None


def test_simulation_record_done():
    now = datetime.now(timezone.utc)
    r = SimulationRecord(
        sim_id="xyz",
        status="done",
        created_at=now,
        finished_at=now,
        params=SimulationParams(),
    )
    assert r.status == "done"
    assert r.finished_at is not None
