# Trajectory Generator

Code Model
---

### 1. Smooth Trajectory Jeneration 
I began with the basic smooth trajectory generation implementation described in the classroom's project Q&A. This consists of: 
  
  * Use 5 points to define a spline (in vehicle coordinates) over the next ~100m 
    1. Second to last point of previous path OR 1m behind the cars current location (estimated using present yaw) if no previous path
    2. Last point of previous path OR car's current position if no previous path 
    3. 30m beyond point 2
    4. 30m beyond point 3
    5. 30m beyond point 4

  * Copy any remaining waypoints from the previous path (usually about 47/50 are generated this way)

  * Use a linear approximation to calculate the appropriate distance to separate each waypoint to achieve the target speed (based on each being reached in .02 seconds) 

  * Finish out the waypoint list using the calculated distance from the linear approximation and the spline

### 2. Slow Down for Traffic
Initially I recreated the slow-down functionality as described in the Q&A - but did not like the way that the velocity would oscillate significantly. Because that algorithm only implemented changes starting at the end of the current path there was significant lag between when the need to slow was sensed and when the car actually slowed. I corrected for this by ditching all but 3 waypoints from the previous path and adding 47 new ones in accordance with the newly increased/decreased speed. I did find this to make the acceleration more sensitive to the stepwise speed increment, and had to lower it from the value in the video (.224) to .100.

### 3. Lane Changing
When the car finds itself behind a car moving more slowly than its target (49.5 mph), it triggers a sweep of all 3 lanes to determine the "safe distance" available in each lane. I define this as the distance in the positive s frenet coordinate direction from a space 10m behind the car's current position to the next nearest vehicle. If the adjacent lane is clear - a lane change is executed. I set a 45m safe distance threshold which seems to work well. If the car is in the center lane and both the right and left lanes are clear, it will merge into the lane with the larger safe distance. If a lane change cannot be executed, the car will slow to match the speed of traffic until an adjacent lane becomes clear. 



Build Instructions
---

1. Clone this repo.
2. Make a build directory: `mkdir build && cd build`
3. Compile: `cmake .. && make`
4. Run it: `./path_planning`.

Here is the data provided from the Simulator to the C++ Program


Dependencies
---

* Udacity Term 3 Simulator
  * You can download the Term3 Simulator which contains the Path Planning Project from the [releases tab (https://github.com/udacity/self-driving-car-sim/releases/tag/T3_v1.2). 
* cmake >= 3.5
  * All OSes: [click here for installation instructions](https://cmake.org/install/)
* make >= 4.1
  * Linux: make is installed by default on most Linux distros
  * Mac: [install Xcode command line tools to get make](https://developer.apple.com/xcode/features/)
  * Windows: [Click here for installation instructions](http://gnuwin32.sourceforge.net/packages/make.htm)
* gcc/g++ >= 5.4
  * Linux: gcc / g++ is installed by default on most Linux distros
  * Mac: same deal as make - [install Xcode command line tools]((https://developer.apple.com/xcode/features/)
  * Windows: recommend using [MinGW](http://www.mingw.org/)
* [uWebSockets](https://github.com/uWebSockets/uWebSockets)
  * Run either `install-mac.sh` or `install-ubuntu.sh`.
  * If you install from source, checkout to commit `e94b6e1`, i.e.
    ```
    git clone https://github.com/uWebSockets/uWebSockets 
    cd uWebSockets
    git checkout e94b6e1
    ```

