FROM ros:kinetic-ros-core

RUN apt-get update && apt-get --no-install-recommends install -y openjdk-8-jdk-headless clang-6.0 llvm-6.0-dev maven python3-pip python3-yaml ros-kinetic-marti-common-msgs python-catkin-tools
RUN pip3 install pytest
WORKDIR /app
RUN git clone --depth 1 -b darpa https://github.com/runtimeverification/rv-monitor
RUN git clone --depth 1          https://github.com/Formal-Systems-Laboratory/ROSRV
RUN cd rv-monitor && mvn install -DskipTests -pl '!installer'
ENV RVMONITOR="/app/rv-monitor"
RUN cd ROSRV/src/rosmop && mvn package -DskipTests
RUN . /opt/ros/kinetic/setup.sh && cd ROSRV && catkin build
