#ifndef RV_SUBSCRIPTION_SHIM
#define RV_SUBSCRIPTION_SHIM

#include <string>
#include "ros/ros.h"

namespace rv { class SubscriptionShim; }

/* This shim creates connections to nodes for a topic (connectTopic) that
 * this node is not subscribed to. A second topic (handlerTopic) is used
 * to handle these connections.
 */
class rv::SubscriptionShim
{
public:
  SubscriptionShim(std::string const& connectTopic, std::string const& handlerTopic);

  bool connect(std::string const& uri);
  bool connectToPublishers();

private:
  ros::SubscriptionPtr getSubscriptionForTopic(std::string const& topic);
  bool executeRequestTopic(ros::SubscriptionPtr subscription, std::string const& xmlrpc_uri, XmlRpc::XmlRpcValue& proto);
  bool startROSTCPConnection(ros::SubscriptionPtr subscription, std::string const& xmlrpc_uri, XmlRpc::XmlRpcValue proto);

  std::string const connectTopic;
  std::string const handlerTopic;
  ros::TopicManagerPtr topic_manager;
};


#endif // RV_SUBSCRIPTION_SHIM

