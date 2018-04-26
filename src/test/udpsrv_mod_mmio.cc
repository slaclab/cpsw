#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <udpsrv_mod_mmio.h>
#include <udpsrv_regdefs.h>

struct udpsrv_mmio_range : udpsrv_range {
	uint8_t            *map;
	udpsrv_mmio_range(const char *name, uint64_t start, uint64_t size, uint8_t *map);
};

struct udpsrv_range *
udpsrv_check_range(uint64_t start, uint64_t size)
{
struct udpsrv_range *r;
uint64_t             end = start + size;
	for ( r = udpsrv_ranges; r; r = r->next ) {
		uint64_t rend = r->base + r->size;
		if ( start >= r->base && start < rend    )
			return r;
		if ( end   <= rend    && end   > r->base )
			return r;
	}
	return 0;
}

static int
mmio_rd(struct udpsrv_range *r, uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
udpsrv_mmio_range *mr = reinterpret_cast<udpsrv_mmio_range*>(r);
	if ( debug )
		range_io_debug( r, 1, off, nbytes );
	memcpy(data, mr->map + off, nbytes);
	return 0;
}

static int
mmio_wr(struct udpsrv_range *r, uint8_t *data, uint32_t nbytes, uint64_t off, int debug)
{
udpsrv_mmio_range *mr = reinterpret_cast<udpsrv_mmio_range*>(r);
	if ( debug )
		range_io_debug( r, 0, off, nbytes );
	memcpy(mr->map + off, data, nbytes);
	return 0;
}

int
register_io_range_1(const char *sysfs_name_and_off)
{
char       *comma;
uint64_t    start;
char        buf[256];

	strncpy(buf, sysfs_name_and_off, sizeof(buf));

	comma = strchr( buf, ',' );

	if ( ! comma ) {
		fprintf(stderr,"register_io_range_1 -- no ',' separator found\n");
		return -1;
	}

	if ( 1 != sscanf(comma + 1, "%"SCNi64, &start) ) {
		fprintf(stderr,"register_io_range_1 -- unable to scan base address\n");
		return -1;
	}

	*comma = 0;

	return register_io_range(buf, start);
}

int
register_io_range(const char *sysfs_name, uint64_t map_start)
{
char          buf[200];
int           fd   = -1;
FILE         *f    =  0;
int           rval = -1;
uint64_t      mapsz;
udpsrv_range *r;
const char   *sla;
void         *bas;
int           mmap_flags = 0;

	// A UIO device name: /sys/class/uio/uioXYZ

	if ( strstr(sysfs_name, "/uio/") ) {

		snprintf(buf, sizeof(buf), "%s/maps/map0/size", sysfs_name);
		if ( ! (f = fopen(buf, "r") ) ) {
			fprintf(stderr,"Expected to find/open %s sysfs file -- FAILED: %s\n", buf, strerror(errno));
			goto bail;
		}

		// UIO device
		if ( 1 != fscanf(f, "%"SCNi64, &mapsz) ) {
			fprintf(stderr,"register_io_range: Unable to scan map size\n");
			goto bail;
		}

		if ( (r = udpsrv_check_range( map_start, mapsz )) ) {
			fprintf(stderr,"%s: [%"PRIx64":%"PRIx64" overlaps existing range [%"PRIx64":%"PRIx64"]\n",
				sysfs_name,
				map_start,
				mapsz,
				r->base,
				r->size);
			goto bail;
		}

		fclose( f ); f = 0;

		/* Now locate the UIO device */
		if ( ! (sla = strrchr(sysfs_name, '/')) ) {
			fprintf(stderr,"Unable to determine UIO device name from %s\n", sysfs_name);
			goto bail;
		}

		snprintf(buf, sizeof(buf), "/dev%s", sla);

	} else if ( strstr(sysfs_name, "/pci/") ) {
		// A PCI sysfs 'resourceXYZ' name (e.g., /sys/bus/pci/devices/xyz/resource4)
		struct stat st;

		if ( ::stat( sysfs_name, &st ) ) {
			fprintf(stderr,"Expected to open %s -- FAILED: %s\n", sysfs_name, strerror(errno));
			goto bail;
		}

		mapsz = st.st_size;
		strncpy(buf, sysfs_name, sizeof(buf));

	} else if ( strstr(sysfs_name,"/fakedebug/") ) {
		/* for testing this module */
		mmap_flags = MAP_ANONYMOUS;
		mapsz      = 4096;
		buf[0]     = 0;
	} else {
		fprintf(stderr,"No supported device found: %s\n", sysfs_name);
		goto bail;
	}

	if ( strlen(buf) > 0 && (fd = open(buf, O_RDWR)) < 0 ) {
		fprintf(stderr,"Unable to open %s: %s\n", buf, strerror(errno));
		goto bail;
	}

	bas = mmap( 0, mapsz, PROT_READ | PROT_WRITE, mmap_flags | MAP_SHARED, fd, 0 );

	if ( MAP_FAILED == bas ) {
		fprintf(stderr,"Unable to mmap UIO device %s: %s\n", buf, strerror(errno));
		goto bail;
	}

	close(fd); fd = -1;

	new udpsrv_mmio_range( strdup(buf), map_start, mapsz, static_cast<uint8_t*>(bas) );

	rval = 0;

bail:
	if ( f )
		fclose(f);
	if ( fd >= 0 )
		close(fd);
	return rval;	
}

udpsrv_mmio_range::udpsrv_mmio_range(const char *name, uint64_t start, uint64_t size, uint8_t *map)
:udpsrv_range(name, start, size, mmio_rd, mmio_wr, 0),
 map( map )
{
}
