#include <fstream>
#include <math.h>
#include <uWS/uWS.h>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include "Eigen-3.3/Eigen/Core"
#include "Eigen-3.3/Eigen/QR"
#include "json.hpp"
#include "spline.h" 

using namespace std;

// for convenience
using json = nlohmann::json;

// For converting back and forth between radians and degrees.
constexpr double pi() { return M_PI; }
double deg2rad(double x) { return x * pi() / 180; }
double rad2deg(double x) { return x * 180 / pi(); }

// Checks if the SocketIO event has JSON data.
// If there is data the JSON object in string format will be returned,
// else the empty string "" will be returned.
string hasData(string s) {
  auto found_null = s.find("null");
  auto b1 = s.find_first_of("[");
  auto b2 = s.find_first_of("}");
  if (found_null != string::npos) {
    return "";
  } else if (b1 != string::npos && b2 != string::npos) {
    return s.substr(b1, b2 - b1 + 2);
  }
  return "";
}

double distance(double x1, double y1, double x2, double y2)
{
	return sqrt((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));
}
int ClosestWaypoint(double x, double y, const vector<double> &maps_x, const vector<double> &maps_y)
{

	double closestLen = 100000; //large number
	int closestWaypoint = 0;

	for(int i = 0; i < maps_x.size(); i++)
	{
		double map_x = maps_x[i];
		double map_y = maps_y[i];
		double dist = distance(x,y,map_x,map_y);
		if(dist < closestLen)
		{
			closestLen = dist;
			closestWaypoint = i;
		}

	}

	return closestWaypoint;

}

int NextWaypoint(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{

	int closestWaypoint = ClosestWaypoint(x,y,maps_x,maps_y);

	double map_x = maps_x[closestWaypoint];
	double map_y = maps_y[closestWaypoint];

	double heading = atan2((map_y-y),(map_x-x));

	double angle = fabs(theta-heading);
  angle = min(2*pi() - angle, angle);

  if(angle > pi()/4)
  {
    closestWaypoint++;
  if (closestWaypoint == maps_x.size())
  {
    closestWaypoint = 0;
  }
  }

  return closestWaypoint;
}

// Transform from Cartesian x,y coordinates to Frenet s,d coordinates
vector<double> getFrenet(double x, double y, double theta, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int next_wp = NextWaypoint(x,y, theta, maps_x,maps_y);

	int prev_wp;
	prev_wp = next_wp-1;
	if(next_wp == 0)
	{
		prev_wp  = maps_x.size()-1;
	}

	double n_x = maps_x[next_wp]-maps_x[prev_wp];
	double n_y = maps_y[next_wp]-maps_y[prev_wp];
	double x_x = x - maps_x[prev_wp];
	double x_y = y - maps_y[prev_wp];

	// find the projection of x onto n
	double proj_norm = (x_x*n_x+x_y*n_y)/(n_x*n_x+n_y*n_y);
	double proj_x = proj_norm*n_x;
	double proj_y = proj_norm*n_y;

	double frenet_d = distance(x_x,x_y,proj_x,proj_y);

	//see if d value is positive or negative by comparing it to a center point

	double center_x = 1000-maps_x[prev_wp];
	double center_y = 2000-maps_y[prev_wp];
	double centerToPos = distance(center_x,center_y,x_x,x_y);
	double centerToRef = distance(center_x,center_y,proj_x,proj_y);

	if(centerToPos <= centerToRef)
	{
		frenet_d *= -1;
	}

	// calculate s value
	double frenet_s = 0;
	for(int i = 0; i < prev_wp; i++)
	{
		frenet_s += distance(maps_x[i],maps_y[i],maps_x[i+1],maps_y[i+1]);
	}

	frenet_s += distance(0,0,proj_x,proj_y);

	return {frenet_s,frenet_d};

}

// Transform from Frenet s,d coordinates to Cartesian x,y
vector<double> getXY(double s, double d, const vector<double> &maps_s, const vector<double> &maps_x, const vector<double> &maps_y)
{
	int prev_wp = -1;

	while(s > maps_s[prev_wp+1] && (prev_wp < (int)(maps_s.size()-1) ))
	{
		prev_wp++;
	}

	int wp2 = (prev_wp+1)%maps_x.size();

	double heading = atan2((maps_y[wp2]-maps_y[prev_wp]),(maps_x[wp2]-maps_x[prev_wp]));
	// the x,y,s along the segment
	double seg_s = (s-maps_s[prev_wp]);

	double seg_x = maps_x[prev_wp]+seg_s*cos(heading);
	double seg_y = maps_y[prev_wp]+seg_s*sin(heading);

	double perp_heading = heading-pi()/2;

	double x = seg_x + d*cos(perp_heading);
	double y = seg_y + d*sin(perp_heading);

	return {x,y};

}

int main() {
  uWS::Hub h;

  // Load up map values for waypoint's x,y,s and d normalized normal vectors
  vector<double> map_waypoints_x;
  vector<double> map_waypoints_y;
  vector<double> map_waypoints_s;
  vector<double> map_waypoints_dx;
  vector<double> map_waypoints_dy;

  // Waypoint map to read from
  string map_file_ = "../data/highway_map.csv";
  // The max s value before wrapping around the track back to 0
  double max_s = 6945.554;

  ifstream in_map_(map_file_.c_str(), ifstream::in);

  string line;
  while (getline(in_map_, line)) {
  	istringstream iss(line);
  	double x;
  	double y;
  	float s;
  	float d_x;
  	float d_y;
  	iss >> x;
  	iss >> y;
  	iss >> s;
  	iss >> d_x;
  	iss >> d_y;
  	map_waypoints_x.push_back(x);
  	map_waypoints_y.push_back(y);
  	map_waypoints_s.push_back(s);
  	map_waypoints_dx.push_back(d_x);
  	map_waypoints_dy.push_back(d_y);
  }

  int lane = 1; 

  double ref_vel = 0.0; //mph

  h.onMessage([&map_waypoints_x,&map_waypoints_y,&map_waypoints_s,&map_waypoints_dx,&map_waypoints_dy, &lane, &ref_vel](uWS::WebSocket<uWS::SERVER> ws, char *data, size_t length,
                     uWS::OpCode opCode) {
    // "42" at the start of the message means there's a websocket message event.
    // The 4 signifies a websocket message
    // The 2 signifies a websocket event
    //auto sdata = string(data).substr(0, length);
    //cout << sdata << endl;
	  




    if (length && length > 2 && data[0] == '4' && data[1] == '2') {

      auto s = hasData(data);

      if (s != "") {
        auto j = json::parse(s);
        
        string event = j[0].get<string>();
        
        if (event == "telemetry") {
          // j[1] is the data JSON object
          
        	// Main car's localization Data
          	double car_x = j[1]["x"];
          	double car_y = j[1]["y"];
          	double car_s = j[1]["s"];
          	double car_d = j[1]["d"];
          	double car_yaw = j[1]["yaw"];
          	double car_speed = j[1]["speed"];

          	// Previous path data given to the Planner
          	auto previous_path_x = j[1]["previous_path_x"];
          	auto previous_path_y = j[1]["previous_path_y"];
          	// Previous path's end s and d values 
          	double end_path_s = j[1]["end_path_s"];
          	double end_path_d = j[1]["end_path_d"];

          	// Sensor Fusion Data, a list of all other cars on the same side of the road.
          	auto sensor_fusion = j[1]["sensor_fusion"];

          	int prev_size = previous_path_x.size();
          	double curr_car_s = car_s;

          	//****************Car Avoidance Module - Start******************

          	// if there are any points left from the previous commanded list of waypoints, 
          	// then set the "own car collision comparison s coordinate" to the last point
          	if(prev_size > 0)
          	{	
          		car_s = end_path_s;
          	}

          	// flag for whether altered behavior is called for
          	bool too_close = false;
          	double speed_match = 0.0; 

          	// cycle through all other cars to determine if they pose a risk
          	for(int i = 0; i < sensor_fusion.size(); i++)
          	{
          		// is this car in my lane? (d is frenet "cte" coord of other vehicle)
          		float d = sensor_fusion[i][6];
          		if(d < (2+4*lane+2) && d > (2+4*lane-2) )
          		{
          			double vx = sensor_fusion[i][3];
          			double vy = sensor_fusion[i][4];
          			double check_speed = sqrt(vx*vx+vy*vy);
          			double check_car_s = sensor_fusion[i][5];

          			// approximate future state of this car relative to the pre-defined 
          			// future state of our car (car_s)
          			check_car_s += ((double)prev_size*.02*check_speed);

          			// essentially - will this car be within 30m in front of me? 
          			if((check_car_s > car_s) && ((check_car_s - car_s) < 30))
          			{
          				too_close = true;
          				speed_match = check_speed;

          				//*****************************************Lane Change Module - Start***************************8
          				double safe_dist_left = 1000.0; 
          				double safe_dist_center = 1000.0;
          				double safe_dist_right = 1000.0; 
          				double safety_threshold = 45.0; 

          				//Evaluate Lane Change Left (if lane = 1 or 2)
          					//Establish baseline s coord based on current pose - constant
          					//How far is the first car ahead of that baseline?  
          				double baseline_s = curr_car_s - 10.0; 

          				//loop through every car - if left or right take data

      					for(int i = 0; i < sensor_fusion.size(); i++)
      					{
      						float check_car_d = sensor_fusion[i][6];
      						float check_car_s = sensor_fusion[i][5];
      						
      						if(check_car_s > baseline_s)
      						{
          						//left
          						if((check_car_d < 4) && (check_car_d > 0))
          						{
          							double new_safe_dist_left = check_car_s - baseline_s; 
          							if(new_safe_dist_left < safe_dist_left)
          							{
          								safe_dist_left = new_safe_dist_left;
          							}

          						}
          						//center
          						else{if((check_car_d > 4) && (check_car_d < 8))
          						{
          							double new_safe_dist_center = check_car_s - baseline_s; 
          							if(new_safe_dist_center < safe_dist_center)
          							{
          								safe_dist_center = new_safe_dist_center;
          							}
          						}
          						//right
          						else{if(check_car_d > 8)
          						{
          							double new_safe_dist_right = check_car_s - baseline_s; 
          							if(new_safe_dist_right < safe_dist_right)
          							{
          								safe_dist_right = new_safe_dist_right;
          							}
          						}}}
          					}

      					}
      					
      					//if in an outside lane and the center is free . . . move to center
      					if(((lane == 0)||(lane == 2)) && (safe_dist_center > safety_threshold))
      					{
      						lane = 1; 
      						//cout << "Moving to center lane. Safe Distance is: " << safe_dist_center << endl;
      					}
      					//if in center, move to whichever outside lane has more free space (cuz dis is 'merica)
      					else{if((lane == 1) && ((safe_dist_left > safety_threshold)||(safe_dist_right > safety_threshold)))
      					{
      						if(safe_dist_right > safe_dist_left)
      						{
      							lane = 2; 
      							//cout << "Moving to right lane. Safe Distance is: " << safe_dist_right << endl;
      						}
      						else
      						{
      							lane = 0; 
      							//cout << "Moving to left lane. Safe Distance is: " << safe_dist_left << endl;
      						}
      					}
      					//otherwise (ie - no available lane changes) you need to speed match
      					else
      					{
      						if(ref_vel > speed_match)
          					{
			          			//ref_vel -= .224;
			          			// .224 was giving occasional accel errors . . . 

			          			ref_vel -= .10;
			          			ref_vel = max(ref_vel, speed_match);

			          			//always only adding on to the end causes response delay and "start-stop-start" behavior. 
			          			//better to keep 3 points for smoothness - but mostly re-write path for better following behavior
			          			prev_size = 3; 	
          					}
      					}}
          			}
          				//Evaluate Lane Change Right (if lane = 0 or 1)
          					//Same procedure as left

          				//If safedistleft >= threshold && >= safedistright, change left
          				//If safedistright >= threshold && > safedistleft, change right 
          				//If both are shy of threshold, then do speedmatching below 



          				//Execute 

          		}
          	}
          	
          	//**********************Lane Change Module - End**********************************************

          	// if(too_close)
          	// {
          	// 	if(ref_vel > speed_match)
          	// 	{
          	// 		//ref_vel -= .224;
          	// 		// .224 was giving occasional accel errors . . . 

          	// 		ref_vel -= .10;
          	// 		ref_vel = max(ref_vel, speed_match);

          	// 		//always only adding on to the end causes response delay and "start-stop-start" behavior. 
          	// 		//better to keep 3 points for smoothness - but mostly re-write path for better following behavior
          	// 		prev_size = 3; 	
          	// 	}
          	// }
          	
          	//Speed Back Up if Free
          	if((!too_close) &&(ref_vel < 49.5))
          	{
          		//ref_vel += .224;
          		ref_vel += .10;
          	}



          	//****************Car Avoidance Module - End********************


          	//****************General Path Planning - Start******************

          	vector<double> ptsx;
          	vector<double> ptsy; 

          	double ref_x = car_x; 
          	double ref_y = car_y; 
          	double ref_yaw = deg2rad(car_yaw);

          	// if length of remaining previous path is less than two waypoints - just walk balk from the car's 
          	// current position to help smooth the spline - not sure when this would ever come into play though? 
          	if(prev_size < 2)
          	{
          		// use 2 points to make path tangeant to car

          		// in both of these lines there's an unspoken "step back 1m" (it's from the not-written 1m * cos(car_yaw) i believe)
          		double prev_car_x = car_x - cos(ref_yaw);
          		double prev_car_y = car_y - sin(ref_yaw);

          		ptsx.push_back(prev_car_x);
          		ptsx.push_back(car_x);

          		ptsy.push_back(prev_car_y);
          		ptsy.push_back(car_y);
          	}

          	else
          	{
          		//redefine reference states as previous path end point and second to end point (to enable tangential splining)
          		ref_x = previous_path_x[prev_size-1];
          		ref_y = previous_path_y[prev_size-1];

          		double ref_x_prev = previous_path_x[prev_size-2];
          		double ref_y_prev = previous_path_y[prev_size-2];
          		ref_yaw = atan2(ref_y-ref_y_prev, ref_x-ref_x_prev);

          		ptsx.push_back(ref_x_prev);
          		ptsx.push_back(ref_x);

          		ptsy.push_back(ref_y_prev);
          		ptsy.push_back(ref_y);
          	}

          	// In Frenet add 3 points - each 30m ahead of the last - to create (in total) a 5 - waypoint path 
          	// 2 of these 5 are very close to each other either at the end of the current path or near the current pose
          	// and 3 of them are 30, 60, and 90m ahead of the current pose respectively. (but what happens if 30m
          	// ahead of current pose is closer than the 2 at the end of the path? - probably not a problem?) 

          	vector<double> next_wp0 = getXY(car_s+30,(2+4*lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);
          	vector<double> next_wp1 = getXY(car_s+60,(2+4*lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);
          	vector<double> next_wp2 = getXY(car_s+90,(2+4*lane),map_waypoints_s,map_waypoints_x,map_waypoints_y);

          	ptsx.push_back(next_wp0[0]);
          	ptsx.push_back(next_wp1[0]);
          	ptsx.push_back(next_wp2[0]);

          	ptsy.push_back(next_wp0[1]);
          	ptsy.push_back(next_wp1[1]);
          	ptsy.push_back(next_wp2[1]);

          	//shift reference to car's coordinate frame 
          	for (int i = 0; i < ptsx.size(); i++)
          	{
          		double shift_x = ptsx[i]-ref_x;
          		double shift_y = ptsy[i]-ref_y;

          		ptsx[i] =(shift_x * cos(0-ref_yaw)-shift_y*sin(0-ref_yaw));
          		ptsy[i] =(shift_x * sin(0-ref_yaw)+shift_y*cos(0-ref_yaw));

          	}

          	//create spline
          	tk::spline s; 

          	// set x-y points as spline anchors
          	s.set_points(ptsx, ptsy);

          	// create the vectors which will pass the waypoints
          	vector<double> next_x_vals;
          	vector<double> next_y_vals; 

          	// mostly will be using points from last time
          	for(int i = 0; i < prev_size; i++)
          	{
          		next_x_vals.push_back(previous_path_x[i]);
          		next_y_vals.push_back(previous_path_y[i]);
          	}

          	// calculation values for defining spline points to elicit desired speed
          	double target_x = 30.0;
          	double target_y = s(target_x);
          	double target_dist = sqrt((target_x)*(target_x)+(target_y)*(target_y)); 

          	double x_add_on = 0; 

          	// use spline and calculated values to fill and publish path planner

          	for(int i = 1; i <= 50 - prev_size; i++)
          	{
          		double N = (target_dist/(.02*ref_vel/2.24));
          		double x_point = x_add_on + (target_x)/N;
          		double y_point = s(x_point);

          		x_add_on = x_point; 

          		double x_ref = x_point; 
          		double y_ref = y_point; 

          		//rotate back into global coords for publishing

          		x_point = (x_ref * cos(ref_yaw) - y_ref*sin(ref_yaw));
          		y_point = (x_ref * sin(ref_yaw) + y_ref*cos(ref_yaw));

          		//remember that x_ref is actually the x coord of the last kept point from prev path
          		//aka the "car's current position" for calc purposes (but it's really a future pose)
          		x_point += ref_x;
          		y_point += ref_y; 

          		//publish
          		next_x_vals.push_back(x_point);
          		next_y_vals.push_back(y_point);
          	}

          	//**************************General Path Planning - End**************************

          	json msgJson;

          	/*
          	vector<double> next_x_vals;
          	vector<double> next_y_vals;


          	// TODO: define a path made up of (x,y) points that the car will visit sequentially every .02 seconds
          	
          	double dist_inc = 0.5;
    		for(int i = 0; i < 50; i++)
		    {
		      double next_s = car_s + (i+1)*dist_inc;
		      double next_d = 6.0; 

		      vector<double> xy = getXY(next_s, next_d, map_waypoints_s, map_waypoints_x, map_waypoints_y);

		      next_x_vals.push_back(xy[0]);
		      next_y_vals.push_back(xy[1]);
		    }

			*/
          	// END

          	msgJson["next_x"] = next_x_vals;
          	msgJson["next_y"] = next_y_vals;

          	auto msg = "42[\"control\","+ msgJson.dump()+"]";

          	//this_thread::sleep_for(chrono::milliseconds(1000));
          	ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
          
        }
      } else {
        // Manual driving
        std::string msg = "42[\"manual\",{}]";
        ws.send(msg.data(), msg.length(), uWS::OpCode::TEXT);
      }
    }
  });

  // We don't need this since we're not using HTTP but if it's removed the
  // program
  // doesn't compile :-(
  h.onHttpRequest([](uWS::HttpResponse *res, uWS::HttpRequest req, char *data,
                     size_t, size_t) {
    const std::string s = "<h1>Hello world!</h1>";
    if (req.getUrl().valueLength == 1) {
      res->end(s.data(), s.length());
    } else {
      // i guess this should be done more gracefully?
      res->end(nullptr, 0);
    }
  });

  h.onConnection([&h](uWS::WebSocket<uWS::SERVER> ws, uWS::HttpRequest req) {
    std::cout << "Connected!!!" << std::endl;
  });

  h.onDisconnection([&h](uWS::WebSocket<uWS::SERVER> ws, int code,
                         char *message, size_t length) {
    ws.close();
    std::cout << "Disconnected" << std::endl;
  });

  int port = 4567;
  if (h.listen(port)) {
    std::cout << "Listening to port " << port << std::endl;
  } else {
    std::cerr << "Failed to listen to port" << std::endl;
    return -1;
  }
  h.run();
}
