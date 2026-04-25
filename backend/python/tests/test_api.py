"""
REST endpoint tests for the warehouse simulation backend.

Coverage:
  POST   /simulations            – launch a simulation
  GET    /simulations            – list all simulations
  GET    /simulations/{id}       – get one simulation record
  GET    /simulations/{id}/result – full result (events + snapshots)

For result tests we inject data directly into the store to avoid
depending on thread-pool timing.
"""
import time
import pytest


# ── Helpers ────────────────────────────────────────────────────────────────────

MINIMAL_PARAMS = {}

PARAMS_WITH_BOXES = {
    "num_slots": 5,
    "num_y": 2,
    "num_sides": 1,
    "max_ticks": 100,
    "boxes": [
        {"id": "B1", "family": "ZARA",    "arrival_tick": 0},
        {"id": "B2", "family": "BERSHKA", "arrival_tick": 5},
    ],
}

PARAMS_WITH_GENERATOR = {
    "num_slots": 10,
    "num_y": 2,
    "num_sides": 1,
    "max_ticks": 200,
    "generator": {
        "num_boxes": 20,
        "num_destinations": 3,
        "seed": 99,
        "mean_inter_arrival_ticks": 8.0,
        "std_inter_arrival_ticks": 2.0,
    },
}


# ══════════════════════════════════════════════════════════════════════════════
# POST /simulations
# ══════════════════════════════════════════════════════════════════════════════

def test_post_returns_202(client):
    r = client.post("/simulations", json=MINIMAL_PARAMS)
    assert r.status_code == 202


def test_post_returns_sim_id(client):
    r = client.post("/simulations", json=MINIMAL_PARAMS)
    body = r.json()
    assert "sim_id" in body
    assert isinstance(body["sim_id"], str)
    assert len(body["sim_id"]) > 0


def test_post_returns_running_status(client):
    r = client.post("/simulations", json=MINIMAL_PARAMS)
    assert r.json()["status"] == "running"


def test_post_with_explicit_boxes(client):
    r = client.post("/simulations", json=PARAMS_WITH_BOXES)
    assert r.status_code == 202
    assert "sim_id" in r.json()


def test_post_with_generator(client):
    r = client.post("/simulations", json=PARAMS_WITH_GENERATOR)
    assert r.status_code == 202
    assert "sim_id" in r.json()


def test_post_two_simulations_have_different_ids(client):
    id1 = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    id2 = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    assert id1 != id2


# ══════════════════════════════════════════════════════════════════════════════
# GET /simulations
# ══════════════════════════════════════════════════════════════════════════════

def test_list_empty_initially(client):
    r = client.get("/simulations")
    assert r.status_code == 200
    assert r.json() == []


def test_list_contains_posted_simulation(client):
    sim_id = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    sims = client.get("/simulations").json()
    ids = [s["sim_id"] for s in sims]
    assert sim_id in ids


def test_list_contains_all_posted_simulations(client):
    ids = set()
    for _ in range(3):
        ids.add(client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"])
    sims = client.get("/simulations").json()
    returned_ids = {s["sim_id"] for s in sims}
    assert ids == returned_ids


def test_list_record_has_required_fields(client):
    client.post("/simulations", json=MINIMAL_PARAMS)
    sim = client.get("/simulations").json()[0]
    for field in ("sim_id", "status", "created_at", "params"):
        assert field in sim, f"missing field: {field}"


def test_list_completed_sim_shows_done_status(client, completed_sim):
    sims = client.get("/simulations").json()
    record = next(s for s in sims if s["sim_id"] == completed_sim)
    assert record["status"] == "done"


# ══════════════════════════════════════════════════════════════════════════════
# GET /simulations/{id}
# ══════════════════════════════════════════════════════════════════════════════

def test_get_sim_not_found(client):
    r = client.get("/simulations/nonexistent-id")
    assert r.status_code == 404


def test_get_running_sim(client, running_sim):
    r = client.get(f"/simulations/{running_sim}")
    assert r.status_code == 200
    body = r.json()
    assert body["sim_id"] == running_sim
    assert body["status"] == "running"
    assert body["finished_at"] is None


def test_get_completed_sim(client, completed_sim):
    r = client.get(f"/simulations/{completed_sim}")
    assert r.status_code == 200
    body = r.json()
    assert body["sim_id"] == completed_sim
    assert body["status"] == "done"
    assert body["finished_at"] is not None


def test_get_sim_has_params(client, completed_sim):
    body = client.get(f"/simulations/{completed_sim}").json()
    assert "params" in body
    params = body["params"]
    assert "num_slots" in params
    assert "num_y" in params
    assert "num_sides" in params
    assert "max_ticks" in params


# ══════════════════════════════════════════════════════════════════════════════
# GET /simulations/{id}/result
# ══════════════════════════════════════════════════════════════════════════════

def test_result_not_found(client):
    r = client.get("/simulations/no-such-sim/result")
    assert r.status_code == 404


def test_result_while_running_returns_202(client, running_sim):
    r = client.get(f"/simulations/{running_sim}/result")
    assert r.status_code == 202


def test_result_done_returns_200(client, completed_sim):
    r = client.get(f"/simulations/{completed_sim}/result")
    assert r.status_code == 200


def test_result_has_events_and_snapshots_keys(client, completed_sim):
    body = client.get(f"/simulations/{completed_sim}/result").json()
    assert "events" in body
    assert "snapshots" in body


# ── Events structure ───────────────────────────────────────────────────────────

def test_result_events_is_list(client, completed_sim):
    events = client.get(f"/simulations/{completed_sim}/result").json()["events"]
    assert isinstance(events, list)


def test_result_events_count_matches_fake(client, completed_sim):
    from conftest import _make_events
    events = client.get(f"/simulations/{completed_sim}/result").json()["events"]
    assert len(events) == len(_make_events())


def test_result_event_has_all_fields(client, completed_sim):
    events = client.get(f"/simulations/{completed_sim}/result").json()["events"]
    for ev in events:
        assert "type" in ev,         f"event missing 'type': {ev}"
        assert "logical_time" in ev, f"event missing 'logical_time': {ev}"
        assert "box_id" in ev,       f"event missing 'box_id': {ev}"
        assert "pallet_id" in ev,    f"event missing 'pallet_id': {ev}"
        assert "family" in ev,       f"event missing 'family': {ev}"


def test_result_event_types_are_strings(client, completed_sim):
    events = client.get(f"/simulations/{completed_sim}/result").json()["events"]
    for ev in events:
        assert isinstance(ev["type"], str)
        assert len(ev["type"]) > 0


def test_result_event_values_match_fake(client, completed_sim):
    from conftest import _make_events
    events = client.get(f"/simulations/{completed_sim}/result").json()["events"]
    for api_ev, fake_ev in zip(events, _make_events()):
        assert api_ev["type"]         == fake_ev.type
        assert api_ev["logical_time"] == fake_ev.logical_time
        assert api_ev["box_id"]       == fake_ev.box_id
        assert api_ev["pallet_id"]    == fake_ev.pallet_id
        assert api_ev["family"]       == fake_ev.family


def test_result_contains_done_event(client, completed_sim):
    events = client.get(f"/simulations/{completed_sim}/result").json()["events"]
    types = [e["type"] for e in events]
    assert "done" in types


def test_result_contains_box_arrived_event(client, completed_sim):
    events = client.get(f"/simulations/{completed_sim}/result").json()["events"]
    types = [e["type"] for e in events]
    assert "box_arrived" in types


# ── Snapshots structure ────────────────────────────────────────────────────────

def test_result_snapshots_is_list(client, completed_sim):
    snaps = client.get(f"/simulations/{completed_sim}/result").json()["snapshots"]
    assert isinstance(snaps, list)


def test_result_snapshots_count_matches_fake(client, completed_sim):
    from conftest import _make_snapshot
    snaps = client.get(f"/simulations/{completed_sim}/result").json()["snapshots"]
    assert len(snaps) == 1  # one fake snapshot per completed_sim fixture


def test_result_snapshot_has_tick_aisle_pallets(client, completed_sim):
    snap = client.get(f"/simulations/{completed_sim}/result").json()["snapshots"][0]
    assert "tick" in snap
    assert "aisle" in snap
    assert "pallets" in snap


def test_result_snapshot_tick_value(client, completed_sim):
    snap = client.get(f"/simulations/{completed_sim}/result").json()["snapshots"][0]
    assert snap["tick"] == 42


# ── Aisle / Shuttle ────────────────────────────────────────────────────────────

def test_result_snapshot_aisle_has_shuttles(client, completed_sim):
    snap = client.get(f"/simulations/{completed_sim}/result").json()["snapshots"][0]
    assert "shuttles" in snap["aisle"]
    assert isinstance(snap["aisle"]["shuttles"], list)


def test_result_snapshot_shuttle_count(client, completed_sim):
    snap = client.get(f"/simulations/{completed_sim}/result").json()["snapshots"][0]
    # fake has 2 shuttles (y=1, y=2)
    assert len(snap["aisle"]["shuttles"]) == 2


def test_result_snapshot_shuttle_has_all_fields(client, completed_sim):
    shuttle = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]
    for field in (
        "y_level", "position", "phase",
        "is_carrying", "carried_box_id", "carried_box_family", "floor_boxes",
    ):
        assert field in shuttle, f"shuttle missing '{field}'"


def test_result_snapshot_shuttle_position_fields(client, completed_sim):
    pos = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]["position"]
    for field in ("x", "y", "z", "side"):
        assert field in pos, f"position missing '{field}'"
    assert isinstance(pos["x"], int)
    assert isinstance(pos["y"], int)
    assert isinstance(pos["z"], int)
    assert isinstance(pos["side"], int)


def test_result_snapshot_shuttle_y_levels(client, completed_sim):
    shuttles = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"]
    y_levels = [s["y_level"] for s in shuttles]
    assert 1 in y_levels
    assert 2 in y_levels


def test_result_snapshot_shuttle1_idle_not_carrying(client, completed_sim):
    s1 = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]
    assert s1["phase"] == "Idle"
    assert s1["is_carrying"] is False
    assert s1["carried_box_id"] == ""
    assert s1["carried_box_family"] == ""


def test_result_snapshot_shuttle2_carrying(client, completed_sim):
    s2 = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][1]
    assert s2["phase"] == "MovingToDrop"
    assert s2["is_carrying"] is True
    assert s2["carried_box_id"] == "BOX3"
    assert s2["carried_box_family"] == "MASSIMO_DUTTI"


# ── Floor boxes ───────────────────────────────────────────────────────────────

def test_result_snapshot_floor_boxes_present(client, completed_sim):
    s1 = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]
    assert len(s1["floor_boxes"]) == 2


def test_result_snapshot_floor_box_has_all_fields(client, completed_sim):
    floor_box = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]["floor_boxes"][0]
    for field in ("id", "family", "arrival_tick", "position"):
        assert field in floor_box, f"floor_box missing '{field}'"


def test_result_snapshot_floor_box_position_has_xyzside(client, completed_sim):
    pos = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]["floor_boxes"][0]["position"]
    for field in ("x", "y", "z", "side"):
        assert field in pos, f"floor_box position missing '{field}'"


def test_result_snapshot_floor_box_values(client, completed_sim):
    boxes = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]["floor_boxes"]
    ids = [b["id"] for b in boxes]
    assert "BOX1" in ids
    assert "BOX2" in ids


def test_result_snapshot_floor_box_families(client, completed_sim):
    boxes = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]["floor_boxes"]
    families = {b["family"] for b in boxes}
    assert "ZARA" in families
    assert "BERSHKA" in families


def test_result_snapshot_floor_box_positions_correct(client, completed_sim):
    boxes = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][0]["floor_boxes"]
    box1 = next(b for b in boxes if b["id"] == "BOX1")
    assert box1["position"]["x"] == 3
    assert box1["position"]["y"] == 1
    assert box1["position"]["z"] == 1
    assert box1["position"]["side"] == 1


def test_result_snapshot_shuttle2_no_floor_boxes(client, completed_sim):
    s2 = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["aisle"]["shuttles"][1]
    assert s2["floor_boxes"] == []


# ── Pallets ────────────────────────────────────────────────────────────────────

def test_result_snapshot_pallets_is_list(client, completed_sim):
    pallets = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"]
    assert isinstance(pallets, list)


def test_result_snapshot_pallet_count(client, completed_sim):
    pallets = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"]
    assert len(pallets) == 1


def test_result_snapshot_pallet_has_all_fields(client, completed_sim):
    pallet = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"][0]
    for field in ("slot", "family", "placed_count", "reserved_count", "boxes"):
        assert field in pallet, f"pallet missing '{field}'"


def test_result_snapshot_pallet_values(client, completed_sim):
    pallet = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"][0]
    assert pallet["slot"] == 0
    assert pallet["family"] == "ZARA"
    assert pallet["placed_count"] == 2
    assert pallet["reserved_count"] == 1


def test_result_snapshot_pallet_boxes_count(client, completed_sim):
    pallet = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"][0]
    assert len(pallet["boxes"]) == 2


def test_result_snapshot_pallet_box_has_fields(client, completed_sim):
    box = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"][0]["boxes"][0]
    assert "id" in box
    assert "family" in box
    assert "arrival_tick" in box


def test_result_snapshot_pallet_box_no_position(client, completed_sim):
    """Pallet boxes carry no aisle position (they are on the pallet)."""
    box = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"][0]["boxes"][0]
    assert box.get("position") is None


def test_result_snapshot_pallet_boxes_ids(client, completed_sim):
    boxes = client.get(f"/simulations/{completed_sim}/result").json()[
        "snapshots"
    ][0]["pallets"][0]["boxes"]
    ids = [b["id"] for b in boxes]
    assert "PBOX1" in ids
    assert "PBOX2" in ids


# ════════════════════════════════════════════════════════════════════════════���═
# POST + wait for result (integration: goes through the real thread pool)
# ══════════════════════════════════════════════════════════════════════════════

def _wait_done(client, sim_id, timeout=5.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        r = client.get(f"/simulations/{sim_id}")
        if r.json()["status"] == "done":
            return True
        time.sleep(0.05)
    return False


def test_integration_full_lifecycle(client):
    """POST → wait for completion → GET result."""
    sim_id = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    assert _wait_done(client, sim_id), "simulation did not finish in time"

    r = client.get(f"/simulations/{sim_id}/result")
    assert r.status_code == 200
    body = r.json()
    assert len(body["events"]) > 0
    assert len(body["snapshots"]) > 0


def test_integration_result_has_done_event(client):
    sim_id = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    _wait_done(client, sim_id)
    events = client.get(f"/simulations/{sim_id}/result").json()["events"]
    assert any(e["type"] == "done" for e in events)


def test_integration_snapshots_have_tick_field(client):
    sim_id = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    _wait_done(client, sim_id)
    snaps = client.get(f"/simulations/{sim_id}/result").json()["snapshots"]
    for s in snaps:
        assert "tick" in s
        assert isinstance(s["tick"], int)


def test_integration_snapshots_tick_monotonic(client):
    """Ticks must be strictly increasing across snapshots."""
    sim_id = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    _wait_done(client, sim_id)
    ticks = [
        s["tick"]
        for s in client.get(f"/simulations/{sim_id}/result").json()["snapshots"]
    ]
    assert ticks == sorted(ticks)
    assert len(ticks) == len(set(ticks))


def test_integration_each_snapshot_has_aisle_shuttles(client):
    sim_id = client.post("/simulations", json=MINIMAL_PARAMS).json()["sim_id"]
    _wait_done(client, sim_id)
    snaps = client.get(f"/simulations/{sim_id}/result").json()["snapshots"]
    for s in snaps:
        assert "aisle" in s
        assert "shuttles" in s["aisle"]
        assert isinstance(s["aisle"]["shuttles"], list)


def test_integration_shuttles_have_position(client):
    sim_id = client.post("/simulations", json=PARAMS_WITH_BOXES).json()["sim_id"]
    _wait_done(client, sim_id)
    snaps = client.get(f"/simulations/{sim_id}/result").json()["snapshots"]
    for s in snaps:
        for shuttle in s["aisle"]["shuttles"]:
            pos = shuttle["position"]
            for field in ("x", "y", "z", "side"):
                assert field in pos


def test_integration_shuttles_have_valid_phase(client):
    valid_phases = {"Idle", "MovingToPickup", "Loading", "MovingToDrop", "Unloading"}
    sim_id = client.post("/simulations", json=PARAMS_WITH_BOXES).json()["sim_id"]
    _wait_done(client, sim_id)
    snaps = client.get(f"/simulations/{sim_id}/result").json()["snapshots"]
    for s in snaps:
        for shuttle in s["aisle"]["shuttles"]:
            assert shuttle["phase"] in valid_phases, f"unknown phase: {shuttle['phase']}"


def test_integration_floor_boxes_have_position(client):
    sim_id = client.post("/simulations", json=PARAMS_WITH_BOXES).json()["sim_id"]
    _wait_done(client, sim_id)
    snaps = client.get(f"/simulations/{sim_id}/result").json()["snapshots"]
    for s in snaps:
        for shuttle in s["aisle"]["shuttles"]:
            for box in shuttle["floor_boxes"]:
                assert box["position"] is not None
                for field in ("x", "y", "z", "side"):
                    assert field in box["position"]
