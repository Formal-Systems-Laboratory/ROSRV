#ifndef RV_MONITOR_H
#define RV_MONITOR_H

#include <string>
#include <functional>
#include <ros/ros.h> // TODO: Use more specific headers
#include <rv/subscription_shim.h>

namespace rv {
namespace monitor {

std::string getMonitorSubscribedTopicForTopic(const std::string& topic);
std::string getMonitorAdvertisedTopicForTopic(const std::string& topic);

struct MonitorTopicErased {
    ros::Publisher  publisher;
    ros::Subscriber subscriber;
    rv::SubscriptionShim subscription_shim;

    MonitorTopicErased(std::string const& topic, ros::Publisher pub, ros::Subscriber sub)
        : publisher(pub)
        , subscriber(sub)
        , subscription_shim(topic, getMonitorSubscribedTopicForTopic(topic))
    {
    }

    virtual ~MonitorTopicErased() = default;
};

using MonitorTopicErasedPtr = boost::shared_ptr<MonitorTopicErased>;

template<class MessageType>
struct MonitorTopic
    : MonitorTopicErased
{
    using Ptr = boost::shared_ptr<MonitorTopic<MessageType>>;

    MonitorTopic(ros::NodeHandle& n, std::string const& topic, uint queue_len)
        : MonitorTopicErased
            ( topic
            , n.advertise<MessageType>(getMonitorAdvertisedTopicForTopic(topic), queue_len, true)
            , n.subscribe( getMonitorSubscribedTopicForTopic(topic)
                                 , queue_len
                                 , &MonitorTopic<MessageType>::callback
                                 , this
            )            )
    {
    }

    template<class T>
    void registerEvent( T* owner
                      , void (T::*callback)(MessageType&)
                      )
    {
        m_events.push_back([owner, callback](MessageType& msg) -> void
                               { (owner->*callback)(msg); });
    }

    void callback(boost::shared_ptr<const MessageType> ptr) {
        MessageType copy = *ptr;
        for (auto event_cb: m_events) { event_cb(copy); }
        publisher.publish(copy);
    }

private:
    std::vector<std::function<void (MessageType&)>> m_events;
};

struct Monitor {
    template<class MessageType>
    typename MonitorTopic<MessageType>::Ptr withTopic(std::string const& topic) {
        typename MonitorTopic<MessageType>::Ptr ret = nullptr;
        if (monitored_topics.find(topic) == monitored_topics.end()) {
            unsigned int const queue_len = 1000;
            ret = boost::make_shared<MonitorTopic<MessageType>>(node_handle, topic, queue_len);
            monitored_topics.insert({topic, ret});
        }
        else {
            ret = boost::dynamic_pointer_cast<MonitorTopic<MessageType>>(monitored_topics.at(topic));
        }
    }

    bool isMonitoring() {

    }

    void enable_rvmaster_shims() {
    };
    ros::NodeHandle node_handle;

    std::map<std::string, MonitorTopicErasedPtr> monitored_topics;
};

}
}

#endif

