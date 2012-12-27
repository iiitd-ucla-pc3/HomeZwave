	//-----------------------------------------------------------------------------
//
//	Main.cpp
//
//	Minimal application to test OpenZWave.
//
//	Creates an OpenZWave::Driver and the waits.  In Debug builds
//	you should see verbose logging to the console, which will
//	indicate that communications with the Z-Wave network are working.
//
//	Copyright (c) 2010 Mal Lansell <mal@openzwave.com>
//
//
//	SOFTWARE NOTICE AND LICENSE
//
//	This file is part of OpenZWave.
//
//	OpenZWave is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Lesser General Public License as published
//	by the Free Software Foundation, either version 3 of the License,
//	or (at your option) any later version.
//
//	OpenZWave is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with OpenZWave.  If not, see <http://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------
#include <iostream>
#include <unistd.h>
#include <ctime>
#include <stdlib.h>
#include <pthread.h>
#include <fstream>
#include "Options.h"
#include "Manager.h"
#include "Driver.h"
#include "Node.h"
#include "Group.h"
#include "Notification.h"
#include "ValueStore.h"
#include "Value.h"
#include "ValueBool.h"
#include "Log.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

// Includes for Zeromq transport layer to make zwave notifications decoupled from updating SensorSafe over http request
//#include <zmq.hpp>

// Defines for different sensors, commented out command classes are classes 
// that are sent by the sensors, but are not needed/implemented OR are classes 
// that are defined by previous sensors
// (i.e. COMMAND_CLASS_BASIC is used in all sensors)

// For Aeon Labs Door/Window Sensor, the following command classes send events
// COMMAND_CLASS_CONFIGURATION
// COMMAND_CLASS_MANUFACTURER_SPECIFIC
// COMMAND_CLASS_ASSOCIATION
// COMMAND_CLASS_SENSOR_ALARM
#define COMMAND_CLASS_BASIC 0x20
#define COMMAND_CLASS_SENSOR_BINARY 0x30
#define COMMAND_CLASS_WAKE_UP 0x84
#define COMMAND_CLASS_BATTERY 0x80
#define COMMAND_CLASS_ALARM 0x71
#define COMMAND_CLASS_VERSION 0x86

// For HSM-100, we have 
// COMMAND_CLASS_BASIC
// COMMAND_CLASS_BATTERY
// COMMAND_CLASS_WAKE_UP,
// COMMAND_CLASS_VERSION
// COMMAND_CLASS_MANUFACTURER_SPECIFIC 
// COMMAND_CLASS_NODE_NAMING
// COMMAND_CLASS_BATTERY
// COMMAND_CLASS_ASSOCIATION
// and the following classes:
#define COMMAND_CLASS_CONFIGURATION 0x70
#define COMMAND_CLASS_SENSOR_MULTILEVEL 0x31
#define COMMAND_CLASS_MULTI_INSTANCE 0x60

// For SmartSwitch, we have 
// COMMAND_CLASS_BASIC
// COMMAND_CLASS_SENSOR_MULTILEVEL,
// COMMAND_CLASS_CONFIGURATION
// COMMAND_CLASS_VERSION
// COMMAND_CLASS_ASSOCIATION
// COMMAND_CLASS_MANUFACTURER_SPECIFIC 
// and the following:
#define COMMAND_CLASS_SWITCH_BINARY 0x25
#define COMMAND_CLASS_SWITCH_ALL 0x27
#define COMMAND_CLASS_METER 0x32
#define COMMAND_CLASS_HAIL 0x82

sql::Driver *driver;
sql::Connection *con;
sql::Statement *stmt;
sql::ResultSet *res;
sql::PreparedStatement *pstmt;
int aa,bb,cc;
std::time_t t;
std::tm* local;
  


using namespace OpenZWave;
using namespace std;

static uint32 g_homeId;
static int mil = 0;
static int previous = 25;
static int current = 0;
bool   g_initFailed = false;

typedef struct
{
	uint32			m_homeId;
	uint8			m_nodeId;
	bool			m_polled;
	list<ValueID>	m_values;
}NodeInfo;

static list<NodeInfo*> g_nodes;
static pthread_mutex_t g_criticalSection;
static pthread_cond_t  initCond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t initMutex = PTHREAD_MUTEX_INITIALIZER;

void sendMessage(const char *sensor, const char *measurement, float f_val, uint8 nodeId) {
    char buffer[40];

    // Note the following mapping is implementation specific!
    // Choose nodeId (1 to number of nodes) based on given nodeIds
    printf("%s",sensor);
        // One SmartSensor
    nodeId = 1;
    sprintf((char *) buffer, "%s_%s_%d %f", sensor, measurement, nodeId, f_val);
}

//-----------------------------------------------------------------------------
// <GetNodeInfo>
// Return the NodeInfo object associated with this notification
//-----------------------------------------------------------------------------
NodeInfo* GetNodeInfo
(
	Notification const* _notification
)
{
	uint32 const homeId = _notification->GetHomeId();
	uint8 const nodeId = _notification->GetNodeId();
	for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
	{
		NodeInfo* nodeInfo = *it;
		if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
		{
			return nodeInfo;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// <configureSmartSwitchParameters>
// Configures several parameters for the SmartSwitch.
//-----------------------------------------------------------------------------
void configureSmartSwitchParameters(uint8 nodeId) 
{
    pthread_mutex_lock( &g_criticalSection );
	printf("Configuring smart switch parameters\n");
    // Send a multisensor report for Group 1.
    Manager::Get()->SetConfigParam(g_homeId, nodeId, 3, 1);
    sleep(1);
    printf("Configured for %d\n",nodeId);
    Manager::Get()->RequestConfigParam(g_homeId, nodeId, 3); 
    pthread_mutex_unlock( &g_criticalSection );
}

//-----------------------------------------------------------------------------
// <printConfigVariable>
// Prints the Configuration Variable
//-----------------------------------------------------------------------------
void printConfigVariable(uint8 index, uint8 byte_value) {
    static const char *parameter_names[] = {"Sensitivity", "On Time", "LED ON/OFF", 
        "Light Threshold", "Stay Awake", "On Value"};
    printf("\"%s\" was set to %u\n", parameter_names[index-1], byte_value);
}

//-----------------------------------------------------------------------------
// <printSmartSwitchMeterValue>
// Prints the Smart Switch Meter Value
//-----------------------------------------------------------------------------
void printSmartSwitchMeterValue(ValueID value_id, uint8 nodeId) {
	
	t = std::time(0);  // t is an integer type
    string str_value = "";
    Manager::Get()->GetValueAsString(value_id, &str_value);
    // Measurement map for SmartSwitch
    static map<uint8, string> SmartSwitchMeasurementMap;
    SmartSwitchMeasurementMap[0] = "Energy";
    SmartSwitchMeasurementMap[1] = "Previous_Energy_Reading";
    SmartSwitchMeasurementMap[2] = "Energy_Interval";
    SmartSwitchMeasurementMap[8] = "Power";
    SmartSwitchMeasurementMap[9] = "Previous_Power_Reading";
    SmartSwitchMeasurementMap[10] = "Power_Interval";
    SmartSwitchMeasurementMap[32] = "Exporting";
    SmartSwitchMeasurementMap[33] = "Reset";
	string measurement = SmartSwitchMeasurementMap[value_id.GetIndex()];
	int valD = atoi(str_value.c_str());
	if ((value_id.GetIndex())==8) 
	{
		printf("Power: %d for node id: %d \n", valD,nodeId);
		pstmt = con->prepareStatement("INSERT INTO smartswitch(node,time,power) VALUES (?,?,?)");
		pstmt->setInt(1, nodeId);
		pstmt->setInt(2,t);
		pstmt->setInt(3,valD);
		try
		{
			pstmt->executeUpdate();
		}
		catch (sql::SQLException &e) 
		{
			//printf("Duplicate\n");
		}	
	}
	else
		printf("\"%s\": %s for node id: %d \n", measurement.c_str(), str_value.c_str(),nodeId);
}


//-----------------------------------------------------------------------------
// <parseSmartSwitchSensor>
// Parses the Aeon Labs Energy Switch Sensor for basic values.
//-----------------------------------------------------------------------------
void parseSmartSwitchSensor(uint8 nodeId, ValueID value_id) {

    // Initialize Variables
    bool success = false;
    bool bool_value = false;
    uint8 byte_value = 0;
    float float_value = 0;
    int int_value = 0;
    string str_value = "";
    printf("Value Type: %d\n", (int) value_id.GetType());
    // Get the Changed Value Based on the type
    switch((int) value_id.GetType()) {
        // See open-zwave/cpp/src/value_classes/ValueID.h for ValueType enum 
        case 0:
            // Boolean Type
            success = Manager::Get()->GetValueAsBool(value_id, &bool_value);
            break;
        case 1:
            // Byte Type
            success = Manager::Get()->GetValueAsByte(value_id, &byte_value);
            break;
        case 2:
            // Float Type
            success = Manager::Get()->GetValueAsFloat(value_id, &float_value);
            break;
        case 3:
            // Int Type
            success = Manager::Get()->GetValueAsInt(value_id, &int_value);
            break;
        case 4:
            //  List Type -> Get as a string for easier parsing
            success = Manager::Get()->GetValueAsString(value_id, &str_value);
            break;
        case 7:
            // String Type
            success = Manager::Get()->GetValueAsString(value_id, &str_value);
            break;
        default:
            printf("Unrecognized Type: %d for node id: %d\n", (int) value_id.GetType(),nodeId);
            break;
    }
    if(!success) {
        printf("Unable to Get the Value\n");
        return;
    }

    // Perform action based on CommandClassID
    // For Aeon Labs SmartSwitch:
    // 1. COMMAND_CLASS_BASIC (0x20)
    // 2. COMMAND_CLASS_SENSOR_MULTIVEL (0x31)
    // 3. COMMAND_CLASS_SWITCH_BINARY (0x25)
    // 4. COMMAND_CLASS_SWITCH_ALL 0x27
    // 5. COMMAND_CLASS_METER 0x32
    // 6. COMMAND_CLASS_HAIL 0x82
    
    switch(value_id.GetCommandClassId()) {
        case COMMAND_CLASS_BASIC:
            break;
        case COMMAND_CLASS_SENSOR_MULTILEVEL:
            printf("Sent Power(%d): %f\n\n",nodeId, float_value);
            break;
        case COMMAND_CLASS_SWITCH_BINARY:
            printf("Binary Switch(%d): %s\n\n", nodeId, (bool_value)?"on":"off");
            if(!bool_value)
				Manager::Get()->SetNodeOn(g_homeId, nodeId);
            break;
        case COMMAND_CLASS_SWITCH_ALL:
            printf("Switch_all: %s\n\n", str_value.c_str());
            break;
        case COMMAND_CLASS_METER:
            printSmartSwitchMeterValue(value_id,nodeId);
			break;
        case COMMAND_CLASS_CONFIGURATION:
            printf("Configuration: %d\n\n", int_value);
            break;
        default:
            printf("Got an Unknown COMMAND CLASS!\n");
            break;
    }
}

//-----------------------------------------------------------------------------
// <OnNotification>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
void OnNotification
(
	Notification const* _notification,
	void* _context
)
{
	// Must do this inside a critical section to avoid conflicts with the main thread
	pthread_mutex_lock( &g_criticalSection );

    // printf("_notification->GetType(): %d \n", _notification->GetType());
	
	switch( _notification->GetType() )
	{
		case Notification::Type_ValueAdded:								// 0
		{
			if(mil)
			{
				ValueID value_id = _notification->GetValueID();
				if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
				{
					// Add the new value to our list
					nodeInfo->m_values.push_back( _notification->GetValueID() );
					printf("Value has been added for Node Id:%d\n",_notification->GetNodeId());
					printf("Value Type:%d\n",value_id.GetType());
					break;
				}
			}
			break;
		}

		case Notification::Type_ValueRemoved:							// 1
		{
			if(mil)
			{
				if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
				{
					// Remove the value from out list
					for( list<ValueID>::iterator it = nodeInfo->m_values.begin(); it != nodeInfo->m_values.end(); ++it )
					{
						if( (*it) == _notification->GetValueID() )
						{
							nodeInfo->m_values.erase( it );
							printf("Value has been Removed");
							break;
						}
					}
				}
			}
			break;
		}

		case Notification::Type_ValueChanged:							// 2
		{
			if (mil) {
				// One of the node values has changed
				uint8 nodeId = _notification->GetNodeId();
				// ValueID of value involved
				ValueID value_id = _notification->GetValueID();
				string str;
				parseSmartSwitchSensor(nodeId, value_id);
			}
            break;
        }

		case Notification::Type_Group:									// 4
		{
			// One of the node's association groups has changed
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo = nodeInfo;		// placeholder for real action
			}
			break;
		}

		case Notification::Type_NodeNew:								// 5
		{
			// New node Detected
			printf("New Node has been Detected with Node ID: %d\n",_notification->GetNodeId());
			break;
        }
        
		case Notification::Type_NodeAdded:								// 6
		{
			// Add the new node to our list
			printf("Node Id:%d has been Added to the Network\n",_notification->GetNodeId());
			configureSmartSwitchParameters(_notification->GetNodeId());
			break;
        }

        case Notification::Type_NodeRemoved:							// 7
        {
            // Remove the node from our list
            printf("Node Id:%d has been Removed\n",_notification->GetNodeId());
			uint32 const homeId = _notification->GetHomeId();
            uint8 const nodeId = _notification->GetNodeId();
            for( list<NodeInfo*>::iterator it = g_nodes.begin(); it != g_nodes.end(); ++it )
            {
                NodeInfo* nodeInfo = *it;
                if( ( nodeInfo->m_homeId == homeId ) && ( nodeInfo->m_nodeId == nodeId ) )
                {
                    g_nodes.erase( it );
                    delete nodeInfo;
                    break;
                }
            }
            break;
        }

        case Notification::Type_NodeEvent:
        {
            // We have received an event from the node, caused by a
            // basic_set or hail message.
            printf("Type_NodeEvent:\n");
            printf("Node Id:%d\n",_notification->GetNodeId());
            break;
		}

		case Notification::Type_PollingDisabled:
		{
			printf("Type_PollingDisabled\n");
			printf("Node Id:%d\n",_notification->GetNodeId());
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = false;
			}
			break;
		}

		case Notification::Type_PollingEnabled:
		{
			printf("Type_PollingEnabled:\n");
			printf("Node Id:%d\n",_notification->GetNodeId());
			if( NodeInfo* nodeInfo = GetNodeInfo( _notification ) )
			{
				nodeInfo->m_polled = true;
			}
			break;
		}

		case Notification::Type_DriverReady:							// 18
		{
			printf("A driver for a PC Z-Wave controller has been added and is ready to use.\n");
			g_homeId = _notification->GetHomeId();
			break;
		}

		case Notification::Type_DriverFailed:
		{
			printf("Type_DriverFailed:\n");
			printf("Node Id:%d\n",_notification->GetNodeId());
			g_initFailed = true;
			pthread_cond_broadcast(&initCond);
			break;
		}

		case Notification::Type_AwakeNodesQueried:
		{
			printf("Type_AwakeNodesQueried:\n");
			printf("Node Id:%d\n",_notification->GetNodeId());
			break;
		}
		case Notification::Type_AllNodesQueried:						// 25
		{
			printf("ALL Nodes Queried\n");
			mil=1;
			pthread_cond_broadcast(&initCond);
			break;
		}

		case Notification::Type_DriverReset:
		{
			printf("Type_DriverReset\n");
			printf("Node Id:%d\n",_notification->GetNodeId());
			break;
		}
		case Notification::Type_EssentialNodeQueriesComplete:
		{
			printf("Completing Essential Queries for Node Id:%d\n",_notification->GetNodeId());
			break;
		}
		case Notification::Type_MsgComplete:
		{
			printf("Type_MsgComplete:\n");
			printf("Node Id:%d\n",_notification->GetNodeId());
			break;
		}
		case Notification::Type_NodeNaming:								// 9
		{
			break;
		}
		case Notification::Type_NodeProtocolInfo:						// 8
        {
			printf("Collecting Protocol Info for Node Id:%d\n",_notification->GetNodeId());
			break;
		}
		case Notification::Type_NodeQueriesComplete:					// 23
		{
			//configureSensorParameters(_notification->GetNodeId());
			string name = Manager::Get()->GetNodeProductName(_notification->GetHomeId(), _notification->GetNodeId());
			string manufacturer_name = Manager::Get()->GetNodeManufacturerName(_notification->GetHomeId(), _notification->GetNodeId());
			string type = Manager::Get()->GetNodeType(_notification->GetHomeId(), _notification->GetNodeId());
			printf("Finished protocol info for Node (%d)\n", _notification->GetNodeId());
			printf("With type: %s\n", type.c_str());
			printf("    With Name: %s\n", name.c_str());
			printf("    and Manufacturer: %s\n", manufacturer_name.c_str());
			printf("    and Manufacturer: %s\n", manufacturer_name.c_str());
			break;
		}
		default:														
		{																
			printf("Default\n");
			printf("Node Id:%d\n",_notification->GetNodeId());
			break;
		}
	}

	pthread_mutex_unlock( &g_criticalSection );
}

//-----------------------------------------------------------------------------
// <main>
// Create the driver and then wait
//-----------------------------------------------------------------------------
int main( int argc, char* argv[] )
{
	
	
	/* Create a connection */
	driver = get_driver_instance();
	con = driver->connect("tcp://127.0.0.1:3306", "root", "password");
	/* Connect to the MySQL test database */
	con->setSchema("home");
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init ( &mutexattr );
	pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE );
	pthread_mutex_init( &g_criticalSection, &mutexattr );
	pthread_mutexattr_destroy( &mutexattr );

	pthread_mutex_lock( &initMutex );

	// Create the OpenZWave Manager.
	// The first argument is the path to the config files (where the manufacturer_specific.xml file is located
	// The second argument is the path for saved Z-Wave network state and the log file.  If you leave it NULL 
	// the log file will appear in the program's working directory.
	Options::Create( "../../../../../config/", "", "" );
	Options::Get()->AddOptionInt( "SaveLogLevel", LogLevel_Detail );
	Options::Get()->AddOptionInt( "QueueLogLevel", LogLevel_Debug );
	Options::Get()->AddOptionInt( "DumpTrigger", LogLevel_Error );
	Options::Get()->AddOptionInt( "PollInterval", 500 );
	Options::Get()->AddOptionBool( "IntervalBetweenPolls", true );
	Options::Get()->AddOptionBool("ValidateValueChanges", true);

    // Turn off Console Logging
    Options::Get()->AddOptionBool("ConsoleOutput", false);
    Options::Get()->AddOptionBool("Logging", false);
	Options::Get()->Lock();

	Manager::Create();
	

	// Add a callback handler to the manager.  The second argument is a context that
	// is passed to the OnNotification method.  If the OnNotification is a method of
	// a class, the context would usually be a pointer to that class object, to
	// avoid the need for the notification handler to be a static.
	Manager::Get()->AddWatcher( OnNotification, NULL );
	// Add a Z-Wave Driver
	// Modify this line to set the correct serial port for your PC interface.

	string port = "/dev/ttyUSB0";
	if ( argc > 1 )
	{
		port = argv[1];
	}
	if( strcasecmp( port.c_str(), "usb" ) == 0 )
	{
		Manager::Get()->AddDriver( "HID Controller", Driver::ControllerInterface_Hid );
	}
	else
	{
		Manager::Get()->AddDriver( port );
	}

	// Now we just wait for either the AwakeNodesQueried or AllNodesQueried notification,
	// then write out the config file.
	// In a normal app, we would be handling notifications and building a UI for the user.
	
	pthread_cond_wait( &initCond, &initMutex );
	// Since the configuration file contains command class information that is only 
	// known after the nodes on the network are queried, wait until all of the nodes 
	// on the network have been queried (at least the "listening" ones) before
	// writing the configuration file.  (Maybe write again after sleeping nodes have
	// been queried as well.)
	if( !g_initFailed )
	{
		Manager::Get()->WriteConfig( g_homeId );
		while(mil)
		{
			pthread_mutex_lock( &g_criticalSection );
			
			pthread_mutex_unlock( &g_criticalSection );	
		}
		Driver::DriverData data;
		Manager::Get()->GetDriverStatistics( g_homeId, &data );
		printf("SOF: %d ACK Waiting: %d Read Aborts: %d Bad Checksums: %d\n", data.s_SOFCnt, data.s_ACKWaiting, data.s_readAborts, data.s_badChecksum);
		printf("Reads: %d Writes: %d CAN: %d NAK: %d ACK: %d Out of Frame: %d\n", data.s_readCnt, data.s_writeCnt, data.s_CANCnt, data.s_NAKCnt, data.s_ACKCnt, data.s_OOFCnt);
		printf("Dropped: %d Retries: %d\n", data.s_dropped, data.s_retries);
	}
	if( strcasecmp( port.c_str(), "usb" ) == 0 )
	{
		Manager::Get()->RemoveDriver( "HID Controller" );
	}
	else
	{
		Manager::Get()->RemoveDriver( port );
	}
	
	Manager::Get()->RemoveWatcher( OnNotification, NULL );
	Manager::Destroy();
	Options::Destroy();
	pthread_mutex_destroy( &g_criticalSection );
	return 0;
}
