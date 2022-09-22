/// \file
/// \brief Draws Each Obstacle in RViz using MarkerArrays
///
/// PARAMETERS:
/// PUBLISHES:
/// SUBSCRIBES:
/// FUNCTIONS:

#include <ros/ros.h>

#include <math.h>
#include <string>
#include <vector>

#include "map/map.hpp"
#include "map/prm.hpp"
#include "map/grid.hpp"
#include <nav_msgs/OccupancyGrid.h>
#include "global_planner/incremental.hpp"

#include "nuslam/TurtleMap.h"

#include <functional>  // To use std::bind
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>
#include <geometry_msgs/Point.h>

// Used to deal with YAML list of lists
#include <xmlrpcpp/XmlRpcValue.h> // catkin component


int main(int argc, char** argv)
/// The Main Function ///
{
    ROS_INFO("STARTING NODE: astar");

    // Vars
    double frequency = 10.0;
    // Scale by which to transform marker poses (10 cm/cell so we use 10)
    double SCALE = 10.0;
    std::string frame_id = "base_footprint";
    XmlRpc::XmlRpcValue xml_obstacles;
    std::vector<double> start_vec{7.0, 3.0};
    std::vector<double> goal_vec{7.0, 26.0};
    double resolution = 0.01;

    // Map Parameters
    double thresh = 0.01;
    double inflate = 0.1;
    int visibility = 5;

    // store Obstacle(s) here to create Map
    std::vector<map::Obstacle> obstacles_v;

    ros::init(argc, argv, "astar_node"); // register the node on ROS
    ros::NodeHandle nh; // get a handle to ROS
    ros::NodeHandle nh_("~"); // get a handle to ROS
    // Parameters
    nh_.getParam("frequency", frequency);
    nh_.getParam("obstacles", xml_obstacles);
    nh_.getParam("start", start_vec);
    nh_.getParam("goal", goal_vec);
    nh_.getParam("map_frame_id", frame_id);
    nh_.getParam("thresh", thresh);
    nh_.getParam("inflate", inflate);
    nh_.getParam("scale", SCALE);
    nh_.getParam("resolution", resolution);
    nh_.getParam("visibility", visibility);

    rigid2d::Vector2D start(start_vec.at(0)/SCALE, start_vec.at(1)/SCALE);
    rigid2d::Vector2D goal(goal_vec.at(0)/SCALE, goal_vec.at(1)/SCALE);

    // 'obstacles' is a triple-nested list.
    // 1st level: obstacle (Obstacle), 2nd level: vertices (std::vector), 3rd level: coordinates (Vector2D)

    // std::vector<Obstacle>
    if(xml_obstacles.getType() != XmlRpc::XmlRpcValue::TypeArray)
    {
    ROS_ERROR("There is no list of obstacles");
    } else
    {
        for(int i = 0; i < xml_obstacles.size(); ++i)
        {
          // Obstacle contains std::vector<Vector2D>
          // create Obstacle with empty vertex vector
          map::Obstacle obs;
          if(xml_obstacles[i].getType() != XmlRpc::XmlRpcValue::TypeArray)
          {
              ROS_ERROR("obstacles[%d] has no vertices", i);
          } else {
              for(int j = 0; j < xml_obstacles[i].size(); ++j)
              {
                // Vector2D contains x,y center_coords
                if(xml_obstacles[i][j].size() != 2)
                {
                   ROS_ERROR("Vertex[%d] of obstacles[%d] is not a pair of coordinates", j, i);
                } else if(
                  xml_obstacles[i][j][0].getType() != XmlRpc::XmlRpcValue::TypeDouble or
                  xml_obstacles[i][j][1].getType() != XmlRpc::XmlRpcValue::TypeDouble)
                {
                  ROS_ERROR("The coordinates of vertex[%d] of obstacles[%d] are not doubles", j, i);
                } else {
                  // PASSED ALL THE TESTS: push Vector2D to vertices list in Obstacle object
                  rigid2d::Vector2D vertex(xml_obstacles[i][j][0], xml_obstacles[i][j][1]);
                  // NOTE: SCALE DOWN
                  vertex.x /= SCALE;
                  vertex.y /= SCALE;
                  obs.vertices.push_back(vertex);
                }
              }
          }
          // push Obstacle object to vector of Object(s) in Map
          obstacles_v.push_back(obs);
        }
    }

    // Initialize Marker
    // Init Marker Array Publisher
    ros::Publisher path_pub = nh.advertise<visualization_msgs::MarkerArray>("path", 1);

    ros::Publisher grid_pub = nh.advertise<nav_msgs::OccupancyGrid>("grid_map", 1);
    // Initialize grid_map outside
    nav_msgs::OccupancyGrid grid_map;
    // rviz representation of the grid
    std::vector<int8_t> map;

    // Init Marker Array
    visualization_msgs::MarkerArray path_arr;

    // Init Marker
    visualization_msgs::Marker marker;
    marker.header.frame_id = frame_id;
    marker.header.stamp = ros::Time::now();
    // marker.ns = "my_namespace";
    // marker.id = 0;
    marker.type = visualization_msgs::Marker::LINE_STRIP;
    marker.action = visualization_msgs::Marker::ADD;
    marker.pose.position.z = 0;
    marker.pose.orientation.x = 0.0;
    marker.pose.orientation.y = 0.0;
    marker.pose.orientation.z = 0.0;
    marker.pose.orientation.w = 1.0;
    marker.scale.x = 0.02;
    marker.color.r = 0.96f;
    marker.color.g = 0.475f;
    marker.color.b = 0.0f;
    marker.color.a = 1.0;
    marker.lifetime = ros::Duration();

    // LINE_STRIP relative to this pose
    marker.pose.position.x = 0.0;
    marker.pose.position.y = 0.0;

    // PATH MARKERS
    visualization_msgs::Marker path_marker;
    path_marker = marker;
    // More visible than PRM
    path_marker.scale.x = 0.03;
    path_marker.color.r = 0.2;
    path_marker.color.g = 1.0;
    path_marker.color.b = 0.2;

    visualization_msgs::Marker path_sph_mkr;
    path_sph_mkr = marker;
    // More visible than PRM
    path_sph_mkr.type = visualization_msgs::Marker::CUBE;
    path_sph_mkr.scale.x = 0.02;
    path_sph_mkr.scale.y = 0.02;
    path_sph_mkr.scale.z = 0.02;
    path_sph_mkr.color.r = 0.0f;
    path_sph_mkr.color.g = 0.0f;
    path_sph_mkr.color.b = 0.0f;

    // CURRENT POSITION MARKER
    visualization_msgs::Marker curr_pos_marker;
    curr_pos_marker = path_sph_mkr;
    // More visible than PRM
    curr_pos_marker.scale.x = 0.1;
    curr_pos_marker.scale.y = 0.1;
    curr_pos_marker.color.r = 1.0;
    curr_pos_marker.color.g = 0.2;
    curr_pos_marker.color.b = 0.2;

    // UpdateCell() MARKER
    visualization_msgs::Marker update_marker;
    update_marker = path_sph_mkr;
    // More visible than PRM
    update_marker.scale.x = 0.04;
    update_marker.scale.y = 0.04;
    update_marker.scale.z = 0.04;
    update_marker.color.r = 0.96f;
    update_marker.color.g = 0.475f;
    update_marker.color.b = 0.0f;
    update_marker.color.a = 1.0;
    update_marker.lifetime = ros::Duration(1.0 / frequency);


    // LPA* or D* Lite Path
    std::vector<global::Node> path;
    
    // GRID Version
    // Initialize Grid
    map::Grid grid(obstacles_v, inflate);

    // Build Map
    grid.build_map(resolution);

    ROS_INFO("Grid Built!");

    // Get map bounds
    auto bounds = grid.return_map_bounds();
    auto gridsize = grid.return_grid_dimensions();

    // Grid Pose
    geometry_msgs::Pose map_pose;
    map_pose.position.x = bounds.at(0).x;
    map_pose.position.y = bounds.at(0).y;
    map_pose.position.z = 0.0;
    map_pose.orientation.x = 0.0;
    map_pose.orientation.y = 0.0;
    map_pose.orientation.z = 0.0;
    map_pose.orientation.w = 1.0;

    // Occupancy grid for visualization
    grid_map.header.frame_id = frame_id;
    grid_map.info.resolution = resolution;
    grid_map.info.width = gridsize.at(0);
    grid_map.info.height = gridsize.at(1);

    // std::cout << "Width: " << gridsize.at(0) << "\t Height: " << gridsize.at(1) << std::endl;

    grid_map.info.origin = map_pose;

    ROS_INFO("Planning using LPA*!");
    global::LPAstar lpastar(obstacles_v, inflate);
    lpastar.Initialize(start, goal, grid, resolution);
    lpastar.ComputeShortestPath();
    path = lpastar.return_path();

    ros::Rate rate(frequency);

    unsigned int path_counter = 0;
    std::vector<global::Node> updated_nodes;

    // SLEEP so that I can prepare my screen recorder
    ros::Duration(2.0).sleep();

    // For visualization purposes (See below)
    bool firstpass = true;

    // Tells us if path contains obstacles
    bool valid_path = true;

    // Main While
    while (ros::ok())
    {
        ros::spinOnce();

        // rviz representation of the grid
        grid.fake_occupancy_grid(map);
        // Publish GRID Map
        grid_map.header.stamp = ros::Time::now();
        grid_map.info.map_load_time = ros::Time::now();
        grid_map.data = map;
        grid_pub.publish(grid_map);

        // DRAW PATH
        path_arr.markers.clear();
        path_marker.points.clear();
        int path_marker_id = 0;
        for (auto path_iter = path.begin(); path_iter != path.end(); path_iter++)
        {
            // Add node as marker cell
            geometry_msgs::Point vtx;
            vtx.x = path_iter->cell.center_coords.x;
            vtx.y = path_iter->cell.center_coords.y;
            vtx.z = 0.0;
            path_marker.points.push_back(vtx);

            // Also push back cubes
            path_sph_mkr.pose.position.x = path_iter->cell.center_coords.x;
            path_sph_mkr.pose.position.y = path_iter->cell.center_coords.y;
            path_sph_mkr.id = path_marker_id;
            path_marker_id++;
            path_arr.markers.push_back(path_sph_mkr);
        }
        path_marker.id = path_marker_id;
        path_arr.markers.push_back(path_marker);
        curr_pos_marker.pose.position.x = path.at(path_counter).cell.center_coords.x;
        curr_pos_marker.pose.position.y = path.at(path_counter).cell.center_coords.y;

        // Push Back Current Position
        path_marker_id++;
        curr_pos_marker.id = path_marker_id;
        path_arr.markers.push_back(curr_pos_marker);

        // Push Back Updated Cell Markers
        for (auto node_iter = updated_nodes.begin(); node_iter != updated_nodes.end(); node_iter++)
        {
          path_marker_id++;
          update_marker.pose.position.x = node_iter->cell.center_coords.x;
          update_marker.pose.position.y = node_iter->cell.center_coords.y;
          update_marker.id = path_marker_id;
          path_arr.markers.push_back(update_marker);
        }

        // Publish Path
        path_pub.publish(path_arr);

        // FAKE Update
        updated_nodes.clear();
        if (path_counter < path.size() - 1 and valid_path)
        {
          grid.update_grid(path.at(path_counter).cell, visibility);
          path_counter++;
          // ROS_INFO("UPDATE NUMBER: %d", path_counter);
          // LPA* Update
          updated_nodes = lpastar.SimulateUpdate(grid.return_fake_grid());
          path = lpastar.return_path();
          // Stop condition in case of obstacles
          valid_path = lpastar.return_valid();
        } else
        {
          // FINAL PATH. INFINITE MARKER DURATION
          path_marker.lifetime = ros::Duration();
          path_sph_mkr.lifetime = ros::Duration();
          curr_pos_marker.lifetime = ros::Duration();
        }

        // Make the markers persist for the first iteration to show original path
        if (firstpass)
        {
            ros::Duration(1.0).sleep();
            path_marker.lifetime = ros::Duration(2.0 / frequency);
            path_sph_mkr.lifetime = ros::Duration(2.0 / frequency);
            curr_pos_marker.lifetime = ros::Duration(2.0 / frequency);
        }

        firstpass = false;

        rate.sleep();
    }

    return 0;
}