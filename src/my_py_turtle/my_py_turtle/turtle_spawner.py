#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from turtlesim.srv import Spawn, Kill
from example_interfaces.msg import Int64, String
from my_robot_interfaces.srv import CatchTurtle, SpawnTurtle
from my_robot_interfaces.msg import Turtle, TurtleArray

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
        self.catch_service_ = self.create_service(CatchTurtle, "trigger_catch",
                                                  self.callback_catch_turtle)
        self.myspawn_service_ = self.create_service(SpawnTurtle, "trigger_myspawn",
                                                  self.callback_myspawn_turtle)
        # Create publisher
        self.turtles_publisher_ = self.create_publisher(Turtle, "alive_turtles", 10)

        # Logging
        self.get_logger().info("Turtle Spawner Node has started.")

    
    def callback_catch_turtle(self, request: CatchTurtle.Request, response: CatchTurtle.Response):
        self.get_logger().info(f"Killing turtle {request.name}")
        request_obj = Kill.Request()
        request_obj.name = request.name
        self.kill_client_.call_async(request_obj)
        response.success = True
        return response

    
    def callback_myspawn_turtle(self, request: SpawnTurtle.Request, response: SpawnTurtle.Response):
        # Forward the request to turtlesim
        self.get_logger().info(f"Spawning turtle {request.name} at ({request.x}, {request.y})")
        
        # Call turtlesim synchronously for simplicity in this proxy
        request_turtlesim = Spawn.Request()
        request_turtlesim.x = request.x
        request_turtlesim.y = request.y
        request_turtlesim.theta = request.theta
        request_turtlesim.name = request.name

        self.spawn_client_.call_async(request_turtlesim)

        
        response.name = request.name

        msg = Turtle()
        msg.name = request.name
        msg.x = request.x
        msg.y = request.y
        msg.theta = request.theta
        self.get_logger().info(f"Spawned Turtle {request.name}")
        self.turtles_publisher_.publish(msg)

        return response


    def callback_kill_turtle(self, request: Kill.Request, response: Kill.Response):
        self.get_logger().info(f"Killing turtle {request.name}")
        self.kill_client_.call_async(request)

        self.get_logger().info(f"Killed Turtle {request.name}")

        return response



    def callback_spawn_turtle(self, request: Spawn.Request, response: Spawn.Response):
        # Forward the request to turtlesim
        self.get_logger().info(f"Spawning turtle {request.name} at ({request.x}, {request.y})")
        
        # Call turtlesim asynchronously
        self.spawn_client_.call_async(request)

        response.name = request.name

        msg = Turtle()
        msg.name = request.name
        msg.x = request.x
        msg.y = request.y
        msg.theta = request.theta
        self.get_logger().info(f"Spawned Turtle {request.name}")
        self.turtles_publisher_.publish(msg)

        return response
    
    


def main(args=None):
    rclpy.init(args=args)
    node = TurtleSpawnerNode()
    rclpy.spin(node)
    rclpy.shutdown()


if __name__ == "__main__":
    main()
