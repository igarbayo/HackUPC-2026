# Multi-Agent Retrieval Heuristics: Cooperative Coordination

## 1. Context & Problem Statement
In the automated warehouse environment, Robots act as tactical commanders for Aisle Shuttles, managing **4 Pallet Slots** each. The primary challenge in a multi-robot system is **Family Fragmentation**, where multiple robots open pallets for the same box family, leading to half-full pallets and system congestion.

To scale beyond a single robot, the system must move from a greedy "Sense-Decide-Act" loop to a **Cooperative Coordination** model.

---

## 2. The Cooperative Scoring Function ($S_{coop}$)
To prevent robots from competing for the same stock, the base scoring function is modified with a **Coordination Multiplier ($C$)** and a **Congestion Penalty ($P$)**.

$$S_{coop}(f, r) = \left( \frac{\text{AvailableBoxes}(f)}{\text{AvgDistanceToPort}(f) + 1} \right) \times C(f, r) - P(f)$$

### A. Coordination Multiplier ($C$)
This multiplier implements **Family Locking** by influencing the desirability of a family based on global metadata:
* **Active Claim (Other Robot):** $C = 0.1$. Discourages opening a redundant pallet unless the backlog is extreme (e.g., $>18$ boxes).
* **Existing Slot (Current Robot):** $C = 1.5$. Encourages the robot to prioritize finishing families it has already started.
* **Unclaimed:** $C = 1.0$. Standard priority.

### B. Congestion Penalty ($P$)
To achieve **Load Balancing**, robots penalize families stored in levels currently serviced by other agents:
* $P(f) = \text{ActiveShuttlesInLevel}(f) \times \text{Weight}$
* This ensures Robot B selects families from different aisle levels than Robot A, minimizing shuttle wait times and maximizing throughput.

---

## 3. Dynamic Coordination Mechanisms

### Virtual Reservations (Soft Locking)
Rather than a hard binary lock, robots use **Virtual Reservations** to manage global stock visibility:
1.  When a robot opens a pallet for Family $X$, it calculates the remaining capacity ($N$) of that pallet.
2.  It broadcasts a reservation for $N$ boxes to the global metadata.
3.  Other robots subtract $N$ from the `AvailableBoxes(f)` count during their scoring phase, preventing them from seeing "phantom" availability that is already spoken for.

### Predictive Pre-fetching
To minimize **Idle Time** and meet throughput KPIs, robots transition from reactive to predictive retrieval:
* **Trigger:** When a pallet reaches **10/12 boxes** (83% capacity).
* **Action:** The robot runs the $S_{coop}$ function for its next available slot and initiates the shuttle request for the first box of the *next* family.
* **Outcome:** The first box of the new family arrives at the port just as the full pallet is dispatched.

---

## 4. Refined Eviction Strategy: Opportunity Cost
The "Forced Eviction" logic is upgraded from a simple completeness check to an **Opportunity Cost** evaluation:

$$E = \text{CompletenessScore}(p) - \text{MaxPotentialScore}(\text{NewFamily})$$

* **Logic:** A robot will only evict a stale, half-full pallet if the $S_{coop}$ of a new arriving box is significantly higher than the likelihood of the current pallet ever finishing.
* **Metric:** If $E < 0$, the slot is cleared for the higher-value family to ensure the 25% system capacity per slot is always used by the highest-performing family.

---

## 5. Optimization Goals & KPI Impact

| Goal | Mechanism | Expected Outcome |
| :--- | :--- | :--- |
| **Utilization** | Coordination Multiplier & Reservations | Near 100% of pallets dispatched with 12/12 boxes. |
| **Throughput** | Congestion Penalty & Pre-fetching | Reduced shuttle wait times and zero-gap pallet transitions. |
| **Scaling** | Global Metadata Awareness | Linear performance gains when adding 3rd or 4th robots. |