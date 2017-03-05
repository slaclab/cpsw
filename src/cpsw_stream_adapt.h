#ifndef CPSW_STREAM_ADAPT_H
#define CPSW_STREAM_ADAPT_H

#include <cpsw_api_user.h>
#include <cpsw_entry_adapt.h>

class CStreamAdapt;
typedef shared_ptr<CStreamAdapt> StreamAdapt;

class CStreamAdapt : public IEntryAdapt, public virtual IStream {
public:
	CStreamAdapt(Key &k, Path p, shared_ptr<const CEntryImpl> ie);

	virtual int64_t read(uint8_t *buf, uint64_t size, const CTimeout timeout, uint64_t off);

	virtual int64_t write(uint8_t *buf, uint64_t size, const CTimeout timeout);
};

#endif
