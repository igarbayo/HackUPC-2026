- We accept 20 destinations (minimum). If you are able to do the simulation with different numer of destinations, it will be great. Maybe, you could consider the number of destinations as an input parameter.

- Each aisle has 8 levels, with a shuttle per level (the Y coordinate). Those shuttles cannot move between levels.
An aisle has an empty space in the middle where each shuttle moves in the x-axis to any position in the corresponding level.
A shuttle can PICK or DROP a box in 4 positions. They correspond to Z=1 and Z=2 on the left side (side: 01), and Z=1 and Z=2 on the right side (side: 02).

- The boxes will arrive online during the simulation at a pace of 1000 boxes/hour. We have shared a semi-empty silo file as a possible starting point, but you can design two or different starting situations (with higher filling percentage) to validate your algorithm and test the robustness of your solution.