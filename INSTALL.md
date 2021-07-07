# ROSRV Manual Installation

Here are instructions for installing and building ROSRV by checking out its
source code on GitHub.

## Prerequisites

### Core Prerequisites

1. [ROS Kinetic](http://wiki.ros.org/kinetic)
2. [PyTest](https://docs.pytest.org/)
3. [Maven](https://maven.apache.org)
4. [Catkin](http://wiki.ros.org/catkin)

### RV-Monitor

1. [RV-Monitor](https://github.com/runtimeverification/rv-monitor)
    To use RV-Monitor supported logics RV-Monitor install RV-Monitor
    using the instructions at the tool's page (use `mvn install`
    to install snapshot to local maven repository).
    Set environment variable `RVMONITOR` to point to `rv-monitor's`
    root directory.

## Install and Build

1. After Installing Prerequisites, run `./Test` to
   Build and Run Tests.

2. The default behavior of the testing setup is to run
   all tests. Use `PYTEST_FLAGS="--skip-dl-tests"` to
   skip Differential Dynamic Logic based tests.

### Running
ROSRV can simply be treated as a catkin project, and
dropped into an existing catkin workspace.

The `bin/rosrv` provides a wrapper to the
C++ code generator. For example `bin/rosrv <SPEC_FILE> -o [MONITOR_NAME]`
will generate a monitor `MONITOR_NAME` which can be run
via catkin as `catkin run rvmonitor <MONITOR_NAME>`.

See [docs/Usage.md](docs/Usage.md) for further information on how to run ROSRV.
Get help or report problems on
[ROSRV's issues page](https://github.com/FormalSystemsLaboratory/ROSRV/issues).

# ROSRV Guided Installation

These instructions will take you from a fresh box to a working ROSRV installation.
For maximum portability, installing a fresh machine/VM is recommended.

0. Find an Ubuntu 16.04 box or spin up a new Ubuntu 16.04 machine/VM.
   All other steps occur on this machine/VM and assume the default shell is bash.

1. Setup ROS PPA

```
sudo sh -c '. /etc/lsb-release && echo "deb http://mirror.umd.edu/packages.ros.org/ros/ubuntu $DISTRIB_CODENAME main" > /etc/apt/sources.list.d/ros-latest.list'
sudo apt-key adv --keyserver 'hkp://keyserver.ubuntu.com:80' --recv-key C1CF6E31E6BADE8868B172B4F42ED6FBAB17C654
sudo apt-get update
```

2. Install ROS package and RV-Monitor and ROSRV dependencies

```
sudo apt install ros-kinetic-desktop-full
sudo apt install default-jdk-headless clang-6.0 maven python3-pip python3-yaml ros-kinetic-marti-common-msgs
```

3. Initialize ROS with this magic

```
sudo rosdep init
rosdep update
echo "source /opt/ros/kinetic/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

4. Install pytest via pip3

```
pip3 install pytest
```

5. Install RVMonitor on the darpa branch and setup environment variable

```
git clone -b darpa https://github.com/runtimeverification/rv-monitor
( cd rv-monitor; mvn install -DskipTests; )
echo "export RVMONITOR=\"$(pwd)/rv-monitor\"" >> $HOME/.bashrc
```

6. Get ROSRV master and run the tests (minus the DDL tests)

```
git clone https://github.com/Formal-Systems-Laboratory/ROSRV
cd ROSRV
PYTEST_FLAGS="--skip-dl-tests" ./Test
```
