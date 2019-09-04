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

template<class MessageType>
struct MonitorTopic
{
    ros::Publisher  publisher;
    ros::Subscriber subscriber;
    rv::SubscriptionShim subscription_shim;

    MonitorTopic(ros::NodeHandle& n, std::string const& topic, uint queue_len)
        : publisher(n.advertise<MessageType>(getMonitorAdvertisedTopicForTopic(topic), queue_len, true))
        , subscriber( n.subscribe( getMonitorSubscribedTopicForTopic(topic)
                                 , queue_len
                                 , &MonitorTopic<MessageType>::callback
                                 , this
                    )            )
        , subscription_shim(topic, getMonitorSubscribedTopicForTopic(topic))
    {
    }

    template<class T, class DatumType>
    void registerEvent( T* owner
                      , void (T::*callback)(DatumType) 
                      , DatumType (T::*getDatum)(MessageType&)
                      )
    {
        m_events.push_back([owner, callback, getDatum](MessageType& msg) -> void
                               { (owner->*callback)((owner->*getDatum)(msg)); });
    }

    void callback(boost::shared_ptr<const MessageType> ptr) {
        MessageType copy = *ptr;
        for (event_cb: m_events) { event_cb(copy); }
        publisher.publish(copy);
    }

private:
    std::vector<std::function<void (MessageType&)>> m_events;
};


}
}

#endif
