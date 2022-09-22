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
#include "global_planner/potential_field.hpp"

#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseStamped.h>
#include <nav_msgs/Path.h>
#include <visualization_msgs/Marker.h>
#include <visualization_msgs/MarkerArray.h>

#include <functional>  // To use std::bind

// Used to deal with YAML list of lists
#include <xmlrpcpp/XmlRpcValue.h> // catkin component


int main(int argc, char** argv)
/// The Main Function ///
{
    ROS_INFO("STARTING NODE: pf");

    // Vars
    double frequency = 10.0;
    // Scale by which to transform marker poses (10 cm/cell so we use 10)
    double SCALE = 10.0;
    std::string frame_id = "base_footprint";
    XmlRpc::XmlRpcValue xml_obstacles;

    // Map Parameters
    double thresh = 0.01;
    double inflate = 0.1;

    std::vector<double> start_vec{7.0, 3.0};
    std::vector<double> goal_vec{7.0, 26.0};

    // PF Parameters
    double eta = 1.0;
    double ada = .1;
    double zeta = 10.0;
    double d_thresh = 2.0;
    double Q_thresh = 0.1;

    // store Obstacle(s) here to create Map
    std::vector<map::Obstacle> obstacles_v;

    ros::init(argc, argv, "pf_node"); // register the node on ROS
    ros::NodeHandle nh; // get a handle to ROS
    ros::NodeHandle nh_("~"); // get a handle to ROS
    // Parameters
    nh_.getParam("frequency", frequency);
    nh_.getParam("obstacles", xml_obstacles);
    nh_.getParam("map_frame_id", frame_id);
    nh_.getParam("thresh", thresh);
    nh_.getParam("inflate", inflate);
    nh_.getParam("scale", SCALE);
    nh_.getParam("start", start_vec);
    nh_.getParam("goal", goal_vec);
    // PF Params
    nh_.getParam("eta", eta);
    nh_.getParam("ada", ada);
    nh_.getParam("zeta", zeta);
    nh_.getParam("d_thresh", d_thresh);
    nh_.getParam("Q_thresh", Q_thresh);

    // START and GOAL
    rigid2d::Vector2D start(start_vec.at(0)/SCALE, start_vec.at(1)/SCALE);
    rigid2d::Vector2D goal(goal_vec.at(0)/SCALE, goal_vec.at(1)/SCALE);

    // Path Publishers
    ros::Publisher path_pub = nh_.advertise<nav_msgs::Path>("robot_path", 1);

    // Using path topic since it's already in rviz
    ros::Publisher sg_pub = nh.advertise<visualization_msgs::MarkerArray>("path", 1);


    // Init Markers
    visualization_msgs::Marker s_marker;
    s_marker.header.frame_id = frame_id;
    s_marker.header.stamp = ros::Time::now();
    s_marker.type = visualization_msgs::Marker::SPHERE;
    s_marker.action = visualization_msgs::Marker::ADD;
    s_marker.pose.position.z = 0;
    s_marker.pose.position.x = start.x;
    s_marker.pose.position.y = start.y;
    s_marker.pose.orientation.x = 0.0;
    s_marker.pose.orientation.y = 0.0;
    s_marker.pose.orientation.z = 0.0;
    s_marker.pose.orientation.w = 1.0;
    s_marker.scale.x = 0.05;
    s_marker.scale.y = 0.05;
    s_marker.scale.z = 0.05;
    s_marker.color.r = 0.5;
    s_marker.color.g = 0.0;
    s_marker.color.b = 0.5;
    s_marker.color.a = 1.0;
    s_marker.id = 0;
    s_marker.lifetime = ros::Duration();

    visualization_msgs::Marker g_marker = s_marker;
    g_marker.pose.position.x = goal.x;
    g_marker.pose.position.y = goal.y;
    g_marker.color.r = 0.0;
    g_marker.color.g = 1.0;
    g_marker.color.b = 0.0;
    g_marker.id = 1;

    // Init Marker Array
    visualization_msgs::MarkerArray sg_arr;

    sg_arr.markers.clear();
    sg_arr.markers.push_back(s_marker);
    sg_arr.markers.push_back(g_marker);



    // 'obstacles' is a triple-nested list.
    // 1st level: obstacle (Obstacle), 2nd level: vertices (std::vector), 3rd level: coordinates (Vector2D)
    // std::vector<Obstacle>
    if(xml_obstacles.getType() != XmlRpc::XmlRpcValue::TypeArray)
    {
    ROS_ERROR("There is no list of obstacles");
    } else {
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
                // Vector2D contains x,y coords
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
    geometry_msgs::PoseStamped ps;
    ps.pose.position.x = start.x;
    ps.pose.position.y = start.y;
    std::vector<geometry_msgs::PoseStamped> robot_poses{ps};
    nav_msgs::Path path;
    path.header.frame_id = frame_id;

    global::PotentialField PF(obstacles_v, eta, ada, zeta, d_thresh, Q_thresh);
    bool terminate = PF.return_terminate();

    // SLEEP so that I can prepare my screen recorder
    ros::Duration(2.0).sleep();

    ros::Rate rate(frequency);

    bool finito = false;

    // Main While
    while (ros::ok())
    {
        ros::spinOnce();

        path.header.stamp = ros::Time::now();

        if (!terminate)
        {
          start = PF.OneStepGD(start, goal, obstacles_v);
          ps.pose.position.x = start.x;
          ps.pose.position.y = start.y;
          robot_poses.push_back(ps);
          terminate = PF.return_terminate();
          ROS_DEBUG("NEW POS: [%.2f, %.2f]", start.x, start.y);
        } else if (!finito)
        {
          ROS_INFO("GOAL REACHED.");
          finito = true;
        }

        path.poses = robot_poses;
        path_pub.publish(path);

        sg_pub.publish(sg_arr);

        
        rate.sleep();
    }

    return 0;
}