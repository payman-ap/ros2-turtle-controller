#include "rclcpp/rclcpp.hpp"

// public inheritance
class MyNode : public rclcpp::Node
{
public:
    MyNode() : Node("turtle_spawner"), counter_(0)
    {
        // using this-> is optional
        RCLCPP_INFO(this->get_logger(), "Hello world");
        timer_ = this->create_wall_timer(std::chrono::seconds(1),
                                         std::bind(&MyNode::timerCallback, this));
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
    auto node = std::make_shared<MyNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
