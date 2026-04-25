# Metrics Calculation Guide

This document explains how to calculate the simulation success metrics in the Python backend using the event log emitted by the C++ engine.

## 1. Data Source: The Event Log

The C++ simulation returns a list of `Event` objects. To calculate metrics, the Python script should iterate through these events after the simulation is `done`.

### Key Event Types
The most relevant event for output metrics is `pallet_dispatched`. This event is emitted whenever a pallet leaves the robot station (either because it reached the capacity of 12 boxes or because the simulation ended).

**Event Fields for Metrics:**
- `type`: Must be `"pallet_dispatched"`.
- `box_count`: The number of boxes physically present on the pallet at the time of dispatch.
- `family`: The destination/family of the boxes on that pallet.

## 2. Metric Definitions

Assuming `events` is a list of events from a finished simulation:

### Total Pallets Sent
The total count of all pallets that reached the dispatch phase.
```python
total_pallets_sent = len([e for e in events if e.type == "pallet_dispatched"])
```

### Full Pallets
The number of pallets that were dispatched with the maximum capacity (12 boxes).
```python
filled_pallets = len([e for e in events if e.type == "pallet_dispatched" and e.box_count == 12])

# Percentage (Success Metric #1)
full_pallet_ratio = (filled_pallets / total_pallets_sent) * 100 if total_pallets_sent > 0 else 0
```

### Total Boxes Sent
The sum of all boxes that were successfully palletized and dispatched.
```python
total_boxes_sent = sum(e.box_count for e in events if e.type == "pallet_dispatched")
```

### Average Fill Rate
The average occupancy level of all dispatched pallets.
```python
# Capacity per pallet is 12
avg_fill_rate = (total_boxes_sent / (total_pallets_sent * 12)) * 100 if total_pallets_sent > 0 else 0
```

### Boxes by Family
A breakdown of throughput categorized by destination.
```python
boxes_by_family = {}
for e in events:
    if e.type == "pallet_dispatched":
        boxes_by_family[e.family] = boxes_by_family.get(e.family, 0) + e.box_count
```

## 3. Required Implementation Updates

To support these calculations, the following updates are required in the Python backend to ensure the `box_count` field (available in C++) is correctly passed to the Python environment:

1.  **`backend/python/models.py`**: Add `box_count: int = -1` to the `EventModel` class.
2.  **`backend/python/scheduler.py`**: Update the `_event_to_model` converter function to include `box_count=e.box_count`.
