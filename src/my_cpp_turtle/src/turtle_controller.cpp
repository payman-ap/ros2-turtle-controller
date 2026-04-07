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

#define PI 3.14159265


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

        max_iterations_ = this->get_parameter("max_alive_turtles").as_int();
        kill_distance_threshold_ = this->get_parameter("kill_distance_threshold").as_double();
        linear_vel_ = this->get_parameter("linear_vel").as_double();
        radius_ = this->get_parameter("radius").as_double();
        angle_decrement_ = this->get_parameter("angle_decrement").as_double();
        p_gain_ = this->get_parameter("p_gain").as_double();
        angular_gain_ = this->get_parameter("angular_gain").as_double();

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
        pose_ptr_ = msg;
        auto pose_message = example_interfaces::msg::String();
        std::stringstream ss;
        ss << "Turtle pose: (" << pose_ptr_->x << "," << pose_ptr_->y <<  ") with " 
                            << pose_ptr_->theta << " degrees";
        pose_message.data = ss.str();
        pose_publisher_->publish(pose_message);

        // Eating turtle logic
        auto it = alive_turtles_.begin();
        while (it != alive_turtles_.end()){
            double dx = msg->x - it->second.first;
            double dy = msg->y - it->second.second;
            double dist = std::sqrt(dx * dx + dy * dy);

            if (dist < this->kill_distance_threshold_){
                this->kill_turtle(it->first);
                this->myspawn_new_turtle(); // Spawn a replacement
                it = alive_turtles_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void move_spiral()
    {
        auto cmd = geometry_msgs::msg::Twist();
        cmd.linear.x = this->linear_vel_;
        cmd.angular.z = this->linear_vel_ / this->radius_;
        this->radius_ += 0.01;
        cmd_vel_publisher_->publish(cmd);
    }
    
    void move_to_closest_turtle()
    {
        if (!pose_ptr_){
            return;
        }
        if (alive_turtles_.empty()){
            RCLCPP_INFO(this->get_logger(), "No turtles to hunt?");
            this->move_spiral();
            return;
        }

        double t1_x = pose_ptr_->x;
        double t1_y = pose_ptr_->y;
        double target_x = 0.0;
        double target_y = 0.0;
        double min_dist = std::numeric_limits<double>::infinity();
        std::string closest_name = "";

        double dx, dy, dist, target_x, target_y;


        auto it = alive_turtles_.begin();
        for (const auto & [name, pos] : this->alive_turtles_) {
            double dx = pos.first - t1_x;  // Target - Current
            double dy = pos.second - t1_y;
            double dist = std::sqrt(dx*dx + dy*dy);

            if (dist < min_dist) {
                min_dist = dist;
                target_x = pos.first;
                target_y = pos.second;
            }
        }
        
        // Calculate the motion
        double target_angle = std::atan2(target_y - t1_y, target_x - t1_x);
        double steering_angle = target_angle - pose_ptr_->theta;


        // Normalize angle_error to stay between -pi and pi (prevents spinning 350 degrees)
        steering_angle = std::atan2(std::sin(steering_angle), std::cos(steering_angle));

        // Generating the Twist command
        auto cmd = geometry_msgs::msg::Twist();

        if (min_dist > 0.1){
            // Tolerance: stop if we are 0.1 units away
            // Proportional Control: move faster if far, slow down as you get closer
            cmd.linear.x = this->p_gain_ * min_dist;
            cmd.angular.z = this->angular_gain_ * steering_angle;
        }
        else {
            cmd.linear.x = 0.0;
            cmd.angular.z = 0.0;
        }
        cmd_vel_publisher_->publish(cmd);

    }
    
    void control_loop()
    {
        if (this->current_iteration_ >= this->max_iterations_){
            this->timer_->cancel();
            return;
        }

        // this->spawn_new_turtle();
        this->myspawn_new_turtle();

        kill_timer_ = this->create_wall_timer(750ms,
        std::bind(&TurtleControllerNode::kill_timer_callback, this));
        ++this->current_iteration_;
    }

    void kill_timer_callback()
    {
        this->kill_timer_->cancel();
        this->kill_last_turtle();
    }

    void callback_alive_turtles(const my_robot_interfaces::msg::Turtle::SharedPtr msg)
    {
        RCLCPP_INFO(this->get_logger(), "Received new turtle");

        this->alive_turtles_[msg->name] = std::make_pair(msg->x, msg->y);
        this->publish_alive_turtles_array();
    }

    void callback_turtles_array(const my_robot_interfaces::msg::TurtleArray::SharedPtr msg)
    {
        // clear the turtles map (like dict in Py)
        this->alive_turtles_.clear(); 
        // Rebuild the dictionary from the message
        for (const auto& turtle : msg->turtles){
            this->alive_turtles_[turtle.name] = std::make_pair(turtle.x, turtle.y); //{turtle.x, turtle.y}
        }
        RCLCPP_INFO(this->get_logger(), "Updated alive turtles dictionary: %d turtles.", 
                                                std::to_string(this->alive_turtles_.size()));
    }

    void publish_alive_turtles_array()
    {
        auto msg = my_robot_interfaces::msg::TurtleArray();
        for (const auto & [name, pos] : this->alive_turtles_){
            auto turtle = my_robot_interfaces::msg::Turtle();

            turtle.name = name;
            turtle.x = pos.first;
            turtle.y = pos.second;
            turtle.theta = 0.0;

            msg.turtles.push_back(turtle);
        }
        turtles_array_publisher_->publish(msg);
    }

    void myspawn_new_turtle()
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
        
        this->last_spawned_name_ = "";
    }

    void spawn_new_turtle()
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
        
        this->last_spawned_name_ = "";
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
        return
    }

    void after_kill(const std::string &name)
    {
        auto it = alive_turtles_.find(name);
        if (it != alive_turtles_.end()){
            alive_turtles_.erase(it);
        }
        this->publish_alive_turtles_array();
        RCLCPP_INFO(this->get_logger(), "Ate %s!", name.c_str());
    }

    void callback_call_spawn(rclcpp::Client<turtlesim::srv::Spawn>::SharedFuture future)
    {
        try {
            // here in C++, the response is the result of the future
            auto response = future.get();
            this->last_spawned_name_ = response->name;
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
            this->last_spawned_name_ = response->name;
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
    int max_iterations_;
    int current_iteration_ = 0;
    double kill_distance_threshold_;
    double p_gain_, angular_gain_, linear_vel_, radius_, angle_decrement_;

    turtlesim::msg::Pose::SharedPtr pose_ptr_; // array/vector
    std::map<std::string, std::pair<double, double>> alive_turtles_;
    std::string last_spawned_name_;

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
    rclcpp::TimerBase::SharedPtr kill_timer_;


};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleControllerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
