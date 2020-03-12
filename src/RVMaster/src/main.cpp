#include "ros/console.h"
#include "rv/xmlrpc_manager.h"
#include "rv/server_manager.h"
#include "rv/master.h"

#include "ros/duration.h"
#include <string>
#include <iostream>
#include <sstream>

namespace rv { namespace monitor {
  extern std::set<std::string> monitorTopics;
}}

int main(int argc, char **argv)
{
  std::cerr << "Parsing args" << std::endl;
  for (int i = 1; i < argc; i++) {
    std::cerr << "i: " << i << std::endl;
    std::cerr << "argv: " << argv[i] << std::endl;
    if (argv[i] == std::string("--monitor-topic")) {
      i++;
      std::cerr << "argv: " << argv[i] << std::endl;
      if (i == argc) throw std::runtime_error("--monitor-topic requires one argument");
      rv::monitor::monitorTopics.insert(argv[i]);
    }
  }

  boost::shared_ptr<rv::XMLRPCManager> xmlrpc_manager_ = rv::XMLRPCManager::instance();
  boost::shared_ptr<rv::ServerManager> server_manager_ = rv::ServerManager::instance();

  ros::M_string remappings;
  rv::master::init(remappings);


  //--- ros master api
  // Order follows http://wiki.ros.org/ROS/Master_API

  // register/unregister methods
  xmlrpc_manager_->bind("registerService", boost::bind(&rv::ServerManager::registerServiceCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("unregisterService", boost::bind(&rv::ServerManager::unregisterServiceCallback, server_manager_,_1,_2,_3));

  xmlrpc_manager_->bind("registerSubscriber",boost::bind(&rv::ServerManager::registerSubscriberCallback,server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("unregisterSubscriber",boost::bind(&rv::ServerManager::unregisterSubscriberCallback,server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("registerPublisher",boost::bind(&rv::ServerManager::registerPublisherCallback,server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("unregisterPublisher",boost::bind(&rv::ServerManager::unregisterPublisherCallback,server_manager_,_1,_2,_3));

  // Name servic and system state
  //subscriber and publisher
  xmlrpc_manager_->bind("lookupNode", boost::bind(&rv::ServerManager::lookupNodeCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("getPublishedTopics", boost::bind(&rv::ServerManager::getPublishedTopicsCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("getTopicTypes", boost::bind(&rv::ServerManager::getTopicTypesCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("getSystemState", boost::bind(&rv::ServerManager::getSystemStateCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("getUri", boost::bind(&rv::ServerManager::getUriCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("lookupService", boost::bind(&rv::ServerManager::lookupServiceCallback, server_manager_,_1,_2,_3));

  // --- parameter server api
  // Order follows http://wiki.ros.org/ROS/Parameter%20Server%20API
  xmlrpc_manager_->bind("deleteParam", boost::bind(&rv::ServerManager::deleteParamCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("setParam", boost::bind(&rv::ServerManager::setParamCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("getParam", boost::bind(&rv::ServerManager::getParamCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("searchParam", boost::bind(&rv::ServerManager::searchParamCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("subscribeParam", boost::bind(&rv::ServerManager::subscribeParamCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("unsubscribeParam", boost::bind(&rv::ServerManager::unsubscribeParamCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("hasParam", boost::bind(&rv::ServerManager::hasParamCallback, server_manager_,_1,_2,_3));
  xmlrpc_manager_->bind("getParamNames", boost::bind(&rv::ServerManager::getParamNamesCallback, server_manager_,_1,_2,_3));

  xmlrpc_manager_->start();
  server_manager_->start();

  ros::WallDuration(7*24*3600).sleep();
}
