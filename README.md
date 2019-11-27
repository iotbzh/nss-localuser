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

It defines the family  *"localuser"* of virtual hostnames as one of the
below names:

- localuser
- localuser-UID
- localuser--APPID
- localuser-UID-APPID
- localuser---APPID

This can be summarized by the following matrix:

  |------------------|------------------|---------------------|-------------------|
  |                  | **current user** | **user of UID**     | **no user**       |
  |------------------|------------------|---------------------|-------------------|
  | **no APP**       | localuser        | localuser-UID       |                   |
  | **app of APPID** | localuser--APPID | localuser-UID-APPID | localuser---APPID |
  |------------------|------------------|---------------------|-------------------|

The delivered NSS service defines one virtual host of name `localuser`
that resolves to an IP address of the localhost loopback that integrates
user ID.

It is intended to enable distinct IP for distinct users, distinct application.

The name *localuser* family is resolved to the IPv4 address range 127.128.0.0/9

The delivered IPv4 address is structured as follow:

```text
+--------+--------+--------+--------+
:01111111:1abbcccc:dddddeee:ffffffff:
+--------+--------+--------+--------+
```

When `a` is `1`, the value 11 bits value `bbccccddddd` encodes the APPID
and the 11 bits value `eeedddddddd` encodes the UID.
This is represented by the following hostnames: `localuser--APPID`
and `localuser-UID-APPID`.

When `abb` is `011`, the 20 bits value `ccccdddddeeeffffffff` encodes the APPID.
This is represented by the following hostnames: `localuser---APPID`.

When `abb` is `010`, the 20 bits value `ccccdddddeeeffffffff` encodes the UID.
This is represented by the following hostnames: `localuser`
and `localuser-UID`.

The values `000` and `001` of `abb` are reserved for futur use.

Examples:

```text
localuser           => 127.160.0.0   (when user has UID = 0)
localuser           => 127.160.3.233 (when user has UID = 1001)

localuser-0         => 127.160.0.0
localuser-45        => 127.160.0.45
localuser-1024      => 127.160.4.0
localuser-1048575   => 127.175.255.255

localuser---0       => 127.176.0.0
localuser---45      => 127.176.0.45
localuser---1048575 => 127.191.255.255

localuser-0-0       => 127.192.0.0
localuser--78       => 127.194.115.233 (when user has UID = 1001)
localuser-23-54     => 127.193.176.23
localuser-2047-2047 => 127.255.255.255
```

The service also provides the reverse resolution.

This module provides a value for IPv6: it translates to a IPv4-mapped IPv6 address
because IPv6 lacks of loopback range.

Example:

```text
localuser-1024 => ::ffff:127.128.4.0
```

For details about NSS integration, see
[Gnu libc documentation](https://www.gnu.org/software/libc/manual/html_node/Name-Service-Switch.html).

## Install

To install this file:

```sh
make all && sudo install
```

The installation directory is automatically detected by the tiny
script detect-nssdir.sh.

If the script detect-nssdir.sh gives the wrong result, just define the
variable nssdir when calling make, as below:

```sh
make install nssdir=~/lib
```

## Configuration and activation

### Manual setting

To activate the NSS module localuser you have to edit
`/etc/nsswitch.conf` and add `localuser` to the
line starting with "`hosts:`".

Your nsswitch file can then look as below:

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

aliases:    files nisplus</pre>

### Scripted setting

The script activate-localuser.sh can be used to activate,
deactivate or query the status of localuser activation.

It accepts 0, 1 or 2 arguments.

<pre>usage: activate-localuser.sh [command [file]]

command:    on, yes, true, 1:           activate
            off, no, false, 0:          deactivate
            status, test, check, query: status (default)
file:       file to change (default /etc/nsswitch.conf)</pre>
