L1VM DATA
=========

This software is copyrighted by Stefan Pietzonke aka koder77 2021.

You need to install the "libssl-dev" package on Debian WSL.

L1vm-data is a simple database for sharing data stored in the l1vm-data server.
Worker programs can access this data and share it as global memory.
There are three commands:

STORE, GET and REMOVE

The "STORE INT64" command store a 64 bit signed integer into the database.
The command must be send over a TCP/IP socket (port 2020) and end with a "\n" newline character like this:

<pre>
STORE INT64
foobar
42
</pre>

The variable foobar gets stored with a value of "42".

you can connect to the data base via "nc" on Linux for example:

<pre>
$ nc localhost 2020
$ STORE INT64
$ foobar
$ 42
$ OK
</pre>

TO get a variable use the GET command:

<pre>
$ GET INT64
$ foobar
$ 42
$ OK
</pre>

To read and remove a variable use the REMOVE command.

The commands list:

<pre>
STORE INT64       store variables
STORE BYTE
STORE STRING
STORE INT64
STORE DOUBLE

GET INT64         get(read) variables
GET BYTE
GET STRING
GET INT64
GET DOUBLE

REMOVE INT64      remove (read and remove) variables
REMOVE BYTE
REMOVE STRING
REMOVE INT64
REMOVE DOUBLE
</pre>

The GET/REMOVE commands also are working with "regular expressions":

<pre>
GET INT64
foobar.*
42
OK
</pre>

The variable: "foobar-1234" would be get. The ".*" stand for every possible chars
on any length. So anything starting with "foobar" would be found.

Usage:
<pre>
$ l1vm-data 1000000
</pre>

Makes space for 1000000 elements. The variable names are set to max 127 chars.

This server database was developed for use in home nets only! There is no way to protect data stored in the database.
So everything inside will be accessible from a connected client!!!

NEW EXAMPLES 3N1 calc
=====================
The 3n1 calc test:
Check if a number is even: if it is even then divide by two, if it is odd then multiply by three
and add one. Every start number should give a one as the final result!

Run the "l1vm-data" database server:

<pre>
$ ./l1vm-data -p 2020
</pre>

Run the L1VM server init program:

<pre>
$ l1vm-jit 3n1-server
</pre>

And finally the 3n1 worker client:

<pre>
$ l1vm-jit 3n1-client -args 10 198
</pre>

At the end with a terminal program connected to the server we can check the results:

<pre>
$ nc localhost 2020
GET INT64
3n1-100
1
</pre>

Have some fun!! :)
