/*
 * Copyright 2018 IoT.bzh <jose.bollo@iot.bzh>
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
 *  of the localhost that integrate user ID.
 *
 *  The name "localuser" is resolved to the IPv4 address 127.x.y.z
 *  where x.y.z resolves to the current user UID = 65536*(x - 128) + 256*y + z
 *
 *  The name "localuser-UID" is resolved to the address 127.x.y.z
 *  where UID = 65536*(x - 128) + 256*y + z
 *
 *  Allowed UID are from 0 to 4194303 included.
 *
 *  Examples:
 *    localuser      => 127.128.0.0   (when UID = 0)
 *    localuser      => 127.128.3.233 (when UID = 1001)
 *    localuser-1024 => 127.128.4.0   (always)
 *
 *  This module provides the reverse resolution.
 *
 *  This module provides a value for IPv6: it translate to a IPv4-mapped IPv6 address
 *  because IPv6 lakes of loopback range.
 *
 *  Example: localuser-1024 => ::ffff:127.128.4.0
 *
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

/* defines the length of adresses */
static const int lenip4 = 4;
static const int lenip6 = 16;

/* masks for IPv4 adresses */
static const uint32_t prefix_mask  = 0xffc00000u; /* 255.192.0.0 */
static const uint32_t prefix_value = 0x7f800000u; /* 127.128.0.0 */
static const uint32_t locusr_mask  = 0x003fffffu; /* 0.63.255.255 */

/* return the IPv4 localuser address for 'uid' */
static uint32_t get_localuser(uint32_t uid)
{
	uint32_t adr = (uint32_t)(prefix_value | (locusr_mask & uid));
	return htonl(adr);
}

/* is 'ip' a localuser IPv4 address ? */
static int is_localuser(uint32_t ip)
{
	return prefix_value == (ntohl(ip) & prefix_mask);
}

/* return the user of the localuser IPv4 'ip' */
static uint32_t uid_of_localuser(uint32_t ip)
{
	return (ntohl(ip) & locusr_mask);
}

/* put in 'buffer' the IPv4 localuser address for 'uid' */
static void getIPv4(uint32_t *buffer, uint32_t uid)
{
	buffer[0] = get_localuser(uid);
}

/* is 'buffer' pointing a localuser IPv4 address ? */
static int isIPv4(const uint32_t *buffer)
{
	return is_localuser(buffer[0]);
}

/* return the user of the localuser IPv4 pointed by 'buffer' */
static uint32_t uidIPv4(const uint32_t *buffer)
{
	return uid_of_localuser(buffer[0]);
}

/* put in 'buffer' the IPv6 localuser address for 'uid' */
static void getIPv6(uint32_t *buffer, uint32_t uid)
{
	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = htonl(0xffff);
	buffer[3] = get_localuser(uid);
}

/* is 'buffer' pointing a localuser IPv6 address ? */
static int isIPv6(const uint32_t *buffer)
{
	return buffer[0] == 0 && buffer[1] == 0
	    && buffer[2] == htonl(0xffff) && is_localuser(buffer[3]);
}

/* return the user of the localuser IPv6 pointed by 'buffer' */
static uint32_t uidIPv6(const uint32_t *buffer)
{
	return uid_of_localuser(buffer[3]);
}

/* fill the output entry */
static enum nss_status fillent(
	const char *name,
	int af,
	struct hostent *result,
	char *buffer,
	size_t buflen,
	int *errnop,
	int *h_errnop,
	uint32_t uid)
{
	int alen = 1 + (int)strlen(name);
	int len = af == AF_INET ? lenip4 : lenip6;

	/* check the family */
	if (af != AF_INET && af != AF_INET6) {
		*errnop = EINVAL;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_UNAVAIL;
	}

	/* fill aliases and addr_list */
	if (buflen < 2 * sizeof result->h_aliases[0] + alen + len) {
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
	result->h_aliases = &result->h_addr_list[1];
	result->h_addr_list[1] = NULL;
	(af == AF_INET ? getIPv4 : getIPv6)((uint32_t*)result->h_addr_list[0], uid);
	memcpy(result->h_name, name, alen);

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
	int valid;
	uint32_t uid;
	const char *i;
	char c;

	/* test the name */
	valid = !strncmp(name, localuser, sizeof localuser - 1);
	if (valid) {
		c = name[sizeof localuser - 1];
		if (!c) {
			/* terminated string: use current UID */
			uid = (uint32_t)getuid();
		} else if (c != separator) {
			valid = 0;
		} else {
			/* has a uid specification */
			i = &name[sizeof localuser];
			c = *i;
			valid = '0' <= c && c <= '9';
			if (valid) {
				uid = (uint32_t)(c - '0');
				while ((c = *++i) && (valid = '0' <= c && c <= '9')) {
					uid = (uid << 3) + (uid << 1) + (uint32_t)(c - '0');
				}
				if (valid)
					valid = uid == (uid & locusr_mask);
			}
		}
	}
	if (!valid) {
		*h_errnop = HOST_NOT_FOUND;
		return NSS_STATUS_NOTFOUND;
	}

	/* set default family to IPv4 */
	if (af == AF_UNSPEC)
		af = AF_INET;

	/* fill the result */
	return fillent(name, af, result, buffer, buflen, errnop, h_errnop, uid);
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
	char c, name[40 + sizeof localuser];
	uint32_t uid, x;
	int l, u;

	/* set default family */
	if (af == AF_UNSPEC) {
		if (len == lenip4)
			af = AF_INET;
		else if (len == lenip6)
			af = AF_INET6;
	}

	/* check whether the IP comforms to localuser */
	if (af == AF_INET && len == lenip4 && isIPv4((const uint32_t*)addr)) {
		/* yes, it's a IPv4, get the uid */
		uid = uidIPv4((const uint32_t*)addr);
	} else if (af == AF_INET6 && len == lenip6 && isIPv6((const uint32_t*)addr)) {
		/* yes, it's a IPv6, get the uid */
		uid = uidIPv6((const uint32_t*)addr);
	} else {
		/* no */
		/* fail */
		*errnop = EINVAL;
		*h_errnop = NO_RECOVERY;
		return NSS_STATUS_NOTFOUND;
	}

	/* build the name */
	memcpy(name, localuser, sizeof localuser - 1);
	if (uid == (uint32_t)getuid())
		name[sizeof localuser - 1] = 0;
	else {
		x = uid;
		name[sizeof localuser - 1] = separator;
		l = u = (int)sizeof localuser;
		do {
			name[u++] = (char)('0' + x % 10);
			x /= 10;
		} while(x);
		name[u--] = 0;
		while (u > l) {
			c = name[u];
			name[u--] = name[l];
			name[l++] = c;
		}
	}
	/* fill the result */
	return fillent(name, af, result, buffer, buflen, errnop, h_errnop, uid);
}
