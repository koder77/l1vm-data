L1VM DATA - 2021-06-21
======================

This software is copyrighted by Stefan Pietzonke aka koder77 2022.

NEW: file "config.txt" contains a whitelist of IP addresses, which are allowed to connect
to the server. Add one IP address per line. And don't put a new line after the last entry!
This makes the data base a bit more safe!

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
STORE BYTE        store variables
STORE STRING
STORE INT64
STORE DOUBLE

GET BYTE         get(read) variables
GET STRING
GET INT64
GET DOUBLE

REMOVE BYTE      remove (read and remove) variables
REMOVE STRING
REMOVE INT64
REMOVE DOUBLE

SEARCH DATA      search in database for data

GET INFO         get variable real name and data type
LOGOUT           disconnect from server

SAVE             save a data base
LOAD             load a data base
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

Get info of variable:

<pre>
$ nc localhost 2020
GET INFO
3n1-.*
3n1-1
INT64
OK
</pre>

So the "GET INFO" and "3n1-.*" was used as input to the database.
And the following lines are the output.

SEARCH DATA
===========
The data base can be searched for a data match.
If the searched data is in the data base then the name of the data
entry will be returned.
The following example creates two data sets: an INT64 and a STRING type:

<pre>
$ nc localhost 2020
STORE INT64
foo
123456
OK
STORE STRING
hello
Hello world!
OK
SEARCH DATA
world
hello
OK
SEARCH DATA
123456
foo
OK
</pre>


SAVE/LOAD
========
<pre>
SAVE
test.l1db
OK

LOAD
test.l1db
OK
</pre>

To save/load a data base you use two input lines for the commands as above!

Have some fun!! :)
