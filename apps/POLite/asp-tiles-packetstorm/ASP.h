// SPDX-License-Identifier: BSD-2-Clause

#ifndef _ASP_H_
#define _ASP_H_

#define POLITE_DUMP_STATS
#define POLITE_COUNT_MSGS

#define POLITE_MAX_FANOUT 64

#include <POLite.h>

#define NROOTS 100
#define PACK_UPD 14
#define MIN_PACK 8

struct ASPUpdateMessage {
	uint16_t rootIdx[PACK_UPD];
	uint16_t hops[PACK_UPD];
};
union ASPMessage {
	ASPUpdateMessage upd;
	uint32_t sum;
};

struct ASPState {
	int16_t rootIdx;
	
	uint16_t buff[NROOTS];
	uint8_t updated[NROOTS];
	uint32_t updatePending;
};

struct ASPDevice : PDevice<ASPState, None, ASPMessage> {
	// Called once by POLite at start of execution
	inline void init() {
		for(uint32_t i = 0; i < NROOTS; i++) {
			s->buff[i] = 0xffff;
			s->updated[i] = 0;
		}
		s->updatePending = 0;
		if(s->rootIdx>=0) {
			s->buff[s->rootIdx] = 0;
			s->updated[s->rootIdx] = 1;
			s->updatePending = 1;
			*readyToSend = Pin(0);
		}
	}

	// Send handler
	inline void send(volatile ASPMessage* msg) {
		uint32_t n = 0;
		for(uint32_t i = 0; i < NROOTS; i++) {
			if(s->updated[i]) {
				msg->upd.rootIdx[n] = i;
				msg->upd.hops[n] = s->buff[i];

				s->updated[i] = 0;
				s->updatePending--;
				n++;
				if(s->updatePending==0 || n>=PACK_UPD)
					break;
			}
		}
		for(; n<PACK_UPD; n++) {
			msg->upd.rootIdx[n] = 0xffff;
		}

		if(s->updatePending>=MIN_PACK)
			*readyToSend = Pin(0);
		else
			*readyToSend = No;
	}

	// Receive handler
	inline void recv(ASPMessage* msg, None* edge) {
		for(uint32_t n=0; n<PACK_UPD; n++) {
			uint16_t rootIdx = msg->upd.rootIdx[n];
			uint16_t hops = msg->upd.hops[n]+1;
			if(rootIdx < NROOTS) {
				if(hops < s->buff[rootIdx]) {
					s->buff[rootIdx] = hops;
					if(s->updated[rootIdx] == 0) {
						s->updatePending++;
					}
					s->updated[rootIdx] = 1;
				}
			}
			else break;
		}
		if(s->updatePending>=MIN_PACK)
			*readyToSend = Pin(0);
	}

	// Called by POLite on idle event
	inline bool step() {
		if(s->updatePending>0) {
			*readyToSend = Pin(0);
			return true;
		}
		else {
			*readyToSend = No;
			return false;
		}
	}

	// Optionally send message to host on termination
	inline bool finish(volatile ASPMessage* msg) {
		uint32_t total = 0;
		for(uint32_t i = 0; i < NROOTS; i++) {
			total += s->buff[i];
		}
		msg->sum = total;
		return true;
	}

};

#endif
