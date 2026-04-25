from __future__ import annotations
from datetime import datetime
from typing import Optional
from pydantic import BaseModel


class PositionModel(BaseModel):
    x: int
    y: int
    z: int
    side: int


class BoxModel(BaseModel):
    id: str
    family: str
    arrival_tick: int
    position: Optional[PositionModel] = None


class ShuttleStateModel(BaseModel):
    y_level: int
    position: PositionModel
    phase: str
    is_carrying: bool
    carried_box_id: str
    carried_box_family: str
    floor_boxes: list[BoxModel]


class InstructionModel(BaseModel):
    kind: str
    family: str
    box_id: str
    issued_at: int
    priority: int
    seq: int
    target: PositionModel


class AisleStateModel(BaseModel):
    aisle_id: int
    shuttles: list[ShuttleStateModel]
    pending_fifo: list[InstructionModel]
    pending_sorted: list[InstructionModel]


class PalletStateModel(BaseModel):
    slot: int
    family: str
    placed_count: int
    reserved_count: int
    boxes: list[BoxModel]


class RobotStateModel(BaseModel):
    pallets: list[PalletStateModel]


class MetricsModel(BaseModel):
    total_pallets_sent: int = 0
    full_pallets: int = 0
    full_pallet_ratio: float = 0.0
    total_boxes_sent: int = 0
    avg_fill_rate: float = 0.0
    boxes_by_family: dict[str, int] = {}


class TickStateModel(BaseModel):
    tick: int
    aisles: list[AisleStateModel]
    robot: RobotStateModel
    metrics: MetricsModel = MetricsModel()


class EventModel(BaseModel):
    type: str
    logical_time: int
    box_id: str
    pallet_id: int
    family: str
    box_count: int = -1


class BoxInput(BaseModel):
    id: str
    family: str
    arrival_tick: int = 0


class GeneratorParams(BaseModel):
    num_boxes: int = 50
    num_destinations: int = 5
    weights: dict[str, float] = {}
    seed: int = 42
    mean_inter_arrival_ticks: float = 10.0
    std_inter_arrival_ticks: float = 3.0
    demand_profile: list[dict] = []


class SimulationParams(BaseModel):
    num_slots: int = 20
    num_y: int = 2
    num_sides: int = 1
    max_ticks: int = 10000
    boxes: Optional[list[BoxInput]] = None
    generator: Optional[GeneratorParams] = None


class SimulationRecord(BaseModel):
    sim_id: str
    status: str              # "running" | "done" | "error"
    created_at: datetime
    finished_at: Optional[datetime] = None
    params: SimulationParams
