/*
 * Copyright 2018-2023 IoT.bzh Company <jose.bollo@iot.bzh>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/*
 * localuser.c
 * -----------
 *  This source file provides NSS (Name Service Switch -see [1]-) facilities
 *  for defining a virtual host of name localuser that resolves to an address
 *  of the localhost that integrate user ID and/or application ID.
 *
 *  It defines the family  *"localuser"* of virtual hostnames as one of the
 *  below names:
 *  
 *  - localuser
 *  - localuser-UID
 *  - localuser--APPID
 *  - localuser-UID-APPID
 *  - localuser---APPID
 *  
 *  This can be summarized by the following matrix:
 *  
 *    |------------------|------------------|---------------------|-------------------|
 *    |                  | **current user** | **user of UID**     | **no user**       |
 *    |------------------|------------------|---------------------|-------------------|
 *    | **no APP**       | localuser        | localuser-UID       |                   |
 *    | **app of APPID** | localuser--APPID | localuser-UID-APPID | localuser---APPID |
 *    |------------------|------------------|---------------------|-------------------|
 *  
 *  The delivered NSS service defines one virtual host of name `localuser`
 *  that resolves to an IP address of the localhost loopback that integrates
 *  user ID.
 *  
 *  It is intended to enable distinct IP for distinct users, distinct application.
 *  
 *  The name *localuser* family is resolved to the IPv4 address range 127.128.0.0/9
 *  
 *  The delivered IPv4 address is structured as follow:
 *  
 *  ```text
 *  +--------+--------+--------+--------+
 *  :01111111:1abbcccc:dddddeee:ffffffff:
 *  +--------+--------+--------+--------+
 *  ```
 *  
 *  When `a` is `1`, the value 11 bits value `bbccccddddd` encodes the APPID
 *  and the 11 bits value `eeedddddddd` encodes the UID.
 *  This is represented by the following hostnames: `localuser--APPID`
 *  and `localuser-UID-APPID`.
 *  
 *  When `abb` is `011`, the 20 bits value `ccccdddddeeeffffffff` encodes the APPID.
 *  This is represented by the following hostnames: `localuser---APPID`.
 *  
 *  When `abb` is `010`, the 20 bits value `ccccdddddeeeffffffff` encodes the UID.
 *  This is represented by the following hostnames: `localuser`
 *  and `localuser-UID`.
 *  
 *  The values `000` and `001` of `abb` are reserved for futur use.
 *  
 *  Examples:
 *  
 *  ```text
 *  localuser      => 127.128.0.0   (when user has UID = 0)
 *  localuser      => 127.128.3.233 (when user has UID = 1001)
 *  localuser-1024 => 127.128.4.0   (for any user)
 *  ```
 *  
 *  The service also provides the reverse resolution.
 * links
 * -----
 *  [1] https://www.gnu.org/software/libc/manual/html_node/Name-Service-Switch.html
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <nss.h>

/* string for "localuser" */
static const char localuser[] = "localuser";
static const char separator = '-';
#define MAXNAMELEN 40

/* defines the length of adresses */
static const int lenip4 = 4;
static const int lenip6 = 16;

/* masks for IPv4 adresses */
static const uint32_t prefix_mask  = 0xff800000u; /* 255.128.0.0 */
static const uint32_t prefix_value = 0x7f800000u; /* 127.128.0.0 */

static const uint32_t locusr_both_ids_mask         = 0x7fc00000u;
static const uint32_t locusr_both_ids_prefix       = 0x7fc00000u;
static const uint32_t locusr_both_ids_uid_max      = 0x000007ffu;
static const uint32_t locusr_both_ids_uid_mask     = 0x000007ffu;
static const uint32_t locusr_both_ids_appid_max    = 0x000007ffu;
static const uint32_t locusr_both_ids_appid_mask   = 0x000007ffu;
static const uint8_t  locusr_both_ids_appid_shift  = 11;

static const uint32_t locusr_appid_only_mask       = 0x7ff00000u;
static const uint32_t locusr_appid_only_prefix     = 0x7fb00000u;
static const uint32_t locusr_appid_only_appid_max  = 0x000fffffu;
static const uint32_t locusr_appid_only_appid_mask = 0x000fffffu;

static const uint32_t locusr_uid_only_mask         = 0x7ff00000u;
static const uint32_t locusr_uid_only_prefix       = 0x7fa00000u;
static const uint32_t locusr_uid_only_uid_max      = 0x000fffffu;
static const uint32_t locusr_uid_only_uid_mask     = 0x000fffffu;

/* structure for coding/decoding */
struct lud
{
	unsigned has_uid: 1;	/* has a uid */
	unsigned has_appid: 1;	/* has a appid */
	uint32_t uid;		/* uid if any */
	uint32_t appid;		/* appid if any */
	uint32_t ipv4;		/* IPv4 representation */
	uint32_t len;		/* name length */
	char name[MAXNAMELEN];	/* name value */
};

/* read a 32 bits integer. returns its length in character or -1 on overflow */
static int read_u32(const char *str, uint32_t *val)
{
	char c;
	int p;
	uint32_t a, b;

	a = 0;
	c = str[p = 0];
	while ('0' <= c && c <= '9') {
		b = (a << 3) + (a << 1) + (uint32_t)(c - '0');
		if (b < a)
			return -1; /* overflow */
		a = b;
		c = str[++p];
	}
	*val = a;
	return p;
}

/* write a 32 bits integer and return the count of char writen */
static unsigned write_u32(char *str, uint32_t val)
{
	unsigned w, l, u;
	char c;

	l = w = 0;
	while (val > 9) {
		str[w++] = (char)('0' + val % 10);
		val /= 10;
	}
	str[w++] = (char)('0' + val);
	u = w;
	while (--u > l) {
		c = str[u];
		str[u] = str[l];
		str[l++] = c;
	}
	return w;
}

static void encode_name(struct lud *lud)
{
	unsigned i;

	/* encode "localuser-" */
	i = (int)(sizeof localuser - 1);
	memcpy(lud->name, localuser, i);

	/* encode the UID if needed */
	if (!lud->has_uid) {
		lud->name[i++] = separator;
		lud->name[i++] = separator;
	} else if (lud->uid != (uint32_t)getuid()) {
		lud->name[i++] = separator;
		i += write_u32(&lud->name[i], lud->uid);
	} else if (lud->has_appid)
		lud->name[i++] = separator;

	/* encode the APPID if needed */
	if (lud->has_appid) {
		lud->name[i++] = separator;
		i += write_u32(&lud->name[i], lud->appid);
	}

	/* finish */
	lud->len = i;
	lud->name[i] = 0;
}

/*
 * Decode the name if valid and stores its ip in lud
 * Returns:
 *   - 0: not a localuser name
 *   - 1: valid local user name
 *   - -1: invalid localuser name
 *   - -2: out of range localuser name
 */
static int decode_name(const char *name, struct lud *lud)
{
	int i, r;
	uint32_t adr;

	/* test the prefix of the name */
	i = (int)(sizeof localuser - 1);
	if (strncmp(name, localuser, (size_t)i) != 0)
		return 0;

	/* prefix matches "localuser" */
	if (!name[i]) {
		/* terminated string: "localuser" */
		lud->has_uid = 1;
		lud->uid = (uint32_t)getuid(); /* use current UID */
		lud->has_appid = 0;
	} else {
		/* should be "localuser-..." */
		if (name[i] != separator)
			return -1;
		/* found "localuser-..." */
		if (name[++i] == separator) {
			/* found "localuser--..." */
			if (name[++i] == separator) {
				/* found "localuser---..." */
				++i;
				lud->has_uid = 0;
			} else {
				/* found "localuser--x.." */
				lud->uid = (uint32_t)getuid(); /* use current UID */
				lud->has_uid = 1;
			}
			lud->has_appid = 1;
		} else {
			/* found "localuser-X..." with X not being a dash */
			r = read_u32(&name[i], &lud->uid);
			if (r <= 0)
				return -1;
			/* found "localuser-UID..." */
			i += r;
			lud->has_uid = 1;
			if (name[i] != separator)
				lud->has_appid = 0;
			else {
				/* found "localuser-UID-..." */
				i++;
				lud->has_appid = 1;
			}
		}
		/* look if appid must be read */
		if (lud->has_appid) {
			/* found "localuser-[UID|-]-..."  */
			r = read_u32(&name[i], &lud->appid);
			if (r <= 0)
				return -1;
			/* found "localuser-[UID|-]-APPID..."  */
			i += r;
		}
		/* the name should be finished now */
		if (name[i])
			return -1;
	}

	/* encode the address */
	if (lud->has_appid && lud->has_uid) {
		/* case of UID and APPID */
		if (lud->appid > locusr_both_ids_appid_max)
			return -2;
		if (lud->uid > locusr_both_ids_uid_max)
			return -2;
		adr = (uint32_t)(locusr_both_ids_prefix
				 | (lud->appid << locusr_both_ids_appid_shift)
				 | lud->uid);
	} else if (lud->has_appid) {
		/* case of only APPID */
		if (lud->appid > locusr_appid_only_appid_max)
			return -2;
		adr = (uint32_t)(locusr_appid_only_prefix | lud->appid);
	} else {
		/* case of only UID */
		if (lud->uid > locusr_uid_only_uid_max)
			return -2;
		adr = (uint32_t)(locusr_uid_only_prefix | lud->uid);
	}
	lud->ipv4 = htonl(adr);

	encode_name(lud);
	return 1;
}

/*
 * Decode the ipv4 if valid and stores its data in lud
 * Returns:
 *   - 0: not a localuser ip
 *   - 1: valid local user ip
 *   - -1: invalid localuser ip
 */
static int decode_ipv4(uint32_t ipv4, struct lud *lud)
{
	uint32_t adr;

	/* check the address range */
	adr = ntohl(ipv4);
	if ((adr & prefix_mask) != prefix_value)
		return 0;

	/* decode */
	lud->ipv4 = ipv4;
	if ((adr & locusr_both_ids_mask) == locusr_both_ids_prefix) {
		lud->has_uid = 1;
		lud->has_appid = 1;
		lud->uid = adr & locusr_both_ids_uid_mask;
		if (lud->uid > locusr_both_ids_uid_max)
			return -1;
		lud->appid = (adr >> locusr_both_ids_appid_shift) & locusr_both_ids_appid_mask;
		if (lud->appid > locusr_both_ids_appid_max)
			return -1;
	} else  if ((adr & locusr_appid_only_mask) == locusr_appid_only_prefix) {
		lud->has_uid = 0;
		lud->has_appid = 1;
		lud->appid = adr & locusr_appid_only_appid_mask;
		if (lud->appid > locusr_appid_only_appid_max)
			return -1;
	} else if ((adr & locusr_uid_only_mask) == locusr_uid_only_prefix) {
		lud->has_uid = 1;
		lud->has_appid = 0;
		lud->uid = adr & locusr_uid_only_uid_mask;
		if (lud->uid > locusr_uid_only_uid_max)
			return -1;
	} else {
		/* reserved address */
		return -1;
	}

	encode_name(lud);
	return 1;
}

/* fill the output entry */
static enum nss_status fillent(
	struct lud *lud,
	int af,
	struct hostent *result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop)
{
	uint32_t *bufip;
	int len, alen;

	/* check the family */
	if (af == AF_INET)
		len = lenip4;
	else if (af == AF_INET6)
		len = lenip6;
	else {
		*errnop = EINVAL;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}

	/* fill aliases and addr_list */
	alen = 1 + lud->len;
	if (buflen < (2 * sizeof result->h_aliases[0]) + alen + len) {
		*errnop = ERANGE;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_TRYAGAIN;
	}

	/* fill the result */
	result->h_addrtype = af;
	result->h_length = len;
	result->h_addr_list = (char**)buffer;
	result->h_addr_list[0] = (char*)&result->h_addr_list[2];
	result->h_name = &result->h_addr_list[0][len];
	memcpy(result->h_name, lud->name, alen);
	result->h_aliases = &result->h_addr_list[1];
	result->h_addr_list[1] = NULL;
	bufip = (uint32_t*)result->h_addr_list[0];
	if (af == AF_INET6) {
		*bufip++ = 0;
		*bufip++ = 0;
		*bufip++ = htonl(0xffff);
	}
	*bufip = lud->ipv4;

	return NSS_STATUS_SUCCESS;
}

/* gethostbyname2 implementation for NSS */
enum nss_status _nss_localuser_gethostbyname2_r(
	const char *name,
	int af,
	struct hostent *result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop)
{
	struct lud lud;

	/* decode the name */
	if (decode_name(name, &lud) <= 0) {
		*h_errnop = HOST_NOT_FOUND;
		return NSS_STATUS_NOTFOUND;
	}

	/* set default family to IPv4 */
	if (af == AF_UNSPEC)
		af = AF_INET;

	/* fill the result */
	return fillent(&lud, af, result, buffer, buflen, errnop, h_errnop);
}

/* use gethostbyname2 implementation */
enum nss_status _nss_localuser_gethostbyname_r(
	const char *name,
	struct hostent *result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop)
{
	return _nss_localuser_gethostbyname2_r(name,
					       AF_UNSPEC,
					       result,
					       buffer, buflen, errnop,
					       h_errnop);
}

/* get the name of the address */
enum nss_status _nss_localuser_gethostbyaddr_r(
	const void *addr,
	int len,
	int af,
	struct hostent *result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop)
{
	struct lud lud;
	const uint32_t *bufip = (const uint32_t*)addr;
	int check;

	/* set default family */
	if (af == AF_UNSPEC) {
		if (len == lenip4)
			af = AF_INET;
		else if (len == lenip6)
			af = AF_INET6;
	}

	/* pre process of ipv6 */
	if (af == AF_INET6 && len == lenip6)
		check = (*bufip++ == 0 && *bufip++ == 0 && *bufip++ == htonl(0xffff));
	else
		check = (af == AF_INET && len == lenip4);

	if (check && decode_ipv4(*bufip, &lud) == 1)
		return fillent(&lud, af, result, buffer, buflen, errnop, h_errnop);

	*errnop = EINVAL;
	*h_errnop = NO_RECOVERY;
	return NSS_STATUS_NOTFOUND;
}
