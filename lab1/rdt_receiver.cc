/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "rdt_struct.h"
#include "rdt_receiver.h"
#include "rdt_protocol.h"


int last_end_of_msg = -1;

class ReceiveInfo {
public:
    bool received;
    bool is_end;
    std::string data;
    ReceiveInfo(): received(false), is_end(false) {}
};

ReceiveInfo receiver_packets[RDT_MAX_SEQ_NO];


/* receiver initialization, called once at the very beginning */
void Receiver_Init()
{
    fprintf(stdout, "At %.2fs: receiver initializing ...\n", GetSimulationTime());

    for (int i = 0; i < RDT_MAX_SEQ_NO; ++i)
        receiver_packets[i].received = false;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final()
{
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

bool Receiver_ParsePacket(packet *pkt, int *payload_size, bool *end_of_msg, int *seq_no, char *payload)
{
    if (!RDT_VerifyChecksum(pkt)) {
        // printf("Receiver: packet corrupted\n"); //###
        return false;
    }

    *payload_size = pkt->data[0] >> 1 & ((1 << RDT_PAYLOAD_SIZE_BITS) - 1);
    if (*payload_size <= 0 || *payload_size > (int)RDT_MAX_PAYLOAD_SIZE) {
        // printf("Receiver: invalid payload_size\n"); //###
        return false;
    }

    *end_of_msg = pkt->data[0] & 1;
    *seq_no = *(int*)(pkt->data + 1) & ((1 << RDT_SEQ_NO_BITS) - 1);

    memcpy(payload, pkt->data + RDT_HEADER_SIZE, *payload_size);
    return true;
}

void Receiver_ConstructAck(int seq_no, packet *pkt)
{
    ASSERT(seq_no >= 0 && seq_no <= RDT_MAX_SEQ_NO);
    ASSERT(pkt);

    // Set Header
    pkt->data[0] = 0;
    memcpy(pkt->data + 1, (char*)&seq_no, RDT_SEQ_NO_SIZE);

    // Fill in payload
    memset(pkt->data + RDT_HEADER_SIZE, 0, RDT_MAX_PAYLOAD_SIZE);

    // Calculate Checksum
    RDT_AddChecksum(pkt);
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt)
{
    int payload_size;
    bool end_of_msg;
    int seq_no;
    char payload[RDT_MAX_PAYLOAD_SIZE];
    std::string message;
    struct message msg;

    if (!Receiver_ParsePacket(pkt, &payload_size, &end_of_msg, &seq_no, payload))
        return;

    packet ackpkt;
    Receiver_ConstructAck(seq_no, &ackpkt);
    Receiver_ToLowerLayer(&ackpkt);

    receiver_packets[seq_no].received = true;
    receiver_packets[seq_no].is_end = end_of_msg;
    receiver_packets[seq_no].data = std::string(payload, payload_size);
    // printf("Receiver: ack [%d] %s\n", seq_no, receiver_packets[seq_no].data.c_str()); //###

    for (int i = last_end_of_msg + 1; ; ++i) {
        if (!receiver_packets[i].received)
            break;
        if (receiver_packets[i].is_end) {
            message.clear();

            for (int j = last_end_of_msg + 1; j <= i; ++j)
                message += receiver_packets[j].data;

            msg.size = message.size();
            msg.data = (char*)(long)message.c_str();
            // printf("Receiver: reassembly [%d] - [%d] %s\n", last_end_of_msg + 1, i, message.c_str()); //###
            Receiver_ToUpperLayer(&msg);
            last_end_of_msg = i;
        }
    }
}
