#ifndef GLOBAL_PLANNER_INCLUDE_GUARD_HPP
#define GLOBAL_PLANNER_INCLUDE_GUARD_HPP
/// \file
/// \brief global_planners library which contains basic planner structure: nodes, neighbours, paths etc.
/// NOTE: rigid2d is from turtlebot3_from_scratch (tb3 in nuturtle.rosinstall)
#include <rigid2d/rigid2d.hpp>
#include <map/grid.hpp>
#include <map/prm.hpp>
#include <vector>
#include <eigen3/Eigen/Dense>
#include <ros/ros.h>

namespace global
{
    // Used to store Obstacle vertex coordinates
    using rigid2d::Vector2D;
    using map::Vertex;
    using map::Cell;
    using map::Index;
    using map::Obstacle;

    /// \brief stores Obstacle(s) to construct basic Planner
    class GlobalPlanner
    {
    public:
        /// \brief the default constructor creates an empty Planner
        GlobalPlanner();


    // protected instead of private so that child Class can access
    protected:
        std::vector<Vertex> PRM;

        std::vector<Cell> GRID;

         // Map obstacles
        std::vector<Obstacle> obstacles;

        // Map maximum coordinatesin x,y
        Vector2D map_max;

        // Map minimum coordinatesin x,y
        Vector2D map_min;

        // Robot's inflation radius for collision checking
        double inflate_robot;
    };
}

#endif
