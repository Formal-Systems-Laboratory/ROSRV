ROS Monitoring Implementation
=============================

ROSRV provides the interface for monitoring ROS systems
with RV-Monitor properties.
It provides an event-definition language for defining
events that will be triggered when ROS messages are sent
on particular ROS topics, and compiles generated monitoring
logic together with ROS interface code to create programs
that implement the monitoring.

The generated programs
are functional ROS nodes that subscribe to and
publish to ROS topics (publishing to allow monitors that
may modify or filter messages).
These nodes can be explicitly added to a system architecture
by modifying the ROS `.launch` files to include the monitor
process, splitting any topic that the monitor needs to
be able to modify into a pre-monitoring and post-monitoring
topic name, and using ROS remapping directives to make
other nodes publish to the pre-monitor topic or subscribe
to the post-monitor topic.

To allow for transparent monitor insertion, at the cost
of a more invasive mechanism, rvmaster is a proxy for
the roscore directory server that connects monitors
into a system by manipulating the lists of publishers
and subscribers provided to subscribers and publishers
when they connect to a topic.
The only installation needed for this is running
rvmaster at the address expected for the ROS master
before launching a system (as an already-running
ROS master is not considered an error), with
a configuration file specifying the topics to
be monitored.

The rvmaster process identifies monitors by node name,
and handles connections to monitored topic names specially.
If `<topicname>` is monitored, ordinary nodes publishing
to `<topicname>` will be connected to any monitor nodes
subscribing to `/rv/monitored/<topicname>`,
and any ordinary nodes subscribing to `<topicname>`
will be connected to any monitor nodes publishing to
`/rv/monitored/<topicname>`.
Establishing a node-to-node connection in ROS
includes exchanging the expected topic name and type,
so to allow these connections to succeed a ROSRV-generated
monitor node uses the ROS library at a lower level than
usual to allow and provide the name `<topicname>`
when connecting to other nodes.

## Improvements over Initial Approach

The original implementation of ROSRV provided instrumentation
only through a version of rvmaster, and compiled the monitoring
code directly into a rvmaster program.
This meant monitoring always required using a non-standard
ROS Master, and one that was recompiled whenever monitoring changed.
This also ended up routing all messages on a monitored topic through
the rvmaster process, which is not a problem on a single-computer
ROS system, but can be on more sophisticated platforms with
multiple control computers.
Moving monitors to separate processes avoids recompiling rvmaster,
allows monitors to be used without rvmaster at all, and allows
monitors to be co-located with the major publishers and subscribers
of a topic.

Differential Dynamic Logic Support Overview
===============================================

ROSRV and RV-Monitor support runtime monitoring of
formulas described in Differential Dynamic Logic (dL) [@DDl08].
We use the following RV-Monitor spec as a running example
to describe the tool's architecture and usage.

```{.cpp}
testDlWatertankSpec() {

    /* Setup/Teardown + Fallback handling code */

    event current_level(float l)
        /level_sensor std_msgs/Float32
                            '{data:l}'
    {
	update_l(static_cast<long double>(l));
    }

    event flow_controller_input(float f, ros::Time cTime)
        /flow_control_cmd marti_common_msgs/Float32Stamped
                         '{value:f, header:{stamp:cTime}}'
    {
	update_f(static_cast<long double>(f));
	update_c(static_cast<long double>(cTime.toSec()));

	if(!check_violation( pre_check_actions
			   , post_check_actions) ) {
	    fallback(message);
	}
    }
    /* DL formula to monitor */

    dL :
	Functions.
	  R m.
	  R ep.
	End.

	ProgramVariables.
	  R f.
	  R l.
	  R c.
	End.

	Problem.
	     (0 <= l & l <= m & 0 < ep)
	  -> [
	      { f :=*;
		?-1 <= f & f <= (m-l)/ep;
		c := 0;
		{ l' = f, c' = 1 & 0 <= l & c <= ep }
	      }*
	     ](0 <= l & l <= m)
	End.

}
```

The dL-formula of interest is expressed using the `dL:` tag.
In our example, the formula is a simple water-tank
model described in detail in [@ModelPlex2016]. The system
is set up to publish the current water level via messages of type `float`
on topic `\level_sensor`
and input to the flow controller via timestamped messages
of type `Float32Stamped` on `\flow_control_cmd`.


Interaction with ModelPlex
--------------------------


We depend on ModelPlex for generation of dL-specific monitoring conditions.
We use KeYmaeraX [@KeymaeraXUrl] (version 4.7.2) to generate
C code to detect violations. However, the generated C code
cannot be directly employed to detect violations in a ROS based
system. The following sections describe how ROSRV uses said
synthesized code in a ROS based setting.


### Binding ROS Message Data to Logical Variables

C code generated by KeYmaeraX has the form

```{.cpp}
    typedef struct parameters {
      long double ep;
      ...
    } parameters;

    typedef struct state {
      long double c;
      long double f;
      ...
    } state;

    bool monitorSatisfied(state pre, state curr,
                        const parameters* const params) {
        ...
    }

```

where structs `parameters` and `state`
contain fields  for `functions`
and `program variable` in the dL formula.


A ROSRV `event` has the form

```{.cpp}
    event <EVENT-NAME>( [EVENT-PARAMETERS] )
            <ROS-CHANNEL> <EVENT-DATATYPE> <BINDINGS-PATTERN>
    {
        <EVENT-BODY>
    }
```

where `BINDINGS-PATTERN` extracts information from
a message of type `EVENT-DATAYPE` recieved on channel `<ROS-CHANNEL>`. The relevant information
is accessible in the event body through `EVENT-PARAMETERS`. In the water tank example,
the `state` struct contains fields corresponding
to hybrid program variables in the formula. For instance,
field `long double f` in the state struct corresponds to the hybrid program variable
`f` of the formula. For each such logical variable, ROSRV synthesizes an
`update_<VAR_NAME>` function which is used in the spec to `bind` the value of the variable
to relevant data extracted from the message. Consider the following event from
our running example -

```{.cpp}

    event flow_controller_input(float f, ros::Time cTime)
            /flow_control_cmd
            marti_common_msgs/Float32Stamped
            '{value:f, header:{stamp:cTime}}'
    {
	update_f(static_cast<long double>(f));
	update_c(static_cast<long double>(cTime.toSec()));

	if(!check_violation( pre_check_actions, post_check_actions) ) {
	    fallback(message);
	}
    }

```

The functions `update_f` and `update_c`, bind observed data to
logical varables by populating corresponding fields in the ModelPlex generated
`state` struct.
Thus, the synthesized monitoring code from the event specification `flow_controller_input` works as following -

 - Listens on channel `flow_control_cmd` for messages of type

   `marti_common_msgs/Float32Stamped`.

 - On receiving messages of aforementioned type, extracts
   the `data` and `stamp` part of the message, which can referred
   to as `f` and `cTime` in the body of the event.
 - The functions `update_f` and `update_c` correspond to RV-Monitor synthesized
   code, which essentially takes care of `binding` the event parameters to
   relevant fields in the ModelPlex generated `state`.
 - `check_violation` is responsible for making a call to the
   ModelPlex-generated `monitorSatisfied` function. Before
   the call is made, additional synchronization related steps need to be
   performed, which are describe later.
 - `check_violation` also accepts as input function pointers to
   carry out any pre/post violation check tasks.
 - In this particular instance, when  violation is detected,
   a `fallback` function is called. Since,
   `ROSRV` allows C++ code in the spec, hence
   any arbitrary fallback mechanism can be implemented,
   including making calls to a ModelPlex synthesized code
   to obtain safe fallback control actions.

### Synchronization

In order to detect violations using `ModelPlex` the
state struct must be properly `populated` before
a call to `monitorSatisfied` is made. But
ROSRV's infrastructure allows monitor nodes
to be injected into a system. In such a situation, the expected
behavior is to still detect violations on events observed
after the monitor is injected.
Thus it is reasonable for a `check_violation` call
to occur on an uninitialized / partly initialized state.
In order to handle this situation, the `check_violation`
call only makes a call to ModelPlex synthesized code
when the monitor has successive properly populated state.
In order to achieve this, we parse the Differential Dynamic Logic
formula and extracts logical variables declarations. ROSRV synthesizes
code that uses maps to track initialization and updates to logical variables across
observed events. Thus `check_violation` only calls ModelPlex generated code when
all fields in successive instances of the state struct have been properly populated.

Codebase Outline
----------------

### Repositories

    * [ROSRV](https://github.com/Formal-Systems-Laboratory/ROSRV/tree/aa-exact)
    * [RV-Monitor](https://github.com/runtimeverification/rv-monitor/tree/aa-exact)

### Building

    * Build instructions for [ROSRV](https://github.com/Formal-Systems-Laboratory/ROSRV/tree/aa-exact#installation)
    * Build instructions for [RV-Monitor](https://github.com/runtimeverification/rv-monitor/tree/aa-exact#build-and-installation)
