# Algorithms for Greater Logistics Agility

At the forefront of the supply chain, efficiency is not an option but a vital necessity. Our company’s distribution centers operate like living organisms, with automated silos serving as their central lungs. These systems manage the temporary storage of goods that are shipped twice a week to thousands of physical stores and e-commerce platforms.

This is a massive infrastructure: each silo has the capacity to hold hundreds of thousands of boxes. The real technical challenge lies not only in space, but in orchestrating the movement of boxes. As participants in this hackathon, your mission is to design the intelligence that governs these flows. You must design the input and output algorithms to execute a perfect choreography that minimizes response times and maximizes the system’s agility.

## Physical Architecture and Coordinate System
The silo is organized as a high-density three-dimensional grid. Each unit load (box) must be perfectly located so that industrial control systems can interact with it.

- Aisle: Each silo has one or more.
- Side: Each aisle has two sides with shelving (Left: 01, Right: 02).
- X-Coordinate (Length): Represents the distance from the head (entrance/exit) to the end of the aisle. Range: 000 to 999.
- Y-Coordinate (Height): The vertical level on the shelf.
- Z-Coordinate (Depth): Indicates whether the box is on the first, second, or third depth on the shelf.

Each position is defined by an 11-digit string:
01_02_003_04_01, where AISLE_SIDE_X_Y_Z
 
## Identification of the boxes
Each box has a unique identity via a 20-digits code.
30100280122093090329
- Source: 3010028 (warehouse)
- Destination: 01220930
- Bulk number: 90329

## Storage Logic
Boxes arrive from the sorting areas, and the system must assign them an aisle, the side, and the X, Y, Z coordinates of the location in real time.

Each box is placed in a single position.
We will create a silo with the following structure:
- Aisles: (from 1 to 4)	2
- Sides: (1 and 2)	60
- X: (from 1 to 60)	8
- Y: (from 1 to 8)	2
- Z: (1 and 2)

The boxes will be added to the queue of the corresponding shuttle. It will not be possible to place a box at z=2 if z=1 is occupied. Similarly, we cannot retrieve a box at z=2 without first moving the box at z=1 if z=1 is occupied. If you need to get the box in z=2, you need to relocate the box in z=1 to another location.

## Movement Dynamics: The Shuttles
Shuttles are automated carts that move longitudinally along the aisles and can pick up boxes, transport them, and deposit them at various positions in the silo.
- Constraint: There is only one shuttle per height level (Y). This resource is shared: it must handle both incoming and outgoing boxes.
- Initial State: At the start of the simulation (t=0), all shuttles are at the head (x=0).
- Time Formula: The execution time of a movement depends on the travel distance of the shuttle, plus a fixed time of 10 seconds for the handling operation (either a pick or drop of a box):
    t = 10 + d
(being 10 a fixed handling time in seconds, and d is the travel distance in X).

## Example of Cycle Calculation (pick and drop)
Suppose a shuttle is at X=5, a box arrives at the front, and we want to store it at x=20:
1.	Trip to Head: Movement from X=5 to X=0. Time: 10 + (5-0) = 15s.
2.	Trip to Destination: Movement from X=0 to X=20. Time: 10 + (20-0) = 30s.
3.	Total Accumulated Time: 15 + 30 = 45 seconds

## Shipping Process and Pallet Formation
Shipping is not done randomly, box by box, but rather through the consolidation of pallets. A pallet is a set of boxes of the same destination.
- Pallet Rule: A pallet consists of 12 boxes with the same Destination.
- Palletizing: We have 2 robots that can palletize 4 pallets each at any given time, so the system can manage up to 8 reserved pallets simultaneously. Each time a pallet is shipped, we can retrieve a new one.
- Dynamic Priority: When a pallet is reserved, all its boxes are reserved at the same time. Even if Pallet 1 is reserved before Pallet 2, if we want a box from Pallet 2 to be picked first because it is in a more favorable position, the algorithm can (and should) decide to pick the box from Pallet 2 first to optimize the flow.
 
## The Challenge Goal
Your main objective is to develop input, output, and queue management algorithms for the shuttles that minimize total operation time.

### Success Metrics
1. Full Pallets: Percentage of pallets from which all 12 boxes are shipped.
2. Throughput (Output Capacity): Number of pallets completed per unit of time. We can consider the average time per pallet.

Welcome to Hack the Flow. 
May your code optimize the heartbeat of our logistics!
