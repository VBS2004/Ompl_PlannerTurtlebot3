#include <builtin_interfaces/msg/time.hpp>
#include <iomanip>
#include <ompl/base/SpaceInformation.h>
#include <ompl/base/spaces/SE2StateSpace.h>
#include <ompl/geometric/planners/rrt/RRTConnect.h>
#include <ompl/geometric/SimpleSetup.h>
#include <opencv2/opencv.hpp>
#include <yaml-cpp/yaml.h>  
#include <ompl/config.h>
#include <opencv2/opencv.hpp>
#include <yaml-cpp/yaml.h>
#include "rclcpp/rclcpp.hpp"
#include "nav_msgs/msg/path.hpp"
#include <tf2/LinearMath/Quaternion.h>
#include </opt/ros/humble/include/tf2_geometry_msgs/tf2_geometry_msgs/tf2_geometry_msgs.hpp>

 namespace ob = ompl::base;
 namespace og = ompl::geometric;

 bool isStateValid(const ob::State *state)
 {
     const auto *se2state = state->as<ob::SE2StateSpace::StateType>();
	const auto *pos = se2state->as<ob::RealVectorStateSpace::StateType>(0);
	const auto *rot = se2state->as<ob::SO2StateSpace::StateType>(1);
	double x = (*pos)[0];
	double y = (*pos)[1];
	double theta = rot->value;  
    double map_width = 348 * 0.05; // Convert map width from pixels to meters
    double map_height = 348 * 0.05; // Convert map height from pixels to meters

    // Use these variables in the if statement:

    if (x < 0 || x > map_width || y < 0 || y > map_height || theta < -3.14 || theta > 3.14) {
        return false;
    }

    YAML::Node config = YAML::LoadFile("/root/ros2_ws/src/maps/map.yaml");
    double occt=config["occupied_thresh"].as<double>();
    double frt=config["free_thresh"].as<double>();
    cv::Mat image = cv::imread("/root/ros2_ws/src/maps/map.pgm", cv::IMREAD_GRAYSCALE);
    cv::Scalar intensity = image.at<uchar>(cv::Point(static_cast<int>(y / 0.05), static_cast<int>(x / 0.05)));
    if(intensity.val[0]<=occt){
        return false;
    }
     return (const void*)rot != (const void*)pos;
 }

 void plan(const rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr& path_publisher)
 {
	auto space(std::make_shared<ob::SE2StateSpace>());
    int mapWidth=384;
    int mapHeight=384;
    YAML::Node config = YAML::LoadFile("/root/ros2_ws/src/maps/map.yaml");
    std::string image_path = config["image"].as<std::string>();
    double resolution = config["resolution"].as<double>();
    YAML::Node originNode = config["origin"];
    double originX = originNode[0].as<double>();
    double originY = originNode[1].as<double>();
    double mapWidthMeters = mapWidth * resolution;
    double mapHeightMeters = mapHeight * resolution;
	ob::RealVectorBounds bounds(2);
    bounds.setLow(0, originX*0.05);
    bounds.setHigh(0, originX*0.05 + mapWidthMeters);
    bounds.setLow(1, originY*0.05);
    bounds.setHigh(1, originY*0.05 + mapHeightMeters);
    space->setBounds(bounds);
	auto si(std::make_shared<ob::SpaceInformation>(space));
	si->setStateValidityChecker(isStateValid);
	ob::ScopedState<> start(space);
	start[0] = 199*0.05; 
	start[1] = 219*0.05; 
	start[2] = 0;
	ob::ScopedState<> goal(space);
	std::cout << goal;
	goal[0] = 188*0.05; 
	goal[1] = 151*0.05; 
	goal[2] = 0;
    auto pdef(std::make_shared<ob::ProblemDefinition>(si));
    pdef->setStartAndGoalStates(start, goal);
    auto planner(std::make_shared<og::RRTConnect>(si));
    planner->setProblemDefinition(pdef);
    planner->setup();
    si->printSettings(std::cout);
    pdef->print(std::cout);
    ob::PlannerStatus solved = planner->ob::Planner::solve(100.0);

    if (solved)
    {
        auto solution_path = pdef->getSolutionPath()->as<og::PathGeometric>();
        auto path_msg = std::make_unique<nav_msgs::msg::Path>();
        auto current_time = std::chrono::system_clock::now();
        auto time_msg = builtin_interfaces::msg::Time();
        time_msg.sec = std::chrono::time_point_cast<std::chrono::seconds>(current_time).time_since_epoch().count();
        time_msg.nanosec = (current_time.time_since_epoch() % std::chrono::seconds(1)).count();
        path_msg->header.stamp = time_msg;
        path_msg->header.frame_id = "map"; 
        for (std::size_t i = 0; i < solution_path->getStateCount(); ++i)
        {
            const ob::State* state = solution_path->getState(i);
            const auto* se2_state = state->as<ob::SE2StateSpace::StateType>();
            const auto* pos = se2_state->as<ob::RealVectorStateSpace::StateType>(0);
            const auto* rot = se2_state->as<ob::SO2StateSpace::StateType>(1);
            geometry_msgs::msg::PoseStamped pose;
            pose.pose.position.x = (*pos)[0];
            pose.pose.position.y = (*pos)[1];
            pose.pose.orientation = tf2::toMsg(tf2::Quaternion(tf2::Vector3(0, 0, 1), rot->value));
            path_msg->poses.push_back(pose);
        }
        path_publisher->publish(std::move(path_msg));

    }
    else{

    	const ob::PlannerDataPtr planner_data = std::make_shared<ob::PlannerData>(si);
        planner->getPlannerData(*planner_data);
        std::cout << "Planner Data Information:" << std::endl;
        std::cout << "Number of states: " << planner_data->numVertices() << std::endl;
        std::cout << "Number of edges: " << planner_data->numEdges() << std::endl;
        std::cout << "No solution found" << std::endl;

        }
 }
 int main(int argc, char** argv)
 {
    rclcpp::init(argc, argv);
    auto node=std::make_shared<rclcpp::Node>("plan_publisher");
    auto pather=node->create_publisher<nav_msgs::msg::Path>("/plannedpath",10);
    std::cout << "OMPL version: " << OMPL_VERSION << std::endl;
    plan(pather);
    return 0;
 }
