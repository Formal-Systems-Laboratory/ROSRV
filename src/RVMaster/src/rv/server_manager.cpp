#include "ros/init.h"
#include "ros/console.h"
#include "ros/network.h"
#include "rv/XmlRpc.h"
#include "rv/server_manager.h"
#include "rv/xmlrpc_manager.h"
#include "rv/master.h"
#include "rv/acctrl_manager.h"
#include "ros/internal_timer_manager.h"
#include "ros/timer_manager.h"
#include "rv/callInfo.h"

#include <map>

using namespace std;

namespace rv
{

namespace monitor {
  std::set<std::string> monitorTopics;
}

// TODO: Fix project structure
string getMonitorSubscribedTopicForTopic(const std::string& topic) {
    return "/rv/monitored" + topic;
}
string getMonitorAdvertisedTopicForTopic(const std::string& topic) {
    return topic;
}

bool ServerManager::isMonitored(std::string const& topic) {
  return rv::monitor::monitorTopics.find(topic) != rv::monitor::monitorTopics.end();
}

ServerManagerPtr g_server_manager;
boost::mutex g_server_manager_mutex;

static string const MONITOR_POSTFIX = "__monitor__";

const ServerManagerPtr& ServerManager::instance()
{
  if (!g_server_manager)
  {
    boost::mutex::scoped_lock lock(g_server_manager_mutex);
    if (!g_server_manager)
    {
      g_server_manager.reset(new ServerManager);
    }
  }

  return g_server_manager;
}

ServerManager::ServerManager() : shutting_down_(false)
{
  acctrl::init();  // initialize access control
  ros::initInternalTimerManager();
}
ServerManager::~ServerManager()
{
  shutdown();
}

void ServerManager::start()
{
  boost::mutex::scoped_lock shutdown_lock(shutting_down_mutex_);
  shutting_down_ = false;

  real_ros_port = rv::master::getPort();

  char* rv_uri_env = NULL;
#ifdef _MSC_VER
  _dupenv_s(&rv_uri_env, NULL, "ROS_MASTER_URI");
#else
  rv_uri_env = getenv("ROS_MASTER_URI");
#endif
  if (!rv_uri_env)
  {
    rv_ros_host = "localhost";
    rv_ros_port = 11311;
  }
  else
  {
    if (!ros::network::splitURI(rv_uri_env, rv_ros_host, rv_ros_port))
    {
      ROS_FATAL("Couldn't parse the master URI [%s] into a host:port pair.", rv_uri_env);
      ROS_BREAK();
    }
  }

  // make localhost to ip address
  if (rv_ros_host == "localhost")
  {
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    string hostname_str(hostname);
    rv_ros_host = hostname_str;
  }
}

void ServerManager::shutdown()
{
  // boost::mutex::scoped_lock shutdown_lock(shutting_down_mutex_)
}
void ServerManager::requestTopicCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  ROS_INFO("REQUESTING TOPIC CALLBACK");

  requestTopic(params[1], params[2], result);
  ROS_FATAL("Client method requestTopic unexpectedly called on rvmaster");
  ros::requestShutdown();
  return;
}

// these functions should be implemented in the client??
bool ServerManager::requestTopic(const string& topic, XmlRpc::XmlRpcValue& protos, XmlRpc::XmlRpcValue& ret)
{
  ROS_INFO("Requesting topic: %s", topic.c_str());
  return false;
}
void ServerManager::pubUpdateCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  ROS_INFO("publisher UPDATE");

  result = rv::xmlrpc::responseInt(1, "", 0);
}

bool ServerManager::getPublishedTopicsCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  // 0: node_name

  string node_name = params[0];

  // keep a map from node_name to url

  ROS_INFO("Node %s trying to getPublishedTopics", node_name.c_str());

  std::string command = "getPublishedTopics";

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    // XmlRpc::XmlRpcValue payload;
    // master::execute("getPublishedTopics",params,result,payload,true);

    // Help Adam Chlipala
    // implement a filter for the topics

    /*
    Here's the pseudocode that should work:

    getPublishedTopics(caller_id, subgraph):
         var result : list of (topic, type) pairs

         if (subgraph != "" && subgraph[0] != "/") {
             // This subgraph reference is relative to the namespace of
    caller_id.
             subgraph := everythingUpToLastSlash(caller_id) + subgraph
         }

         result := mit_rosmaster.getPublishedTopics(caller_id, "/")
         result := filter result to drop any topic T where subgraph is not a
    string prefix of T
         return result
    */

    string subgraph = params[1];
    if (subgraph != "" && subgraph[0] != '/')
    {
      subgraph = node_name.substr(0, node_name.rfind("/") + 1) + subgraph;
    }
    params[1] = "/";
    XmlRpc::XmlRpcValue payload;
    master::execute("getPublishedTopics", params, result, payload, true);
    if (subgraph != "")
    {
      XmlRpc::XmlRpcValue result2;  // filtered result
      int k = 0;
      XmlRpc::XmlRpcValue pairs = result[2];
      for (int i = 0; i < pairs.size(); i++)
      {
        XmlRpc::XmlRpcValue pair = pairs[i];
        string topic = pair[0];
        if (topic.substr(0, subgraph.size()) == subgraph)
          result2[k++] = pair;
      }
      result[2] = result2;
    }
    ROS_INFO("Node %s successfully getPublishedTopics from %s", node_name.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to getPublishedTopics from %s due to access control!", node_name.c_str(),
             ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::getUriCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string node_name = params[0];

  // keep a map from node_name to url

  ROS_INFO("Node %s trying to getUri from %s", node_name.c_str(), ci.ip.c_str());

  std::string command = "getUri";

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;

    master::execute("getUri", params, result, payload, true);

    ROS_INFO("Node %s successfully getUri from %s", node_name.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to getUri from %s due to access control!", node_name.c_str(), ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::getPidCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  // 0: node_name

  string node_name = params[0];

  // keep a map from node_name to url

  ROS_INFO("Node %s trying to getPid from %s", node_name.c_str(), ci.ip.c_str());

  std::string command = "getPid";

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;

    master::execute("getPid", params, result, payload, true);

    ROS_INFO("Node %s successfully getPid from %s", node_name.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to getPid from %s due to access control!", node_name.c_str(), ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}
bool ServerManager::getTopicTypesCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  // 0: node_name

  string node_name = params[0];

  // keep a map from node_name to url

  ROS_INFO("Node %s trying to getTopicTypes from %s", node_name.c_str(), ci.ip.c_str());

  std::string command = "getTopicTypes";

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("getTopicTypes", params, result, payload, true);

    ROS_INFO("Node %s successfully getTopicTypes from %s", node_name.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to getTopicTypes from %s due to access control!", node_name.c_str(), ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}


/*[real_ros_port,
   rv_ros_port,
   [[name,topic,xmlrpc_port,tcp_port],[...],[...]]]
*/
bool ServerManager::getRVStateCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string node_name = params[0];

  ROS_INFO("Node %s trying to getRVState", node_name.c_str());

  std::string command = "getRVState";

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    /*
    [[realrosmaster, portnumber],
    [rvrosmaster, portnumber],
    monitors
    ]

    ['name','topic',[publisher,portnumber],[subscriber,portnumber]]
    */

    XmlRpc::XmlRpcValue monitor_info;

    monitor_info[0] = int(real_ros_port);
    monitor_info[1] = int(rv_ros_port);

    XmlRpc::XmlRpcValue monitors_value;

    monitor_info[2] = monitors_value;

    result[0] = 1;
    result[1] = "RV State";
    result[2] = monitor_info;

    ROS_INFO("Node %s successfully getRVState from %s", node_name.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to getRVState from %s due to access control!", node_name.c_str(), ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::getSystemStateCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  // 0: node_name

  string node_name = params[0];

  // keep a map from node_name to url

  ROS_INFO("Node %s trying to getSystemState", node_name.c_str());

  std::string command = "getSystemState";

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("getSystemState", params, result, payload, true);

    ROS_INFO("Node %s successfully getSystemState from %s", node_name.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to getSystemState from %s due to access control!", node_name.c_str(), ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::lookupServiceCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  // 0: node_name
  // 1: service name

  string node_name = params[0];
  string service = params[1];

  // keep a map from node_name to url

  ROS_INFO("Node %s trying to lookup service %s from %s", node_name.c_str(), service.c_str(), ci.ip.c_str());

  std::string command = "lookupService";  //+service;

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("lookupService", params, result, payload, true);

    ROS_INFO("Node %s successfully lookup-ed service %s from %s", node_name.c_str(), service.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to lookup service %s from %s due to access control!", node_name.c_str(),
             service.c_str(), ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::lookupNodeCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  // 0: node_name
  // 1: lookup node name

  string node_name = params[0];
  string lookup_node_name = params[1];

  // keep a map from node_name to url

  ROS_INFO("Node %s trying to lookup node %s from %s", node_name.c_str(), lookup_node_name.c_str(), ci.ip.c_str());

  std::string command = "lookupNode";  //+lookup_node_name;

  if (acctrl::isCommandAllowed(command, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;

    master::execute("lookupNode", params, result, payload, true);
    string uri = result[2];
    ROS_INFO("Node %s successfully lookup-ed node %s with uri %s from %s", node_name.c_str(), lookup_node_name.c_str(),
             uri.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to lookup node %s from %s due to access control!", node_name.c_str(),
             lookup_node_name.c_str(), ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::registerServiceCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string node_name = params[0];
  string service = params[1];
  string service_uri = params[2];
  string uri = params[3];

  // string host;
  // uint32_t port;
  // if(!ros::network::splitURI(uri,host,port))
  // return false;

  ROS_INFO("Node %s trying to register service %s at address %s from %s", node_name.c_str(), service.c_str(),
           service_uri.c_str(), uri.c_str());

  XmlRpc::XmlRpcValue payload;
  master::execute("registerService", params, result, payload, true);

  ROS_INFO("Node %s successfully registered service %s from %s", node_name.c_str(), service.c_str(), ci.ip.c_str());
  return true;
  /*if(acctrl::isSubscriberAllowed(service,host))//host or node_name??
  {

  XmlRpcValue payload;
  master::execute("registerService",params,result,payload,true);

  ROS_INFO("Node %s successfully registered service %s", node_name.c_str(), service.c_str());
  return true;
  }
  else
  {
  ROS_WARN("Node %s is not able to subscribe service %s due to access control!",node_name.c_str(), service.c_str());
  return false;
  }*/
}

bool ServerManager::registerSubscriberCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string node_name = params[0];
  string topic = params[1];
  string datatype = params[2];
  string uri = params[3];

  ROS_INFO("Node %s trying to subscribe to topic %s from %s with datatype %s", node_name.c_str(), topic.c_str(),
           ci.ip.c_str(), datatype.c_str());

  XmlRpc::XmlRpcValue payload;
  if (!acctrl::isSubscriberAllowed(topic, node_name, ci.ip))  // ip address
  {
    ROS_WARN("Node %s is not able to subscribe to topic %s due to access control!", node_name.c_str(), topic.c_str());
    return false;
  }

  master::execute("registerSubscriber", params, result, payload, true);

  std::cerr << "result: " << result << std::endl;
  return true;
}

bool ServerManager::unregisterServiceCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string name = params[0];
  string service = params[1];
  ROS_INFO("Node %s unregister service %s from %s", name.c_str(), service.c_str(), ci.ip.c_str());
  XmlRpc::XmlRpcValue payload;
  master::execute("unregisterService", params, result, payload, true);
  return true;
}
bool ServerManager::unregisterSubscriberCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci,
                                                 XmlRpc::XmlRpcValue& result)
{
  string node_name = params[0];
  string topic = params[1];
  string uri = params[2];
  ROS_INFO("Node %s trying to unregister as a subscriber to topic %s from %s", node_name.c_str(), topic.c_str(),
           ci.ip.c_str());
  // ROS_INFO("Real ip address: %s  port: %d", ci.ip.c_str(), ci.port);

  if (acctrl::isSubscriberAllowed(topic, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("unregisterSubscriber", params, result, payload, true);
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to unregister subscriber to topic %s due to access control!", node_name.c_str(),
             topic.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

using namespace XmlRpc;

bool lookupNode(string const& caller_id, string const& node_name, XmlRpcValue& ret)
{
  XmlRpcValue request; request[0] = caller_id; request[1] = node_name;
  XmlRpcValue response;
  XmlRpcValue payload;
  master::execute("lookupNode", request, response, payload, false);
  if (int(response[0]) != 1)
  {
    ROS_WARN_STREAM("Could not lookup node: '" << node_name << "' " << response[1] );
    return false;
  }
  ret = response[2];
  std::cerr << "lookNode: " << node_name << " --- " << response << std::endl;
  return true;
}

bool getPublishersForTopic(string const& caller_id, string const& topic, XmlRpcValue& ret) {
  XmlRpcValue request; request[0] = caller_id;
  XmlRpcValue response;
  XmlRpcValue payload;
  master::execute("getSystemState", request, response, payload, false);
  std::cerr << "getSystemState: " << response << std::endl;
  ROS_ASSERT(response.getType() == XmlRpc::XmlRpcValue::TypeArray);
  ROS_ASSERT(response.size() == 3);
  XmlRpcValue& systemState = response[2];
  std::cerr << "   systemState: " << systemState<< std::endl;
  XmlRpcValue& publish_topics = systemState[0];
  ROS_ASSERT(publish_topics.getType() == XmlRpc::XmlRpcValue::TypeArray);
  std::cerr << "   publish_topics: " << publish_topics << std::endl;
  std::cerr << "   publish_topics.size(): " << publish_topics.size() << std::endl;
  std::cerr << "looking for topic: " << topic << std::endl;

//  for (publish_topic: publish_topics) {
  for (size_t i = 0; i < publish_topics.size(); i++) {
    XmlRpcValue& publish_topic = publish_topics[i];
    if (topic != string(publish_topic[0])) continue;

    XmlRpcValue& publishers = publish_topic[1];
    ret.setSize(publishers.size());
    for (size_t j = 0; j < publishers.size(); j++) {
      XmlRpcValue& publisher = publishers[j]; 
      lookupNode(caller_id, publisher, ret[j]);
    }
    return true;
  }
  ret.setSize(0);
  return true;
}

bool ServerManager::registerPublisherCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string& node_name = params[0];
  string& topic = params[1];
  string& datatype = params[2];
  string& uri = params[3];

  if (!acctrl::isPublisherAllowed(topic, node_name, ci.ip))
  {
    ROS_WARN("Node %s is not able to publish to topic %s due to access control!", node_name.c_str(), topic.c_str());
    result = rv::xmlrpc::responseInt(0, "Access Control", 0);
    return false;
  }

  ROS_INFO("Node %s trying to publish to topic %s from %s", node_name.c_str(), topic.c_str(), ci.ip.c_str());
  bool is_monitored = isMonitored(topic);
  if (is_monitored && node_name != "/rvmonitor") {
    string monitor_topic = getMonitorSubscribedTopicForTopic(topic);
    ROS_INFO("Topic %s is monitored. Registering to %s instead.", topic.c_str(), monitor_topic.c_str());
    topic = monitor_topic;
  }

  XmlRpc::XmlRpcValue payload;
  master::execute("registerPublisher", params, result, payload, true);

//  if (is_monitored) {
//    getPublishersForTopic(node_name, topic, result[2]);
//  }

  std::cerr << "RESPONDING _---------------------- " << result << std::endl;

  ROS_INFO("Node %s successfully registered as a publisher to topic %s", node_name.c_str(), topic.c_str());
  return true;
}

bool ServerManager::unregisterPublisherCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci,
                                                XmlRpc::XmlRpcValue& result)
{
  // 0: node name
  // 1: topic name
  // 2: uri
  string node_name = params[0];
  string topic = params[1];

  ROS_INFO("Node %s trying to unregister as a publisher to topic %s from %s", node_name.c_str(), topic.c_str(),
           ci.ip.c_str());
  // ROS_INFO("Real ip address: %s  port: %d", ci.ip.c_str(), ci.port);

  if (acctrl::isPublisherAllowed(topic, node_name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;

    master::execute("unregisterPublisher", params, result, payload, true);

    ROS_INFO("Node %s successfully unregistered as a publisher to topic %s", node_name.c_str(), topic.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to unregister publish to topic %s due to access control!", node_name.c_str(),
             topic.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::unsubscribeParamCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string node_name = params[0];
  string mapped_key = params[2];
  string uri = params[1];

  ROS_INFO("Node %s trying to unsubscribeParam %s from %s", node_name.c_str(), mapped_key.c_str(), ci.ip.c_str());

  string command = "unsubscribeParam";                      //+mapped_key;
  if (acctrl::isCommandAllowed(command, node_name, ci.ip))  //??host or node_name
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("unsubscribeParam", params, result, payload, true);

    ROS_INFO("Node %s successfully unsubscribed to param %s from %s", node_name.c_str(), mapped_key.c_str(),
             ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to unsubscribe to param %s from %s due to access control!", node_name.c_str(),
             mapped_key.c_str(), ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::subscribeParamCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string node_name = params[0];
  string mapped_key = params[2];
  string uri = params[1];

  // string host;
  // uint32_t port;
  // if(!ros::network::splitURI(uri,host,port))
  // return false;

  ROS_INFO("Node %s trying to subscribeParam %s from %s", node_name.c_str(), mapped_key.c_str(), ci.ip.c_str());

  string command = "subscribeParam";                        //+mapped_key;
  if (acctrl::isCommandAllowed(command, node_name, ci.ip))  //??host or node_name
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("subscribeParam", params, result, payload, true);

    ROS_INFO("Node %s successfully subscribed to param %s from %s", node_name.c_str(), mapped_key.c_str(),
             ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to subscribe to param %s from %s due to access control!", node_name.c_str(),
             mapped_key.c_str(), ci.ip.c_str());

    // return bad result to the publisher??

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::hasParamCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string name = params[0];
  string mapped_key = params[1];

  ROS_INFO("Node %s trying to hasParam %s from %s", name.c_str(), mapped_key.c_str(), ci.ip.c_str());
  string command = "hasParam";  //+mapped_key;
  if (acctrl::isCommandAllowed(command, name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("hasParam", params, result, payload, true);
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to hasParam %s from %s due to access control!", name.c_str(), mapped_key.c_str(),
             ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::searchParamCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string name = params[0];
  string mapped_key = params[1];

  bool flag(true);
  while (flag)
  {
    size_t pos = name.find("//");
    if (pos != std::string::npos)
    {
      name.replace(pos, 2, "/");
    }
    else
    {
      flag = false;
    }
  }

  ROS_INFO("Node %s trying to search parameter %s from %s", name.c_str(), mapped_key.c_str(), ci.ip.c_str());

  string command = "searchParam";  //+mapped_key;
  if (acctrl::isCommandAllowed(command, name, ci.ip))
  {
    // XmlRpc::XmlRpcValue payload;
    // master::execute("searchParam",params,result,payload,true);

    // Help Adam Chlipala
    // change to a series of hasParam calls

    string ns = name;
    bool hasParam = false;
    size_t found = ns.length();
    do
    {
      ns = ns.substr(0, found);
      string key = ns + "/" + mapped_key;
      params[1] = key;

      ROS_INFO("converted to hasParam %s", key.c_str());

      XmlRpc::XmlRpcValue payload;
      master::execute("hasParam", params, result, payload, true);

      hasParam = result[2];
      /*if(hasParam)
        ROS_INFO("searchparam %s returned True",key.c_str());
      else
        ROS_INFO("searchparam %s returned False",key.c_str());
      */
      if (hasParam)
      {
        result[0] = 1;
        result[1] = "Found [" + key + "]";
        result[2] = key;
        break;
      }

      if (found == 0)
        break;

      found = ns.find_last_of("/");

    } while (found != std::string::npos);

    // if no key is found, return:
    // [-1, 'Cannot find parameter [key] in upward search', '']
    if (!hasParam)
    {
      result[0] = -1;
      result[1] = "Cannot find parameter [" + mapped_key + "] in an upwards search";
      result[2] = "";
    }

    // ROS_INFO("Node %s successfully searched parameter %s and return %s from %s",name.c_str(),
    // mapped_key.c_str(),(string(result[2])).c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to search parameter %s from %s due to access control!", name.c_str(),
             mapped_key.c_str(), ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::setParamCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string name = params[0];
  string mapped_key = params[1];
  // string value = params[2]; //may not be a string

  ROS_INFO("Node %s trying to set parameter %s from %s", name.c_str(), mapped_key.c_str(), ci.ip.c_str());

  string command = "setParam";  //+mapped_key;
  if (acctrl::isCommandAllowed(command, name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("setParam", params, result, payload, true);
    // ROS_INFO("Node %s succesfully set parameter %s to value %s from %s",name.c_str(), mapped_key.c_str(),
    // value.c_str(), ci.ip.c_str());
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to set parameter %s from %s due to access control!", name.c_str(), mapped_key.c_str(),
             ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::getParamNamesCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string name = params[0];

  ROS_INFO("Node %s trying to getParamNames from %s", name.c_str(), ci.ip.c_str());
  string command = "getParam";  //+mapped_key;
  if (acctrl::isCommandAllowed(command, name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("getParamNames", params, result, payload, true);
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to getParamNames from %s due to access control!", name.c_str(), ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}
bool ServerManager::getParamCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string name = params[0];
  string mapped_key = params[1];

  ROS_INFO("Node %s trying to get parameter %s from %s", name.c_str(), mapped_key.c_str(), ci.ip.c_str());
  string command = "getParam";  //+mapped_key;
  if (acctrl::isCommandAllowed(command, name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("getParam", params, result, payload, true);
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to get parameter %s from %s due to access control!", name.c_str(), mapped_key.c_str(),
             ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

bool ServerManager::deleteParamCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  string name = params[0];
  string mapped_key = params[1];

  ROS_INFO("Node %s trying to delete parameter %s from %s", name.c_str(), mapped_key.c_str(), ci.ip.c_str());
  string command = "deleteParam";  //+mapped_key;
  if (acctrl::isCommandAllowed(command, name, ci.ip))
  {
    XmlRpc::XmlRpcValue payload;
    master::execute("deleteParam", params, result, payload, true);
    return true;
  }
  else
  {
    ROS_WARN("Node %s is not able to delete parameter %s from %s due to access control!", name.c_str(),
             mapped_key.c_str(), ci.ip.c_str());

    result = rv::xmlrpc::responseInt(0, "Access Control", 0);

    return false;
  }
}

/**
for testing only
*/
bool ServerManager::testxmlCallback(XmlRpc::XmlRpcValue& params, ClientInfo& ci, XmlRpc::XmlRpcValue& result)
{
  ROS_INFO("TEST XML CALLBACK");
  result = 100;
  return true;
}

}  // namespace rv
