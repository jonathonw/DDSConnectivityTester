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
 * LOGICAL_NAME:    Chatter.cpp
 * FUNCTION:        OpenSplice Tutorial example code.
 * MODULE:          Tutorial for the C++ programming language.
 * DATE             june 2007.
 ************************************************************************
 * 
 * This file contains the implementation for the 'Chatter' executable.
 * 
 ***/
#include <string>
#include <sstream>
#include <iostream>
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

#define MAX_MSG_LEN 256
#define NUM_MSG 10
#define TERMINATION_MESSAGE -1 

using namespace DDS;
using namespace Chat;
using namespace CORBA;

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
    DomainParticipant_var           participant;
    Topic_var                       chatMessageTopic;
    Topic_var                       nameServiceTopic;
    Publisher_var                   chatPublisher;
    DataWriter_ptr                  parentWriter;

    /* QosPolicy holders */
    TopicQos                        reliable_topic_qos;
    TopicQos                        setting_topic_qos;
    PublisherQos                    pub_qos;
    DataWriterQos                   dw_qos;

    /* DDS Identifiers */
    DomainId_t                      domain = 0;
    InstanceHandle_t                userHandle;
    ReturnCode_t                    status;

    /* Type-specific DDS entities */
    ChatMessageTypeSupport_var      chatMessageTS;
    NameServiceTypeSupport_var      nameServiceTS;
    ChatMessageDataWriter_var       talker;
    NameServiceDataWriter_var       nameServer;

    /* Sample definitions */
    ChatMessage                     *msg;   /* Example on Heap */
    NameService                     ns;     /* Example on Stack */
    
    /* Others */
    int                             ownID = 1;
    int                             i;
    char                            *chatterName = NULL;
    const char                      *partitionName = "ChatRoom";
    char                            *chatMessageTypeName = NULL;
    char                            *nameServiceTypeName = NULL;
    ostringstream                   buf;

#ifdef INTEGRITY
#ifdef CHATTER_QUIT
    ownID = -1;
#else
    ownID = 1;
#endif
    chatterName = "dds_user";
#else
    /* Options: Chatter [ownID [name]] */
    if (argc > 1) {
        istringstream args(argv[1]);
        args >> ownID;
        if (argc > 2) {
            chatterName = argv[2];
        }
    }
#endif

    /* Create a DomainParticipantFactory and a DomainParticipant (using Default QoS settings. */
    dpf = TheParticipantFactoryWithArgs(argc, argv);
    checkHandle(dpf.in(), "DDS::DomainParticipantFactory::get_instance");
    participant = dpf->create_participant(domain, PARTICIPANT_QOS_DEFAULT, NULL, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(participant.in(), "DDS::DomainParticipantFactory::create_participant");  
    
    OpenDDS::DCPS::TransportImpl_rch transport_impl =
      TheTransportFactory->create_transport_impl( OpenDDS::DCPS::DEFAULT_SIMPLE_TCP_ID, OpenDDS::DCPS::AUTO_CONFIG);

    /* Register the required datatype for ChatMessage. */
    chatMessageTS = new Chat::ChatMessageTypeSupportImpl();
    checkHandle(chatMessageTS.in(), "new ChatMessageTypeSupport");
    chatMessageTypeName = chatMessageTS->get_type_name();
    status = chatMessageTS->register_type(
        participant.in(), 
        chatMessageTypeName);
    checkStatus(status, "Chat::ChatMessageTypeSupport::register_type");
    
    /* Register the required datatype for NameService. */
    nameServiceTS = new Chat::NameServiceTypeSupportImpl();
    checkHandle(nameServiceTS.in(), "new NameServiceTypeSupport");
    nameServiceTypeName =  nameServiceTS->get_type_name();
    status = nameServiceTS->register_type(
        participant.in(), 
        nameServiceTypeName);
    checkStatus(status, "Chat::NameServiceTypeSupport::register_type");

    /* Set the ReliabilityQosPolicy to RELIABLE. */
    status = participant->get_default_topic_qos(reliable_topic_qos);
    printTopicQos(reliable_topic_qos);
    checkStatus(status, "DDS::DomainParticipant::get_default_topic_qos");
    reliable_topic_qos.reliability.kind = RELIABLE_RELIABILITY_QOS;
    printTopicQos(reliable_topic_qos);
    
    /* Make the tailored QoS the new default. */
    status = participant->set_default_topic_qos(reliable_topic_qos);
    checkStatus(status, "DDS::DomainParticipant::set_default_topic_qos");

    /* Use the changed policy when defining the ChatMessage topic */
    chatMessageTopic = participant->create_topic(
        "Chat_ChatMessage", 
        chatMessageTypeName, 
        reliable_topic_qos, 
        NULL,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(chatMessageTopic.in(), "DDS::DomainParticipant::create_topic (ChatMessage)");
    
    
    /* Set the DurabilityQosPolicy to TRANSIENT. */
    status = participant->get_default_topic_qos(setting_topic_qos);
    checkStatus(status, "DDS::DomainParticipant::get_default_topic_qos");
    setting_topic_qos.durability.kind = TRANSIENT_DURABILITY_QOS;

    /* Create the NameService Topic. */
    nameServiceTopic = participant->create_topic( 
        "Chat_NameService", 
        nameServiceTypeName, 
        setting_topic_qos, 
        NULL,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(nameServiceTopic.in(), "DDS::DomainParticipant::create_topic (NameService)");

    /* Adapt the default PublisherQos to write into the "ChatRoom" Partition. */
    status = participant->get_default_publisher_qos (pub_qos);
    checkStatus(status, "DDS::DomainParticipant::get_default_publisher_qos");
    pub_qos.partition.name.length(1);
    pub_qos.partition.name[0] = partitionName;

    /* Create a Publisher for the chatter application. */
    chatPublisher = participant->create_publisher(pub_qos, NULL, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(chatPublisher.in(), "DDS::DomainParticipant::create_publisher");
    
    // Attach the publisher to the transport. 
    status = transport_impl->attach(chatPublisher.in());
    if (status != OpenDDS::DCPS::ATTACH_OK) {
      std::cerr << "Failed to attach to the transport." << std::endl; 
      return 1;
    }
    
    /* Create a DataWriter for the ChatMessage Topic (using the appropriate QoS). */
    parentWriter = chatPublisher->create_datawriter(
        chatMessageTopic.in(), 
        DATAWRITER_QOS_USE_TOPIC_QOS,
        NULL,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(parentWriter, "DDS::Publisher::create_datawriter (chatMessage)");
    
    /* Narrow the abstract parent into its typed representative. */
    talker = ChatMessageDataWriter::_narrow(parentWriter);
    checkHandle(talker.in(), "Chat::ChatMessageDataWriter::_narrow");

    /* Create a DataWriter for the NameService Topic (using the appropriate QoS). */
    status = chatPublisher->get_default_datawriter_qos(dw_qos);
    checkStatus(status, "DDS::Publisher::get_default_datawriter_qos");
    status = chatPublisher->copy_from_topic_qos(dw_qos, setting_topic_qos);
    checkStatus(status, "DDS::Publisher::copy_from_topic_qos");
    dw_qos.writer_data_lifecycle.autodispose_unregistered_instances = false;
    parentWriter = chatPublisher->create_datawriter( 
        nameServiceTopic.in(), 
        dw_qos, 
        NULL,
        OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    checkHandle(parentWriter, "DDS::Publisher::create_datawriter (NameService)");
    cout << "Data Writer QOS: " << endl;
    printWriterQos(dw_qos);
    
    /* Narrow the abstract parent into its typed representative. */
    nameServer = NameServiceDataWriter::_narrow(parentWriter);
    checkHandle(nameServer.in(), "Chat::NameServiceDataWriter::_narrow");
    
    /* Initialize the NameServer attributes located on stack. */
    ns.userID = ownID;
    if (chatterName) {
        ns.name = string_dup(chatterName);
    } else {
        buf << "Chatter " << ownID;
        ns.name = string_dup( buf.str().c_str() );
    }

    /* Write the user-information into the system (registering the instance implicitly). */
    status = nameServer->write(ns, HANDLE_NIL);
    checkStatus(status, "Chat::ChatMessageDataWriter::write");
    
    /* Initialize the chat messages on Heap. */
    msg = new ChatMessage();
    checkHandle(msg, "new ChatMessage");
    msg->userID = ownID;
    msg->index = 0;
    buf.str( string("") );
    if (ownID == TERMINATION_MESSAGE) {
        buf << "Termination message.";
    } else { 
        buf << "Hi there, I will send you " << NUM_MSG << " more messages.";
    }
    msg->content = string_dup( buf.str().c_str() );
    cout << "Writing message: \"" << msg->content  << "\"" << endl;

    /* Register a chat message for this user (pre-allocating resources for it!!) */
    userHandle = talker->register_instance(*msg);

    /* Write a message using the pre-generated instance handle. */
    status = talker->write(*msg, userHandle);
    checkStatus(status, "Chat::ChatMessageDataWriter::write");

    sleep (1); /* do not run so fast! */
 
    /* Write any number of messages, re-using the existing string-buffer: no leak!!. */
    for (i = 1; i <= NUM_MSG && ownID != TERMINATION_MESSAGE; i++) {
        printCurrentTime(*participant);
        
        buf.str( string("") );
        msg->index = i;
        buf << "Message no. " << i;
        msg->content = string_dup( buf.str().c_str() );
        cout << "Writing message: \"" << msg->content << "\"" << endl;
        status = talker->write(*msg, userHandle);
        checkStatus(status, "Chat::ChatMessageDataWriter::write");
        sleep (1); /* do not run so fast! */
    }

    /* Leave the room by disposing and unregistering the message instance. */
    status = talker->dispose(*msg, userHandle);
    checkStatus(status, "Chat::ChatMessageDataWriter::dispose");
    status = talker->unregister_instance(*msg, userHandle);
    checkStatus(status, "Chat::ChatMessageDataWriter::unregister_instance");

    /* Also unregister our name. */
    status = nameServer->unregister_instance(ns, HANDLE_NIL);
    checkStatus(status, "Chat::NameServiceDataWriter::unregister_instance");

    /* Release the data-samples. */
    delete msg;     // msg allocated on heap: explicit de-allocation required!!

    /* Remove the DataWriters */
    status = chatPublisher->delete_datawriter( talker.in() );
    checkStatus(status, "DDS::Publisher::delete_datawriter (talker)");
    
    status = chatPublisher->delete_datawriter( nameServer.in() );
    checkStatus(status, "DDS::Publisher::delete_datawriter (nameServer)");
    
    /* Remove the Publisher. */
    status = participant->delete_publisher( chatPublisher.in() );
    checkStatus(status, "DDS::DomainParticipant::delete_publisher");
    
    /* Remove the Topics. */
    status = participant->delete_topic( nameServiceTopic.in() );
    checkStatus(status, "DDS::DomainParticipant::delete_topic (nameServiceTopic)");
    
    status = participant->delete_topic( chatMessageTopic.in() );
    checkStatus(status, "DDS::DomainParticipant::delete_topic (chatMessageTopic)");

    /* Remove the type-names. */
    string_free(chatMessageTypeName);
    string_free(nameServiceTypeName);
    
    /* Remove the DomainParticipant. */
    status = dpf->delete_participant( participant.in() );
    checkStatus(status, "DDS::DomainParticipantFactory::delete_participant");

    printf("Completed chatter example.\n");
    fflush(stdout);
    return 0;
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
  cout << "  synchronous: unsupported on OpenDDS" << endl;
  
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
  cout << "  synchronous: unsupported on OpenDDS" << endl;
  
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
  cout << "  synchronous: unsupported on OpenDDS" << endl;
  
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

