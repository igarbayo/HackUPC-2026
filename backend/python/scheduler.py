"""Thin wrapper around the compiled C++ scheduler_cpp extension."""
from __future__ import annotations
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone

import scheduler_cpp
import store
from models import (
    AisleStateModel,
    BoxModel,
    EventModel,
    InstructionModel,
    PalletStateModel,
    PositionModel,
    RobotStateModel,
    ShuttleStateModel,
    TickStateModel,
)

_executor = ThreadPoolExecutor(max_workers=8)


# ── C++ parameter builders ────────────────────────────────────────────────────

def build_cpp_params(
    num_slots: int,
    num_y: int,
    num_sides: int,
    max_ticks: int,
    boxes: list[scheduler_cpp.Box] | None = None,
) -> scheduler_cpp.Params:
    p = scheduler_cpp.Params()
    p.num_slots = num_slots
    p.num_y = num_y
    p.num_sides = num_sides
    p.max_ticks = max_ticks
    if boxes:
        p.boxes = boxes
    return p


def generate_boxes(gen_params: scheduler_cpp.BoxGeneratorParams) -> list[scheduler_cpp.Box]:
    return scheduler_cpp.generate_boxes(gen_params)


def available_destinations() -> list[str]:
    return scheduler_cpp.available_destinations()


# ── Snapshot / event converters (used by drain + run_cpp threads) ─────────────

def _to_position(pos) -> PositionModel:
    return PositionModel(x=pos.x, y=pos.y, z=pos.z, side=pos.side)


def _to_box_model(b, position=None) -> BoxModel:
    return BoxModel(
        id=b.id,
        family=b.family,
        arrival_tick=b.arrival_tick,
        position=_to_position(position) if position is not None else None,
    )


def _to_instruction_model(instr) -> InstructionModel:
    return InstructionModel(
        kind=instr.kind,
        family=instr.family,
        box_id=instr.box_id,
        issued_at=instr.issued_at,
        priority=instr.priority,
        seq=instr.seq,
        target=_to_position(instr.target),
    )


def _snapshot_to_tick_state(snap) -> TickStateModel:
    aisles = []
    for a in snap.aisles:
        shuttles = []
        for s in a.shuttles:
            floor_boxes = [_to_box_model(b, b.position) for b in s.floor_boxes]
            shuttles.append(
                ShuttleStateModel(
                    y_level=s.y_level,
                    position=_to_position(s.position),
                    phase=s.phase,
                    is_carrying=s.is_carrying,
                    carried_box_id=s.carried_box_id,
                    carried_box_family=s.carried_box_family,
                    floor_boxes=floor_boxes,
                )
            )
        aisles.append(
            AisleStateModel(
                aisle_id=a.aisle_id,
                shuttles=shuttles,
                pending_fifo=[_to_instruction_model(i) for i in a.pending_fifo],
                pending_sorted=[_to_instruction_model(i) for i in a.pending_sorted],
            )
        )
    pallets = []
    for p in snap.pallets:
        boxes = [_to_box_model(b) for b in p.boxes]
        pallets.append(
            PalletStateModel(
                slot=p.slot,
                family=p.family,
                placed_count=p.placed_count,
                reserved_count=p.reserved_count,
                boxes=boxes,
            )
        )
    return TickStateModel(
        tick=snap.tick,
        aisles=aisles,
        robot=RobotStateModel(pallets=pallets),
    )


def _event_to_model(e) -> EventModel:
    return EventModel(
        type=e.type,
        logical_time=e.logical_time,
        box_id=e.box_id,
        pallet_id=e.pallet_id,
        family=e.family,
    )


# ── Two-thread submit ─────────────────────────────────────────────────────────

def submit(sim_id: str, params: scheduler_cpp.Params) -> None:
    """Launch hilo-A (C++ run) and hilo-B (queue drain) for one simulation."""
    queue = scheduler_cpp.SnapshotQueue()
    store.snapshots[sim_id] = []
    store.events[sim_id] = []
    store.done[sim_id] = False

    def run_cpp() -> None:
        try:
            scheduler_cpp.run_simulation_streaming(params, queue)
        except Exception:
            store.simulations[sim_id].status = "error"
            store.simulations[sim_id].finished_at = datetime.now(timezone.utc)
        finally:
            queue.markDone()

    def drain() -> None:
        while True:
            snap = queue.pop()
            if snap is None:
                break
            for e in snap.events:
                store.events[sim_id].append(_event_to_model(e))
            store.snapshots[sim_id].append(_snapshot_to_tick_state(snap))
        if store.simulations[sim_id].status != "error":
            store.simulations[sim_id].status = "done"
            store.simulations[sim_id].finished_at = datetime.now(timezone.utc)
        store.done[sim_id] = True

    _executor.submit(run_cpp)
    _executor.submit(drain)
