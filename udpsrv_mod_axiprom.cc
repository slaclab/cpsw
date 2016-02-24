#include <udpsrv_regdefs.h>
#include <string.h>
#define _STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>

#define PROMSIZE (1024*1024)

#define MODE_32 (1<<0)

#define READ_MASK          0x00000000
#define WRITE_MASK         0x80000000

#define CMD_OFFSET         16

#define WRITE_3BYTE_CMD    (0x02 << CMD_OFFSET)
#define WRITE_4BYTE_CMD    (0x12 << CMD_OFFSET)

#define READ_3BYTE_CMD     (0x03 << CMD_OFFSET)
#define READ_4BYTE_CMD     (0x13 << CMD_OFFSET)
   
#define FLAG_STATUS_REG    (0x70 << CMD_OFFSET)
#define FLAG_STATUS_RDY    (0x80)
   
#define WRITE_ENABLE_CMD   (0x06 << CMD_OFFSET)
#define WRITE_DISABLE_CMD  (0x04 << CMD_OFFSET)

#define ADDR_ENTER_CMD     (0xB7 << CMD_OFFSET)
#define ADDR_EXIT_CMD      (0xE9 << CMD_OFFSET)

#define ENABLE_RESET_CMD   (0x66 << CMD_OFFSET)
#define RESET_CMD          (0x99 << CMD_OFFSET)
   
#define ERASE_CMD          (0xD8 << CMD_OFFSET)
#define ERASE_SIZE         (0x10000)

#define STATUS_REG_WR_CMD  (0x01 << CMD_OFFSET)
#define STATUS_REG_RD_CMD  (0x05 << CMD_OFFSET)

#define DEV_ID_RD_CMD      (0x9F << CMD_OFFSET)

#define WRITE_NONVOLATILE_CONFIG (0xB1 << CMD_OFFSET)
#define WRITE_VOLATILE_CONFIG    (0x81 << CMD_OFFSET)
#define READ_NONVOLATILE_CONFIG  (0xB5 << CMD_OFFSET)
#define READ_VOLATILE_CONFIG     (0x85 << CMD_OFFSET)

#define PGSIZE 64

static uint32_t modereg = 0;
static uint32_t addrreg = 0;
static uint32_t cmndreg = 0;

static uint8_t  datareg[PGSIZE*4] = {0};

static uint8_t  promdat[PROMSIZE] = {0};

static bool     writeEn = false;

static int readreg(uint32_t *data, uint32_t nwrds, uint32_t off, int debug)
{
	memset(data, 0, nwrds*4);

	switch ( off ) {
		case AXI_SPI_EEPROM_32BIT_MODE_OFF: *data = modereg; break;
		case AXI_SPI_EEPROM_ADDR_OFF      : *data = addrreg; break;
		case AXI_SPI_EEPROM_CMD_OFF       : *data = cmndreg; break;
		case AXI_SPI_EEPROM_DATA_OFF      :
			if ( nwrds > PGSIZE )
				nwrds = PGSIZE;
			memcpy(data, datareg, nwrds*4);
			break;
		default:
			return -1;
	}
	return 0;
}

static void erase()
{
int sz = ERASE_SIZE;

	if ( addrreg < PROMSIZE && writeEn ) {
		if ( addrreg + sz > PROMSIZE )
			sz = PROMSIZE - addrreg;
		memset( &promdat[addrreg], 0xff, sz);
	}
}

static void prog()
{
int sz = PGSIZE*4;
int i;
	if ( addrreg < PROMSIZE && writeEn ) {
		if ( addrreg + sz > PROMSIZE )
			sz = PROMSIZE - addrreg;
		for (i=0; i<sz; i++ ) {
			promdat[addrreg + i] &= datareg[i]; 
		}
	}
}

static void rdout()
{
int sz = PGSIZE*4;
int i;
	if ( addrreg < PROMSIZE ) {
		if ( addrreg + sz > PROMSIZE )
			sz = PROMSIZE - addrreg;
		for (i=0; i<sz; i++ ) {
			datareg[i] = promdat[addrreg + i];
		}
	}
}

static int mcheck(bool is32, void (*proc)())
{
	if ( is32 == !!(modereg & MODE_32) ) {
		proc();
		return 0;
	}
	return -1;
}

static int writereg(uint32_t *data, uint32_t nwrds, uint32_t off, int debug)
{
uint32_t msk = (modereg & MODE_32) ? 0xffffffff : 0x00ffffff;
uint32_t ncmd;

	if ( nwrds < 1 )
		return -1;

	switch (off) {
		case AXI_SPI_EEPROM_32BIT_MODE_OFF: modereg = *data;         break;
		case AXI_SPI_EEPROM_ADDR_OFF      : addrreg = (*data & msk); break;
		case AXI_SPI_EEPROM_CMD_OFF       :
#ifdef DEBUG
			if ( debug )
				printf("AXIPROM: set cmd: 0x%08"PRIx32"\n", *data);
#endif
			switch ( ncmd = *data ) {
				case READ_MASK | FLAG_STATUS_REG | 0x1:
					ncmd &= ~0xff;
					ncmd |= FLAG_STATUS_RDY;
				break;	

				case WRITE_MASK | ERASE_CMD | 4:
				case WRITE_MASK | ERASE_CMD | 3:
					if ( mcheck( !(ncmd & 1), erase ) ) 
						goto fault;
				break;

				case WRITE_MASK | WRITE_4BYTE_CMD | 0x104:
				case WRITE_MASK | WRITE_3BYTE_CMD | 0x103:
					if ( mcheck( !(ncmd & 1), prog ) )
						goto fault;
				break;

				case READ_MASK | READ_4BYTE_CMD | 0x104:
				case READ_MASK | READ_3BYTE_CMD | 0x103:
					if ( mcheck( !(ncmd & 1), rdout ) )
						goto fault;
				break;

				case WRITE_MASK | WRITE_ENABLE_CMD:
					writeEn = true;
				break;
					
				case WRITE_MASK | WRITE_DISABLE_CMD:
					writeEn = false;
				break;
					
				default:
					goto fault;
			}
			cmndreg = ncmd;
		break;
		case AXI_SPI_EEPROM_DATA_OFF      :
			if ( nwrds > PGSIZE )
				nwrds = PGSIZE;
			memcpy(datareg, data, nwrds*4);
			break;
		default:
			fprintf(stderr,"AXIPROM: Unrecognized offset 0x%08"PRIx32"\n", off);
			return -1;
	}
	return 0;
fault:
	fprintf(stderr,"AXIPROM: Unable to handle write command 0x%08"PRIx32"\n", *data);
	return -1;
}

static struct udpsrv_range axiprom_range(AXI_SPI_EEPROM_ADDR, AXI_SPI_EEPROM_SIZE, readreg, writereg, 0);
