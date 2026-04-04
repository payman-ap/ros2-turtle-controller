#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import random
import math
from turtlesim.srv import Spawn, Kill
from turtlesim.msg import Pose
from geometry_msgs.msg import Twist
from example_interfaces.msg import Int64, String
import time


class TurtleControllerNode(Node):
    def __init__(self):
        super().__init__("turtle_controller")
        self.counter_ = 2
        self.max_iterations = 9
        self.current_iteration = 0
        self.alive_turtles = {}
        self.kill_distance_threshold = 0.6

        self.pose_ = None  # To store turtle1's current position so the timer can access it

        self.linear_vel = 2.0
        self.radius = 0.5
        self.angle_decrement = 0.02

        self.p_gain = 1.5
        self.angular_gain = 2.0
        

        self.spawn_client_ = self.create_client(Spawn, "trigger_spawn")
        self.kill_client_ = self.create_client(Kill, "trigger_kill")

        while not self.spawn_client_.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for trigger_spawn service...")

        while not self.kill_client_.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for trigger_kill service...")


        # Create subscriber
        self.turtles_subscriber_ = self.create_subscription(String, "alive_turtles", 
                                                            self.callback_turtles_alive, 10)
        self.pose_subscriber_ = self.create_subscription(Pose, "/turtle1/pose", 
                                                         self.pose_callback, 10)
        self.pose_publisher_ = self.create_publisher(String, "/turtle1_pose", 10)
        self.cmd_vel_publisher_ = self.create_publisher(Twist, "/turtle1/cmd_vel", 10)

        self.timer_ = self.create_timer(0.25, self.control_loop)

        self.move_timer_ = self.create_timer(0.1, self.move_to_closest_turtle)

        # Logging
        self.get_logger().info("Turtle Controller Node has started.")


    def pose_callback(self, msg: Pose):
        # self.get_logger().info(f"Turtle pose: ({str(msg.x)} , {str(msg.y)}) with {str(msg.theta)} degrees")
        # self.get_logger().info(str(self.alive_turtles))
        self.pose_ = msg
        pose_message = String()
        pose_message.data = f"Turtle pose: ({str(msg.x)} , {str(msg.y)}) with {str(msg.theta)} degrees"
        self.pose_publisher_.publish(pose_message)
        t1_x = msg.x
        t1_y = msg.y
        # Eating turtle logic
        for name in list(self.alive_turtles.keys()):
            pos = self.alive_turtles[name]
            dist = math.sqrt((t1_x - pos[0])**2 + (t1_y - pos[1])**2)
            if dist < self.kill_distance_threshold:
                self.kill_turtle(name)
                self.spawn_new_turtle()

    def move_spiral(self):
        cmd = Twist()
        # Constant forward motion
        cmd.linear.x = self.linear_vel
        # Angular velocity decreases over time to expand the spiral
        # msg.angular.z = v / r
        cmd.angular.z = self.linear_vel / self.radius
        self.radius += 0.01
        self.cmd_vel_publisher_.publish(cmd)

    def move_to_closest_turtle(self):
        if self.pose_ is None:
            return
        if not self.alive_turtles:
            # self.move_spiral()
            self.get_logger().info("No turtles to hunt?")
            return
        
        
        # Find the closest turtle
        t1_x = self.pose_.x
        t1_y = self.pose_.y
        closest_name = None
        min_dist = math.inf
        target_x, target_y = 0.0, 0.0

        for name, pos in self.alive_turtles.items():
            dist = math.sqrt((t1_x - pos[0])**2 + (t1_y - pos[1])**2)
            if dist < min_dist:
                target_x, target_y = pos[0], pos[1]
                min_dist = dist
                closest_name = name
        
        # Calculate the motion
        target_angle = math.atan2(target_y - t1_y, target_x - t1_x)
        steering_angle = target_angle - self.pose_.theta

        # Normalize angle_error to stay between -pi and pi (prevents spinning 350 degrees)
        steering_angle = math.atan2(math.sin(steering_angle), math.cos(steering_angle))

        # Generating the Twist command
        cmd = Twist()
        if dist > 0.1:  # Tolerance: stop if we are 0.1 units away
            # Proportional Control: move faster if far, slow down as you get closer
            cmd.linear.x = self.p_gain * min_dist  
            cmd.angular.z = self.angular_gain * steering_angle
        else:
            # Goal reached, Stop
            cmd.linear.x = 0.0
            cmd.angular.z = 0.0
        self.cmd_vel_publisher_.publish(cmd)


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
        request.name = 'turtle' + str(self.counter_)

        future = self.spawn_client_.call_async(request)
        self.alive_turtles[request.name] = [request.x, request.y]
        future.add_done_callback(self.callback_call_spawn)
        self.counter_ += 1
        self.last_spawned_name_ = None

    def kill_turtle(self, name):
        request = Kill.Request()
        request.name = name

        future = self.kill_client_.call_async(request)

        future.add_done_callback(lambda future: self.after_kill(name))
        

    def kill_last_turtle(self):
        return
        # if self.last_spawned_name_ is None:
        #     return
        # request = Kill.Request()
        # request.name = self.last_spawned_name_
        # future = self.kill_client_.call_async(request)
        # future.add_done_callback(self.callback_call_kill)

    def after_kill(self, name):
        if name in self.alive_turtles:
            del self.alive_turtles[name]
        self.get_logger().info(f"Ate {name}!")




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
