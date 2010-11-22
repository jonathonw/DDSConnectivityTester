/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to 2009 PrismTech 
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $OSPL_HOME/LICENSE 
 *
 *   for full copyright notice and license terms. 
 *
 */

/************************************************************************
 * LOGICAL_NAME:    MessageBoard.cpp
 * FUNCTION:        OpenSplice Tutorial example code.
 * MODULE:          Tutorial for the C++ programming language.
 * DATE             june 2007.
 ************************************************************************
 * 
 * This file contains the implementation for the 'MessageBoard' executable.
 * 
 ***/
 
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <iomanip>

#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/PublisherImpl.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>
#include <dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h>

#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/simpleTCP/SimpleTcp.h>
#endif

#include <ace/streams.h>
#include "ace/Get_Opt.h"

#include "CheckStatus.h"
#include "ChatC.h"
#include "ChatS.h"
#include "ChatTypeSupportS.h"
#include "ChatTypeSupportC.h"
#include "ChatTypeSupportImpl.h"

using namespace DDS;
using namespace Chat;
using namespace CORBA;


#define TERMINATION_MESSAGE -1 

void printTopicQos(DDS::TopicQos topicQos);
void printReaderQos(DDS::DataReaderQos readerQos);
void printWriterQos(DDS::DataWriterQos readerQos);
void printCurrentTime(DDS::DomainParticipant &participant);


int
main (
    int argc,
    char *argv[])
{
    /* Generic DDS entities */
    DomainParticipantFactory_var    dpf;
    DomainParticipant_var           parentDP;
    //ExtDomainParticipant_var        participant;
    Topic_var                       chatMessageTopic;
    Topic_var                       nameServiceTopic;
    TopicDescription_var            namedMessageTopic;
    Subscriber_var                  chatSubscriber;
    DataReader_ptr                  parentReader;

    /* Type-specific DDS entities */
    ChatMessageTypeSupport_var      chatMessageTS;
    NameServiceTypeSupport_var      nameServiceTS;
    ChatMessageDataReader_var      chatAdmin;
    ChatMessageSeq             msgSeq ;
    SampleInfoSeq               infoSeq ;

    /* QosPolicy holders */
    TopicQos                        reliable_topic_qos;
    TopicQos                        setting_topic_qos;
    SubscriberQos                   sub_qos;
    DDS::StringSeq                  parameterList;

    /* DDS Identifiers */
    DomainId_t                      domain = 0;
    ReturnCode_t                    status;

    /* Others */
    bool                            terminated = false;
    const char *                    partitionName = "ChatRoom";
    char  *                         chatMessageTypeName = NULL;
    char  *                         nameServiceTypeName = NULL;
    char  *                         namedMessageTypeName = NULL;

#ifdef USE_NANOSLEEP
    struct timespec                 sleeptime;
    struct timespec                 remtime;
#endif

    /* Options: MessageBoard [ownID] */
    /* Messages having owner ownID will be ignored */
    parameterList.length(1);
    
    if (argc > 1) {
        parameterList[0] = string_dup(argv[1]);
    }
    else
    {
        parameterList[0] = "0";
    }
      
    /* Create a DomainParticipantFactory and a DomainParticipant (using Default QoS settings. */
    dpf = TheParticipantFactoryWithArgs(argc, argv);
    checkHandle(dpf.in(), "DDS::DomainParticipantFactory::get_instance");
    parentDP = dpf->create_participant (
        domain, 
        PARTICIPANT_QOS_DEFAULT, 
        NULL,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(parentDP, "DDS::DomainParticipantFactory::create_participant");
    
    OpenDDS::DCPS::TransportImpl_rch transport_impl =
      TheTransportFactory->create_transport_impl( OpenDDS::DCPS::DEFAULT_SIMPLE_TCP_ID, OpenDDS::DCPS::AUTO_CONFIG);
    
    /* Narrow the normal participant to its extended representative */
    //participant = ExtDomainParticipantImpl::_narrow(parentDP);
    //checkHandle(participant.in(), "DDS::ExtDomainParticipant::_narrow");

    /* Register the required datatype for ChatMessage. */
    chatMessageTS = new ChatMessageTypeSupportImpl();
    checkHandle(chatMessageTS.in(), "new ChatMessageTypeSupport");
    chatMessageTypeName = chatMessageTS->get_type_name();
    status = chatMessageTS->register_type(
        parentDP.in(), 
        chatMessageTypeName);
    checkStatus(status, "Chat::ChatMessageTypeSupport::register_type");
    
    /* Register the required datatype for NameService. */
    nameServiceTS = new NameServiceTypeSupportImpl();
    checkHandle(nameServiceTS.in(), "new NameServiceTypeSupport");
    nameServiceTypeName =  nameServiceTS->get_type_name();
    status = nameServiceTS->register_type(
        parentDP.in(), 
        nameServiceTypeName);
    checkStatus(status, "Chat::NameServiceTypeSupport::register_type");
    
    
    /* Set the ReliabilityQosPolicy to RELIABLE. */
    status = parentDP->get_default_topic_qos(reliable_topic_qos);
    checkStatus(status, "DDS::DomainParticipant::get_default_topic_qos");
    reliable_topic_qos.reliability.kind = DDS::BEST_EFFORT_RELIABILITY_QOS;
    
    /* Make the tailored QoS the new default. */
    status = parentDP->set_default_topic_qos(reliable_topic_qos);
    checkStatus(status, "DDS::DomainParticipant::set_default_topic_qos");
    
    cout << "Topic QOS: " << endl;
    printTopicQos(reliable_topic_qos);

    /* Use the changed policy when defining the ChatMessage topic */
    chatMessageTopic = parentDP->create_topic( 
        "Chat_ChatMessage", 
        chatMessageTypeName, 
        reliable_topic_qos, 
        NULL,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(chatMessageTopic.in(), "DDS::DomainParticipant::create_topic (ChatMessage)");

    /* Adapt the default SubscriberQos to read from the "ChatRoom" Partition. */
    status = parentDP->get_default_subscriber_qos (sub_qos);
    checkStatus(status, "DDS::DomainParticipant::get_default_subscriber_qos");
    sub_qos.partition.name.length(1);
    sub_qos.partition.name[0] = partitionName;

    /* Create a Subscriber for the MessageBoard application. */
    chatSubscriber = parentDP->create_subscriber(sub_qos, NULL, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(chatSubscriber.in(), "DDS::DomainParticipant::create_subscriber");
    
    // Attach the subscriber to the transport. 
    status = transport_impl->attach(chatSubscriber.in());
    if (status != OpenDDS::DCPS::ATTACH_OK) {
      std::cerr << "Failed to attach to the transport." << std::endl; 
      return 1;
    }
    
    /* Create a DataReader for the NamedMessage Topic (using the appropriate QoS). */
    parentReader = chatSubscriber->create_datareader( 
        chatMessageTopic.in(), 
        DATAREADER_QOS_USE_TOPIC_QOS, 
        NULL,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    DDS::DataReaderQos dr_qos;
    status = parentReader->get_qos(dr_qos);
    checkStatus(status, "DDS::DataReader::get_default_datareader_qos");
    checkHandle(parentReader, "DDS::Subscriber::create_datareader");
    
    cout << "Data Reader QOS: " << endl;
    printReaderQos(dr_qos);
    
    /* Narrow the abstract parent into its typed representative. */
    chatAdmin = Chat::ChatMessageDataReader::_narrow(parentReader);
    checkHandle(chatAdmin.in(), "Chat::NamedMessageDataReader::_narrow");
    
    /* Print a message that the MessageBoard has opened. */
    cout << "MessageBoard has opened: send a ChatMessage with userID = -1 to close it...." << endl << endl;

    while (!terminated) {
        /* Note: using read does not remove the samples from
           unregistered instances from the DataReader. This means
           that the DataRase would use more and more resources.
           That's why we use take here instead. */

        status = chatAdmin->take( 
            msgSeq, 
            infoSeq, 
            LENGTH_UNLIMITED, 
            ANY_SAMPLE_STATE, 
            ANY_VIEW_STATE, 
            ALIVE_INSTANCE_STATE );
        checkStatus(status, "Chat::ChatMessageDataReader::take");

        for (ULong i = 0; i < msgSeq.length(); i++) {
            ChatMessage *msg = &(msgSeq[i]);
            if (msg->userID == TERMINATION_MESSAGE) {
                cout << "Termination message received: exiting..." << endl;
                terminated = true;
            } else {
                printCurrentTime(*parentDP);
                cout << msg->content << endl;
            }
        }

        status = chatAdmin->return_loan(msgSeq, infoSeq);
        checkStatus(status, "Chat::ChatMessageDataReader::return_loan");
        
        /* Sleep for some amount of time, as not to consume too much CPU cycles. */
#ifdef USE_NANOSLEEP
        sleeptime.tv_sec = 0;
        sleeptime.tv_nsec = 100000000;
        nanosleep(&sleeptime, &remtime);
#else
        usleep(100000);
#endif
    }

    /* Remove the DataReader */
    status = chatSubscriber->delete_datareader(chatAdmin.in());
    checkStatus(status, "DDS::Subscriber::delete_datareader");

    /* Remove the Subscriber. */
    status = parentDP->delete_subscriber(chatSubscriber.in());
    checkStatus(status, "DDS::DomainParticipant::delete_subscriber");
    
    /* Remove the Topics. */
    //status = parentDP->delete_simulated_multitopic(namedMessageTopic.in());
    //checkStatus(status, "DDS::ExtDomainParticipant::delete_simulated_multitopic");

    status = parentDP->delete_topic(nameServiceTopic.in());
    checkStatus(status, "DDS::DomainParticipant::delete_topic (nameServiceTopic)");

    status = parentDP->delete_topic(chatMessageTopic.in());
    checkStatus(status, "DDS::DomainParticipant::delete_topic (chatMessageTopic)");

    /* De-allocate the type-names. */
    string_free(namedMessageTypeName);
    string_free(nameServiceTypeName);
    string_free(chatMessageTypeName);

    /* Remove the DomainParticipant. */
    status = dpf->delete_participant(parentDP.in());
    checkStatus(status, "DDS::DomainParticipantFactory::delete_participant");
    
    exit(0);    
}

void printTopicQos(DDS::TopicQos topicQos) {
  cout << endl;
  
  cout << "--Qos Policy for " << &topicQos << endl;
  //reliability
  cout << "Reliability: " << endl;
  cout << "  kind:";
  if(topicQos.reliability.kind == BEST_EFFORT_RELIABILITY_QOS) {
    cout << "BEST_EFFORT" << endl;
  } else if(topicQos.reliability.kind == RELIABLE_RELIABILITY_QOS) {
    cout << "RELIABLE" << endl;
  } else {
    cout << "??" << endl;
  }
  double seconds = topicQos.reliability.max_blocking_time.sec + topicQos.reliability.max_blocking_time.nanosec/1.0E9;
  cout << "  max_blocking_time (sec): " << seconds  << endl;
  cout << "  synchronous: unsupported in OpenDDS" << endl;
  
  //liveliness
  cout << "Liveliness: " << endl;
  cout << "  kind: ";
  if(topicQos.liveliness.kind == AUTOMATIC_LIVELINESS_QOS) {
    cout << "AUTOMATIC" << endl;
  } else if(topicQos.liveliness.kind == MANUAL_BY_PARTICIPANT_LIVELINESS_QOS) {
    cout << "MANUAL_BY_PARTICIPANT" << endl;
  } else if(topicQos.liveliness.kind == MANUAL_BY_TOPIC_LIVELINESS_QOS) {
    cout << "MANUAL_BY_TOPIC" << endl;
  } else {
    cout << "??" << endl;
  }
  
  seconds = topicQos.liveliness.lease_duration.sec + topicQos.liveliness.lease_duration.nanosec/1.0E9;
  cout << "  lease duration: " << seconds << endl;
  
  //latency budget
  seconds = topicQos.latency_budget.duration.sec + topicQos.latency_budget.duration.nanosec/1.0E9;
  cout << "Latency Budget: " << seconds << endl;
  
  //lifespan
  seconds = topicQos.lifespan.duration.sec + topicQos.lifespan.duration.nanosec/1.0E9;
  cout << "Lifespan: " << seconds << endl;
  
  cout << endl;
}

void printWriterQos(DDS::DataWriterQos writerQos) {
  cout << endl;
  
  cout << "--Qos Policy for " << &writerQos << endl;
  //reliability
  cout << "Reliability: " << endl;
  cout << "  kind:";
  if(writerQos.reliability.kind == BEST_EFFORT_RELIABILITY_QOS) {
    cout << "BEST_EFFORT" << endl;
  } else if(writerQos.reliability.kind == RELIABLE_RELIABILITY_QOS) {
    cout << "RELIABLE" << endl;
  } else {
    cout << "??" << endl;
  }
  double seconds = writerQos.reliability.max_blocking_time.sec + writerQos.reliability.max_blocking_time.nanosec/1.0E9;
  cout << "  max_blocking_time (sec): " << seconds  << endl;
  cout << "  synchronous: unsupported in OpenDDS" << endl;
  
  //liveliness
  cout << "Liveliness: " << endl;
  cout << "  kind: ";
  if(writerQos.liveliness.kind == AUTOMATIC_LIVELINESS_QOS) {
    cout << "AUTOMATIC" << endl;
  } else if(writerQos.liveliness.kind == MANUAL_BY_PARTICIPANT_LIVELINESS_QOS) {
    cout << "MANUAL_BY_PARTICIPANT" << endl;
  } else if(writerQos.liveliness.kind == MANUAL_BY_TOPIC_LIVELINESS_QOS) {
    cout << "MANUAL_BY_TOPIC" << endl;
  } else {
    cout << "??" << endl;
  }
  
  seconds = writerQos.liveliness.lease_duration.sec + writerQos.liveliness.lease_duration.nanosec/1.0E9;
  cout << "  lease duration: " << seconds << endl;
  
  //latency budget
  seconds = writerQos.latency_budget.duration.sec + writerQos.latency_budget.duration.nanosec/1.0E9;
  cout << "Latency Budget: " << seconds << endl;
  
  //lifespan
  seconds = writerQos.lifespan.duration.sec + writerQos.lifespan.duration.nanosec/1.0E9;
  cout << "Lifespan: " << seconds << endl;
  
  cout << endl;
}

void printReaderQos(DDS::DataReaderQos readerQos) {
  cout << endl;
  
  cout << "--Qos Policy for " << &readerQos << endl;
  //reliability
  cout << "Reliability: " << endl;
  cout << "  kind:";
  if(readerQos.reliability.kind == BEST_EFFORT_RELIABILITY_QOS) {
    cout << "BEST_EFFORT" << endl;
  } else if(readerQos.reliability.kind == RELIABLE_RELIABILITY_QOS) {
    cout << "RELIABLE" << endl;
  } else {
    cout << "??" << endl;
  }
  double seconds = readerQos.reliability.max_blocking_time.sec + readerQos.reliability.max_blocking_time.nanosec/1.0E9;
  cout << "  max_blocking_time (sec): " << seconds  << endl;
  cout << "  synchronous: unsupported in opendds"<< endl;
  
  //liveliness
  cout << "Liveliness: " << endl;
  cout << "  kind: ";
  if(readerQos.liveliness.kind == AUTOMATIC_LIVELINESS_QOS) {
    cout << "AUTOMATIC" << endl;
  } else if(readerQos.liveliness.kind == MANUAL_BY_PARTICIPANT_LIVELINESS_QOS) {
    cout << "MANUAL_BY_PARTICIPANT" << endl;
  } else if(readerQos.liveliness.kind == MANUAL_BY_TOPIC_LIVELINESS_QOS) {
    cout << "MANUAL_BY_READER" << endl;
  } else {
    cout << "??" << endl;
  }
  
  seconds = readerQos.liveliness.lease_duration.sec + readerQos.liveliness.lease_duration.nanosec/1.0E9;
  cout << "  lease duration: " << seconds << endl;
  
  //latency budget
  seconds = readerQos.latency_budget.duration.sec + readerQos.latency_budget.duration.nanosec/1.0E9;
  cout << "Latency Budget: " << seconds << endl;
  
  cout << endl;
}

void printCurrentTime(DDS::DomainParticipant &participant) {
	Time_t currentTime;
	ReturnCode_t status = participant.get_current_time(currentTime);
	checkStatus(status, "DDS::DomainParticipant::get_current_time");

	double formattedTime = (double)currentTime.sec + (double)currentTime.nanosec * 1.0E-9;
	cout << "Current time: " << fixed << setprecision(6) << formattedTime << endl;
}
