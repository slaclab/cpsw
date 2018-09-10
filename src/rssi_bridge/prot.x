struct PortDesc {
	short portNum;
	bool  hasRssi;
};

struct MapReq {
	int    ipAddr;
	struct PortDesc ports<>;
};

struct AddArgs {
	int    a;
	int    b;
};

struct McAddr {
	int    ipaddr;
	int    portno;
};

#ifndef NO_RSSIB_REL

program RSSIB_REL {
	version RSSIB_REL_V0 {
		void
		RSSIB_REL_ECHO(void)          = 0;

		struct McAddr
		RSSIB_REL_GETMCPORT(void)     = 1;

		struct MapReq
		RSSIB_REL_LKUP(struct MapReq) = 2;
	} = 0;
} = 0x40cafe00;

#endif

#ifndef NO_RSSIB_MAP
program RSSIB_MAP {
	version RSSIB_MAP_V0 {
		void
		RSSIB_MAP_ECHO(void)          = 0;

		struct MapReq
		RSSIB_MAP_LKUP(struct MapReq) = 1;
    } = 0;
} = 0x40cafe01;
#endif
