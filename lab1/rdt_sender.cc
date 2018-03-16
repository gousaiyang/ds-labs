/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <queue>
#include <map>
// #include <string>//###

#include "rdt_struct.h"
#include "rdt_sender.h"
#include "rdt_protocol.h"

bool packet_acked[RDT_MAX_SEQ_NO] = {};
int seq_no = 0;
int seq_no_wait = 0;
const int window_size = 10;
const double timeout = 0.3;

class SenderTask {
public:
    int seq_no;
    packet *pkt;
    SenderTask(): seq_no(0), pkt(NULL) {}
    SenderTask(int s, packet *p): seq_no(s), pkt(p) {}
};

std::queue<SenderTask> packet_queue;
std::map<int, packet*> packet_wait_ack;

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final()
{
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

void Sender_IncrementSeq()
{
    seq_no = (seq_no + 1) % (RDT_MAX_SEQ_NO + 1);
}

void Sender_ConstructPacket(int payload_size, bool end_of_msg, int seq_no, const char *payload, packet *pkt)
{
    ASSERT(payload_size >= 0 && payload_size <= (int)RDT_MAX_PAYLOAD_SIZE);
    ASSERT(seq_no >= 0 && seq_no <= RDT_MAX_SEQ_NO);
    ASSERT(payload);
    ASSERT(pkt);

    // Set Header
    pkt->data[0] = (char)(payload_size << RDT_END_OF_MSG_BITS | end_of_msg);
    memcpy(pkt->data + 1, (char*)&seq_no, RDT_SEQ_NO_SIZE);

    // Fill in payload
    memcpy(pkt->data + RDT_HEADER_SIZE, payload, payload_size);
    memset(pkt->data + RDT_HEADER_SIZE + payload_size, 0, RDT_MAX_PAYLOAD_SIZE - payload_size);

    // Calculate Checksum
    RDT_AddChecksum(pkt);
}

bool Sender_CheckAck(packet *pkt, int *seq_no)
{
    ASSERT(pkt);

    // Packet corrupted or not an ACK packet.
    if (!RDT_VerifyChecksum(pkt) || pkt->data[0] >> RDT_END_OF_MSG_BITS) {
        // printf("Sender: ack corrupted\n"); //###
        return false;
    }

    *seq_no = *(int*)(pkt->data + 1) & ((1 << RDT_SEQ_NO_BITS) - 1);
    return true;
}

void Sender_DoQueue()
{
    // printf("Sender: scanning queue\n"); //###
    SenderTask st;

    while (!packet_queue.empty()) {
        st = packet_queue.front();
        if (st.seq_no >= window_size && !packet_acked[st.seq_no - window_size]) {
            seq_no_wait = st.seq_no - window_size;
            // printf("Sender: packet %d not acked yet, start timer\n", seq_no_wait); //###
            if (!Sender_isTimerSet())
                Sender_StartTimer(timeout);
            return;
        }

        // printf("Sender: send [%d] %s\n", st.seq_no, std::string(st.pkt->data + RDT_HEADER_SIZE, st.pkt->data[0] >> 1 & ((1 << RDT_PAYLOAD_SIZE_BITS) - 1)).c_str()); //###
        Sender_ToLowerLayer(st.pkt);
        packet_wait_ack[st.seq_no] = st.pkt;
        packet_queue.pop();
    }

    for (std::map<int, packet*>::iterator it = packet_wait_ack.begin(); it != packet_wait_ack.end(); ++it) {
        if (!packet_acked[it->first]) {
            seq_no_wait = it->first;
            // printf("Sender: packet %d not acked yet, start timer\n", seq_no_wait); //###
            if (!Sender_isTimerSet())
                Sender_StartTimer(timeout);
            return;
        }
    }
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    ASSERT(msg);
    ASSERT(msg->size >= 0);

    if (!msg->size)
        return;

    ASSERT(msg->data);

    // printf("Sender: want to transmit %s\n", std::string(msg->data, msg->size).c_str()); //###

    int last_payload_size = msg->size % RDT_MAX_PAYLOAD_SIZE;
    if (!last_payload_size)
        last_payload_size = RDT_MAX_PAYLOAD_SIZE;

    int whole_packets_num = (msg->size - last_payload_size) / RDT_MAX_PAYLOAD_SIZE;
    for (int i = 0; i < whole_packets_num; ++i) {
        packet *pkt = (packet*)malloc(sizeof(packet));
        Sender_ConstructPacket(RDT_MAX_PAYLOAD_SIZE, false, seq_no, msg->data + i * RDT_MAX_PAYLOAD_SIZE, pkt);
        packet_queue.push(SenderTask(seq_no, pkt));
        // printf("Sender: append task [%d] %s\n", seq_no, std::string(msg->data + i * RDT_MAX_PAYLOAD_SIZE, RDT_MAX_PAYLOAD_SIZE).c_str()); //###
        Sender_IncrementSeq();
    }

    packet *pkt = (packet*)malloc(sizeof(packet));
    Sender_ConstructPacket(last_payload_size, true, seq_no, msg->data + whole_packets_num * RDT_MAX_PAYLOAD_SIZE, pkt);
    packet_queue.push(SenderTask(seq_no, pkt));
    // printf("Sender: append task [%d] %s\n", seq_no, std::string(msg->data + whole_packets_num * RDT_MAX_PAYLOAD_SIZE, last_payload_size).c_str()); //###
    Sender_IncrementSeq();

    Sender_DoQueue();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    int seq_no;
    if (!Sender_CheckAck(pkt, &seq_no))
        return;

    // printf("Sender: got ack [%d]\n", seq_no); //###
    packet_acked[seq_no] = true;
    if (Sender_isTimerSet() && seq_no == seq_no_wait) {
        Sender_StopTimer();
        // printf("Sender: kill timer for [%d]\n", seq_no); //###
    }
    free(packet_wait_ack[seq_no]);
    packet_wait_ack.erase(seq_no);

    Sender_DoQueue();
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    // printf("Sender: resend [%d]\n", seq_no_wait); //###
    Sender_ToLowerLayer(packet_wait_ack[seq_no_wait]);
    Sender_DoQueue();
}
