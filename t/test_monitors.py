#!/usr/bin/env python2

import rospy as ros
from subprocess import Popen, check_call
from std_msgs.msg import String, ColorRGBA, Float32, Header
from marti_common_msgs.msg import Float32Stamped
import time as pytime

import pytest

# Base Test Packet Class
# ======================

class RosTestPacket(object):
    def __init__(self, topic_name, msg_class, msg_type, to_send, callback):
        self.topic_name = topic_name
        self.msg_class = msg_class
        self.msg_type = msg_type
        self.to_send = to_send
        self.callback = callback

    @classmethod
    def to_formatted_string(cls, msg):
        pass

    def stamp_msg(self, msg):
        return msg


# Derived Test Packets
# ====================

class StringPacket(RosTestPacket):
    def __init__(self, topic_name, to_send, callback):
        super(StringPacket, self).__init__( topic_name
                                          , String, 'std_msgs/String'
                                          , to_send, callback)

    @classmethod
    def to_formatted_string(cls, msg):
        return msg.data


class ColorRGBAPacket(RosTestPacket):
    def __init__(self, topic_name, to_send, callback):
        super(ColorRGBAPacket, self).__init__( topic_name
                                             , ColorRGBA, 'std_msgs/ColorRGBA'
                                             , to_send, callback)

    @classmethod
    def to_formatted_string(cls, msg):
        return '({},{},{},{})'.format(msg.r, msg.g, msg.b, msg.a)


class Float32Packet(RosTestPacket):

    def __init__(self, topic_name, to_send, callback):
        super(Float32Packet, self).__init__( topic_name
                                           , Float32, 'std_msgs/Float32'
                                           , to_send, callback)

    @classmethod
    def to_formatted_string(cls, msg):
        return "{0:0.1f}".format(msg.data)

    def stamp_msg(self, msg):
        return msg

class Float32StampedPacket(RosTestPacket):

    def __init__(self, topic_name, to_send, callback):
        super(Float32StampedPacket, self).__init__( topic_name
                                                  , Float32Stamped , 'marti_common_msgs/Float32Stamped'
                                                  , to_send, callback)

    @classmethod
    def to_formatted_string(cls, msg):
        return "{0:0.1f}".format(msg.value)

    def stamp_msg(self, msg):
        h = ros.Header()
        h.stamp = ros.Time.now()
        return Float32Stamped(h, msg.value)

# Fixtures
#=========

@pytest.fixture(scope='session')
def launch_master(request):
    def run_launch_master(monitor_topics_list):
        nested_list__topics = [ ['--monitor-topic',  topic]
                                    for topic in monitor_topics_list ]
        flattened_list__topics = [ elements
                                    for sub_list__topics in nested_list__topics
                                        for elements in sub_list__topics ]
        master = Popen( ['/usr/bin/env', 'rosrun', 'rvmaster', 'rvmaster']
                      + flattened_list__topics)
        def cleanup():
            master.terminate()
        request.addfinalizer(cleanup)
    return run_launch_master


@pytest.fixture(scope="session")
def session_init(get_options, launch_master):
    if 'with-rvmaster' in get_options['monitor_flags']:
        launch_master(get_options['monitor_topics_list'])
    # Sleep(1)
    pytime.sleep(1)
    ros.init_node('test', anonymous=True)

@pytest.fixture()
def simple_pipeline(request, session_init, get_options):
    def run_simple_pipeline(test_packets):
        registered_topics = {}
        if('unmonitored_channel' in request.node.name):
            prefix = ''
        else:
            prefix = get_options['topic_prefix']
        rate = ros.Rate(10);
        rate.sleep(); rate.sleep(); rate.sleep(); rate.sleep()
        for packet in test_packets:
            # register callbacks. Don't re-register new callbacks for existing topics
            if not packet.topic_name in registered_topics:
                registered_topics[packet.topic_name] = \
                    ros.Subscriber(packet.topic_name, packet.msg_class, packet.callback.as_callable)
                rate.sleep()
            for unstamped_msg in packet.to_send:
                stamped_msg = packet.stamp_msg(unstamped_msg)
                check_call(['rostopic', 'pub', '--once'
                           , prefix + packet.topic_name
                           , packet.msg_type
                           , str(stamped_msg)])
            rate.sleep(); rate.sleep(); rate.sleep(); rate.sleep()
        for subscription in registered_topics.itervalues():
            subscription.unregister()
    return run_simple_pipeline

@pytest.fixture()
def launch_monitor(request, get_options):
    def run_launch_monitor(monitor_name):
        monitor = Popen(['/usr/bin/env', 'rosrun', 'rvmonitor', monitor_name, get_options['monitor_flags']])
        def cleanup():
            monitor.terminate()
        request.addfinalizer(cleanup)
        return True
    return run_launch_monitor

class CallBack(object):
    def __init__(self, to_formatted_string):
        self.recieved_list = []
        self.to_formatted_string = to_formatted_string

    def as_callable(self, msg):
        self.recieved_list.append(self.to_formatted_string(msg))


# Basic Tests
# ===========

def test_roscore__unmonitored_channel_pipeline(simple_pipeline):
    string_callback = CallBack(StringPacket.to_formatted_string)
    test_packets = [StringPacket('/unmonitored', ['Hi!'], string_callback)]
    simple_pipeline(test_packets)
    assert ['Hi!'] == string_callback.recieved_list

def test_roscore__monitored_channel__no_monitor_running(simple_pipeline):
    string_callback = CallBack(StringPacket.to_formatted_string)
    test_packets = [StringPacket('/chatter', ['Hi!', 'Hi!'], string_callback)]
    simple_pipeline(test_packets)

    assert([] == string_callback.recieved_list)

def test_roscore__monitored_channel(simple_pipeline, launch_monitor):
    launch_monitor('monitor-single-parameter')
    string_callback = CallBack(StringPacket.to_formatted_string)
    test_packets = [StringPacket('/chatter', ['Hi!', 'Hi!'], string_callback)]
    simple_pipeline(test_packets)

    assert(['Hi!RV1', 'Hi!RV2'] == string_callback.recieved_list)

def test_roscore__monitored_multiparam_channel(simple_pipeline, launch_monitor):
    launch_monitor('monitor-multiple-parameters')
    color_rgba_callback = CallBack(ColorRGBAPacket.to_formatted_string)
    test_packets = [ ColorRGBAPacket( '/color_chatter'
                                    , [str(ColorRGBA(100 ,100 ,100 ,0.5))]
                                    , color_rgba_callback)]
    simple_pipeline(test_packets)

    assert(['(105.0,110.0,115.0,0.5)'] == color_rgba_callback.recieved_list)

def test_roscore__monitored_multiparam_multichannel(simple_pipeline, launch_monitor):
    launch_monitor('monitor-multiple-channels')
    color_rgba_callback = CallBack(ColorRGBAPacket.to_formatted_string)
    string_callback = CallBack(StringPacket.to_formatted_string)
    test_packets = [ ColorRGBAPacket('/color_chatter'
                                    , [str(ColorRGBA(100 ,100 ,100 ,0.5))]
                                    , color_rgba_callback)
                   , StringPacket('/chatter', ['Hi!'], string_callback)]
    simple_pipeline(test_packets)

    assert(['(105.0,110.0,115.0,0.5)'] == color_rgba_callback.recieved_list)
    assert(['Hi!RV']                   == string_callback.recieved_list)

# Dl Tests
# ========

@pytest.mark.dlTest
def test_roscore__monitored_dl_watertank_unsafe(simple_pipeline, launch_monitor):
    h = ros.Header()
    h.stamp = ros.Time.now()
    launch_monitor('monitor-dl-watertank')
    sensor_callback = CallBack(Float32Packet.to_formatted_string)
    sensor_packets = [ Float32Packet( '/level_sensor', [x], sensor_callback)
                         for x in [0.0, 0.0] ]

    control_callback = CallBack(Float32StampedPacket.to_formatted_string)
    control_packets = [ Float32StampedPacket( '/flow_control_cmd'
                                            , [Float32Stamped(h, x)]
                                            , control_callback)
                            for x in [0.0, 0.7] ]

    simple_pipeline( [ sensor_packets[0], control_packets[0]
                   ,   sensor_packets[1], control_packets[1] ] )

    rate = ros.Rate(10)
    rate.sleep(); rate.sleep(); rate.sleep(); rate.sleep()

    # Unsafe Control Changed to safe control
    assert(['0.0', '0.0'] == control_callback.recieved_list)

    # Sensor Readings Unchanged
    assert(['0.0', '0.0'] == sensor_callback.recieved_list)

@pytest.mark.dlTest
def test_roscore__monitored_dl_watertank_safe_after_unsafe(simple_pipeline, launch_monitor):
    h = ros.Header()
    h.stamp = ros.Time.now()
    launch_monitor('monitor-dl-watertank')
    sensor_callback = CallBack(Float32Packet.to_formatted_string)
    sensor_packets = [ Float32Packet( '/level_sensor', [x], sensor_callback)
                           for x in [0.0, 0.0, 0.0] ]

    control_callback = CallBack(Float32StampedPacket.to_formatted_string)
    control_packets = [ Float32StampedPacket( '/flow_control_cmd'
                                            , [Float32Stamped(h, x)]
                                            , control_callback)
                            for x in [0.0, 0.5, 0.1] ]

    simple_pipeline( [ sensor_packets[0], control_packets[0]
                     , sensor_packets[1], control_packets[1]
                     , sensor_packets[2], control_packets[2] ])

    rate = ros.Rate(10)
    rate.sleep(); rate.sleep(); rate.sleep(); rate.sleep()

    # Unsafe Control to safe (fallback) control and safe (blackbox) control
    assert(['0.0', '0.0', '0.1'] == control_callback.recieved_list)

    # Sensor Readings Unchanged
    assert(['0.0', '0.0', '0.0'] == sensor_callback.recieved_list)

