cmake_minimum_required(VERSION 2.8.3)
project(fusion)

set(CMAKE_CXX_FLAGS "-std=c++11")

## Find catkin macros and libraries
## if COMPONENTS list like find_package(catkin REQUIRED COMPONENTS xyz)
## is used, also find other catkin packages
find_package(catkin REQUIRED COMPONENTS
  cv_bridge
  image_transport
  roscpp
  sensor_msgs
  std_msgs
  ros_visual_msgs
  vision
)

find_package(Boost  COMPONENTS filesystem)

catkin_package(CATKIN_DEPENDS std_msgs)

aux_source_directory(src/ SRC_LIST)

include_directories(
  include
  /usr/include/
  ${catkin_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${vision_INCLUDE_DIRS}
)

link_directories(
  /usr/lib/
  /usr/local/lib/ 
  /usr/lib/x86_64-linux-gnu/ 
)

add_executable(fusion ${SRC_LIST})


target_link_libraries(${PROJECT_NAME} 
  ${catkin_LIBRARIES}
  ${Boost_LIBRARIES}
  ${vision_LIBRARIES}
)

