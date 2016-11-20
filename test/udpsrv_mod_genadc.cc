 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#include <udpsrv_regdefs.h>
#include <cpsw_thread.h>
#include <cpsw_mutex.h>
#include <byteswap.h>

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>

#include <vector>

#define BSA_CHANNELS 4
#define BSA_SPACING 0x100000
#define BSA_INI_SIZE 256

#define TIMING_HEADER_SIZE (14*4) /* bytes */
#define TIMING_TS_OFF_HI   (6*4)
#define TIMING_TS_OFF_LO   (7*4)

#define DAQMUX_BUFFER_SIZE_OFF          0x000c
#define DAQMUX_CSR_OFF                  0x0000

#define DAQMUX_CSR_HWTRIG_AUTO (1<<2)
#define DAQMUX_CSR_HWTRIG      (1<<3)
#define DAQMUX_CSR_HDR_EN      (1<<6)

static uint32_t daqmux_regs[DAQMUX_SIZE/sizeof(uint32_t)];

static uint32_t daqmuxGet(unsigned off)
{
	if ( off > DAQMUX_SIZE/sizeof(uint32_t) - 1 )
		throw "Offset out of range"; // internal error
	uint32_t rval = daqmux_regs[off/sizeof(uint32_t)];
	if ( ! hostIsLE() )
		rval = __builtin_bswap32( rval );
	return rval;
}

static void daqmuxSet(unsigned off, uint32_t val)
{
	if ( off > DAQMUX_SIZE/sizeof(uint32_t) - 1 )
		throw "Offset out of range"; // internal error
	if ( ! hostIsLE() )
		val = __builtin_bswap32( val );
	daqmux_regs[off/sizeof(uint32_t)] = val;
}


class CAdcModl {
private:
	uint32_t        dac_tbl_[DAC_TABLE_TBL_SIZE/sizeof(uint32_t)];
	unsigned        tdest_;
	unsigned        instance_;

public:
	CAdcModl(unsigned tdest, unsigned instance)
	: tdest_(tdest),
	  instance_(instance)
	{
		memset( &dac_tbl_, 0, sizeof(dac_tbl_) );
	}

	unsigned getTDEST() { return tdest_; }

	int readReg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug);
	int writeReg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug);
};


class CBSA : public CRunnable {
private:
	uint64_t        startAddr_ [BSA_CHANNELS];
	uint64_t        endAddr_   [BSA_CHANNELS];
	double          pollms_;
	struct timespec dly_;

	unsigned cachedSize_[BSA_CHANNELS];

protected:
	void updateCache();
public:
	CBSA(double pollms);

	virtual void *threadBody();

	int readReg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
	{
		if ( debug )
			printf("BSA read reg (off %lx, size %x\n", (long unsigned int)off, (unsigned)nbytes);
		/* overlapping? */
		if (    BSA_START_ADDR_OFF+BSA_START_ADDR_SIZE == BSA_END_ADDR_OFF  ) {
			throw "Not Implemented";
		} else if ( BSA_END_ADDR_OFF+BSA_END_ADDR_SIZE == BSA_START_ADDR_OFF  ) {
			throw "Not Implemented";
		} 

		if ( ! RANGE_VIOLATION(off, nbytes, BSA_START_ADDR_OFF, BSA_START_ADDR_SIZE) ) {
			off -= BSA_START_ADDR_OFF;
			memcpy( data, ((uint8_t*)&startAddr_) + off, nbytes );
			return 0;
		}
   		if ( ! RANGE_VIOLATION(off, nbytes, BSA_END_ADDR_OFF, BSA_END_ADDR_SIZE) ) {
			off -= BSA_END_ADDR_OFF;
			memcpy( data, ((uint8_t*)&endAddr_) + off, nbytes );
			return 0;
		}
		return -1;
	}

	int writeReg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
	{
		if ( debug )
			printf("BSA write reg (off %lx, size %x\n", (long unsigned int)off, (unsigned)nbytes);
		/* overlapping? */
		if (    BSA_START_ADDR_OFF+BSA_START_ADDR_SIZE == BSA_END_ADDR_OFF  ) {
			throw "Not Implemented";
		} else if ( BSA_END_ADDR_OFF+BSA_END_ADDR_SIZE == BSA_START_ADDR_OFF  ) {
			throw "Not Implemented";
		} 

		if ( ! RANGE_VIOLATION(off, nbytes, BSA_START_ADDR_OFF, BSA_START_ADDR_SIZE) ) {
			off -= BSA_START_ADDR_OFF;
			memcpy( ((uint8_t*)&startAddr_) + off, data, nbytes );
			updateCache();
			return 0;
		}
   		if ( ! RANGE_VIOLATION(off, nbytes, BSA_END_ADDR_OFF, BSA_END_ADDR_SIZE) ) {
			off -= BSA_END_ADDR_OFF;
			memcpy( ((uint8_t*)&endAddr_) + off, data, nbytes );
			updateCache();
			return 0;
		}
		return -1;
	}

	uint64_t getStart(unsigned index)
	{
		if ( index >= BSA_CHANNELS )
			throw "BSA Channel index out of range";
		uint64_t rval = startAddr_[index];
		if ( ! hostIsLE() )
			rval = __builtin_bswap64( rval );
		return rval;
	}

	uint64_t getEnd(unsigned index)
	{
		if ( index >= BSA_CHANNELS )
			throw "BSA Channel index out of range";
		uint64_t rval = endAddr_[index];
		if ( ! hostIsLE() )
			rval = __builtin_bswap64( rval );
		return rval;
	}

	void setStart(unsigned index, uint64_t val)
	{
		if ( index >= BSA_CHANNELS )
			throw "BSA Channel index out of range";
		if ( ! hostIsLE() )
			val = __builtin_bswap64( val );
		startAddr_[index] = val;
	}

	void setEnd(unsigned index, uint64_t val)
	{
		if ( index >= BSA_CHANNELS )
			throw "BSA Channel index out of range";
		if ( ! hostIsLE() )
			val = __builtin_bswap64( val );
		endAddr_[index] = val;
	}

	unsigned getSize(unsigned index)
	{
		if ( index >= BSA_CHANNELS )
			throw "BSA Channel index out of range";
		return cachedSize_[index];
	}
};

CBSA::CBSA(double pollms)
: CRunnable("BSAwaveform"),
  pollms_(pollms)
{
unsigned i;
	for ( i = 0; i < BSA_CHANNELS; i++ ) {
		setStart( i, 0 + i*BSA_SPACING );
		setEnd  ( i, getStart( i ) + BSA_INI_SIZE );
		updateCache();
	}

	dly_.tv_sec  = floor( pollms_ / 1000. );
	double ms = pollms_ - dly_.tv_sec*1000.;
	if ( ms < 0 )
		dly_.tv_nsec = 0;
	dly_.tv_nsec = ms * 1000000.;
	if ( dly_.tv_nsec >= 1000000000 )
		dly_.tv_nsec = 1000000000 - 1;

	threadStart();
}

void
CBSA::updateCache()
{
	for ( unsigned i = 0; i < BSA_CHANNELS; i++ ) {
		cachedSize_[i] = getEnd(i) - getStart(i);
	}
}


int
CAdcModl::readReg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	if ( RANGE_VIOLATION(off, nbytes, DAC_TABLE_TBL_OFF, DAC_TABLE_TBL_SIZE) )
		return -1;
	off -= DAC_TABLE_TBL_OFF;
	memcpy( data, ((uint8_t*)&dac_tbl_) + off, nbytes );
	return 0;
}

int 
CAdcModl::writeReg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	if ( RANGE_VIOLATION(off, nbytes, DAC_TABLE_TBL_OFF, DAC_TABLE_TBL_SIZE) )
		return -1;
	off -= DAC_TABLE_TBL_OFF;
	memcpy( ((uint8_t*)&dac_tbl_) + off, data, nbytes );
	return 0;
}

static int
readDaq(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	if ( RANGE_VIOLATION(off, nbytes, 0, DAQMUX_SIZE) )
		return -1;
	memcpy( data, ((uint8_t*)&daqmux_regs) + off, nbytes );
	return 0;
}

static int
writeDaq(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	if ( RANGE_VIOLATION(off, nbytes, 0, DAQMUX_SIZE) )
		return -1;
	memcpy( ((uint8_t*)&daqmux_regs) + off, data, nbytes );
	return 0;
}

static CAdcModl theModlAdc0(GENERICADC0_TDEST, 0);
static CAdcModl theModlAdc1(GENERICADC1_TDEST, 1);
static CAdcModl theModlAdc2(GENERICADC2_TDEST, 2);
static CAdcModl theModlAdc3(GENERICADC3_TDEST, 3);

static CAdcModl *theModlAdcs[]={
	&theModlAdc0,
	&theModlAdc1,
	&theModlAdc2,
	&theModlAdc3
};

static CBSA     theBSA( 1000 );

void *CBSA::threadBody()
{
struct   timespec    dly       = dly_;
unsigned             maxsz     = DAC_TABLE_TBL_SIZE + TIMING_HEADER_SIZE + STREAMBUF_HEADROOM;
std::vector<uint8_t> streambuf;

unsigned             actsz;

	streambuf.reserve( maxsz );

	for ( unsigned i = 0; i<maxsz; ++i )
		streambuf.push_back( 0 );

	while ( 1 ) {
		while ( nanosleep( &dly, &dly ) ) {
			if ( EINTR != errno ) {
				return 0;
			}
		}

		uint32_t daqmux_depth = daqmuxGet( DAQMUX_BUFFER_SIZE_OFF );
		uint32_t daqmux_csr   = daqmuxGet( DAQMUX_CSR_OFF         );

		if ( daqmux_depth && (daqmux_csr & DAQMUX_CSR_HWTRIG) ) {

			struct timespec now;

			clock_gettime( CLOCK_REALTIME, &now );

			uint32_t ts[2];
			ts[0] = now.tv_sec;
			ts[1] = now.tv_nsec;

			if ( ! hostIsLE() ) {
				ts[0] = __builtin_bswap32( ts[0] );
				ts[1] = __builtin_bswap32( ts[1] );
			}

			for ( unsigned inst=0; inst < sizeof(theModlAdcs)/sizeof(theModlAdcs[0]); ++inst ) {
				uint8_t *payload = &streambuf[0] + STREAMBUF_HEADROOM;
				memcpy( payload + TIMING_TS_OFF_HI, &ts[0], sizeof(ts[0]) );
				memcpy( payload + TIMING_TS_OFF_LO, &ts[1], sizeof(ts[1]) );

				actsz = theBSA.getSize( inst );

				if ( daqmux_depth * sizeof(uint32_t) != actsz ) {
					printf("WARNING DAQ/BSA size mismatch\n");
				}

				unsigned thdrsz = (daqmux_csr & DAQMUX_CSR_HDR_EN) ? TIMING_HEADER_SIZE : 0;

				if ( actsz < thdrsz ) {
					printf("BSA size < min; clamping\n");
					actsz = thdrsz;
				}

				if ( actsz > maxsz )
					throw "ERROR: requested size out of range";

				theModlAdcs[inst]->readReg( payload + thdrsz, actsz - thdrsz, (uint64_t)0, 0 );

				streamSend( &streambuf[0], actsz + STREAMBUF_HEADROOM, theModlAdcs[inst]->getTDEST());
			}
		}
		/* should mutex here but who cares... */
		if ( ! (daqmux_csr & DAQMUX_CSR_HWTRIG_AUTO) ) {
			daqmuxSet( DAQMUX_CSR_OFF, daqmuxGet( DAQMUX_CSR_OFF ) & ~ DAQMUX_CSR_HWTRIG );
		}
	}

	return 0;
}

static int readreg0(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc0.readReg(data, nbytes, off, debug);
}

static int writereg0(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc0.writeReg(data, nbytes, off, debug);
}

static int readreg1(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc1.readReg(data, nbytes, off, debug);
}

static int writereg1(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc1.writeReg(data, nbytes, off, debug);
}

static int readreg2(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc2.readReg(data, nbytes, off, debug);
}

static int writereg2(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc2.writeReg(data, nbytes, off, debug);
}

static int readreg3(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc3.readReg(data, nbytes, off, debug);
}

static int writereg3(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theModlAdc3.writeReg(data, nbytes, off, debug);
}

static int readregBsa(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theBSA.readReg(data, nbytes, off, debug);
}

static int writeregBsa(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	return theBSA.writeReg(data, nbytes, off, debug);
}

static struct udpsrv_range genAdc0Range(DAC_TABLE_0_ADDR, DAC_TABLE_SIZE, readreg0, writereg0, 0);
static struct udpsrv_range genAdc1Range(DAC_TABLE_1_ADDR, DAC_TABLE_SIZE, readreg1, writereg1, 0);
static struct udpsrv_range genAdc2Range(DAC_TABLE_2_ADDR, DAC_TABLE_SIZE, readreg2, writereg2, 0);
static struct udpsrv_range genAdc3Range(DAC_TABLE_3_ADDR, DAC_TABLE_SIZE, readreg3, writereg3, 0);

static struct udpsrv_range daqMuxRange (DAQMUX_ADDR, DAQMUX_SIZE, readDaq, writeDaq, 0);

static struct udpsrv_range bsaRange    (BSA_ADDR, BSA_SIZE, readregBsa, writeregBsa, 0);
