FROM osrf/ros:humble-desktop

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

ARG USERNAME=ros
ARG USER_UID=1000
ARG USER_GID=1000

RUN apt update && apt install -y \
    build-essential \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-vcstool \
    python3-argcomplete \
    ros-humble-ros-base \
    ros-humble-rviz2 \
    ros-humble-navigation2 \
    ros-humble-nav2-bringup \
    ros-humble-turtlesim \
    ros-humble-rqt \
    ros-humble-rqt-common-plugins \
    git \
    vim \
    nano \
    gcc \
    g++ \
    gdb \
    cmake \
    make \
    pkg-config \
    python3-pip \
    python3-setuptools \
    wget \
    curl \
    lsb-release \
    locales \
    sudo \
    dbus-x11 \
    x11-apps \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US en_US.UTF-8 && \
    update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8

ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

RUN rosdep init || true && rosdep update

RUN groupadd --gid ${USER_GID} ${USERNAME} && \
    useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# VS Code
RUN wget -qO- https://packages.microsoft.com/keys/microsoft.asc | gpg --dearmor > /usr/share/keyrings/ms.gpg && \
    echo "deb [arch=amd64 signed-by=/usr/share/keyrings/ms.gpg] https://packages.microsoft.com/repos/code stable main" > /etc/apt/sources.list.d/vscode.list && \
    apt update && apt install -y code

RUN echo "source /opt/ros/humble/setup.bash" >> /etc/bash.bashrc

WORKDIR /ros2_ws

USER ${USERNAME}
ENV HOME=/home/${USERNAME}

CMD ["bash"]
