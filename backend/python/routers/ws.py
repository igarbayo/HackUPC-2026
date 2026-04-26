"""WebSocket endpoint: streams per-tick snapshots for a simulation."""
from __future__ import annotations
import asyncio
import json

from fastapi import APIRouter, WebSocket, WebSocketDisconnect

import store

router = APIRouter()

_DEFAULT_SPEED = 1.0  # ticks per second


@router.websocket("/simulations/{sim_id}/stream")
async def stream_simulation(websocket: WebSocket, sim_id: str):
    await websocket.accept()

    if sim_id not in store.simulations:
        await websocket.send_json({"type": "error", "detail": "Simulation not found"})
        await websocket.close()
        return

    if store.simulations[sim_id].status == "error":
        await websocket.send_json({"type": "error", "detail": "Simulation failed"})
        await websocket.close()
        return

    speed = _DEFAULT_SPEED
    paused = False
    idx = 0

    # A persistent background task lets the event loop receive messages during
    # any asyncio.sleep — unlike wait_for(timeout=0), which never yields enough.
    recv_task: asyncio.Task = asyncio.create_task(websocket.receive_text())

    try:
        while True:
            if recv_task.done():
                try:
                    msg = json.loads(recv_task.result())
                    action = msg.get("action")
                    if action == "set_speed":
                        speed = float(msg.get("value", speed))
                    elif action == "pause":
                        paused = True
                    elif action == "resume":
                        paused = False
                except Exception:
                    pass
                recv_task = asyncio.create_task(websocket.receive_text())

            if paused:
                await asyncio.sleep(0.05)
                continue

            snapshots = store.snapshots.get(sim_id, [])
            if idx < len(snapshots):
                await websocket.send_text(snapshots[idx].model_dump_json())
                idx += 1
                if speed > 0:
                    await asyncio.sleep(1.0 / speed)
            elif store.done.get(sim_id):
                break
            else:
                await asyncio.sleep(0.01)

        total = len(store.snapshots.get(sim_id, []))
        await websocket.send_json({"type": "done", "total_ticks": total})
    except WebSocketDisconnect:
        pass
    finally:
        recv_task.cancel()
