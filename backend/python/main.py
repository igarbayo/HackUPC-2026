import argparse
import os
from dotenv import load_dotenv
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

load_dotenv()

# parse_known_args so this works both as `python main.py --csv-path ...`
# and when imported by uvicorn (csv_path will be None in that case)
_p = argparse.ArgumentParser(add_help=False)
_p.add_argument("--csv-path", default=None, dest="csv_path")
_cli, _ = _p.parse_known_args()

import scheduler as sched  # noqa: E402 — must come after path parse
sched.set_silo_csv(_cli.csv_path)

from routers import simulations, ws  # noqa: E402

app = FastAPI(title="Warehouse Scheduler")

app.add_middleware(
    CORSMiddleware,
    allow_origins=os.environ.get("FRONTEND_ORIGIN", "http://localhost:3000").split(","),
    allow_credentials=False,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(simulations.router, prefix="/simulations", tags=["simulations"])
app.include_router(ws.router, tags=["websocket"])

if __name__ == "__main__":
    import uvicorn
    uvicorn.run("main:app", host="0.0.0.0", port=8000)
