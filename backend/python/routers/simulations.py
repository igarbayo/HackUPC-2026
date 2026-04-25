from __future__ import annotations
import uuid
from datetime import datetime, timezone

import scheduler_cpp
from fastapi import APIRouter, HTTPException, status
from fastapi.responses import JSONResponse

import scheduler as sched
import store
from models import (
    BoxInput,
    GeneratorParams,
    SimulationParams,
    SimulationRecord,
)

router = APIRouter()


def _build_cpp_params(params: SimulationParams) -> scheduler_cpp.Params:
    boxes: list[scheduler_cpp.Box] = []

    if params.boxes:
        for b in params.boxes:
            boxes.append(scheduler_cpp.Box(b.id, b.family, b.arrival_tick))
    elif params.generator:
        g = params.generator
        gp = scheduler_cpp.BoxGeneratorParams()
        gp.num_boxes = g.num_boxes
        gp.num_destinations = g.num_destinations
        gp.weights = g.weights
        gp.seed = g.seed
        gp.mean_inter_arrival_ticks = g.mean_inter_arrival_ticks
        gp.std_inter_arrival_ticks = g.std_inter_arrival_ticks
        for dp in g.demand_profile:
            d = scheduler_cpp.DemandPeriod()
            d.from_tick = dp.get("from_tick", 0)
            d.to_tick = dp.get("to_tick", 0)
            d.rate_multiplier = dp.get("rate_multiplier", 1.0)
            gp.demand_profile.append(d)
        boxes = sched.generate_boxes(gp)

    return sched.build_cpp_params(
        num_slots=params.num_slots,
        num_y=params.num_y,
        num_sides=params.num_sides,
        max_ticks=params.max_ticks,
        boxes=boxes if boxes else None,
    )


@router.post("", status_code=status.HTTP_202_ACCEPTED)
def create_simulation(params: SimulationParams):
    sim_id = str(uuid.uuid4())
    cpp_params = _build_cpp_params(params)
    record = SimulationRecord(
        sim_id=sim_id,
        status="running",
        created_at=datetime.now(timezone.utc),
        params=params,
    )
    store.simulations[sim_id] = record
    sched.submit(sim_id, cpp_params)
    return {"sim_id": sim_id, "status": "running"}


@router.get("")
def list_simulations():
    return list(store.simulations.values())


@router.get("/{sim_id}")
def get_simulation(sim_id: str):
    if sim_id not in store.simulations:
        raise HTTPException(status_code=404, detail="Simulation not found")
    return store.simulations[sim_id]


@router.get("/{sim_id}/result")
def get_result(sim_id: str):
    if sim_id not in store.simulations:
        raise HTTPException(status_code=404, detail="Simulation not found")
    if store.simulations[sim_id].status == "error":
        raise HTTPException(status_code=500, detail="Simulation failed")
    if not store.done.get(sim_id):
        return JSONResponse(status_code=202, content={"detail": "Simulation still running"})
    return {
        "events":    store.events[sim_id],
        "snapshots": store.snapshots[sim_id],
    }
