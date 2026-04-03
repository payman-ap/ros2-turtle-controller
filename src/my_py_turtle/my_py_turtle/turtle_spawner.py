#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from turtlesim.srv import Spawn, Kill
from example_interfaces.msg import Int64, String

class TurtleSpawnerNode(Node):
    def __init__(self):
        super().__init__("turtle_spawner")
        # creating a client to talkl to the turtlesim node
        self.spawn_client_ = self.create_client(Spawn, "spawn")
        self.kill_client_ = self.create_client(Kill, "kill")
        
        while not self.spawn_client_.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for /spawn service...")

        while not self.kill_client_.wait_for_service(timeout_sec=1.0):
            self.get_logger().info("Waiting for /kill service...")
        
        # Creating a service so the controller can trigger a spawn
        self.spawn_service_ = self.create_service(Spawn, "trigger_spawn", 
                                                  self.callback_spawn_turtle)
        self.kill_service_ = self.create_service(Kill, "trigger_kill",
                                                 self.callback_kill_turtle)
        # Create publisher
        self.turtles_publisher_ = self.create_publisher(String, "alive_turtles", 10)

        # Logging
        self.get_logger().info("Turtle Spawner Node has started.")

    
    def callback_kill_turtle(self, request: Kill.Request, response: Kill.Response):
        self.get_logger().info(f"Killing turtle {request.name}")
        self.kill_client_.call_async(request)

        msg = String()
        msg.data = f"Killed Turtle {request.name}"

        self.turtles_publisher_.publish(msg)

        return response



    def callback_spawn_turtle(self, request: Spawn.Request, response: Spawn.Response):
        # Forward the request to turtlesim
        self.get_logger().info(f"Spawning turtle {request.name} at ({request.x}, {request.y})")
        
        # Call turtlesim synchronously for simplicity in this proxy
        self.spawn_client_.call_async(request)

        response.name = request.name

        msg = String()
        msg.data = f"Spawned Turtle {request.name}"

        self.turtles_publisher_.publish(msg)

        return response
    
    


def main(args=None):
    rclpy.init(args=args)
    node = TurtleSpawnerNode()
    rclpy.spin(node)
    rclpy.shutdown()


if __name__ == "__main__":
    main()
