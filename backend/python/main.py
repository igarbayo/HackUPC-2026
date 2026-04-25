import os
from dotenv import load_dotenv
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from routers import simulations, ws

load_dotenv()

app = FastAPI(title="Warehouse Scheduler")

app.add_middleware(
    CORSMiddleware,
    allow_origins=[os.environ["FRONTEND_ORIGIN"]],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(simulations.router, prefix="/simulations", tags=["simulations"])
app.include_router(ws.router, tags=["websocket"])
