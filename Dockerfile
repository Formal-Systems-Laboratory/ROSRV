FROM ros:kinetic-ros-core

RUN apt-get update && apt-get install -y default-jdk-headless clang-6.0 maven python3-pip python3-yaml ros-kinetic-marti-common-msgs python-catkin-tools
RUN pip3 install pytest
WORKDIR /app
RUN git clone -b darpa https://github.com/runtimeverification/rv-monitor
RUN git clone          https://github.com/Formal-Systems-Laboratory/ROSRV
RUN cd rv-monitor && mvn install -DskipTests -pl '!installer'
ENV RVMONITOR="/app/rv-monitor"
RUN cd ROSRV/src/rosmop && mvn package -DskipTests
RUN . /opt/ros/kinetic/setup.sh && cd ROSRV && catkin build
