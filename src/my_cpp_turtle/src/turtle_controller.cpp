#include "rclcpp/rclcpp.hpp"
#include "rclcpp/rclcpp.hpp"
#include "example_interfaces/msg/string.hpp"
#include "my_robot_interfaces/srv/catch_turtle.hpp"
#include "my_robot_interfaces/srv/spawn_turtle.hpp"
#include "my_robot_interfaces/msg/turtle.hpp"
#include "my_robot_interfaces/msg/turtle_array.hpp"
#include "turtlesim/srv/spawn.hpp"
#include "turtlesim/srv/kill.hpp"
#include "turtlesim/msg/pose.hpp"
#include "geometry_msgs/msg/twist.hpp"

#include <cmath>
#include <random>
#include <sstream>


using namespace std::placeholders;
using namespace std::chrono_literals;


class TurtleControllerNode : public rclcpp::Node
{
public:
    TurtleControllerNode() : Node("turtle_controller"), counter_(2)
    {
        // === Parameters ===
        this->declare_parameter("max_alive_turtles", 9);
        this->declare_parameter("kill_distance_threshold", 0.6);
        this->declare_parameter("linear_vel", 2.0);
        this->declare_parameter("radius", 0.5);
        this->declare_parameter("angle_decrement", 0.02);
        this->declare_parameter("p_gain", 1.5);
        this->declare_parameter("angular_gain", 2.0);

        int max_iterations = this->get_parameter("max_alive_turtles").as_int();
        double kill_distance_threshold = this->get_parameter("kill_distance_threshold").as_double();
        double linear_vel = this->get_parameter("linear_vel").as_double();
        double radius = this->get_parameter("radius").as_double();
        double angle_decrement = this->get_parameter("angle_decrement").as_double();
        double p_gain = this->get_parameter("p_gain").as_double();
        double angular_gain = this->get_parameter("angular_gain").as_double();

        // random generator
        std::random_device rd;
        gen_.seed(rd());
        
        // === Clients ===

        spawn_client_ = this->create_client<turtlesim::srv::Spawn>("spawn");
        kill_client_ = this->create_client<turtlesim::srv::Kill>("kill");
        myspawn_client_ = this->create_client<my_robot_interfaces::srv::SpawnTurtle>("myspawn");
        catch_client_ = this->create_client<my_robot_interfaces::srv::CatchTurtle>("catch");

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
        while (!catch_client_->wait_for_service(1s)) {
            if (!rclcpp::ok()) {
                RCLCPP_INFO(this->get_logger(), "Interrupted while waiting for /catch service...");
                return;
                }
                RCLCPP_INFO(this->get_logger(), "/catch service not available, waiting again...");
        }
        while (!myspawn_client_->wait_for_service(1s)) {
            if (!rclcpp::ok()) {
                RCLCPP_INFO(this->get_logger(), "Interrupted while waiting for /myspawn service...");
            return;
            }
                RCLCPP_INFO(this->get_logger(), "/myspawn service not available, waiting again...");
        }

        // === Publishers ===
        pose_publisher_ = this->create_publisher<example_interfaces::msg::String>("turtle1_pose",10);
        cmd_vel_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("turtle1/cmd_vel", 10);
        turtles_array_publisher_ = this->create_publisher<my_robot_interfaces::msg::TurtleArray>("turtles_array", 10);

        // Subscribers
        pose_subscriber_ = this->create_subscription<turtlesim::msg::Pose>("turtle1/pose",10,
        std::bind(&TurtleControllerNode::pose_callback, this, std::placeholders::_1));
        turtles_array_subscriber_ = this->create_subscription<my_robot_interfaces::msg::TurtleArray>("turtles_array",10,
        std::bind(&TurtleControllerNode::callback_turtles_array, this, std::placeholders::_1));
        turtles_subscriber_ = this->create_subscription<my_robot_interfaces::msg::Turtle>("alive_turtles",10,
        std::bind(&TurtleControllerNode::callback_alive_turtles, this, std::placeholders::_1));

        // === Timers ===
        timer_ = this->create_wall_timer(250ms, 
        std::bind(&TurtleControllerNode::control_loop, this));
        move_timer_ = this->create_wall_timer(100ms,
        std::bind(&TurtleControllerNode::move_to_closest_turtle, this));


        RCLCPP_INFO(this->get_logger(), "Turtle Controller Node has started.");
    }

private:
    void pose_callback(const turtlesim::msg::Pose::SharedPtr msg)
    {
        pose_ = msg;
        auto pose_message = example_interfaces::msg::String();
        std::stringstream ss;
        ss = "Turtle pose: (" << pose_.x << "," << std::string(pose_.y) <<  ") with " 
                            << std::string(pose_.theta) << "degrees";
        pose_message.data = ss.str();
        pose_publisher_->publish(pose_message);

        double t1_x = msg.x;
        double t1_y = msg.y;
        // Eating turtle logic
        for(int i; i <= sizeof(alive_turtles); ++i)
        {
            double pos = alive_turtles[i];
            string name = "";
            double dist = sqrt(pow(t1_x - pos[0], 2) + pow(t1_y - pos[1], 2));
            if (dist < this->kill_distance_threshold)
            {
                this->kill_turtle(name);
                this->myspawn_new_turtle();
            }
        }

    }

    void move_spiral()
    {
        auto cmd = geometry_msgs::msg::Twist();
        cmd->linear.x = this->linear_vel;
        cmd->angular.z = this->linear_vel / this->radius;
        this->radius += 0.01;
        cmd_vel_publisher_->publish(cmd);
    }
    
    void move_to_closest_turtle()
    {
        //
    }
    
    void control_loop()
    {
        //
    }

    void kill_timer_callback()
    {
        //
    }

    void callback_alive_turtles()
    {
        //
    }

    void callback_turtles_array()
    {
        //
    }

    void publish_alive_turtles_array()
    {
        //
    }

    void myspawn_new_turtle(rclcpp::Client<my_robot_interfaces::srv::SpawnTurtle>::SharedFuture future)
    {
        std::uniform_real_distribution<double> dist_pos(1.0, 10.0);
        std::uniform_real_distribution<double> dist_theta(0.0, 6.28);
        // What it does: It allocates memory for the Request object and returns a SharedPtr pointing to it.
        auto request = std::make_shared<my_robot_interfaces::srv::SpawnTurtle::Request>();
        request->x = dist_pos(gen_);
        request->y = dist_pos(gen_);
        request->theta = dist_theta(gen_);
        request->name = "turtle" + std::to_string(this->counter_++);

        myspawn_client_->async_send_request(request,
                        std::bind(&TurtleControllerNode::callback_call_myspawn,this,_1));
        
        this->last_spawned_name = "";
    }

    void spawn_new_turtle(rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture future)
    {
        std::uniform_real_distribution<double> dist_pos(1.0, 10.0);
        std::uniform_real_distribution<double> dist_theta(0.0, 6.28);
        auto request = std::make_shared<turtlesim::srv::Spawn::Request>();

        request->x = dist_pos(gen_);
        request->y = dist_pos(gen_);
        request->theta = dist_theta(gen_);
        request->name = "turtle" + std::to_string(this->counter_++);

        spawn_client_->async_send_request(request,
                std::bind(&TurtleControllerNode::callback_call_spawn, this, _1));
        
        this->last_spawned_name = "";
    }

    void kill_turtle(const std::string name)
    {
        // auto request = std::make_shared<turtlesim::srv::Kill::Request>();
        auto request = std::make_shared<my_robot_interfaces::srv::CatchTurtle::Request>();
        request->name = name;
        catch_client_->async_send_request(request,
                        std::bind(&TurtleControllerNode::callback_call_kill, this, _1));
    }

    void kill_last_turtle()
    {
        //
    }

    void after_kill()
    {
        //
    }

    void callback_call_spawn(rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture future)
    {
        try {
            // here in C++, the response is the result of the future
            auto response = future.get();
            this->last_spawned_name = response->name;
            RCLCPP_INFO(this->get_logger(), "Successfully spawned: %s", response->name.c_str());
        }
        catch (const std::exception &e){
            RCLCPP_ERROR(this->get_logger(), "Service call failed: %s", e.what());
        }
    }

    void callback_call_myspawn(rclcpp::Client<my_robot_interfaces::srv::SpawnTurtle>::SharedFuture future)
    {
        try {
            // here in C++, the response is the result of the future
            auto response = future.get();
            this->last_spawned_name = response->name;
            RCLCPP_INFO(this->get_logger(), "Successfully spawned: %s", response->name.c_str());
        }
        catch (const std::exception &e){
            RCLCPP_ERROR(this->get_logger(), "Service call failed: %s", e.what());
        }
    }

    void callback_call_kill(rclcpp::Client<my_robot_interfaces::srv::CatchTurtle>::SharedFuture future)
    {
        try {
            auto response = future.get();
            RCLCPP_INFO(this->get_logger(), "Successfully killed turtle");
        }
        catch(const std::exception &e) {
            RCLCPP_ERROR(this->get_logger(), "Service call failed: %s", e.what());
        }
    }



    int counter_;
    int current_iteration = 0;
    turtlesim::msg::Pose::SharedPtr pose_ptr_; // array/vector
    int alive_turtles; // array/vector
    std::string last_spawned_name;

    std::mt19937 gen_; // shuffler

    rclcpp::Client<turtlesim::srv::Spawn>::SharedPtr spawn_client_;
    rclcpp::Client<turtlesim::srv::Kill>::SharedPtr kill_client_;
    rclcpp::Client<my_robot_interfaces::srv::SpawnTurtle>::SharedPtr myspawn_client_;
    rclcpp::Client<my_robot_interfaces::srv::CatchTurtle>::SharedPtr catch_client_;

    rclcpp::Subscription<my_robot_interfaces::msg::Turtle>::SharedPtr turtles_subscriber_;
    rclcpp::Subscription<my_robot_interfaces::msg::TurtleArray>::SharedPtr turtles_array_subscriber_;
    rclcpp::Subscription<turtlesim::msg::Pose>::SharedPtr pose_subscriber_;

    rclcpp::Publisher<example_interfaces::msg::String>::SharedPtr pose_publisher_;
    rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_vel_publisher_;
    rclcpp::Publisher<my_robot_interfaces::msg::TurtleArray>::SharedPtr turtles_array_publisher_;
    
    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::TimerBase::SharedPtr move_timer_;


};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleControllerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
