#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import random
from turtlesim.srv import Spawn, Kill
from example_interfaces.msg import Int64, String
import time


class TurtleControllerNode(Node):
    def __init__(self):
        super().__init__("turtle_controller")
        self.counter_ = 1
        self.max_iterations = 9
        self.current_iteration = 0

        self.spawn_client_ = self.create_client(Spawn, "trigger_spawn")
        self.kill_client_ = self.create_client(Kill, "trigger_kill")

        while not self.spawn_client_.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for trigger_spawn service...")

        while not self.kill_client_.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for trigger_kill service...")


        # Create subscriber
        self.turtles_subscriber_ = self.create_subscription(String, "alive_turtles", 
                                                            self.callback_turtles_alive, 10)

        self.timer_ = self.create_timer(1.5, self.control_loop)

        # Logging
        self.get_logger().info("Turtle Controller Node has started.")



    def control_loop(self):

        if self.current_iteration >= self.max_iterations:
            self.timer_.cancel()
            return

        self.spawn_new_turtle()

        # create one-shot timer
        self.kill_timer_ = self.create_timer(
            0.75,
            self.kill_timer_callback
        )

        self.current_iteration += 1



    def kill_timer_callback(self):
        self.kill_timer_.cancel()
        self.kill_last_turtle()


    def callback_turtles_alive(self, msg: String):
        self.get_logger().info(msg.data)

    def spawn_new_turtle(self):

        request = Spawn.Request()
        # Turtlesim coordinates are roughly 0.0 to 11.0
        request.x = random.uniform(1.0, 10.0)
        request.y = random.uniform(1.0, 10.0)
        request.theta = random.uniform(0.0, 6.28)
        request.name = 'turtle_' + str(self.counter_)

        future = self.spawn_client_.call_async(request)
        future.add_done_callback(self.callback_call_spawn)
        self.counter_ += 1
        self.last_spawned_name_ = None

        

    def kill_last_turtle(self):

        if self.last_spawned_name_ is None:
            return
        request = Kill.Request()
        request.name = self.last_spawned_name_
        future = self.kill_client_.call_async(request)
        future.add_done_callback(self.callback_call_kill)



    def callback_call_spawn(self, future):
        try:
            response = future.result()
            self.last_spawned_name_ = response.name
            self.get_logger().info(f"Successfully spawned: {response.name}")
        except Exception as e:
            self.get_logger().error(f"Service call failed: {e}")

    def callback_call_kill(self, future):
        try:
            response = future.result()
            self.get_logger().info(f"Successfully killed turtle")
        except Exception as e:
            self.get_logger().error(f"Service call failed: {e}")



def main(args=None):
    rclpy.init(args=args)
    node = TurtleControllerNode()
    rclpy.spin(node)
    rclpy.shutdown()


if __name__ == "__main__":
    main()
