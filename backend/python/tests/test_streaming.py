"""
Tests for the producer-consumer streaming architecture.

Coverage:
  - FakeSnapshotQueue (Python equivalent of C++ SnapshotQueue)
  - scheduler.submit two-thread pipeline
  - WebSocket streams partial results before simulation ends
"""
import asyncio
import json
import threading
import time

import pytest

import store as _store
from conftest import (
    FakeAisleSnap,
    FakeTickSnapshot,
    _FakeSnapshotQueue,
    _make_events,
    _make_snapshot,
)


# ══════════════════════════════════════════════════════════════════════════════
# SnapshotQueue unit tests (via Python _FakeSnapshotQueue)
# ══════════════════════════════════════════════════════════════════════════════

def test_queue_push_pop_order():
    """Items come out FIFO."""
    q = _FakeSnapshotQueue()
    snaps = [FakeTickSnapshot(tick=t) for t in [1, 2, 3]]
    for s in snaps:
        q.push(s)
    q.markDone()
    results = []
    while True:
        item = q.pop()
        if item is None:
            break
        results.append(item.tick)
    assert results == [1, 2, 3]


def test_queue_pop_returns_none_when_done_and_empty():
    q = _FakeSnapshotQueue()
    q.markDone()
    assert q.pop() is None


def test_queue_drains_all_before_none():
    """markDone with items remaining: pop returns all items, then None."""
    q = _FakeSnapshotQueue()
    for t in [10, 20, 30]:
        q.push(FakeTickSnapshot(tick=t))
    q.markDone()
    ticks = []
    while True:
        item = q.pop()
        if item is None:
            break
        ticks.append(item.tick)
    assert ticks == [10, 20, 30]


def test_queue_pop_blocks_then_unblocks():
    """pop() blocks until push() delivers an item."""
    q = _FakeSnapshotQueue()
    received = []

    def consumer():
        item = q.pop()
        if item is not None:
            received.append(item.tick)
        q.pop()  # drain the markDone sentinel

    t = threading.Thread(target=consumer)
    t.start()
    time.sleep(0.05)
    assert received == [], "pop should be blocking"
    q.push(FakeTickSnapshot(tick=99))
    q.markDone()
    t.join(timeout=1.0)
    assert received == [99]


def test_queue_is_done_after_mark():
    q = _FakeSnapshotQueue()
    assert not q.isDone()
    q.markDone()
    assert q.isDone()


# ══════════════════════════════════════════════════════════════════════════════
# scheduler.submit integration (uses fake C++ via conftest)
# ══════════════════════════════════════════════════════════════════════════════

def _wait_done_store(sim_id, timeout=3.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if _store.done.get(sim_id):
            return True
        time.sleep(0.02)
    return False


def test_submit_populates_snapshots(client):
    """After submit completes, store.snapshots has the converted TickStateModels."""
    import scheduler as sched
    import scheduler_cpp

    sim_id = "stream-test-1"
    from models import SimulationParams, SimulationRecord
    from datetime import datetime, timezone

    params = scheduler_cpp.Params()
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="running",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    sched.submit(sim_id, params)

    assert _wait_done_store(sim_id), "simulation did not finish in time"

    from conftest import _make_snapshot
    from models import TickStateModel
    assert len(_store.snapshots[sim_id]) == 1  # one fake snapshot
    assert isinstance(_store.snapshots[sim_id][0], TickStateModel)
    assert _store.snapshots[sim_id][0].tick == 42


def test_submit_populates_events(client):
    """After submit completes, store.events has the converted EventModels."""
    import scheduler as sched
    import scheduler_cpp
    from models import SimulationParams, SimulationRecord, EventModel
    from datetime import datetime, timezone

    sim_id = "stream-test-2"
    params = scheduler_cpp.Params()
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="running",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    sched.submit(sim_id, params)

    assert _wait_done_store(sim_id)

    from conftest import _make_events
    assert len(_store.events[sim_id]) == len(_make_events())
    assert all(isinstance(e, EventModel) for e in _store.events[sim_id])


def test_submit_sets_status_done(client):
    import scheduler as sched
    import scheduler_cpp
    from models import SimulationParams, SimulationRecord
    from datetime import datetime, timezone

    sim_id = "stream-test-3"
    params = scheduler_cpp.Params()
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="running",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    sched.submit(sim_id, params)
    assert _wait_done_store(sim_id)
    assert _store.simulations[sim_id].status == "done"
    assert _store.simulations[sim_id].finished_at is not None


# ══════════════════════════════════════════════════════════════════════════════
# WebSocket streams partial results (simulation not yet done)
# ══════════════════════════════════════════════════════════════════════════════

@pytest.fixture(autouse=True)
def no_sleep(monkeypatch):
    async def instant(_=0):
        pass
    monkeypatch.setattr(asyncio, "sleep", instant)


def test_ws_streams_partial_before_done(client):
    """
    WS reads snapshots that are already in the store while done=False,
    then gets the done message once done=True is set.
    """
    import scheduler as _sched
    from models import SimulationParams, SimulationRecord
    from datetime import datetime, timezone

    sim_id = "partial-stream"
    raw_snaps = [FakeTickSnapshot(tick=t, aisles=[FakeAisleSnap(shuttles=[])], pallets=[]) for t in [5, 10]]
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="running",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    _store.snapshots[sim_id] = [_sched._snapshot_to_tick_state(s) for s in raw_snaps]
    _store.events[sim_id] = []
    _store.done[sim_id] = False

    # Flip done=True and status in a background thread after a short delay
    def finish():
        time.sleep(0.05)
        _store.done[sim_id] = True
        _store.simulations[sim_id].status = "done"

    threading.Thread(target=finish, daemon=True).start()

    with client.websocket_connect(f"/simulations/{sim_id}/stream") as ws:
        messages = []
        for _ in range(3):  # 2 tick msgs + done
            messages.append(json.loads(ws.receive_text()))

    assert messages[0]["tick"] == 5
    assert messages[1]["tick"] == 10
    assert messages[2]["type"] == "done"
    assert messages[2]["total_ticks"] == 2


def test_ws_waits_when_no_snapshots_yet(client):
    """WS does not crash if it connects before any snapshots arrive."""
    import scheduler as _sched
    from models import SimulationParams, SimulationRecord
    from datetime import datetime, timezone

    sim_id = "late-stream"
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="running",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    _store.snapshots[sim_id] = []
    _store.events[sim_id] = []
    _store.done[sim_id] = False

    def add_snap_and_finish():
        time.sleep(0.05)
        raw = FakeTickSnapshot(tick=7, aisles=[FakeAisleSnap(shuttles=[])], pallets=[])
        _store.snapshots[sim_id].append(_sched._snapshot_to_tick_state(raw))
        time.sleep(0.02)
        _store.done[sim_id] = True
        _store.simulations[sim_id].status = "done"

    threading.Thread(target=add_snap_and_finish, daemon=True).start()

    with client.websocket_connect(f"/simulations/{sim_id}/stream") as ws:
        msg1 = json.loads(ws.receive_text())
        msg2 = json.loads(ws.receive_text())

    assert msg1["tick"] == 7
    assert msg2["type"] == "done"
    assert msg2["total_ticks"] == 1
