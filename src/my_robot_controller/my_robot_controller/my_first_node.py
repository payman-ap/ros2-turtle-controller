#!/usr/bin/env python3
import rclpy
from rclpy.node import Node

class MyNode(Node):
    def __init__(self):
        # super: calling the constructor of the upper class, defining the Node name
        super().__init__("first_node")
        # self.get_logger().info("1- Hello from ROS2 Payman")
        self.counter_ = 0
        self.create_timer(1.0, self.timer_callback)

    def timer_callback(self):
        self.get_logger().info("2- Hello " + str(self.counter_))
        self.counter_ += 1


def main(args=None):
    # First thing: initialize ros2 communication
    rclpy.init(args=args)

    # ==== | All other codes | ====
    # create a Node: the node is created inside the program
    node = MyNode() # Here we don't have yet defined other arguments to pass
    rclpy.spin(node) # to keep alive the node and be kill Ctrl+C, all the callbacks will then be able to run



    # last thing
    rclpy.shutdown()

if __name__=='__main__':
    main()



