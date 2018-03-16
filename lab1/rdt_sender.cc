/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_sender.h"
#include "rdt_protocol.h"


bool sending_started = false; // Whether sender has started sending packets.
const double timeout = 0.3; // Timeout for ACK.
const double timer_interval = 0.1; // Time interval for ACK checker routine.
int nothing = 0; // Times of nothing done in the ACK checker.
const int max_nothing = 10; // Threshold for "nothing" to indicate end of sending.
int seq_no = 0; // Next sequence number to use.
int last_seq_no = -1; // Last sequence number seen by the ACK checker.

class PacketInfo {
public:
    packet *pkt; // Packet data.
    double send_time; // The time when it was sent.
    bool acked; // Whether it has been ACKed.
    PacketInfo(): pkt(NULL), send_time(0.0), acked(false) {}
};

PacketInfo sender_packets[RDT_MAX_SEQ_NO]; // Status of all packets (indexed by seq_no) on the sender side.


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

// Increment current sequence number.
void Sender_IncrementSeq()
{
    seq_no = (seq_no + 1) % (RDT_MAX_SEQ_NO + 1);
}

// Construct a data packet with data and metadata.
void Sender_ConstructPacket(int payload_size, bool end_of_msg, int seq_no, const char *payload, packet *pkt)
{
    ASSERT(payload_size >= 0 && payload_size <= (int)RDT_MAX_PAYLOAD_SIZE);
    ASSERT(seq_no >= 0 && seq_no <= RDT_MAX_SEQ_NO);
    ASSERT(payload);
    ASSERT(pkt);

    // Set header.
    pkt->data[0] = (char)(payload_size << RDT_END_OF_MSG_BITS | end_of_msg);
    memcpy(pkt->data + 1, (char*)&seq_no, RDT_SEQ_NO_SIZE);

    // Fill in payload (pad with zero bytes).
    memcpy(pkt->data + RDT_HEADER_SIZE, payload, payload_size);
    memset(pkt->data + RDT_HEADER_SIZE + payload_size, 0, RDT_MAX_PAYLOAD_SIZE - payload_size);

    // Add checksum.
    RDT_AddChecksum(pkt);
}

// Check whether a packet is a valid ACK packet.
bool Sender_CheckAck(packet *pkt, int *seq_no)
{
    ASSERT(pkt);

    // Packet corrupted or not an ACK packet.
    if (!RDT_VerifyChecksum(pkt) || pkt->data[0] >> RDT_END_OF_MSG_BITS)
        return false;

    *seq_no = *(int*)(pkt->data + 1) & ((1 << RDT_SEQ_NO_BITS) - 1);
    return true;
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    ASSERT(msg);
    ASSERT(msg->size >= 0);

    // Ignore empty messages.
    if (!msg->size)
        return;

    ASSERT(msg->data);

    // Start the ACK checker routine on first entry.
    if (!sending_started) {
        sending_started = true;
        Sender_StartTimer(timer_interval);
    }

    double current_time = GetSimulationTime();

    // Split the message and send every part. Record every packet.
    int last_payload_size = msg->size % RDT_MAX_PAYLOAD_SIZE;
    if (!last_payload_size)
        last_payload_size = RDT_MAX_PAYLOAD_SIZE;

    int whole_packets_num = (msg->size - last_payload_size) / RDT_MAX_PAYLOAD_SIZE;
    for (int i = 0; i < whole_packets_num; ++i) {
        sender_packets[seq_no].pkt = (packet*)malloc(sizeof(packet));
        ASSERT(sender_packets[seq_no].pkt);
        sender_packets[seq_no].send_time = current_time;
        sender_packets[seq_no].acked = false;
        Sender_ConstructPacket(RDT_MAX_PAYLOAD_SIZE, false, seq_no, msg->data + i * RDT_MAX_PAYLOAD_SIZE,
            sender_packets[seq_no].pkt);
        Sender_ToLowerLayer(sender_packets[seq_no].pkt);
        Sender_IncrementSeq();
    }

    sender_packets[seq_no].pkt = (packet*)malloc(sizeof(packet));
    ASSERT(sender_packets[seq_no].pkt);
    sender_packets[seq_no].send_time = current_time;
    sender_packets[seq_no].acked = false;
    Sender_ConstructPacket(last_payload_size, true, seq_no, msg->data + whole_packets_num * RDT_MAX_PAYLOAD_SIZE,
        sender_packets[seq_no].pkt);
    Sender_ToLowerLayer(sender_packets[seq_no].pkt);
    Sender_IncrementSeq();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    int seq_no;
    if (!Sender_CheckAck(pkt, &seq_no)) // Not valid ACK.
        return;

    // Mark the packet as ACKed. Free corresponding space.
    sender_packets[seq_no].acked = true;
    free(sender_packets[seq_no].pkt);
    sender_packets[seq_no].pkt = NULL;
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    // This function works as the ACK checker routine. It starts the timer again before returning, thus running
    // over and over again (until packet sending is end), mimicking the JavaScript function `setInterval()`.

    double current_time = GetSimulationTime();
    bool remaining = false; // Whether there is packet sent but not ACKed.

    for (int i = 0; i < seq_no; ++i) {
        if (!sender_packets[i].acked) { // Found an unACKed packet.
            remaining = true;
            if (current_time - sender_packets[i].send_time >= timeout) // Time out. Retransmit this packet.
                Sender_ToLowerLayer(sender_packets[i].pkt);
        }
    }

    nothing = remaining || last_seq_no != seq_no ? 0 : nothing + 1;
    last_seq_no = seq_no;

    if (nothing < max_nothing) // Packet sending still active, continue routine after interval.
        Sender_StartTimer(timer_interval);
}
