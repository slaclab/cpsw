 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_RSSI_H
#define CPSW_RSSI_H

#include <cpsw_rssi_proto.h>
#include <cpsw_event.h>
#include <cpsw_rssi_timer.h>
#include <cpsw_error.h>
#include <cpsw_thread.h>

#include <stdint.h>

#include <cpsw_buf.h>

#include <vector>

#include <boost/weak_ptr.hpp>
#include <boost/atomic.hpp>
using boost::weak_ptr;
using boost::atomic;

#define RSSI_DEBUG 0 // level

#ifdef RSSI_DEBUG
extern int rssi_debug;
#endif

class IRxEventHandler : public IEventHandler {
protected:
	virtual void handleRxEvent(IIntEventSource*) = 0;
	virtual void handle(IIntEventSource *s) { handleRxEvent(s); }
};

class IUsrInputEventHandler : public IEventHandler {
protected:
	virtual void handleUsrInputEvent(IIntEventSource*) = 0;
	virtual void handle(IIntEventSource *s) { handleUsrInputEvent(s); }
};

class IUsrOutputEventHandler : public IEventHandler {
protected:
	virtual void handleUsrOutputEvent(IIntEventSource*) = 0;
	virtual void handle(IIntEventSource *s) { handleUsrOutputEvent(s); }
};

class IRexTimer : public RssiTimer {
protected:
	virtual void processRetransmissionTimeout() = 0;
public:
	IRexTimer(RssiTimerList *l) : RssiTimer("REX", l) {}
	virtual void process() { processRetransmissionTimeout(); }
};

class IAckTimer : public RssiTimer {
protected:
	virtual void processAckTimeout() = 0;
public:
	IAckTimer(RssiTimerList *l) : RssiTimer("ACK", l) {}
	virtual void process() { processAckTimeout(); }

};

class INulTimer : public RssiTimer {
protected:
	virtual void processNulTimeout() = 0;
public:
	INulTimer(RssiTimerList *l) : RssiTimer("NUL", l) {}
	virtual void process() { processNulTimeout(); }

};

// posts events when the state changes
class CConnectionStateChangedEventSource : public CIntEventSource {
public:
	static const int      CONN_STATE_OPEN   = 10; // could support other states
	static const int      CONN_STATE_CLOSED = -1;
};

// posts events when the state is changed to
// the defined target state (currently: open/closed)
class CConnectionStateEventSource : public IIntEventSource {
protected:
	atomic<int>  connState_;

public:
	CConnectionStateEventSource();

	virtual void connectionStateChanged(int);
};

class CConnectionOpenEventSource   : public CConnectionStateEventSource {
public:
	CConnectionOpenEventSource();
protected:
	virtual bool checkForEvent();
};

class CConnectionNotOpenEventSource   : public CConnectionStateEventSource {
public:
	CConnectionNotOpenEventSource();
protected:
	virtual bool checkForEvent();
};


class CConnectionClosedEventSource : public CConnectionStateEventSource {
public:
	CConnectionClosedEventSource();
protected:
	virtual bool checkForEvent();
};

// Obtain the max. segment size to advertise to the peer
// The return value of 'getRxMTU' is the max. size the
// system can accept **including** any RSSI headers
class IMTUQuerier {
public:
	virtual int getRxMTU() = 0;

	virtual ~IMTUQuerier() {}
};


class CRssi : public CRunnable,
              public IRxEventHandler,
              public IUsrInputEventHandler,
              public IUsrOutputEventHandler,
              public IRexTimer,
              public IAckTimer,
              public INulTimer,
              public CConnectionStateChangedEventSource,
              public CConnectionOpenEventSource,
              public CConnectionNotOpenEventSource,
              public CConnectionClosedEventSource
{

public:
	static const uint8_t  LD_MAX_UNACKED_SEGS = 4;
	static const uint8_t  MAX_UNACKED_SEGS = 1<<LD_MAX_UNACKED_SEGS; // must be power of two
	static const uint16_t MAX_SEGMENT_SIZE = 1500 - 20 - 8 - 8; // - IP - UDP - RSSI
	static const uint16_t RETRANSMIT_TIMEO = 10;            // ms ?
	static const uint16_t CUMLTD_ACK_TIMEO =  5;            // < rexmit TO
	static const uint16_t NUL_SEGMEN_TIMEO = 3000;
	static const uint8_t  MAX_RETRANSMIT_N = 15;
	static const uint8_t  MAX_CUMLTD_ACK_N =  2;
	static const unsigned UNIT_US          = 1000;
	static const unsigned UNIT_US_EXP      =  3; // value used by server; must match UNIT_US (i.e., UNIT_US = 10^-UNIT_US_EXP)


private:
	bool        isServer_;
	const char  *name_;
	EventSet    eventSet_;
	IMTUQuerier *mtuQuerier_;

protected:
	BufQueue    outQ_;
	BufQueue    inpQ_;

private:
	static const unsigned OQ_DEPTH = 8;
	static const unsigned IQ_DEPTH = 4;

public:

	CRssi(bool isServer, int threadPrio = DFLT_PRIORITY, IMTUQuerier *mtuQuerier = 0);

	virtual ~CRssi();

	const char *getName()  { return name_;     }

	virtual void dumpStats(FILE *);

protected:

	typedef uint8_t SeqNo;

	class BufBase {
	protected:
		typedef unsigned BufIdx;

		unsigned capa_;
		std::vector<BufChain> buf_;
		BufIdx rp_;
		BufIdx msk_;

		SeqNo    oldest_;

	public:
		BufBase(unsigned ldsz)
		: buf_( (capa_= (1<<ldsz)) ),
		  rp_(0),
		  msk_( ((1<<ldsz)-1) )
		{
		}

		unsigned getCapa()
		{
			return capa_;
		}

		static SeqNo nxtSeqNo(SeqNo s)
		{
			return ( s + 1 ) & 0xff;
		}

		BufChain pop()
		{
		BufChain rval;

			rval.swap( buf_[rp_&msk_] );

			if ( rval ) {
				rp_++;
				oldest_ = nxtSeqNo( oldest_ );
			}

			return rval;
		}

		BufChain peek()
		{
			return buf_[rp_ & msk_];
		}


		unsigned getOldest()
		{
			return oldest_;
		}

		void seed(SeqNo synSeqNo)
		{
			oldest_ = synSeqNo;
		}

		void purge()
		{
			while ( pop() )
				;
		}

	};

	class RingBuf : public BufBase {
	private:
		BufIdx wp_;

		friend class iterator;

	public:
		RingBuf(unsigned ldsz)
		: BufBase(ldsz),
		  wp_(rp_)
		{}

		void dump();

		class iterator {
		private:
			RingBuf *rb_;
			unsigned idx_;

			class RinBuf;

			friend void RingBuf::dump();

		public:
			iterator(RingBuf *rb)
			: rb_(rb),
			  idx_(rb->rp_)
			{}

			iterator & operator++()
			{
				if ( idx_ != rb_->wp_ )
					idx_++;
				return *this;
			}

			BufChain operator*()
			{
				if ( idx_ == rb_->wp_ )
					return BufChain();
				return rb_->buf_.at( (idx_ & rb_->msk_) );
			}
		};

		iterator begin()
		{
			return iterator(this);
		}

		unsigned getSize()
		{
			return (unsigned)(wp_ - rp_);
		}

		int ack(SeqNo ackNo)
		{
		SeqNo    cumAck = ackNo - oldest_ + 1;
		unsigned i;

			if ( cumAck > getSize() ) {
				// an error -- misformed packet?
				// the caller should discard...
				return -1;
			}

			for ( i=0; i<cumAck; i++ )
				pop();

			return cumAck;
		}

		void push(BufChain b)
		{
		unsigned widx = wp_ & msk_;
			if ( getSize() > capa_ ) {
				// should never happen since we must
				// not have more than ossMX outstanding
				// transmits
				throw InternalError("RingBuf Overflow");
			}
			buf_[ widx ] = b;
			wp_++;
		}

		void resize(unsigned new_capa)
		{
		unsigned         i;
		unsigned         sz = getSize();
		std::vector<BufChain> tmp( sz );
		SeqNo            tmpo = getOldest();
			for ( i=0; i<sz; i++ )
				tmp[i] = pop();
			for ( capa_=1; capa_ < new_capa; capa_<<=1 )
				;
			buf_.resize(capa_);
			rp_  = wp_ = 0;
			msk_ = capa_ - 1;
			buf_.resize( capa_ );
			seed( tmpo );
			for ( i=0; i<sz; i++ )
				push( tmp[i] );
		}
	};

	class ReassembleBuf : public BufBase {
	private:
		SeqNo lim_;

	public:
		// capa is a power of two, limit is anything
		ReassembleBuf(unsigned ld_capa, unsigned limit)
		: BufBase(ld_capa),
		  lim_(limit)
		{}

		bool canAccept(SeqNo seqNo)
		{
		SeqNo off = seqNo - oldest_;
			return off < lim_;
		}

		void store(SeqNo seqNo, BufChain b)
		{
		SeqNo off = seqNo - oldest_;
		unsigned idx;

			if ( off < lim_ ) {
				// inside acceptance window
				idx = (off + rp_) & msk_;
				buf_[idx] = b;
			} else {
				// the caller supposedly has used 'canAccept' earlier
				throw InternalError("Send buffer overflow");
			}
		}

		SeqNo getLastInOrder()
		{
			return (getOldest() - 1) & 0xff;
		}

		void purge()
		{
		unsigned i;
			for ( i=0; i<capa_; i++ )
				buf_[i].reset();
		}
	};

	// The states
	class STATE {
	private:
		const char *name_;
	public:
		STATE(const char *name) : name_(name) {}
		virtual const char *getName()
		{
			return name_;
		}

		virtual void handleRxEvent       (CRssi *context, IIntEventSource *src);
		virtual void handleUsrInputEvent (CRssi *context, IIntEventSource *src);
		virtual void handleUsrOutputEvent(CRssi *context, IIntEventSource *src);
		virtual void processRetransmissionTimeout(CRssi *context);
		virtual void processAckTimeout(CRssi *context);
		virtual void processNulTimeout(CRssi *context);

		virtual  int getConnectionState(CRssi *context);

		virtual void shutdown(CRssi *context);

		virtual void advance(CRssi *context) = 0;
	};

	class CLOSED : public STATE {
	public:
		CLOSED():STATE("CLOSED"){}
		virtual void advance(CRssi *context);
		virtual  int getConnectionState(CRssi *context);
	};
	friend class CLOSED;

	class NOTCLOSED: public STATE {
	public:
		NOTCLOSED(const char *name):STATE(name){}
		virtual void processRetransmissionTimeout(CRssi *context);
		virtual void handleRxEvent(CRssi *context, IIntEventSource *src);
		virtual void drainReassembleBuffer(CRssi *context);
		virtual bool handleSYN(CRssi *context, RssiSynHeader &hdr);
		virtual bool handleOTH(CRssi *context, RssiHeader &hdr, bool);
		virtual BufChain  hasBufToSend(CRssi *context);
		        void advance(CRssi *context, bool checkTimer);
		virtual void advance(CRssi *context);
		virtual void shutdown(CRssi *context);
	};
	friend class NOTCLOSED;

	class WAIT_SYN : public NOTCLOSED {
	public:
		WAIT_SYN(const char *name):NOTCLOSED(name) {}
		virtual bool handleOTH(CRssi *context, RssiHeader &hdr, bool);

		virtual void extractConnectionParams(CRssi *context, RssiSynHeader &);
	};

	class CLNT_WAIT_SYN_ACK : public WAIT_SYN {
	public:
		CLNT_WAIT_SYN_ACK():WAIT_SYN("CLNT_WAIT_SYN_ACK") {}
		virtual bool handleSYN(CRssi *context, RssiSynHeader &hdr);
		virtual void extractConnectionParams(CRssi *context, RssiSynHeader &);
	};
	friend class CLNT_WAIT_SYN_ACK;

	class LISTEN : public WAIT_SYN {
	public:
		LISTEN():WAIT_SYN("LISTEN") {}
		virtual bool handleSYN(CRssi *context, RssiSynHeader &hdr);
		virtual void advance(CRssi *context);
		virtual void shutdown(CRssi *context);
	};
	friend class LISTEN;

	class SERV_WAIT_SYN_ACK : public NOTCLOSED {
	public:
		SERV_WAIT_SYN_ACK():NOTCLOSED("SERV_WAIT_SYN_ACK") {}
		virtual void handleRxEvent(CRssi *context, IIntEventSource *src);
	};
	friend class SERV_WAIT_SYN_ACK;

	class PREP_OPEN : public NOTCLOSED {
	public:
		PREP_OPEN():NOTCLOSED("PREP_OPEN") {}

		virtual void advance(CRssi *context);
	};
	friend class PREP_OPEN;

	class OPEN : public NOTCLOSED {
	public:
		OPEN():NOTCLOSED("OPEN")         {}
		OPEN(const char *n):NOTCLOSED(n) {}

		virtual void drainReassembleBuffer(CRssi *context);
		virtual void handleUsrInputEvent (CRssi *context, IIntEventSource *src);
		virtual void handleUsrOutputEvent(CRssi *context, IIntEventSource *src);
		virtual BufChain  hasBufToSend(CRssi *context);
		virtual void processAckTimeout(CRssi *context);
		virtual void processNulTimeout(CRssi *context);
		virtual  int getConnectionState(CRssi *context);
	};
	friend class OPEN;

	class OPEN_OUTWIN_FULL : public OPEN {
	public:
		OPEN_OUTWIN_FULL():OPEN("OPEN_OUTWIN_FULL") {}

		virtual void handleRxEvent(CRssi *context, IIntEventSource *src);
	};
	friend class OPEN_OUTWIN_FULL;

	CLOSED            stateCLOSED;
	LISTEN            stateLISTEN;
	CLNT_WAIT_SYN_ACK stateCLNT_WAIT_SYN_ACK;
	SERV_WAIT_SYN_ACK stateSERV_WAIT_SYN_ACK;
	PREP_OPEN         statePREP_OPEN;
	OPEN              stateOPEN;
	OPEN_OUTWIN_FULL  stateOPEN_OUTWIN_FULL;

	STATE *state_;

	bool  verifyChecksum_;
	bool  addChecksum_;

	unsigned timerUnits_;

	CTimeout rexTO_, cakTO_, nulTO_;
	unsigned rexMX_;
	int      cakMX_;
	unsigned units_;
	uint32_t conID_;
	unsigned peerOssMX_, peerSgsMX_;
	unsigned numRex_;
	int      numCak_; // may temporarily fall below zero
	bool     peerBSY_;

	struct timespec closedReopenDelay_;

	SeqNo lastSeqSent_;
	SeqNo lastSeqRecv_; // last segment received in sequence; this is what we ACK


	RingBuf        unAckedSegs_;
	ReassembleBuf  unOrderedSegs_;

	RssiTimerList  timers_;

	void close();
	void open();
	void sendBufAndKeepForRetransmission(BufChain);
	void sendBuf(BufChain, bool);
	void armRexAndNulTimer();
	void processAckNumber(uint8_t, SeqNo);

	void sendSYN(bool do_ack);
	void sendACK();
	bool sendNUL();
	void sendRST();
	void sendDAT(BufChain);

	struct {
		unsigned outgoingDropped_;
		unsigned rexSegments_;
		unsigned rexTimeouts_;
		unsigned ackTimeouts_;
		unsigned nulTimeouts_;
		unsigned connFailed_;
		unsigned badChecksum_;
		unsigned badSynDropped_;
		unsigned badHdrDropped_;
		unsigned rejectedSegs_;
		unsigned skippedNULs_;
		unsigned numSegsAckedByPeer_;
		unsigned numSegsGivenToUser_;
		unsigned busyFlagsCounted_;
		unsigned busyDeassertRex_;
	} stats_;


	// all of the event handler callbacks are executed
	// in the context of the thread 'body'
	virtual void handleRxEvent       (IIntEventSource *src);
	virtual void handleUsrInputEvent (IIntEventSource *src);
	virtual void handleUsrOutputEvent(IIntEventSource *src);

	IRxEventHandler        *recvEH()   { return (IRxEventHandler *)       this; }
	IUsrInputEventHandler  *usrIEH()   { return (IUsrInputEventHandler *) this; }
	IUsrOutputEventHandler *usrOEH()   { return (IUsrOutputEventHandler *)this; }

	virtual void processRetransmissionTimeout();
	virtual void processAckTimeout();
	virtual void processNulTimeout();

	IRexTimer              *rexTimer() { return (IRexTimer*) this; }
	IAckTimer              *ackTimer() { return (IAckTimer*) this; }
	INulTimer              *nulTimer() { return (INulTimer*) this; }

	virtual void attach(IEventSource*);

	virtual BufChain tryPopUpstream()      = 0;
	virtual bool tryPushUpstream(BufChain) = 0;

	void getAbsTime(CTimeout *to)
	{
		if ( clock_gettime( CLOCK_REALTIME, &to->tv_ ) )
			throw InternalError("clock_gettime failed");
	}

	virtual void forceACK()
	{
		numCak_ = cakMX_;
	}

	virtual void * threadBody();

	virtual void changeState(STATE *newState);

	virtual bool threadStop();

};

#endif
