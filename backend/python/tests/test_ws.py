"""
WebSocket endpoint tests: WS /simulations/{id}/stream

The server streams one JSON message per tick snapshot, then a final
{"type": "done", "total_ticks": N} message.  asyncio.sleep is patched to
a no-op so tests run without delay.
"""
import asyncio
import json
import pytest

from models import TickStateModel
import store as _store


# ── Speed up: disable all asyncio.sleep calls inside the ws router ─────────────

@pytest.fixture(autouse=True)
def no_sleep(monkeypatch):
    async def instant(_=0):
        pass
    monkeypatch.setattr(asyncio, "sleep", instant)


# ── Helpers ────────────────────────────────────────────────────────────────────

def _collect_all(ws):
    """Read messages until WebSocket closes; returns list of parsed dicts."""
    messages = []
    try:
        while True:
            raw = ws.receive_text()
            messages.append(json.loads(raw))
    except Exception:
        pass
    return messages


def _collect_n(ws, n):
    """Read exactly n text messages."""
    return [json.loads(ws.receive_text()) for _ in range(n)]


# ══════════════════════════════════════════════════════════════════════════════
# Sim not found
# ══════════════════════════════════════════════════════════════════════════════

def test_ws_unknown_sim_sends_error(client):
    with client.websocket_connect("/simulations/no-such-id/stream") as ws:
        msg = json.loads(ws.receive_text())
        assert msg["type"] == "error"
        assert "not found" in msg["detail"].lower()


# ══════════════════════════════════════════════════════════════════════════════
# Completed simulation streaming
# ══════════════════════════════════════════════════════════════════════════════

def test_ws_streams_correct_number_of_ticks(client, completed_sim):
    import store as _store
    n_snaps = len(_store.snapshots[completed_sim])
    with client.websocket_connect(f"/simulations/{completed_sim}/stream") as ws:
        # n_snaps tick messages + 1 done message
        messages = _collect_n(ws, n_snaps + 1)
    tick_messages = messages[:-1]
    done_message = messages[-1]
    assert len(tick_messages) == n_snaps
    assert done_message["type"] == "done"


def test_ws_last_message_is_done(client, completed_sim):
    import store as _store
    n_snaps = len(_store.snapshots[completed_sim])
    with client.websocket_connect(f"/simulations/{completed_sim}/stream") as ws:
        messages = _collect_n(ws, n_snaps + 1)
    assert messages[-1]["type"] == "done"


def test_ws_done_message_has_total_ticks(client, completed_sim):
    import store as _store
    n_snaps = len(_store.snapshots[completed_sim])
    with client.websocket_connect(f"/simulations/{completed_sim}/stream") as ws:
        messages = _collect_n(ws, n_snaps + 1)
    done = messages[-1]
    assert done["total_ticks"] == n_snaps


# ══════════════════════════════════════════════════════════════════════════════
# Tick message structure
# ══════════════════════════════════════════════════════════════════════════════

def _get_tick_message(client, sim_id):
    """Return the first tick message from the WS stream."""
    with client.websocket_connect(f"/simulations/{sim_id}/stream") as ws:
        return json.loads(ws.receive_text())


def test_ws_tick_has_tick_field(client, completed_sim):
    msg = _get_tick_message(client, completed_sim)
    assert "tick" in msg


def test_ws_tick_has_aisle_field(client, completed_sim):
    msg = _get_tick_message(client, completed_sim)
    assert "aisle" in msg


def test_ws_tick_has_pallets_field(client, completed_sim):
    msg = _get_tick_message(client, completed_sim)
    assert "pallets" in msg


def test_ws_tick_validates_as_tick_state_model(client, completed_sim):
    """The JSON must be parseable as TickStateModel."""
    msg = _get_tick_message(client, completed_sim)
    tick_state = TickStateModel.model_validate(msg)
    assert isinstance(tick_state, TickStateModel)


def test_ws_tick_value_matches_fake(client, completed_sim):
    msg = _get_tick_message(client, completed_sim)
    assert msg["tick"] == 42


# ── Aisle ─────────────────────────────────────────────────────────────────────

def test_ws_tick_aisle_has_shuttles(client, completed_sim):
    msg = _get_tick_message(client, completed_sim)
    assert "shuttles" in msg["aisle"]
    assert isinstance(msg["aisle"]["shuttles"], list)


def test_ws_tick_shuttle_count(client, completed_sim):
    msg = _get_tick_message(client, completed_sim)
    assert len(msg["aisle"]["shuttles"]) == 2


def test_ws_tick_shuttle_fields_complete(client, completed_sim):
    shuttle = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]
    for field in (
        "y_level", "position", "phase",
        "is_carrying", "carried_box_id", "carried_box_family", "floor_boxes",
    ):
        assert field in shuttle, f"shuttle missing '{field}'"


def test_ws_tick_shuttle_position_has_xyzside(client, completed_sim):
    pos = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]["position"]
    for field in ("x", "y", "z", "side"):
        assert field in pos


def test_ws_tick_shuttle_idle_not_carrying(client, completed_sim):
    s1 = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]
    assert s1["phase"] == "Idle"
    assert s1["is_carrying"] is False


def test_ws_tick_shuttle_carrying(client, completed_sim):
    s2 = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][1]
    assert s2["is_carrying"] is True
    assert s2["carried_box_id"] == "BOX3"
    assert s2["carried_box_family"] == "MASSIMO_DUTTI"


# ── Floor boxes ───────────────────────────────────────────────────────────────

def test_ws_tick_floor_boxes_present(client, completed_sim):
    s1 = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]
    assert len(s1["floor_boxes"]) == 2


def test_ws_tick_floor_box_has_position(client, completed_sim):
    s1 = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]
    for box in s1["floor_boxes"]:
        assert "position" in box
        assert box["position"] is not None
        for field in ("x", "y", "z", "side"):
            assert field in box["position"]


def test_ws_tick_floor_box_ids(client, completed_sim):
    s1 = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]
    ids = [b["id"] for b in s1["floor_boxes"]]
    assert "BOX1" in ids
    assert "BOX2" in ids


def test_ws_tick_floor_box_families(client, completed_sim):
    s1 = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]
    families = {b["family"] for b in s1["floor_boxes"]}
    assert "ZARA" in families
    assert "BERSHKA" in families


def test_ws_tick_floor_box_position_values(client, completed_sim):
    s1 = _get_tick_message(client, completed_sim)["aisle"]["shuttles"][0]
    box1 = next(b for b in s1["floor_boxes"] if b["id"] == "BOX1")
    assert box1["position"]["x"] == 3
    assert box1["position"]["z"] == 1
    assert box1["position"]["side"] == 1


# ── Pallets ───────────────────────────────────────────────────────────────────

def test_ws_tick_pallets_is_list(client, completed_sim):
    pallets = _get_tick_message(client, completed_sim)["pallets"]
    assert isinstance(pallets, list)


def test_ws_tick_pallet_count(client, completed_sim):
    pallets = _get_tick_message(client, completed_sim)["pallets"]
    assert len(pallets) == 1


def test_ws_tick_pallet_fields(client, completed_sim):
    pallet = _get_tick_message(client, completed_sim)["pallets"][0]
    for field in ("slot", "family", "placed_count", "reserved_count", "boxes"):
        assert field in pallet, f"pallet missing '{field}'"


def test_ws_tick_pallet_values(client, completed_sim):
    pallet = _get_tick_message(client, completed_sim)["pallets"][0]
    assert pallet["slot"] == 0
    assert pallet["family"] == "ZARA"
    assert pallet["placed_count"] == 2
    assert pallet["reserved_count"] == 1


def test_ws_tick_pallet_boxes_count(client, completed_sim):
    pallet = _get_tick_message(client, completed_sim)["pallets"][0]
    assert len(pallet["boxes"]) == 2


def test_ws_tick_pallet_box_has_fields(client, completed_sim):
    box = _get_tick_message(client, completed_sim)["pallets"][0]["boxes"][0]
    for field in ("id", "family", "arrival_tick"):
        assert field in box


def test_ws_tick_pallet_box_no_position(client, completed_sim):
    """Pallet boxes have no aisle position."""
    box = _get_tick_message(client, completed_sim)["pallets"][0]["boxes"][0]
    assert box.get("position") is None


# ══════════════════════════════════════════════════════════════════════════════
# Multi-tick: all snapshots are delivered in order
# ══════════════════════════════════════════════════════════════════════════════

def test_ws_multi_tick_all_delivered(client):
    """Inject 3 snapshots and verify all 3 are streamed in order."""
    from conftest import FakeTickSnapshot, FakeAisleSnap
    from models import SimulationParams, SimulationRecord, TickStateModel, AisleStateModel
    from datetime import datetime, timezone
    import scheduler as _sched

    sim_id = "multi-tick-sim"
    raw_snaps = [FakeTickSnapshot(tick=t, aisles=[FakeAisleSnap(shuttles=[])], pallets=[]) for t in [10, 20, 30]]
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="done",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    _store.snapshots[sim_id] = [_sched._snapshot_to_tick_state(s) for s in raw_snaps]
    _store.events[sim_id] = []
    _store.done[sim_id] = True

    with client.websocket_connect(f"/simulations/{sim_id}/stream") as ws:
        messages = _collect_n(ws, 3 + 1)  # 3 ticks + done

    tick_msgs = messages[:3]
    assert [m["tick"] for m in tick_msgs] == [10, 20, 30]
    assert messages[3]["type"] == "done"
    assert messages[3]["total_ticks"] == 3


def test_ws_multi_tick_each_validates_as_tick_state(client):
    from conftest import FakeTickSnapshot, FakeAisleSnap
    from models import SimulationParams, SimulationRecord
    from datetime import datetime, timezone
    import scheduler as _sched

    sim_id = "multi-tick-validate"
    raw_snaps = [
        FakeTickSnapshot(tick=t, aisles=[FakeAisleSnap(shuttles=[])], pallets=[])
        for t in [1, 2, 3]
    ]
    _store.simulations[sim_id] = SimulationRecord(
        sim_id=sim_id,
        status="done",
        created_at=datetime.now(timezone.utc),
        params=SimulationParams(),
    )
    _store.snapshots[sim_id] = [_sched._snapshot_to_tick_state(s) for s in raw_snaps]
    _store.events[sim_id] = []
    _store.done[sim_id] = True

    with client.websocket_connect(f"/simulations/{sim_id}/stream") as ws:
        tick_msgs = _collect_n(ws, 3)

    for msg in tick_msgs:
        TickStateModel.model_validate(msg)  # must not raise
