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

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <inttypes.h>

class Num {

	typedef uint8_t NormBits;

private:
	double   val_;
    double   norm_;
    bool     isSigned_;
	uint64_t off_;
public:


	void set(double   dval)
	{
		val_ = dval;
	}

	double getVal() const
	{
		return val_;
	}

    void set(uint32_t ival)
	{
		if ( isSigned_ ) {
			val_ = (double)(int32_t)ival/norm_;
		} else {
			val_ = (double)ival/norm_;
		}
	}

	uint32_t get() const
	{
	int64_t v = val_*norm_;
		if ( isSigned_ ) {
			if ( v < -(((int64_t)1)<<31) )
				v = -(((int64_t)1)<<31);
			else if ( v > (int64_t)0x7fffffff )
				v = (int64_t)0x7fffffff;
		} else {
			if ( v < 0 )
				v = (int64_t)0;
			else if ( v > (int64_t)0xffffffff )
				v = (int64_t)0xffffffff;
		}
		return (uint32_t)v;
	}

	Num(double val, NormBits normBits, bool isSigned, uint64_t off)
	: val_(val),
	  norm_( (double)((uint64_t)1<<normBits) ),
	  isSigned_(isSigned),
	  off_(off)
	{
	}

	int readReg(uint8_t **data, uint32_t *nbytes, uint64_t *off, int debug)
	{
	int rval = 0;
		if ( *off < off_ || *off >= off_ + sizeof(uint32_t) )
			return rval;

		if ( debug ) {
			printf("readReg: off 0x%"PRIx64" off_ 0x%"PRIx64" nbytes %"PRId32"\n", *off, off_, *nbytes);
		}

		uint32_t val = get();

		for (unsigned i=0; off_ + i < *off; i++ ) {
			val >>= 8;
		}

		while ( *off < off_  + sizeof(uint32_t) && *nbytes > 0 ) {
			**data = (val & 0xff);
			val >>= 8;
			(*data)++;
			(*nbytes)--;
			(*off)++;
			rval++;
		}
		return rval;
	}

	int writeReg(uint8_t **data, uint32_t *nbytes, uint64_t *off, int debug)
	{
	int rval = 0;

		if ( *off < off_ || *off >= off_ + sizeof(uint32_t) )
			return rval;

		if ( debug ) {
			printf("writeReg: off 0x%"PRIx64" off_ 0x%"PRIx64" nbytes %"PRId32"\n", *off, off_, *nbytes);
		}

		uint32_t oldval = get();
		uint32_t newval = 0;

		unsigned startbit = 0;

		for (unsigned i=0; off_+i < *off; i++ ) {
			startbit += 8;
		}

		unsigned endbit = startbit;

		while ( *off < off_  + sizeof(uint32_t) && *nbytes > 0 ) {
			newval |= ((**data) << endbit);
			(*data)++;
			(*nbytes)--;
			(*off)++;
			endbit += 8;
			rval++;
		}
		uint32_t mask = (1<<startbit) - 1;
		if ( endbit < 32 )
			mask |= ~ ((1<<endbit)-1);

		set( (oldval & mask) | newval );

		return rval;
	}

};

class CPendSim: public CRunnable {
private:
	Num av_;          // viscous damping coefficient, normalized to angular inertia 'm l' of pendulum
	Num af_;          // kinetic damping coefficient (Ff_kin = af_ * Fnormal)
	Num w2_;          // g/l (wo^2)
	Num nu_;          // ratio of mass of pendulum / mass of cart
    Num l_;           // pendulum length, x runs from -1..1 (hard limits)
	Num x_;
	Num phi_;
    Num iv_;          // initial velocity
    Num Fe_;          // external horizontal force applied to cart
	Num x_rb_;
	Num phi_rb_;
	Num t_rb_;
    double t_;        // time
    double h_;        // RK- stepsize
    double state_[4]; // [ phi, dphi/dt, x, dx/dt ]
	double lim_;
    double pollms_;   // simulated time vs real time (update rate)
	struct timespec dly_;

	CMtx   mtx_;

public:
	void initState()
	{
	CMtx::lg guard( &mtx_ );

		lim_      = 1./l_.getVal();
		state_[0] = phi_.getVal()*2.*M_PI;
		state_[1] = 0.;
		state_[2] = x_.getVal()/l_.getVal();
		state_[3] = iv_.getVal()/l_.getVal();
printf("InitState: %g %g %g %g %g\n", state_[0], state_[1], state_[2], state_[3], Fe_.getVal());
		t_        = 0.;
	}

	CPendSim(double pollms)
	: CRunnable("pendsim"),
	  av_    ( .050, 32, false, PENDULUM_SIMULATOR_AV_OFF),
	  af_    ( .000, 32, false, PENDULUM_SIMULATOR_AF_OFF),
	  w2_    (2.000, 16, false, PENDULUM_SIMULATOR_W2_OFF),
	  nu_    ( .800, 16, false, PENDULUM_SIMULATOR_NU_OFF),
	  l_     ( .500, 16, false, PENDULUM_SIMULATOR_LE_OFF),

	  x_     ( .000, 31, true,  PENDULUM_SIMULATOR_DX_OFF),
	  phi_   ( .490, 31, true,  PENDULUM_SIMULATOR_PH_OFF), // normalized to 2*Pi
	  iv_    ( .200, 15, true,  PENDULUM_SIMULATOR_VE_OFF),
	  Fe_    ( .000, 15, true,  PENDULUM_SIMULATOR_FE_OFF),

	  x_rb_  ( .000, 31, true,  PENDULUM_SIMULATOR_DX_RB_OFF),
	  phi_rb_( .490, 31, true,  PENDULUM_SIMULATOR_PH_RB_OFF), // normalized to 2*Pi
	  t_rb_  ( .000,  8, false, PENDULUM_SIMULATOR_T_RB_OFF),
	  t_     ( .0 ),
	  h_     ( .0125 ),
	  pollms_(pollms)
	{
		dly_.tv_sec  = floor( pollms_ / 1000. );
		double ms = pollms_ - dly_.tv_sec*1000.;
		if ( ms < 0 )
			dly_.tv_nsec = 0;
		dly_.tv_nsec = ms * 1000000.;
		if ( dly_.tv_nsec >= 1000000000 )
			dly_.tv_nsec = 1000000000 - 1;
		initState();
		threadStart();
	}

	int readreg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
	{
	int rval = 0;
		rval += av_.readReg(   &data, &nbytes, &off, debug );
		rval += af_.readReg(   &data, &nbytes, &off, debug );
		rval += w2_.readReg(   &data, &nbytes, &off, debug );
		rval += nu_.readReg(   &data, &nbytes, &off, debug );
		rval += Fe_.readReg(   &data, &nbytes, &off, debug );
		rval += iv_.readReg(   &data, &nbytes, &off, debug );
		rval += l_.readReg(    &data, &nbytes, &off, debug );
		rval += x_.readReg(    &data, &nbytes, &off, debug );
		rval += phi_.readReg(  &data, &nbytes, &off, debug );

		if ( off >= PENDULUM_SIMULATOR_PH_RB_OFF && off < PENDULUM_SIMULATOR_PH_RB_OFF + sizeof(uint32_t) ) {
			CMtx::lg guard( &mtx_ );
			phi_rb_.set( state_[0]/2./M_PI       );
			x_rb_.set  ( state_[2] * l_.getVal() );
			t_rb_.set  ( t_                      );
		}
		rval += x_rb_.readReg(    &data, &nbytes, &off, debug );
		rval += phi_rb_.readReg(  &data, &nbytes, &off, debug );
		rval += t_rb_.readReg(    &data, &nbytes, &off, debug );

		return rval;
	}

	int writereg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
	{
	int rval = 0;
		rval += av_.writeReg(   &data, &nbytes, &off, debug );
		rval += af_.writeReg(   &data, &nbytes, &off, debug );
		rval += w2_.writeReg(   &data, &nbytes, &off, debug );
		rval += nu_.writeReg(   &data, &nbytes, &off, debug );
		rval += Fe_.writeReg(   &data, &nbytes, &off, debug );
		rval += iv_.writeReg(   &data, &nbytes, &off, debug );
		rval += l_.writeReg(    &data, &nbytes, &off, debug );
		rval += x_.writeReg(    &data, &nbytes, &off, debug );

	int wrotePhi = phi_.writeReg(  &data, &nbytes, &off, debug );

		if ( wrotePhi ) {
			initState();
			rval += wrotePhi;
		}

		return rval;
	}

	virtual void ydot(double t, double *dotState, double *state);

	virtual void rkSteplet(double *res, double *inp, double step)
	{
		res[0] = state_[0] + inp[0]*step;
		res[1] = state_[1] + inp[1]*step;
		res[2] = state_[2] + inp[2]*step;
		res[3] = state_[3] + inp[3]*step;
	}

	virtual void vaddTo(double *res, double *inc)
	{
		res[0] += inc[0];
		res[1] += inc[1];
		res[2] += inc[2];
		res[3] += inc[3];
	}

	virtual void rk4()
	{
	double k1[4];
	double k2[4];
	double k3[4];
	double k4[4];
	double tmp[4];

		ydot( t_        , k1, state_ );

		rkSteplet( tmp, k1, h_/2. );

		ydot( t_ + h_/2., k2, tmp         );

		rkSteplet( tmp, k2, h_/2. );

		ydot( t_ + h_/2., k3, tmp         );

		rkSteplet( tmp, k3, h_ );

		ydot( t_ + h_   , k4, tmp         );

		vaddTo(k2, k3); // k2 = k2 + k3
		vaddTo(k1, k4); // k1 = k1 + k4
		vaddTo(k1,k2);  // k1 = k1 + k4 + k2 + k3
		vaddTo(k1,k2);  // k1 = k1 + k4 + 2*(k2 + k3)

		rkSteplet( state_, k1, h_/6. );

		while ( state_[0] >= M_PI )
			state_[0] -= 2.*M_PI;
		while ( state_[0] < -M_PI )
            state_[0] += 2.*M_PI;

		t_ += h_;
	}

	virtual void *threadBody();
};

static void encodeDblLE(uint8_t *buf, double v)
{
int64_t vi = v*4294967296.; // 2^32
	for (unsigned i=0; i<sizeof(vi); i++) {
		*buf++ = vi & 0xff;
		vi >>= 8;
	}
}

void *CPendSim::threadBody()
{
struct   timespec dly = dly_;
uint8_t  streambuf[8+3*8];

	while ( 1 ) {
		while ( nanosleep( &dly, &dly ) ) {
			if ( EINTR != errno ) {
				return 0;
			}
		}

		{
		CMtx::lg guard( &mtx_ );
			for (unsigned k=0; k<10; k++ )
				rk4();

			encodeDblLE(streambuf + 8, t_                   );
			encodeDblLE(streambuf +16, state_[0]/2./M_PI    );
			encodeDblLE(streambuf +24, state_[2]*l_.getVal());

			//printf("%g - %g\n", state_[0], state_[2]*l_.getVal());
		}

		streamSend(streambuf, sizeof(streambuf), PENDULUM_SIMULATOR_TDEST);
	}

	return 0;
}

void CPendSim::ydot(double t, double *dotState, double *state)
{
	double sphi = sin( state[0] );
	double cphi = cos( state[0] );
	double A    = w2_.getVal()*sphi - state[1]*state[1];
	double B    = cphi;
	double Kext;

	// Fext normalized to (Ml), dx normalized to l -> elastic coeff. normalized to M
	if ( state[2] < -lim_ ) {
		Kext = -100. * (state[2] + lim_); // hit negative limit
	} else if ( state[2] > lim_ ) {
		Kext = -100. * (state[2] - lim_); // hit positive limit
	} else {
		Kext = Fe_.getVal();
	}

	double C = (Kext*w2_.getVal() - sphi * av_.getVal() * state[1]) * nu_.getVal();
	double D = - cphi * nu_.getVal();
	double dphiSign = state[1] > 0. ? 1. : (state[1] < 0. ? -1. : 0.);
	double E = - dphiSign * sphi * af_.getVal() * nu_.getVal();

	double a = A + B*C;
	double b = B*D-1.0;
	double c = B*E;
	double Kn;
	if ( b+c < 0. ) {
		if ( b-c >= 0. )
			throw "pendsim: Friction model violation -- b+c < 0., b-c >= 0.; two or no solution(s)";
		else
			Kn = - a / ( a > 0. ? b + c : b - c );
	} else if ( b+c > 0. ) {
		if ( b-c <= 0. )
			throw "pendsim: Friction model violation -- b+c > 0., b-c <= 0.; two or no solution(s)";
		else
			Kn = - a / ( a < 0. ? b + c : b - c );
	} else {
		throw "pendsim: Friction model violation -- b+c == 0.!";
	}
	double xdotdot   = C + D*Kn + E * fabs(Kn);
	double phidotdot = - w2_.getVal() * cphi - av_.getVal() * state[1] - dphiSign * af_.getVal() * fabs(Kn) + sphi * xdotdot;

    //printf("phi %8.5g pos %8.5g\n", state[0], state[2]);

	dotState[0] = state[1];
	dotState[1] = phidotdot;
	dotState[2] = state[3];
	dotState[3] = xdotdot;
}

static CPendSim theSim(50.);

static int readreg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	theSim.readreg(data, nbytes, off, debug);
	return 0; // indicates success
}

static int writereg(uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
	theSim.writereg(data, nbytes, off, debug);
	return 0; // indicates success
}


static struct udpsrv_range pendsim_range(PENDULUM_SIMULATOR_ADDR, PENDULUM_SIMULATOR_SIZE, readreg, writereg, 0);
