 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.

#ifndef CPSW_API_TIMEOUT_H
#define CPSW_API_TIMEOUT_H

#include <cpsw_error.h>

/*!
 * Timeout class
 */
class CTimeout {
protected:
	static const time_t INDEFINITE_S = -1;
public:
	struct timespec tv_;

	/*!
	 * No-args constructor - INDEFINITE timeout
	 */
	CTimeout()
	{
		setIndefinite();
	}

	/*!
	 * Constructor supplying microseconds.
	 */
	CTimeout(uint64_t timeout_us)
	{
		set(timeout_us);
	}

	/*!
	 * Constructor from a POSIX timespec.
	 */
	CTimeout(const struct timespec &tv)
	:tv_(tv)
	{
	}

	/*!
	 * Constructor from explicit seconds and nanoseconds
	 */
	CTimeout(time_t s, long ns)
	{
		tv_.tv_sec  = s;
		if ( ns >= 1000000000 )
			throw InvalidArgError("Timeout 'ns' must be < 1E9");
		tv_.tv_nsec = ns;
	}

	/*!
	 * Set the timeout to 'timeout_us' microseconds.
	 */
	void set(uint64_t timeout_us)
	{
		if ( timeout_us < 1000000 ) {
			tv_.tv_sec  = 0;
			tv_.tv_nsec = timeout_us * 1000;
		} else {
			tv_.tv_sec  = (timeout_us / 1000000);
			tv_.tv_nsec = (timeout_us % 1000000) * 1000;
		}
	}

	/*!
	 * Set the timeout to a POSIX timespec.
	 */
	void set(const struct timespec &tv)
	{
		tv_ = tv;
	}

	/*!
	 * Set to INDEFINITE
	 */
	void setIndefinite()
	{
		tv_.tv_sec  = INDEFINITE_S;
		tv_.tv_nsec = 0;
	}

	/*!
	 * Retrieve timeout in microseconds.
	 */
	uint64_t getUs() const
	{
		return (uint64_t)tv_.tv_sec * (uint64_t)1000000 + (uint64_t)tv_.tv_nsec/(uint64_t)1000;
	}

	/*!
	 * Is the timeout indefinite?
	 */
	bool isIndefinite() const { return tv_.tv_sec == INDEFINITE_S; }

	/*!
	 * Is the timeout zero (called function returns immediately
	 * if there is no work to do).
	 */
	bool isNone()       const { return tv_.tv_sec ==  0 && tv_.tv_nsec == 0; }

	/*!
	 * Add a timeout to THIS.
	 */
	CTimeout & operator +=(const CTimeout &rhs)
	{
		if ( ! isIndefinite() ) {
			if ( rhs.isIndefinite() )
				return (*this = rhs);
			tv_.tv_sec  += rhs.tv_.tv_sec;
			tv_.tv_nsec += rhs.tv_.tv_nsec;
			if ( tv_.tv_nsec >= 1000000000 ) {
				tv_.tv_nsec -= 1000000000;
				tv_.tv_sec  ++;
			}
		}
		return *this;
	}

	/*!
	 * Subtract a timeout from THIS.
	 */
	CTimeout & operator -=(const CTimeout &rhs)
	{
		if ( ! isIndefinite() ) {
			if ( rhs.isIndefinite() )
				return (*this = rhs);
			tv_.tv_sec  -= rhs.tv_.tv_sec;
			if ( tv_.tv_nsec < rhs.tv_.tv_nsec ) {
				tv_.tv_nsec += 1000000000;
				tv_.tv_sec  --;
			}
			tv_.tv_nsec -= rhs.tv_.tv_nsec;
		}
		return *this;
	}

	/*!
	 * Add two timeouts and return the sum
	 */
	friend CTimeout operator +(CTimeout lhs, const CTimeout &rhs)
	{
		lhs += rhs;
		return lhs;
	}

	/*!
	 * Subtract two timeouts and return the difference
	 */
	friend CTimeout operator -(CTimeout lhs, const CTimeout &rhs)
	{
		lhs -= rhs;
		return lhs;
	}

	/*!
	 * Comparison operator: lhs < rhs (lhs expires before rhs)
	 */
	friend int operator <(const CTimeout lhs, const CTimeout &rhs)
	{
		return lhs.tv_.tv_sec < rhs.tv_.tv_sec
		     || (lhs.tv_.tv_sec == rhs.tv_.tv_sec && lhs.tv_.tv_nsec < rhs.tv_.tv_nsec);
	}
};

/*!
 * Constant for 'no timeout'
 */
static const CTimeout TIMEOUT_NONE( 0, 0 );

/*!
 * Constant for 'indefinite timeout'
 */
static const CTimeout TIMEOUT_INDEFINITE;

#endif
