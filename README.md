# nss-localuser

- [License](#license)
- [Overview](#overview)
- [Install](#documentation)

## License

This code is published with the license MIT (see LICENSE.txt) for details.

## Overview

`nss-localuser` is a plugin for the GNU Name Service Switch (NSS)
functionality of the GNU C Library (`glibc`) providing host name
resolution for *"localuser"* family of virtual hostnames.

The delivered NSS service defines one virtual host of name `localuser`
that resolves to an IP address of the localhost loopback that integrates
user ID. It is intended to enable distinct IP for distinct users.

The name "localuser" is resolved to the IPv4 address 127.x.y.z
where x.y.z encode the current user UID in such way that
UID = 65536*(x - 128) + 256*y + z.

The names "localuser-UID", where UID is a decimal number, are resolved
to addresses 127.x.y.z where UID = 65536*(x - 128) + 256*y + z.

Allowed UID are from 0 to 4194303 included.

Examples:
  localuser      => 127.128.0.0   (when user has UID = 0)
  localuser      => 127.128.3.233 (when user has UID = 1001)
  localuser-1024 => 127.128.4.0   (for any user)

The service also provides the reverse resolution.

This module provides a value for IPv6: it translate to a IPv4-mapped IPv6 address
because IPv6 lakes of loopback range.

Example: localuser-1024 => ::ffff:127.128.4.0

For details about NSS integration, see
[Gnu libc documentation](https://www.gnu.org/software/libc/manual/html_node/Name-Service-Switch.html).

## Install

To install this file:

 make all && sudo install 

The installation directory is automatically detected by the tiny
script detect-nssdir.sh. If that gives the wrong result define the
variable nssdir when calling make, as below:

 make install nssdir=~/lib

To activate the NSS module localuser you have to edit
`/etc/nsswitch.conf` and add `localuser` to the
line starting with "`hosts:`".

Your nsswitch file can then looks like that at the end:

<pre># /etc/nsswitch.conf

passwd:     files sss systemd
shadow:     files sss
group:      files sss systemd

hosts:      <b>localuser</b> files dns myhostname

ethers:     files
netmasks:   files
networks:   files
protocols:  files
services:   files sss

aliases:    files nisplus
</pre>

