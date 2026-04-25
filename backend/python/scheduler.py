"""Thin wrapper around the compiled C++ scheduler_cpp extension."""
from __future__ import annotations
import csv as _csv
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
from pathlib import Path

import scheduler_cpp
import store

# Standard silo dimensions per challenge specification
_SILO_NUM_AISLES = 4
_SILO_NUM_SLOTS  = 60
_SILO_NUM_Y      = 8
_SILO_NUM_SIDES  = 2

_silo_csv_path: Path | None = None
_silo_boxes: list | None = None


def set_silo_csv(path: str | None) -> None:
    global _silo_csv_path, _silo_boxes
    _silo_csv_path = Path(path) if path else None
    _silo_boxes = None


def has_silo_csv() -> bool:
    return _silo_csv_path is not None


def get_silo_topology() -> dict | None:
    """Return standard silo dimensions when a CSV is loaded, else None."""
    if not has_silo_csv():
        return None
    return {
        "num_aisles": _SILO_NUM_AISLES,
        "num_slots":  _SILO_NUM_SLOTS,
        "num_y":      _SILO_NUM_Y,
        "num_sides":  _SILO_NUM_SIDES,
    }


def _parse_silo_once() -> None:
    global _silo_boxes
    if _silo_boxes is not None:
        return

    result = []
    with open(_silo_csv_path, newline="") as f:
        for row in _csv.DictReader(f):
            etiqueta = row["etiqueta"].strip()
            if not etiqueta:
                continue
            p = row["posicion"].strip()
            aisle_idx = int(p[0:2]) - 1
            side      = int(p[2:4])
            x_cpp     = int(p[4:7]) - 1   # 1-based → 0-based
            y         = int(p[7:9])
            z         = int(p[9:11])
            family    = etiqueta[7:15]

            box = scheduler_cpp.Box(etiqueta, family, 0)

            pos = scheduler_cpp.Position()
            pos.x    = x_cpp
            pos.y    = y
            pos.z    = z
            pos.side = side

            pb = scheduler_cpp.PrePlacedBox()
            pb.box       = box
            pb.aisle_idx = aisle_idx
            pb.pos       = pos
            result.append(pb)

    _silo_boxes = result


def parse_silo_csv() -> list:
    _parse_silo_once()
    return _silo_boxes


from models import (
    AisleStateModel,
    BoxModel,
    EventModel,
    InstructionModel,
    MetricsModel,
    PalletStateModel,
    PositionModel,
    RobotStateModel,
    ShuttleStateModel,
    TickStateModel,
)

_executor = ThreadPoolExecutor(max_workers=8)


# ── C++ parameter builders ────────────────────────────────────────────────────

def build_cpp_params(
    num_aisles: int,
    num_slots: int,
    num_y: int,
    num_sides: int,
    max_ticks: int,
    boxes: list[scheduler_cpp.Box] | None = None,
) -> scheduler_cpp.Params:
    p = scheduler_cpp.Params()
    p.num_aisles = num_aisles
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


def _snapshot_to_tick_state(snap, metrics: MetricsModel | None = None) -> TickStateModel:
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
        pending_fifo = [
            InstructionModel(
                kind=i.kind, family=i.family, box_id=i.box_id,
                issued_at=i.issued_at, priority=i.priority, seq=i.seq,
                target=_to_position(i.target),
            )
            for i in a.pending_fifo
        ]
        pending_sorted = [
            InstructionModel(
                kind=i.kind, family=i.family, box_id=i.box_id,
                issued_at=i.issued_at, priority=i.priority, seq=i.seq,
                target=_to_position(i.target),
            )
            for i in a.pending_sorted
        ]
        aisles.append(AisleStateModel(
            aisle_id=a.aisle_id,
            shuttles=shuttles,
            pending_fifo=pending_fifo,
            pending_sorted=pending_sorted,
        ))
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
        metrics=metrics if metrics is not None else MetricsModel(),
    )


def _event_to_model(e) -> EventModel:
    return EventModel(
        type=e.type,
        logical_time=e.logical_time,
        box_id=e.box_id,
        pallet_id=e.pallet_id,
        family=e.family,
        box_count=getattr(e, "box_count", -1),
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
        _totals: dict = {"total": 0, "full": 0, "boxes": 0, "by_family": {}}
        # Fallback box counter per slot — used when box_count is not exposed by the .so
        _slot_boxes: dict = {}
        try:
            while True:
                snap = queue.pop()
                if snap is None:
                    break
                for e in snap.events:
                    store.events[sim_id].append(_event_to_model(e))
                    if e.type == "box_on_pallet":
                        slot = e.pallet_id
                        _slot_boxes[slot] = _slot_boxes.get(slot, 0) + 1
                    elif e.type == "pallet_dispatched":
                        bc = getattr(e, "box_count", -1)
                        if bc < 0:
                            bc = _slot_boxes.pop(e.pallet_id, 0)
                        else:
                            _slot_boxes.pop(e.pallet_id, None)
                        _totals["total"] += 1
                        if bc == 12:
                            _totals["full"] += 1
                        if bc > 0:
                            _totals["boxes"] += bc
                        _totals["by_family"][e.family] = _totals["by_family"].get(e.family, 0) + max(bc, 0)
                t = _totals["total"]
                metrics = MetricsModel(
                    total_pallets_sent=t,
                    full_pallets=_totals["full"],
                    full_pallet_ratio=round(_totals["full"] / t * 100, 1) if t else 0.0,
                    total_boxes_sent=_totals["boxes"],
                    avg_fill_rate=round(_totals["boxes"] / (t * 12) * 100, 1) if t else 0.0,
                    boxes_by_family=dict(_totals["by_family"]),
                )
                store.snapshots[sim_id].append(_snapshot_to_tick_state(snap, metrics))
        except Exception:
            import traceback
            traceback.print_exc()
            store.simulations[sim_id].status = "error"
        finally:
            if store.simulations[sim_id].status != "error":
                store.simulations[sim_id].status = "done"
                store.simulations[sim_id].finished_at = datetime.now(timezone.utc)
            store.done[sim_id] = True

    _executor.submit(run_cpp)
    _executor.submit(drain)
