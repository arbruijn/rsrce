
/* Resource header */
struct reshdr {
	uint32_t dofs, mofs, dlen, mlen;
};

/* Resource map header */
struct resmaphdr {
	uint8_t reserved[22];
	uint16_t attr;
	uint16_t tlistofs;
	uint16_t nlistofs;
	uint16_t tnum;
};

/* Resource type list entry */
struct restype {
	restype_t type;
	uint16_t rnum;
	uint16_t refofs;
};

/* Resource reference list entry */
struct resref {
	uint16_t id;
	uint16_t nameofs;
	unsigned int attr:8, dataofs:24;
	uint32_t reserved;
};

/* Resource name list entry */
struct resname {
	uint8_t len;
	char name[0];
};

/* Resource data */
struct resdata {
	uint32_t len;
	uint8_t data[0];
};

/* Useful macros for the 3-bytes data offset */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ntoh3(a) (ntohl(a) >> 8)
#define hton3(a) (htonl(a) >> 8)
#else
#define ntoh3(a) (a)
#define hton3(a) (a)
#endif

