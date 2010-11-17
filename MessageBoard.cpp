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

#include "ccpp_dds_dcps.h"
#include "CheckStatus.h"
#include "ccpp_Chat.h"
#include "multitopic.h"

using namespace DDS;
using namespace Chat;



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
    DomainParticipant_ptr           parentDP;
    ExtDomainParticipant_var        participant;
    Topic_var                       chatMessageTopic;
    Topic_var                       nameServiceTopic;
    TopicDescription_var            namedMessageTopic;
    Subscriber_var                  chatSubscriber;
    DataReader_ptr                  parentReader;

    /* Type-specific DDS entities */
    ChatMessageTypeSupport_var      chatMessageTS;
    NameServiceTypeSupport_var      nameServiceTS;
    NamedMessageTypeSupport_var     namedMessageTS;
    ChatMessageDataReader_var      chatAdmin;
    ChatMessageSeq_var             msgSeq = new ChatMessageSeq();
    SampleInfoSeq_var               infoSeq = new SampleInfoSeq();

    /* QosPolicy holders */
    TopicQos                        reliable_topic_qos;
    TopicQos                        setting_topic_qos;
    SubscriberQos                   sub_qos;
    DDS::StringSeq                  parameterList;

    /* DDS Identifiers */
    DomainId_t                      domain = NULL;
    ReturnCode_t                    status;

    /* Others */
    bool                            terminated = FALSE;
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
    dpf = DomainParticipantFactory::get_instance();
    checkHandle(dpf.in(), "DDS::DomainParticipantFactory::get_instance");
    parentDP = dpf->create_participant (
        domain, 
        PARTICIPANT_QOS_DEFAULT, 
        NULL,
        STATUS_MASK_NONE);
    checkHandle(parentDP, "DDS::DomainParticipantFactory::create_participant");
    
    /* Narrow the normal participant to its extended representative */
    participant = ExtDomainParticipantImpl::_narrow(parentDP);
    checkHandle(participant.in(), "DDS::ExtDomainParticipant::_narrow");

    /* Register the required datatype for ChatMessage. */
    chatMessageTS = new ChatMessageTypeSupport();
    checkHandle(chatMessageTS.in(), "new ChatMessageTypeSupport");
    chatMessageTypeName = chatMessageTS->get_type_name();
    status = chatMessageTS->register_type(
        participant.in(), 
        chatMessageTypeName);
    checkStatus(status, "Chat::ChatMessageTypeSupport::register_type");
    
    /* Register the required datatype for NameService. */
    nameServiceTS = new NameServiceTypeSupport();
    checkHandle(nameServiceTS.in(), "new NameServiceTypeSupport");
    nameServiceTypeName =  nameServiceTS->get_type_name();
    status = nameServiceTS->register_type(
        participant.in(), 
        nameServiceTypeName);
    checkStatus(status, "Chat::NameServiceTypeSupport::register_type");
    
    /* Register the required datatype for NamedMessage. */
    namedMessageTS = new NamedMessageTypeSupport();
    checkHandle(namedMessageTS.in(), "new NamedMessageTypeSupport");
    namedMessageTypeName = namedMessageTS->get_type_name();
    status = namedMessageTS->register_type(
        participant.in(), 
        namedMessageTypeName);
    checkStatus(status, "Chat::NamedMessageTypeSupport::register_type");
    
    /* Set the ReliabilityQosPolicy to RELIABLE. */
    status = participant->get_default_topic_qos(reliable_topic_qos);
    checkStatus(status, "DDS::DomainParticipant::get_default_topic_qos");
    reliable_topic_qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    
    /* Make the tailored QoS the new default. */
    status = participant->set_default_topic_qos(reliable_topic_qos);
    checkStatus(status, "DDS::DomainParticipant::set_default_topic_qos");
    
    cout << "Topic QOS: " << endl;
    printTopicQos(reliable_topic_qos);

    /* Use the changed policy when defining the ChatMessage topic */
    chatMessageTopic = participant->create_topic( 
        "Chat_ChatMessage", 
        chatMessageTypeName, 
        reliable_topic_qos, 
        NULL,
        STATUS_MASK_NONE);
    checkHandle(chatMessageTopic.in(), "DDS::DomainParticipant::create_topic (ChatMessage)");

    /* Adapt the default SubscriberQos to read from the "ChatRoom" Partition. */
    status = participant->get_default_subscriber_qos (sub_qos);
    checkStatus(status, "DDS::DomainParticipant::get_default_subscriber_qos");
    sub_qos.partition.name.length(1);
    sub_qos.partition.name[0] = partitionName;

    /* Create a Subscriber for the MessageBoard application. */
    chatSubscriber = participant->create_subscriber(sub_qos, NULL, STATUS_MASK_NONE);
    checkHandle(chatSubscriber.in(), "DDS::DomainParticipant::create_subscriber");
    
    /* Create a DataReader for the NamedMessage Topic (using the appropriate QoS). */
    parentReader = chatSubscriber->create_datareader( 
        chatMessageTopic.in(), 
        DATAREADER_QOS_USE_TOPIC_QOS, 
        NULL,
        STATUS_MASK_NONE);
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

        for (ULong i = 0; i < msgSeq->length(); i++) {
            ChatMessage *msg = &(msgSeq[i]);
            if (msg->userID == TERMINATION_MESSAGE) {
                cout << "Termination message received: exiting..." << endl;
                terminated = TRUE;
            } else {
		printCurrentTime(*participant);
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
    status = participant->delete_subscriber(chatSubscriber.in());
    checkStatus(status, "DDS::DomainParticipant::delete_subscriber");
    
    /* Remove the Topics. */
    status = participant->delete_simulated_multitopic(namedMessageTopic.in());
    checkStatus(status, "DDS::ExtDomainParticipant::delete_simulated_multitopic");

    status = participant->delete_topic(nameServiceTopic.in());
    checkStatus(status, "DDS::DomainParticipant::delete_topic (nameServiceTopic)");

    status = participant->delete_topic(chatMessageTopic.in());
    checkStatus(status, "DDS::DomainParticipant::delete_topic (chatMessageTopic)");

    /* De-allocate the type-names. */
    string_free(namedMessageTypeName);
    string_free(nameServiceTypeName);
    string_free(chatMessageTypeName);

    /* Remove the DomainParticipant. */
    status = dpf->delete_participant(participant.in());
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
  cout << "  synchronous: " << (int)topicQos.reliability.synchronous << endl;
  
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
  cout << "  synchronous: " << (int)writerQos.reliability.synchronous << endl;
  
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
  cout << "  synchronous: " << (int)readerQos.reliability.synchronous << endl;
  
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

