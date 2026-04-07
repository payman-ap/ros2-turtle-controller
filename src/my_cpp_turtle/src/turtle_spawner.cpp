#include "rclcpp/rclcpp.hpp"
#include "example_interfaces/msg/string.hpp"
#include "my_robot_interfaces/srv/catch_turtle.hpp"
#include "my_robot_interfaces/srv/spawn_turtle.hpp"
#include "my_robot_interfaces/msg/turtle.hpp"
#include "my_robot_interfaces/msg/turtle_array.hpp"
#include "turtlesim/srv/spawn.hpp"
#include "turtlesim/srv/kill.hpp"

using namespace std::placeholders;
using namespace std::chrono_literals;

class TurtleSpawnerNode : public rclcpp::Node
{
public:
    TurtleSpawnerNode() : Node("turtle_spawner")
    {
        spawn_client_ = this->create_client<turtlesim::srv::Spawn>("spawn");
        kill_client_ = this->create_client<turtlesim::srv::Kill>("kill");

        while (!spawn_client_->wait_for_service(1s)) {
            if (!rclcpp::ok()) {
                RCLCPP_INFO(this->get_logger(), "Interrupted while waiting for /spawn service...");
                return;
                }
                RCLCPP_INFO(this->get_logger(), "/spawn service not available, waiting again...");
        }
        while (!kill_client_->wait_for_service(1s)) {
            if (!rclcpp::ok()) {
                RCLCPP_INFO(this->get_logger(), "Interrupted while waiting for /kill service...");
            return;
            }
                RCLCPP_INFO(this->get_logger(), "/kill service not available, waiting again...");
        }
        
        // === Services ===
        spawn_service_ = this->create_service<turtlesim::srv::Spawn>(
            "trigger_spawn",
        std::bind(&TurtleSpawnerNode::callback_spawn_turtle, this, _1, _2));
        kill_service_ = this->create_service<turtlesim::srv::Kill>(
            "trigger_kill",
        std::bind(&TurtleSpawnerNode::callback_kill_turtle, this, _1, _2));
        catch_service_ = this->create_service<my_robot_interfaces::srv::CatchTurtle>(
            "trigger_catch",
        std::bind(&TurtleSpawnerNode::callback_catch_turtle, this, _1, _2));
        myspawn_service_ = this->create_service<my_robot_interfaces::srv::SpawnTurtle>(
            "trigger_myspawn",
        std::bind(&TurtleSpawnerNode::callback_myspawn_turtle, this, _1, _2));

        // === Publishers ===
        turtles_publisher_ = this->create_publisher<my_robot_interfaces::msg::Turtle>(
                                            "alive_turtles", 10);

        // Checking while



        RCLCPP_INFO(this->get_logger(), "Turtle Spawner Node has started.");
    }

private:
    void callback_spawn_turtle(const turtlesim::srv::Spawn::Request::SharedPtr request,
                        turtlesim::srv::Spawn::Response::SharedPtr response)
    {
        RCLCPP_INFO(this->get_logger(), "Spawning turtle  %s at (%.2f, %.2f)",
                            request->name.c_str(), request->x, request->y);
        // Call turtlesim asyncronously
        auto result_future = this->spawn_client_->async_send_request(request);
        response->name = request->name;

        auto request_obj = std::make_shared<my_robot_interfaces::msg::Turtle>();
        request_obj->name = request->name;
        request_obj->x = request->x;
        request_obj->y = request->y;
        request_obj->theta = request->theta;

        turtles_publisher_->publish(*request_obj);

        RCLCPP_INFO(this->get_logger(), "Spawned Turtle  %s", request->name.c_str());

    }

    void callback_kill_turtle(const turtlesim::srv::Kill::Request::SharedPtr request,
                        turtlesim::srv::Kill::Response::SharedPtr /*response*/)
    {
        RCLCPP_INFO(this->get_logger(), "Kill request received for turtle: %s", request->name.c_str());

        auto result_future = this->kill_client_->async_send_request(request,
        [this, name = request->name](rclcpp::Client<turtlesim::srv::Kill>::SharedFuture fut) {
        try {
            fut.get();  // throws if failed
            RCLCPP_INFO(this->get_logger(), "Successfully killed turtle: %s", name.c_str());
        } catch (...) {
            RCLCPP_ERROR(this->get_logger(), "Failed to kill turtle: %s", name.c_str());
        }}
        );
    }

    void callback_catch_turtle(const my_robot_interfaces::srv::CatchTurtle::Request::SharedPtr request,
                        my_robot_interfaces::srv::CatchTurtle::Response::SharedPtr response)
    {
        RCLCPP_INFO(this->get_logger(), "Catch request received for turtle: %s", request->name.c_str());

        auto request_obj = std::make_shared<turtlesim::srv::Kill::Request>();
        request_obj->name = request->name;
        auto result_future = this->kill_client_->async_send_request(request_obj);
        response->success = true;
        RCLCPP_INFO(this->get_logger(), "Successfully catched turtle: %s", request->name.c_str());

    }

    void callback_myspawn_turtle(const my_robot_interfaces::srv::SpawnTurtle::Request::SharedPtr request,
                        my_robot_interfaces::srv::SpawnTurtle::Response::SharedPtr response)
    {
        RCLCPP_INFO(this->get_logger(), "Spawning turtle  %s at (%.2f, %.2f)",
                            request->name.c_str(), request->x, request->y);

        // making object in spawn service type
        auto request_obj = std::make_shared<turtlesim::srv::Spawn::Request>();
        request_obj->name = request->name;
        request_obj->x = request->x;
        request_obj->y = request->y;
        request_obj->theta = request->theta;

        auto result_future = this->spawn_client_->async_send_request(request_obj);

        response->name = request->name;

        // making object in publisher turtle message type
        auto request_obj_pub = std::make_shared<my_robot_interfaces::msg::Turtle>();
        request_obj_pub->name = request->name;
        request_obj_pub->x = request->x;
        request_obj_pub->y = request->y;
        request_obj_pub->theta = request->theta;
        turtles_publisher_->publish(*request_obj_pub);

        RCLCPP_INFO(this->get_logger(), "Spawned Turtle  %s", request->name.c_str());

    }
    
    // Definitions
    rclcpp::Client<turtlesim::srv::Spawn>::SharedPtr spawn_client_;
    rclcpp::Client<turtlesim::srv::Kill>::SharedPtr kill_client_;
    rclcpp::Service<turtlesim::srv::Spawn>::SharedPtr spawn_service_;
    rclcpp::Service<turtlesim::srv::Kill>::SharedPtr kill_service_;
    rclcpp::Service<my_robot_interfaces::srv::CatchTurtle>::SharedPtr catch_service_;
    rclcpp::Service<my_robot_interfaces::srv::SpawnTurtle>::SharedPtr myspawn_service_;
    rclcpp::Publisher<my_robot_interfaces::msg::Turtle>::SharedPtr turtles_publisher_;


};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleSpawnerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
