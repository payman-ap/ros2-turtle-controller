from setuptools import find_packages, setup

package_name = 'my_py_turtle'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='ros',
    maintainer_email='ros@todo.todo',
    description='TODO: Package description',
    license='TODO: License declaration',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            "py_node = my_py_turtle.my_first_node:main",
            "turtle_spawner = my_py_turtle.turtle_spawner:main",
            "turtle_controller = my_py_turtle.turtle_controller:main"
        ],
    },
)
