lode - Linzhi Operations DaEmon
===============================

Copyright (C) 2021, 2022 Linzhi Ltd.

This work is licensed under the terms of the MIT License.
A copy of the license can be found in the file COPYING.txt


Introduction
------------

Lode listens to messages from the crew and obtains some miner attributes like
its ID number, IP address, name, etc. from it. Once it knows a miner's IP
address, it opens an MQTT connection to each miner and retrieves its
configuration.

It then runs a configuration script (see below) that can change configuration
variables based on the miner's attributes and its existing configuration. The
changes can then be reviewed through a Web-based user interface, and applied.

If changes should require a restart of some daemon, or a reboot of the miner,
it can be commanded at the time the changes are made, or deferred until a
later time.


Build and installation
----------------------

Prerequisites:
- flex
- bison
- libmosquitto-dev
- libjson-c-dev
- libmd-dev
- fonts-wqy-microhei (for font WenQuanYi-Micro-Hei, used for the favicon)

To build lode, simply

  git clone https://github.com/LinzhiChips/lode.git

and run

  make

When running, lode expects to find files in the following subdirectories of the
current directory:

- ui/: HTML, Javascript, and CSS of the Web UI
- active/: the files of the active configuration, rules.txt and any files
  referenced by it
- test/: the files of the test configuration, rules.txt and any files
  referenced by it

You have to create directories or symbolic links for active/ and test/. The
lode repository does not contain them, so that one can use symbolic links also
if running lode directly from the working tree.

Note that, for security reasons, the use of directories in path names is
restricted. Certain well-known directories are allowed, e.g., one can access
ui/index.html or active/rules.txt, but things like /etc/passwd or ../secret.txt
would not work. This affects the Web interface and path names in rules files.

When starting lode, the rules file can be given as an argument. Example:

  ./lode active/rules.txt

If no rules file is given, lode will run without rules, i.e., it will not
propose any configuration changes. If there is a rules file in active/, it can
be loaded later with "Reload" on the "Active" screen.

After it has started, lode sets up an HTTP server on port 8003. This HTTP
server provides the files of the Web user interface and access to lode's JSON
API. The port number can be changed with the option -j port.

To access the user interface, simply direct a Web browser to
http://machine.running.lode:8003


Known bugs
----------

- The vertical structure of the Web UI is not as intended. E.g., if the right
  side needs a horizontal scrollbar, you need to scroll down to reach it.

- The interaction of miner selection (left) and the variables (right) can be
  confusing in Test mode.

- When the destination the mining daemon currently uses is removed, the daemon
  will continue using the destination until a new destination is selected by
  the user, the connection fails, or the daemon restarts. Either mined or lode
  should perform the destination change automatically, to avoid having a stale
  configuration and possibly confusing miner behaviour.


User interface
==============

The user interface shows a header area at the top of the page, and below it one
or two panels. If two panels are shown, the division can be changed by dragging
the vertical white line.


Header
------

The header contains the "Auto-refresh" checkbox. If selected, the browser
requests an update every ten seconds. If not selected, configuration changes
will not be shown until reloading the page or selecting auto-refresh.

The Active/Test selector changes what is shown in the right panel: in Active
mode, the current configuration and any changes based on the current rules
(from active/rules.txt) are shown, and changes can be applied by clicking
"Update" or "Update & restart".

In Test mode, the rules in test/rules.txt are used. They are only processed,
for the currently selected miner, when clicking on "Run". The changes obtained
in Test mode do not change the left panel, nor are they applied to the miner.

Workflow for making changes to the rules or any files referenced by them:
1) Copy the new files to test/
2) Select a miner for whose configuration you wish to see the effect of the new
   rules.
3) Select Test mode and click "Run".
4) You can switch between Active and Test mode to compare the results.
5) Repeat the above steps until you are satisfied with the results.
6) Copy the new files to active/
7) Select Active mode and click "Reload".


Left panel
----------

The left display area shows the miners. Miners with the same characteristics
are grouped together. The state of the interaction with the miner is indicated
with the following colors:

- Medium grey: the operations daemon is collecting data from the crew and from
  the miner's configuration daemon. Depending on how information it has already
  received, the miner is identified in the following ways:

  - if the IP address is not yet known, a hexadecimal number

  - if the miner's NAME is not yet known, the numeric IP address.

  - if the NAME is known, the name.

- Light grey: the operations daemon has retrieved the miner's current
  configuration but is still waiting for additional data (from the crew).

  The configuration can be viewed by clicking on the colored rectangle in which
  the miner is shown. If there are multiple light grey groups, each represents
  a different configuration.

- Yellow: the configuration rules produced a setting the miner would not
  accept. Details can be found by clicking on the miner.

- Red and/or green: configuration rules have been processed and the resulting
  configuration differs from the miner's current configuration.

  If the group is red, one or more configuration variables will be deleted.
  If the group is green, new configuration variables will be added.
  If the group is red and green, there are both deletions and additions.

  The changes can be viewed by clicking on the respective group.

  Again, different groups represent different changes.

- Black with a white border: no configuration change is needed.

Short explanations are shown at the bottom of the first box of each coloring.
Clicking on a miner entry shows a detail view on the left right side.


Right panel
-----------

The detail view contains general information about the miner at the top,
followed by an alphabetically sorted list of configuration variables. Variables
that are to be deleted or changed are shown in red. Variables that are to be
added or changed are shown in green.

In Active mode, a "Update" and an "Update & restart" button are shown. Clicking
on either sends the configuration change to the miner. If using "Update &
restart", if the changes require any daemon restart or miner reboot, it is
automatically performed after making the configuration change.

If a miner configuration has changed but still requires a restart, the "Update
& restart" button changes to "Restart". Clicking it will command the restart.

Next to the update and restart buttons, one can select which miners are to be
updated: just the miner shown, all miners with exactly the same configuration
change, or all miners.

The path to the rules file is shown on top of the right panel. Clicking on it
opens a read-only viewer.


Syntax
======

A rules file consists of variable assignments and conditional variable
assignments:

var = value
...
condition:
	var = value
	...
...


Variables
---------

There are two types of variables: configuration variables and script variables.
Before the script runs, the user (*) configuration from the miner is retrieved
and stored in the respective variables. When the script ends, any changes to
configuration variables or new variables are copied back to the miner.

(*) For now, we ignore the factory configuration.

Script variables store intermediary results in the script. For example, one may
want to store the address of a wallet in an intermediary variable, and then
just the variable when setting up destinations.

Names of configuration variables contain one or more upper-case characters, and
possibly underlines and digits. Names of script variable contain at least one
lower-case character. Variable names must begin with a letter or an underscore.

Configuration variables can also be prefixed with 0/ or 1/ to indicate that
they are for the board in the respective slot. Without such a prefix, the
respective variable is on the controller.

Examples:

NAME	configuration variable, on the controller
local	script variable
room5	script variable
0/TSHUT	configuration variable, on the board in slot 0


Special variables
-----------------

The following script variables are set before the script starts:

id	the hexadecimal ID number of the controller (the lower 24 bits of the
	MAC address)
ip	the IPv4 address of the miner, as dotted quad
name	the name of the miner (or an empty string, if not defined)
serial0	the serial number of the board in the first slot (or an empty string)
serial1	the serial number of the board in the second slot

These variables can be changed in the script.


Variable assignments
--------------------

A variable assignment consists of the variable name, an equal sign, and the
value. Note that values can not be boolean expressions. To store a boolean
value, use this construct:

foo = NAME == "xyz"	# this does not work

NAME == "xyz":		# this works
	foo = 1

A variable can be set multiple times. In the case of configuration variables,
the last value assigned will be used. Example:

BANNER = "this"

some_condition:
	BANNER = "that"

Note that a configuration variable cannot be "unchanged", e.g.,

BANNER = "something"
BANNER = ""

will delete the variable BANNER (if it was set) on the miner(s).


Value expressions
-----------------

There are the following values and expressions:

- Numbers: all numbers are non-negative integers. They can be decimal or
  hexadecimal (beginning with 0x), and should fit in 32 bits.

  Examples: 123 0x200

  Note that numeric values retain their original format. E.g., if 0xa is stored
  in a configuration variable, it is stored as "0xa", while 10 would be stored
  as "10".

- IPv4 addresses: IPv4 addresses are in the form of dotted quads, without
  leading zeroes.

  Example: 192.168.17.5

- Quoted strings: either with " or '.

  Examples: "this is a string" 
	    'and so is this'

- String concatenation: strings are concatenated with the plus (+) operator.
  If one or both operands are numbers, their string format is used.

  Example: "foo" + "bar"

- Variable: any configuration or script variable can be used as a value.

  key = name + ":" + id

- Map file lookup. The syntax is as follows: a quoted string with the file
  name, followed by a string value in square brackets.

  Example: "my.map"[0/serial]

  This searches the map file for the corresponding key. If it is found, the
  value is returned. If the key is now found, the result is an empty string.

  Map files have a key followed by a value on each line. Empty lines and
  comments (beginning with # and extending to the end of the line) are ignored.
  Keys and values are either quoted strings, or sequences of non-whitespace
  characters.

  Example:
  key-a	valueA
  key-b	"value B"	# we need quotes because of the space


Conditional settings
--------------------

Conditional settings begin with a condition, a colon (:), followed by any
number of settings. The settings can me on the same line as the condition, or
they can be in the following lines. If on separate lines, settings must be
indented. The first non-indented setting will end the conditional settings. The
condition must not be indented. Example:

name == "foo":
	BANNER = "this is foo"	# only if name is "foo"
SWITCHES_ON_BOOT = ""		# always executed (ends conditional settings)
	TRIP_MASTER = "y"	# always executed, too


Conditions
----------

Conditions are made with the following values and expressions:

- Numeric value: "true" if non-zero, else "false".

- String value: "true" is non-empty, else "false". Note that 0 is "false", but
  "0" is "true".

- host name lookup: a string expression, the keyword "in", and the name of the
  hosts file, as a quoted string. This looks up the respective host name in an
  /etc/hosts type file (see man 5 hosts). The expression evaluates to "true" if
  the name is found, "false" otherwise. Example:

name in "roomx.list":
	is_in_room_X = 1

- List membership: a string expression, the keyword "in", and a comma-separated
  list of string expressions, in parentheses. This evaluates to "true" if any
  of the expressions in the list matches the key. This string comparison is not
  case-sensitive.

   Example: name in ("miner-123", "miner-007")

- Comparison: expression operator expression. The operator can be any of <. <=,
  ==, !=, >, and ==. If both expressions are numeric, a numeric comparison is
  made. Else, the strings are compared.

  Examples: name == "blah"
	    ip == 192.168.17.5

- Boolean operator: the boolean operators "and" (or &&), "or" (||), and "not"
  (!) are available. Parentheses can be used to override operator precedence.

  Examples: ip >= 10.0.1.0 && ip <= 10.0.1.255
	    room_X and no rack_Y


Associative arrays
------------------

Variables can form associative arrays. Elements are accessed by following the
base name with an expression in square brackets. This corresponds to an access
to a variable with the base name, an underscore, and the value of the
expression. The resulting name can contain characters that are not allowed in
variable names.

Associative arrays are mainly used for setting destinations.

Examples:	DEST["pool1"] = "etc.stratum0://..."	# DEST_pool1
		DEST["pool-2"] = "..."			# DEST_pool-2

To empty an associative array. use

DEST = {}


Example
=======

This rules file sets up miners for mining either ETC only or ZIL + ETC on
crazypool.org.


#
# ETC and ZIL wallets
#
etc = "0x309d6C35a3877EC20A726A8dF19B3dBaeE354CDA"
zil = "zil1zgxl38c67duz9dfwvzgxl3xhv8tgz8c6F72577"

#
# Pools
#
pool_eu = "eu.crazypool.org"
pool_as = "asia.crazypool.org"

#
# Destination with ETC-only
#
base = "etc.stratum1://" + etc + ".${name}@"

#
# Do we want to mine ZIL ? (We use hashboard serial numbers here, since they're
# more likely to be stable than machine names or IP addresses. The choice of an
# identification scheme is site specific. Other possibilities would include
# the IP address, the machine name, or the controller ID.)
#
"zil-miners.txt"[0/serial] || "zil-miners.txt"[1/serial]:
	#
	# Enable ZILLIQA support (Note: if this is not a ZIL miner, we don't
	# change ZILLIQA_SUPPORT.)
	#
	ZILLIQA_SUPPORT = "y"
	#
	# Change the base for ETC + ZIL
	#
	base = "etc.stratum1://" + etc + ".${name}:" + zil + "@"

#
# Clear all existing destinations
#
DEST = {}
#
# Define pool URIs
#
DEST["etc-eu"] = base + pool_eu + ":7000"
DEST["etc-as"] = base + pool_as + ":7000"


The corresponding list of ZIL miners (zil-miners.txt) could look like this:


# Hashboard serial number, some text
HDROU6F3        minerX
HDDJ6HRJ        minerX


Configuration variables and values
==================================

The default value is used if a variable isn't set. Note that some variables
have only "y" (yes) but no "n" (no), or vice versa. Unsetting the variable then
sets the opposite beaviour.

Dotted quad: a dotted quad is an IPv4 address written as four byte values,
  separated by dots. Each value is a decimal number from 0 to 255, without
  leading zeroes.
  Example: 192.168.71.64
Hostname: the fully qualifies name of an Internet host, e.g.,
  time.cloudflare.com


Controller variables
--------------------

BANNER	Text shown in the top area of the UI, for notes and remarks.
	Any number of printable characters (includes spaces).
	Default: no banner text.
CREW	Enable the crew and select communication mode.
	Values:
	y	Enable, with unicast (note: this isn't useful at the moment)
	n	Disable
	multicast
		Enable, with multicast
	Default: multicast
CREW_API
	Control access of the crew HTTP JSON API.
	Values:
	off	Disable API
	local	Allow only access from the miner
	remote	Allow network access
	Default: off
CREW_API_PORT
	Set the TCP port of the HTTP JSON API.
	This is a decimal number from 1 to 65535.
	Default: 81
CREW_MCAST_ADDR
	The multicast group address used for peer-to-peer communication in the
	crew. (If enabled, see CREW.)
	This is a dotted quad.
	Default: 239.255.49.44
CREW_PORT
	UDP port used for peer-to-peer communication in the crew.
	This is a decimal number from 1 to 65535.
	Default: 12588
CREW_SHARED_SECRET
	A shared secret used to authenticate crew messages.
	This is a string of arbitrary content.
	Default: no shared secret
DARK_TIMEOUT
	If the RGB LED is green, turn it off after a while.
	This is the number of seconds after which to turn the LED off, from 0
	to 999999. 0 never turns the LED off.
	Default: 0
DHCP	Use DHCP for network configuration.
	Values:
	y	Use DHCP to configure IP, GW, ...	
	n	Use configured "static" values.
	Default: n
DHCP_NAME
	Choose DHCP name over NAME when naming the miner.
	Values:
	y	Prefer a DHCP-name over NAME.
	n	Use NAME.
	Default: n
DNS	List of nameservers.
	Comma-separated names of dotted quads.
	Default: do not use DNS (may be set by DHCP, see DHCP)
DYNAMIC_MEMORY
	Support non-uniform memory allocation
	Value:
	y	Skip nodes with uncorrectable defects
	Default: ignore uncorrectable defects
ECDSA_HOST_KEY
	Host key for use by dropbear (SSH).
	This is a base64-encoded SSH private key in ECDSA format.
FAN_PROFILE
	Temperature:duty-cycle points (e.g., 50:30,80:100)
	This is a comma-separated list of temperature:duty-cycle pairs. The
	temperature (in degrees Celsius) and the duty cycle (in percent) are
	non-negative decimal numbers.
	Example: 49:0,50:30,70:100
	Default: always run the fans at 100%.
GW	Router to reach non-local addresses.
	A dotted quad.
	Default: no access to non-local addresses (or from DHCP, see DHCP)
IP	IPv4 address used by the miner.
	A dotted quad.
	Default: 192.168.71.64 (may be set by DHCP, see DHCP)
KEEP_FSCK
	Keep recovered files (FSCK*.REC) on memory card.
	Value:
	y	Keep files for examination.
	Default: remove files automatically at boot time.
NAME	Machine name, for identification purposes.
	1-16 characters, alphanumeric or underscore (_) or dash (-). The first
	character must be alphanumeric or an underscore.
	Default: linzhi-phoenix (may be set by DHCP, see DHCP_NAME)
NETMASK	A bitmask to distinguish local (same network segment or broadcast
	domain) from non-local (need routing) addresses.
	A dotted quad.
	Default: 255.255.0.0 (may be set by DHCP, see DHCP)
NONCE_OFFSET
	Start value for the nonce search
	This is a hexadecimal number with up to 16 digits. It can optionally be
	prefixed with "0x".
	Default: 0
NTP	NTP (Network Time Protocol) server.
	Value:
	- the hostname of the NTP server to use, or
	- "off"
	Default: time.cloudflare.com
REPORTS_MAX
	The maximum number of problem reports the miner stores.
	This is a decimal number >= 0.
	Default: 10
REPORTS_ROTATE
	What to do when reaching the maximum number of reports.
	Value:
	n	Stop making new reports.
	Default: delete old reports to make room for new ones
REPORT_CREWD_CRASH
	If the crew daemon crashes, ...
	Value:
	y	generate a report
	Default: no report for crewd crashes
REPORT_DAGD_CRASH
	If the DAG daemon crashes, ...
	Value:
	y	generate a report
	Default: no report for dagd crashes
REPORT_MINED_CRASH
	If the mining daemon crashes, ...
	Value:
	y	generate a report
	Default: no report for dagd crashes
REPORT_OOM
	If the miner runs out of memory, ...
	Value:
	y	try to generate a report
	Default: no report when out of memory
RESERVED_SPACE
	uSD card space reserved for things other than DAGs.
	Value: a decimal number >= 1, followed by the letter "G".
	Default: 6G
SECURE	Control MQTT access.
	Values:
	y	Login required
	guest	Unauthenticated users can read some things but not write
	n	Access is open to anyone
SEPARATE_MINED
	Separate mining on both boards.
	Value:
	y	Run one mining daemon (mined) for each board
	Default: single mined operates both boards
SUB_EXTRANONCE
	Request the mining.extranonce.subscribe protocol extension
	Value:
	y	Send mining.extranonce.subscribe
	Default: do not send mining.extranonce.subscribe (but still accept
		mining.set_extranonce)
SWITCH_OPS
	Enable operations-defined master switch override
	Value:
	n	Ops switch is ignored
	Default: Ops switch can turn off power
SWITCHES_ON_BOOT
	Setting of board power switches after booting
	Values:
	off	Power off
	last	Restore last switch setting
	Default: power on
TRIP_MASTER
	Shutdown (DCDC, temperature) trips master switch
	Value:
	y	Turn off master power switch on shutdown
	Default: only power down until shutdown is cleared.
ZILLIQA_SUPPORT
	Zilliqa (ZIL) dual mining
	Value:
	y	Support Zilliqa mining
	standby	Keep the Zilliqa DAG but don't recognize ZIL jobs
	Default: do not support Zilliqa


Per-board variables
-------------------

The names of these variables are prefixed with 0/ or 1/. for slot 0 or 1,
respectively.

GEN_RATIO
	Percentage of maximum task generation rate.
	This is a decimal number from 1 to 100 (percent).
	Default: 100.
IGNORE	Do not use the board in this slot.
	Values:
	y	Board does not mine (but temperature and power are monitored)
	ban	Board does not mine, is not monitored, alerts are ignored.
	Default: board is used normally.
SERDES_RATE
	Speed of the SerDes lanes, in Mbps.
	A decimal number from 23125 to 31875, optionally followed by an "M".
	The number must be a multiple of 625.
SERDES_TRIM
	List of channels that should be disabled.
	The word "none", or a comma-separated list of from-to pairs. From and
	to are die numbers, 0-63, separated by a dash (-).
	Default: use the list of disabled channels (if any) from the factory
	settings.
SKIP_DIES
	List of dies that should not be configured for mining.
	The word "none", or a comma-separated list of die numbers.
	Default: use the list of disabled dies (if any) from the factory
	settings.
SKIP_TCHAN
	List of temperature sensor channels that should not be used.
	The word "none", or a comma-separated list of channels. A channel is
	specified with
	- the number of the bus of the board (0 or 1),
	- the hexadecimal I2C device address, beginning with 0x,
	- a colon, and
	- the channel number (1-8).
	Example: 1:0x48:8
	Default: use the list of disabled channels (if any) from the factory
	settings.
TSHUT	Thermal shutdown threshold
	A decimal number from 0 to 100 (degrees Celsius).
	Default: 95 degrees.
TWARN	Temperature warning threshold
	A decimal number from 0 to 99 (degrees Celsius).
	Default: 90 degrees.
VAA	Analog voltage.
	A decimal number from 1400 to 1800 (mV).
	Default: 1525.
VDD	Logic voltage.
	A decimal number from 800 to 900 (mV).
	Default: 850.
VIN_UV	Input undervoltage fault limit.
	A decimal number from 11000 to 12000 (mV).
	Default: 11000.
VIO	IO voltage.
	A decimal number from 1700 to 1900 (mV).
	Default: 1800.
ZOMBIE_COMPENSATION
	Percentage of maximum compensation to apply when zombie-mining.
	A decimal number from 0 to 100 (percent).
	Default: 80.
	See also
	https://github.com/LinzhiChips/doc/blob/master/zombie-mining.pdf
