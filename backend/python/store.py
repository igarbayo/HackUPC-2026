"""In-memory simulation store shared by all routers."""
from __future__ import annotations
from models import SimulationRecord

simulations: dict[str, SimulationRecord] = {}
snapshots:   dict[str, list] = {}   # list[TickStateModel], grows tick by tick
events:      dict[str, list] = {}   # list[EventModel], populated when done
done:        dict[str, bool] = {}   # True once drain thread finishes
