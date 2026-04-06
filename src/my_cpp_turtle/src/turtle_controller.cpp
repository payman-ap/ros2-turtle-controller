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

using namespace std::placeholders;
using namespace std::chrono_literals;


class TurtleControllerNode : public rclcpp::Node
{
public:
    TurtleControllerNode() : Node("turtle_controller"), counter_(0)
    {
        // using this-> is optional
        RCLCPP_INFO(this->get_logger(), "Hello world");
        timer_ = this->create_wall_timer(std::chrono::seconds(1),
                                         std::bind(&TurtleControllerNode::timerCallback, this));
    }

private:
    void timerCallback()
    {
        RCLCPP_INFO(this->get_logger(), "Hello %d", counter_);
        counter_++;
    }
    
    rclcpp::TimerBase::SharedPtr timer_;
    int counter_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<TurtleControllerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
