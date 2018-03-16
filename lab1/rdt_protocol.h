/*
 * FILE: rdt_protocol.h
 * DESCRIPTION: Common constants and functions used by the RDT protocol.
 * NOTE: In this implementation, the packet format is laid out as the following:
 *
 *       |<-           header 4 bytes           ->|
 *       |<-  7 bits  ->|<- 1 bit ->|<- 3 bytes ->|<  120 bytes ->|<- 4 bytes ->|
 *       | payload_size |end_of_msg |   seq_no    |    payload    |   checksum  |
 *
 *       payload_size = 0 indicates an ACK packet instead of a data packet.
 */


#ifndef _RDT_PROTOCOL_H_
#define _RDT_PROTOCOL_H_

#include "rdt_struct.h"

#define RDT_PAYLOAD_SIZE_BITS 7
#define RDT_END_OF_MSG_BITS 1
#define RDT_SEQ_NO_SIZE 3
#define RDT_SEQ_NO_BITS (RDT_SEQ_NO_SIZE * 8)
// #define RDT_MAX_SEQ_NO ((1 << RDT_SEQ_NO_BITS) - 1)
#define RDT_MAX_SEQ_NO ((1 << (RDT_SEQ_NO_BITS - 4)) - 1) // Reduce memory usage.
#define RDT_HEADER_SIZE ((RDT_PAYLOAD_SIZE_BITS + RDT_END_OF_MSG_BITS + RDT_SEQ_NO_BITS) / 8)
#define RDT_CHECKSUM_SIZE sizeof(unsigned int)
#define RDT_MAX_PAYLOAD_SIZE (RDT_PKTSIZE - RDT_HEADER_SIZE - RDT_CHECKSUM_SIZE)


void RDT_AddChecksum(packet *pkt); // Calculate checksum of a packet and put it into the footer.
bool RDT_VerifyChecksum(packet *pkt); // Verify checksum of a packet.

#endif /* _RDT_PROTOCOL_H_ */
