/*
 * Copyright 2018-2022 IoT.bzh Company <jose.bollo@iot.bzh>
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
#include <stdio.h>
#include <stdint.h>
#include <netdb.h>

void dumphostent(char *tag, char *arg, struct hostent *h)
{
	printf("\n----------------- %s %s\n", tag, arg);
	if (h) {
		printf("name: %s\n", h->h_name);
		if (h->h_addrtype == AF_INET)
			printf("ipv4: %d.%d.%d.%d\n",
				(int)(unsigned char)h->h_addr_list[0][0],
				(int)(unsigned char)h->h_addr_list[0][1],
				(int)(unsigned char)h->h_addr_list[0][2],
				(int)(unsigned char)h->h_addr_list[0][3]);
		else if (h->h_addrtype == AF_INET6)
			printf("ipv6: %08lx.%08lx.%08lx.%08lx\n",
				(long)ntohl(((uint32_t*)h->h_addr_list[0])[0]),
				(long)ntohl(((uint32_t*)h->h_addr_list[0])[1]),
				(long)ntohl(((uint32_t*)h->h_addr_list[0])[2]),
				(long)ntohl(((uint32_t*)h->h_addr_list[0])[3]));
	} else {
		printf("NULL!\n");
	}
	printf("\n");
}

int main(int ac, char **av)
{
	struct hostent *h;

	while (*++av) {
		h = gethostbyname2(*av, AF_INET);
		dumphostent("name->addr", *av, h);

		if (h) {
			h = gethostbyaddr(h->h_addr_list[0], h->h_length, h->h_addrtype);
			dumphostent("addr->name", *av, h);
		}

		h = gethostbyname2(*av, AF_INET6);
		dumphostent("name->addr", *av, h);

		if (h) {
			h = gethostbyaddr(h->h_addr_list[0], h->h_length, h->h_addrtype);
			dumphostent("addr->name", *av, h);
		}
	}
	return 0;
}

