/*
 * This file main.cpp is part of l1vm-data.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2021
 *
 * L1vm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * L1vm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with L1vm.  If not, see <http://www.gnu.org/licenses/>.
 */

//
// to connect manualy to data bank:
// nc localhost 2020
//

#include "include/global.h"
#include "include/protos.h"

// networking stuff

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <float.h>
#include <bits/stdc++.h>

// crypto stuff
// #include <openssl/sha.h>

#define MACHINE_BIG_ENDIAN 0

#define SOCKADDRESSLEN      16      /* max dotted ip len */
#define SOCKBUFSIZE         10240    /* socket data buffer len */

#define SOCKETOPEN 1              // state flags
#define SOCKETCLOSED 0

#define PORT 2020

#define SOCKSERVER          0
#define SOCKCLIENT          1

#define MAXSOCKETSCONN       10

// ERROR codes returned by VM
#define ERR_FILE_OK         0
#define ERR_FILE_OPEN      -1
#define ERR_FILE_CLOSE     -2
#define ERR_FILE_READ      -3
#define ERR_FILE_WRITE     -4
#define ERR_FILE_NUMBER    -5
#define ERR_FILE_EOF       -6
#define ERR_FILE_FPOS      -7

#define DATANAME     256
#define STRINGLEN    4096

using namespace std;

typedef int NINT;

struct socket
{
    S2 socket;                                /* socket handle */
    S2 serv_conn;                           /* server connection */
    struct addrinfo *servinfo;
    U1 type;                                /* server / client */
    U1 state;                              /* open, closed */
    U1 client_ip[SOCKADDRESSLEN];           /* client ip */
    U1 buf[SOCKBUFSIZE];                    /* socket data buffer */
};

static struct socket sockets[MAXSOCKETSCONN];

extern pthread_mutex_t data_mutex;

size_t strlen_safe (const char * str, int maxlen);

// global data set by main and load_database
S8 size = 200;
S8 port = 2020;

// legal client ip addresses
#define MAX_CLIENT_IP_ADDRESSES 256

char clientipaddress_str[MAX_CLIENT_IP_ADDRESSES][INET_ADDRSTRLEN];
S8 max_clientipaddresses = 0;

//byte order =================================================================
// helper functions endianess

#if ! MACHINE_BIG_ENDIAN
S8 htonq (S8 num)
{
    U1 *num_ptr, *new_ptr;
    S8 newv;

    num_ptr = (U1 *) &num;
    new_ptr = (U1 *) &newv;

    new_ptr[0] = num_ptr[7];
    new_ptr[1] = num_ptr[6];
    new_ptr[2] = num_ptr[5];
    new_ptr[3] = num_ptr[4];
    new_ptr[4] = num_ptr[3];
    new_ptr[5] = num_ptr[2];
    new_ptr[6] = num_ptr[1];
    new_ptr[7] = num_ptr[0];

    return (newv);
}

S8 ntohq (S8 num)
{
    U1 *num_ptr, *new_ptr;
    S8 newv;

    num_ptr = (U1 *) &num;
    new_ptr = (U1 *) &newv;

    new_ptr[0] = num_ptr[7];
    new_ptr[1] = num_ptr[6];
    new_ptr[2] = num_ptr[5];
    new_ptr[3] = num_ptr[4];
    new_ptr[4] = num_ptr[3];
    new_ptr[5] = num_ptr[2];
    new_ptr[6] = num_ptr[1];
    new_ptr[7] = num_ptr[0];

    return (newv);
}

#else
S8 htonq (S8 num)
{
	return (num);
}

S8 ntohq (S8 num)
{
	return (num);
}
#endif

F8 htond (F8 hostd)
{
    U1 *netdptr;
    U1 *hostdptr;
    S2 i;
    F8 netd;

    netdptr = (U1 *) &netd;

    hostdptr = (U1 *) &hostd;
    hostdptr += sizeof (F8) - 1;

    #if ! MACHINE_BIG_ENDIAN
        for (i = 0; i <= (S2) sizeof (F8) - 1; i++)
        {
            *netdptr++ = *hostdptr--;
        }
    #else
        netd = hostd;
    #endif

    return (netd);
}

F8 ntohd (F8 netd)
{
    U1 *netdptr;
    U1 *hostdptr;
    S2 i;
    F8 hostd;

    hostdptr = (U1 *) &hostd;

    netdptr = (U1 *) &netd;
    netdptr += sizeof (F8) - 1;

    #if ! MACHINE_BIG_ENDIAN
        for (i = 0; i <= (S2) sizeof (F8) - 1; i++)
        {
            *hostdptr++ = *netdptr--;
        }
    #else
        hostd = netd;
    #endif

    return (hostd);
}


// socket I/O =================================================================

U1 exe_sread (S4 slist_ind, S4 len)
{
    U1 *buf;
    S2 sockh = 0, ret;
    S4 todo, buf_ind = 0;

    if (len < 0 || len > SOCKBUFSIZE)
    {
        return (ERR_FILE_READ);
    }

    sockh = sockets[slist_ind].serv_conn;
    todo = len;
    buf = sockets[slist_ind].buf;

    while (todo > 0)
    {
        ret = recv (sockh, &(buf[buf_ind]), todo, MSG_NOSIGNAL);
        if (ret == -1)
        {
            return (errno);
        }

        todo = todo - ret;
        buf_ind = buf_ind + ret;
    }

    return (ERR_FILE_OK);
}

U1 exe_swrite (S4 slist_ind, S4 len)
{
    U1 *buf;
    S2 sockh = 0, ret;
    S4 todo, buf_ind = 0;

    if (len < 0 || len > SOCKBUFSIZE)
    {
        return (ERR_FILE_WRITE);
    }

    sockh = sockets[slist_ind].serv_conn;
    todo = len;
    buf = sockets[slist_ind].buf;

    while (todo > 0)
    {
        ret = send (sockh, &(buf[buf_ind]), todo, MSG_NOSIGNAL);
        if (ret == -1)
        {
            return (errno);
        }

        todo = todo - ret;
        buf_ind = buf_ind + ret;
    }

    return (ERR_FILE_OK);
}

// read/write data ============================================================

// int64 ======================================================================
S2 socket_read_int64 (S2 handle, S8 *value)
{
    S8 ret;
    S8 n;
    U1 *ptr;
    S8 i;

    ret = exe_sread (handle, sizeof (S8));
    if (ret != ERR_FILE_OK)
    {
        // ERROR
        value = 0;
        return (1);
    }

    ptr = (U1 *) &n;

    for (i = 0; i <= (S2) sizeof (S8) - 1; i++)
    {
        *ptr++ = sockets[handle].buf[i];
    }

    value = (S8 *) ntohq (n);
    return (0);
}

S2 socket_write_int64 (S2 handle, S8 value)
{
    S8 ret;
    U1 *ptr;
    S8 n;
    S8 i;

    n = htonq (value);
    ptr = (U1 *) &n;

    for (i = 0; i <= (S2) sizeof (S8) - 1; i++)
    {
        sockets[handle].buf[i] = *ptr++;
    }

    ret = exe_swrite (handle, sizeof (S8));
    return (ret);
}

// string =====================================================================
S2 socket_read_string (S2 handle, U1 *data, S8 slen)
{
     /* read CRLF or LF terminated line */

    S8 ret;
    U1 ch;
    U1 end = FALSE;
    U1 error = FALSE;
    S8 i = 0;

    while (! end)
    {
        ret = exe_sread (handle, sizeof (U1));
        if (ret != ERR_FILE_OK)
        {
            error = TRUE;
            end = TRUE;

            if (i == 0)
            {
                /* error at first read, break while */
                break;
            }
        }

        ch = sockets[handle].buf[0];

        if (ch != '\n')
        {
            if (i <= slen)
            {
                data[i] = ch;
                i++;
            }
            else
            {
               error = TRUE; end = TRUE;
            }
        }
        else
        {
            data[i] = '\0';
            end = TRUE;
        }
    }

    if (error == FALSE)
    {
        return (ERR_FILE_OK);
    }
    else
    {
        return (ERR_FILE_READ);
    }
}

S2 socket_write_string (S2 handle, U1 *data)
{
     /* write string + LF terminated line */

    S8 ret;
    U1 end = FALSE;
    S8 i = -1;

    // DEBUG
    // cout << "socket_write_string: " << data << " handle: " << handle << endl;

    while (! end)
    {
        i++;
        sockets[handle].buf[i] = data[i];
        if (data[i] == '\0')
        {
            sockets[handle].buf[i] = '\n';
            // sockets[handle].buf[i + 1] = '\0';
            end = TRUE;
        }
    }

    ret = exe_swrite (handle, i + 1);
	return (ret);
}


pthread_mutex_t data_mutex;

// string helper functions ====================================================
size_t strlen_safe (const char * str, int maxlen)
{
	 long long int i = 0;

	 while (1)
	 {
	 	if (str[i] != '\0')
		{
			i++;
		}
		else
		{
			return (i);
		}
		if (i > maxlen)
		{
			return (0);
		}
	}
}


union mem
{
    U1 *byte;
    S8 qword;
    F8 dfloat;
};

struct data
{
    U1 type;
    union mem mem;
    U1 name[DATANAME];
    S8 size;
};

class data_store
{
    S8 maxdata;
    S8 port;
    struct data *data;

public:
    S2 init_mem (S8 max_size);
    void init_port (S8 port_set);

	// data store
    S8 find_free_element (void);

	S2 store_byte (U1 *name, U1 *string);
	S2 store_int64 (U1 *name, S8 value);
	S2 store_double (U1 *name, F8 value);

	// data get
	S8 find_element (U1 *name, S2 &type);

	S2 get_byte (U1 *name, U1 *value);
	S2 get_int64 (U1 *name, S8 &value);
	S2 get_double (U1 *name, F8 &value);

	// data remove
	S2 remove_byte (U1 *name, U1 *value);
	S2 remove_int64 (U1 *name, S8 &value);
	S2 remove_double (U1 *name, F8 &value);
	S2 remove_all (void);

    // data base save/load
    S2 save_database (U1 *filename);
    S2 load_database (U1 *filename);

    // data info
    S2 data_get_info (U1 *name, U1 *realname, S8 &type);
    S2 find_element_realname (U1 *name, S8 &type, U1 *realname);

    S2 data_get_input_line (U1 *line, U1 *input, S2 type);

    S8 find_data (U1 *data_find, S8 start_index);                                       // internal search function
    S8 search_data (U1 *data_find, U1 *data_name);                                      // called by search data command
    S8 search_data_list (U1 *data_find, U1 *data_name, S8 start_index);                 // used by "search data list" has start index variable
    S8 search_name_list (U1 *data_find, U1 *data_name, S8 start_index);

    S8 find_element_index (U1 *name, S8 start_index);
    S8 get_maxdata (void);
    S2 get_data_name_value (S8 index, U1 *data_name, U1 *value);
    S8 find_element_list (U1 *name, S8 start_index);
    S8 search_element_list (U1 *data_search_name, U1 *data_name, S8 start_index);


    data_store (S8 max_size, S8 port)
    {
        if (init_mem (max_size) != 0)
        {
            cout << "ERROR allocating data stucture!" << endl;
        }
        init_port (port);
    }

    ~data_store ()
    {
        {
            S8 i;
            for (i = 0; i < maxdata; i++)
            {
                if (data[i].type == BYTE || data[i].type == STRING)
                {
                    delete data[i].mem.byte;
                }
            }

            delete data;
        }
    }
};

// declare global pointer
data_store* data_mem;

S8 data_store::get_maxdata (void)
{
    return (maxdata);
}

S2 data_store::get_data_name_value (S8 index, U1 *data_name, U1 *value)
{
    if (index < 0 || index >= maxdata)
    {
        // index overflow, return error code
        return (1);
    }

    // copy name
    strcpy ((char *) data_name, (const char *) data[index].name);

    // copy value
    switch (data[index].type)
    {
        case BYTE:
            value = data[index].mem.byte;
            break;

        case STRING:
            strcpy ((char *) value, (const char *) data[index].mem.byte);
            break;

        case QWORD:
            sprintf ((char *) value, "%lli", data[index].mem.qword);
            break;

        case DOUBLE:
            sprintf ((char *) value, "%10.10f", data[index].mem.dfloat);
            break;
    }
    return (0);
}

S2 data_store::init_mem (S8 size)
{
    S8 i;

    try
    {
        data = new struct data[size];
        // set all elements as free
        for (i = 0; i < size; i++)
        {
            data[i].type = FREE;
        }
        maxdata = size;
        return (0);
    }
    catch(std::bad_alloc&)
    {
        return (1);         // error code
    }
}

void data_store::init_port (S8 port_set)
{
    port = port_set;
}

S8 data_store::find_free_element (void)
{
    S8 i;
    for (i = 0; i < maxdata; i++)
    {
        if (data[i].type == FREE)
        {
            return (i);
        }
    }
    return (-1);    // no free element found!
}

// data store =================================================================

S2 data_store::store_byte (U1 *name, U1 *string)
{
    // store byte or string in data storage
    S8 ind;
    S8 size;

    size = strlen_safe ((const char *) string, STRINGLEN);

	// cout << "store_byte: name: " << name << " value: " << string << endl;
	// cout << "value len: " << size << endl;

	pthread_mutex_lock (&data_mutex);
    ind = find_free_element ();
    if (ind > -1)
    {
        if (size == 1)
        {
            // copy byte, set as data
            data[ind].type = BYTE;

			try
			{
				data[ind].mem.byte = new U1 [size + 1];
			}
			catch(std::bad_alloc&)
		    {
				cout << "store_byte: can't allocate memory!" << endl;
				pthread_mutex_unlock (&data_mutex);
		        return (1);         // error code
		    }
            data[ind].mem.byte = &string[0];
            data[ind].size = 1;
        }
        else
        {
            // save string, allocate space
			try
			{
	        	data[ind].mem.byte = new U1 [size + 1];
			}
			catch(std::bad_alloc&)
		    {
				cout << "store_byte: can't allocate memory!" << endl;
				pthread_mutex_unlock (&data_mutex);
		        return (1);         // error code
		    }
            strcpy ((char *) data[ind].mem.byte, (const char*) string);
            data[ind].type = STRING;
            data[ind].size = size + 1;
        }
        // save string name
        if (strlen_safe ((const char *) name, DATANAME - 1) < DATANAME - 1)
        {
            strcpy ((char *) data[ind].name, (const char*) name);
        }
        else
        {
            pthread_mutex_unlock (&data_mutex);
            return (0);
        }

		pthread_mutex_unlock (&data_mutex);
        return (0);     // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}

S2 data_store::store_int64 (U1 *name, S8 value)
{
    S8 ind;

	pthread_mutex_lock (&data_mutex);
    ind = find_free_element ();
    if (ind > -1)
    {
        data[ind].type = QWORD;
        data[ind].mem.qword = value;
        data[ind].size = 1;
        // save string name
        if (strlen_safe ((const char *) name, DATANAME -1) < DATANAME - 1)
        {
            strcpy ((char *) data[ind].name, (const char*) name);
        }
        else
        {
            pthread_mutex_unlock (&data_mutex);
            return (0);
        }
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}

S2 data_store::store_double (U1 *name, F8 value)
{
    S8 ind;

	pthread_mutex_lock (&data_mutex);
    ind = find_free_element ();
    if (ind > -1)
    {
        data[ind].type = DOUBLE;
        data[ind].mem.dfloat = value;
        data[ind].size = 1;
        // save string name
        if (strlen_safe ((const char *) name, DATANAME - 1) < DATANAME - 1)
        {
            strcpy ((char *) data[ind].name, (const char*) name);
        }
        else
        {
            pthread_mutex_unlock (&data_mutex);
            return (0);
        }
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}

// data get ===================================================================

S8 data_store::find_element (U1 *name, S2 &type)
{
    S8 i;

	regex pat ((char *) name);
	// string valstr;
	bool match;

    for (i = 0; i < maxdata; i++)
    {
        if (data[i].type != FREE)
        {
			// cout << "find_element: " << i << " type: " << data[i].type << endl;

			string valstr((char *) data[i].name);
			match = regex_match (valstr, pat);
			if (match)
			{
				// found a name match, return type
				type = data[i].type;
				return (i);
			}
        }
    }
    return (-1);    // no element found!
}

S8 data_store::find_element_index (U1 *name, S8 start_index)
{
    S8 i;

    regex pat ((char *) name);
	// string valstr;
	bool match;

    for (i = start_index; i < maxdata; i++)
    {
        if (data[i].type != FREE)
        {
			// cout << "find_element: " << i << " type: " << data[i].type << endl;

			string valstr((char *) data[i].name);
			match = regex_match (valstr, pat);
			if (match)
			{
				return (i);
			}
        }
    }
    return (-1);    // no element found!
}


S8 data_store::find_data (U1 *data_find, S8 start_index)
{
    U1 *data_str[STRINGLEN];
    S8 i;

    regex pat ((char *) data_find);
    // string valstr;
    bool match;
    string valstr;

    for (i = start_index; i < maxdata; i++)
    {
        if (data[i].type != FREE)
        {
            switch (data[i].type)
            {
                case BYTE:
                    if (data[i].mem.byte[0] == data_find[0])
                    {
                        // found data match, return index of data
                        return (i);
                    }
                    break;

                case STRING:
                    valstr.assign ((char *) data[i].mem.byte);
                    match = regex_match (valstr, pat);
                    if (match)
                    {
                        // found data match, return index of data
                        return (i);
                    }
                    break;

                case QWORD:
                    sprintf ((char *) data_str, "%lli", data[i].mem.qword);
                    valstr.assign ((char *) data_str);
                    match = regex_match (valstr, pat);
                    if (match)
                    {
                        // found data match, return index of data
                        return (i);
                    }
                    break;

                case DOUBLE:
                    sprintf ((char *) data_str, "%10.10f", data[i].mem.dfloat);
                    valstr.assign ((char *) data_str);
                    match = regex_match (valstr, pat);
                    if (match)
                    {
                        // found data match, return index of data
                        return (i);
                    }
                    break;
            }
        }
    }
    return (-1);        // data not found
}

S8 data_store::find_element_list (U1 *name, S8 start_index)
{
    S8 i;

    regex pat ((char *) name);
    // string valstr;
    bool match;
    string valstr;

    for (i = start_index; i < maxdata; i++)
    {
        if (data[i].type != FREE)
        {
            string valstr((char *) data[i].name);
			match = regex_match (valstr, pat);
			if (match)
			{
				// found a name match, return type
                strcpy ((char *) name, (const char *) data[i].name);
				return (i);
			}
        }
    }
    return (-1);        // no element found!
}

S8 data_store::search_data (U1 *data_find, U1 *data_name)
{
    S8 ind;

    pthread_mutex_lock (&data_mutex);
    ind = find_data (data_find, 0);
    if (ind > -1)
    {
        // found data index
        strcpy ((char *) data_name, (const char *) data[ind].name);
        pthread_mutex_unlock (&data_mutex);
        return (ind);
    }
    strcpy ((char *) data_name, "");
    pthread_mutex_unlock (&data_mutex);
    return (-1);
}

S8 data_store::search_data_list (U1 *data_find, U1 *data_name, S8 start_index)
{
    S8 ind;

    pthread_mutex_lock (&data_mutex);
    ind = find_data (data_find, start_index);
    if (ind > -1)
    {
        // found data index
        strcpy ((char *) data_name, (const char *) data[ind].name);
        pthread_mutex_unlock (&data_mutex);
        return (ind);
    }
    strcpy ((char *) data_name, "");
    pthread_mutex_unlock (&data_mutex);
    return (-1);
}

S8 data_store::search_element_list (U1 *data_search_name, U1 *data_name, S8 start_index)
{
    S8 ind;

    pthread_mutex_lock (&data_mutex);
    ind = find_element_list (data_search_name, start_index);
    if (ind > -1)
    {
        // found data index
        strcpy ((char *) data_name, (const char *) data[ind].name);
        pthread_mutex_unlock (&data_mutex);
        return (ind);
    }
    strcpy ((char *) data_name, "");
    pthread_mutex_unlock (&data_mutex);
    return (-1);
}

S2 data_store::find_element_realname (U1 *name, S8 &type, U1 *realname)
{
    S8 i;

	regex pat ((char *) name);
	// string valstr;
	bool match;

    for (i = 0; i < maxdata; i++)
    {
        if (data[i].type != FREE)
        {
			// cout << "find_element: " << i << " type: " << data[i].type << endl;

			string valstr((char *) data[i].name);
			match = regex_match (valstr, pat);
			if (match)
			{
				// found a name match, return type
				type = data[i].type;
                strcpy ((char *) realname, (const char *) data[i].name);
				return (i);
			}
        }
    }
    return (-1);    // no element found!
}

S2 data_store::get_byte (U1 *name, U1 *value)
{
    S8 ind;
	S2 type;

	pthread_mutex_lock (&data_mutex);
    ind = find_element (name, type);
    if (ind > -1)
    {
		if (! (type == BYTE || type == STRING))
		{
			cout << "get_byte: element " << ind << "is not byte/string!" << endl;
			pthread_mutex_unlock (&data_mutex);
			return (1);
		}
		if (data[ind].size == 1)
		{
			value = data[ind].mem.byte;
		}
		else
		{
			strcpy ((char *) value, (const char *) data[ind].mem.byte);
		}
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}


S2 data_store::get_int64 (U1 *name, S8 &value)
{
    S8 ind;
	S2 type;

	pthread_mutex_lock (&data_mutex);
    ind = find_element (name, type);
    if (ind > -1)
    {
		if (type != QWORD)
		{
			cout << "get_int64: element " << ind << " is not int64!" << endl;
			pthread_mutex_unlock (&data_mutex);
			return (1);
		}
		value = data[ind].mem.qword;
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}

S2 data_store::get_double (U1 *name, F8 &value)
{
    S8 ind;
	S2 type;

	pthread_mutex_lock (&data_mutex);
    ind = find_element (name, type);
    if (ind > -1)
    {
		if (type != DOUBLE)
		{
			cout << "get_double: element " << ind << " is not double!" << endl;
			pthread_mutex_unlock (&data_mutex);
			return (1);
		}
		value = data[ind].mem.dfloat;
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}




// data remove ================================================================

S2 data_store::remove_byte (U1 *name, U1 *value)
{
    S8 ind;
	S2 type;

	pthread_mutex_lock (&data_mutex);
    ind = find_element (name, type);
    if (ind > -1)
    {
		if (! (type == BYTE || type == STRING))
		{
			cout << "get_byte: element " << ind << "is not byte/string!" << endl;
			pthread_mutex_unlock (&data_mutex);
			return (1);
		}

		if (data[ind].size == 1)
		{
			value = data[ind].mem.byte;
		}
		else
		{
			strcpy ((char *) value, (const char *) data[ind].mem.byte);
			delete data[ind].mem.byte;
		}
		data[ind].type = FREE;
		strcpy ((char *) data[ind].name, "");
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}

S2 data_store::remove_int64 (U1 *name, S8 &value)
{
    S8 ind;
	S2 type;

	pthread_mutex_lock (&data_mutex);
    ind = find_element (name, type);
    if (ind > -1)
    {
		if (type != QWORD)
		{
			cout << "get_int64: element " << ind << " is not int64!" << endl;
			pthread_mutex_unlock (&data_mutex);
			return (1);
		}
		value = data[ind].mem.qword;
		data[ind].type = FREE;
		strcpy ((char *) data[ind].name, "");
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}

S2 data_store::remove_double (U1 *name, F8 &value)
{
    S8 ind;
	S2 type;

	pthread_mutex_lock (&data_mutex);
    ind = find_element (name, type);
    if (ind > -1)
    {
		if (type != DOUBLE)
		{
			cout << "get_double: element " << ind << " is not double!" << endl;
			pthread_mutex_unlock (&data_mutex);
			return (1);
		}
		value = data[ind].mem.dfloat;
		data[ind].type = FREE;
		strcpy ((char *) data[ind].name, "");
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}

S2 data_store::data_get_info (U1 *name, U1 *realname, S8 &type)
{
    S8 ind;

    pthread_mutex_lock (&data_mutex);
    ind = find_element_realname (name, type, realname);
    if (ind > -1)
    {
		pthread_mutex_unlock (&data_mutex);
        return (0); // all ok
    }
    else
    {
		pthread_mutex_unlock (&data_mutex);
        return (1); // error
    }
}


// remove all data ============================================================

S2 data_store::remove_all (void)
{
    S8 i;

	for (i = 0; i < maxdata; i++)
    {
        if (data[i].type != FREE)
        {
			if (data[i].type == STRING)
			{
				delete data[i].mem.byte;
			}
		}
	}
	return (0);
}


// save/load database =========================================================

S2 data_store::save_database (U1 *filename)
{
    S8 i;
    ofstream file;
    file.open ((const char *) filename, ios::out);
    if (file.is_open ())
    {
        file.precision (200);
        file << "l1vm-data database" << endl;
        if (! file.good())
        {
            file.close ();
            return (1);
        }
        file << "maxdata: " << maxdata << endl;
        if (! file.good())
        {
            file.close ();
            return (1);
        }
        cout << "save_database: maxdata: " << maxdata << endl;
        for (i = 0; i < maxdata; i++)
        {
            if (data[i].type != FREE)
            {
                // save data name
                file << "dataname: " << data[i].name << endl;
                if (! file.good())
                {
                    file.close ();
                    return (1);
                }

                switch (data[i].type)
                {
                    case BYTE:
                        file << "datatype: BYTE" << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }

                        file << "datasize: 1" << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        file << "data: " << data[i].mem.byte << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        break;

                    case STRING:
                        file << "datatype: STRING" << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }

                        file << "datasize: " << data[i].size << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        file << "data: " << data[i].mem.byte << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        break;

                    case QWORD:
                        file << "datatype: INT64" << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }

                        file << "datasize: " << data[i].size << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        file << "data: " << data[i].mem.qword << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        break;

                    case DOUBLE:
                        file << "datatype: DOUBLE" << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }

                        file << "datasize: " << data[i].size << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        file << "data: " << data[i].mem.dfloat << endl;
                        if (! file.good())
                        {
                            file.close ();
                            return (1);
                        }
                        break;
                }
                // end of entry, write newline
                file << endl;
                if (! file.good())
                {
                    file.close ();
                    return (1);
                }
            }
        }
        file.close ();
        return (0);
    }
    else
    {
        // error file open
        cout << "save_database: ERROR can't save data base file: " << filename << endl;
        return (1);
    }
}

S2 data_store::load_database (U1 *filename)
{
    S8 maxdata_new;
    size_t pos;
    string buf;
    string command;
    string datav;   // read data
    fstream file;
    U1 read = 1;
    S2 data_type;
    string data_name;
    S8 data_size;
    S8 i = 0;

    file.open ((const char *) filename, ios::in);
    if (file.is_open ())
    {
        // check data base header
        getline (file, buf);
        if (! file.good ())
        {
            read = 0;
        }
        pos = buf.find ("l1vm-data database");
        if (pos == std::string::npos)
        {
            // header not found: ERROR
            cout << "load_database: ERROR can't open data base!" << endl;
            file.close ();
            return (1);
        }
        // read max data
        getline (file, buf);
        if (! file.good ())
        {
            read = 0;
        }
        pos = buf.find ("maxdata: ");
        if (pos == std::string::npos)
        {
            // max data not found: ERROR
            cout << "load_database: ERROR can't read maxdata!" << endl;
            file.close ();
            return (1);
        }

        datav = buf.substr (9, buf.length () - 9);    // get max data
        cout << "max data: '" << datav << "' pos: " << pos << endl;
        maxdata_new = std::stoll (datav);

        // check if data base size is big enough to load new data base
        if (maxdata_new  > size)
        {
            cout << "load_database: ERROR can't load new data base with: " << maxdata_new << " elements into space of: " << size << "!" << endl;
            file.close ();
            return (1);
        }

        while (read)
        {
            getline (file, buf);
            if (! file.good ())
            {
                read = 0;
                break;
            }
            if (i == maxdata_new)
            {
                cout << "load_database: ERROR data size to low to read data base!" << endl;
                file.close ();
                return (1);
            }
            pos = buf.find ("dataname: ");
            if (pos != std::string::npos)
            {
                datav = buf.substr (10, buf.length () - 10);
                data_name = datav;
            }

            pos = buf.find ("datatype: ");
            if (pos != std::string::npos)
            {
                data_type = FREE;         // the read in data type of current data
                datav = buf.substr (10, buf.length () - 10);
                if (datav.compare ("BYTE") == 0)
                {
                    data_type = BYTE;
                }
                if (datav.compare ("STRING") == 0)
                {
                    data_type = STRING;
                }
                if (datav.compare ("INT64") == 0)
                {
                    data_type = QWORD;
                }
                if (datav.compare ("DOUBLE") == 0)
                {
                    data_type = DOUBLE;
                }
                if (data_type == FREE)
                {
                    // data not of legal type ERROR!
                    cout << "load_database: ERROR: illegal data type: " << datav << endl;
                    file.close ();
                    return (1);
                }
            }

            pos = buf.find ("datasize: ");
            if (pos != std::string::npos)
            {
                datav = buf.substr (10, buf.length () - 9);
                data_size = std::stoll (datav);
                // data[i].size = data_size;
            }

            pos = buf.find ("data: ");
            if (pos != std::string::npos)
            {
                datav = buf.substr (6, buf.length () - 6);

                switch (data_type)
                {
                    case BYTE:
                        if (data_mem->store_byte ((U1 *) data_name.c_str (), (U1 *) datav.c_str()) != 0)
                        {
                            cout << "load_database: ERROR store byte!" << endl;
                            file.close ();
                            return (1);
                        }
                        break;

                    case STRING:
                        if (data_mem->store_byte ((U1 *) data_name.c_str (), (U1 *) datav.c_str()) != 0)
                        {
                            cout << "load_database: ERROR store string!" << endl;
                            file.close ();
                            return (1);
                        }
                        break;

                    case QWORD:
                        if (data_mem->store_int64 ((U1 *) data_name.c_str (), std::stoll (datav)) != 0)
                        {
                            cout << "load_database: ERROR store int64!" << endl;
                            file.close ();
                            return (1);
                        }
                        break;

                    case DOUBLE:
                        if (data_mem->store_double ((U1 *) data_name.c_str (), std::stod (datav)) != 0)
                        {
                            cout << "load_database: ERROR store double!" << endl;
                            file.close ();
                            return (1);
                        }
                        break;
                }
                i++;        // data element counter
            }

            cout << buf << endl;
        }
        file.close ();
        return (0);
    }
    else
    {
        // error file open
        cout << "load_database: ERROR can't read data base file: " << filename << endl;
        return (1);
    }
}

// socket handling functions ==================================================
void init_sockets (void)
{
    S2 i;
    pthread_mutex_lock (&data_mutex);
    for (i = 0; i < MAXSOCKETSCONN; i++)
    {
        // set socket as free, unused
        sockets[i].state = SOCKETCLOSED;
    }
    pthread_mutex_unlock (&data_mutex);
}

S2 get_free_socket (void)
{
    S2 i, free_socket = -1;
    pthread_mutex_lock (&data_mutex);
    for (i = 0; i < MAXSOCKETSCONN; i++)
    {
        if (sockets[i].state == SOCKETCLOSED)
        {
            // use empty socket
            free_socket = i;
            sockets[i].state = SOCKETOPEN;
            pthread_mutex_unlock (&data_mutex);
            return (free_socket);
        }
    }
    pthread_mutex_unlock (&data_mutex);
    return (free_socket);
}

void free_socket (S2 socket)
{
    pthread_mutex_lock (&data_mutex);
    sockets[socket].state = SOCKETCLOSED;
    pthread_mutex_unlock (&data_mutex);
}
// ============================================================================

// server socket open =========================================================
void *socket_conn_handler (void *socket_accept_v)
{
    S2 priv_sock = *(S2*) socket_accept_v;

    char buffer[STRINGLEN] = {0};
    U1 var_name[DATANAME];
    S8 var = 0;
    F8 var_d = 0.0;
    U1 var_str[STRINGLEN];

    U1 info_var_name[DATANAME];
    S8 info_type;
    U1 info_type_str[STRINGLEN];

    U1 run = 1;

    U1 file_name[STRINGLEN];  // data base save/load filename

    U1 data_str[STRINGLEN];
    U1 data_name[DATANAME];

    while (run)
    {
        // for socket read/write functions

        strcpy (buffer, "");    // empty buffer

        // read new command

        printf ("socket_conn_handler: %i running...\n", priv_sock);

    /*
        if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
        {
            perror ("read command string: string ACK");
            return ((void *) EXIT_FAILURE);
        }
    */
        if (socket_read_string (priv_sock, (U1 *) buffer, STRINGLEN) != 0)
        {
            perror ("read command string");
            printf ("read string ERROR!!\n");
            return ((void *) EXIT_FAILURE);
        }

        // printf ("> buffer: '%s'\n", buffer);

        // store byte/string ==================================================
        if (strcmp (buffer, "STORE STRING") == 0 || strcmp (buffer, "STORE BYTE") == 0)
        {
            // cout << "STORE STRING" << endl;

            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get string value
            if (socket_read_string (priv_sock, (U1 *) var_str, STRINGLEN) != 0)
            {
                perror ("read command string: string value");
                return ((void *) EXIT_FAILURE);
            }

            // store data
            if (data_mem->store_byte (var_name, var_str) == 0)
            {
                // all OK!
                if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                {
                    perror ("read command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("read command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // store int64 ========================================================
        if (strcmp (buffer, "STORE INT64") == 0)
        {
            // cout << "STORE INT64" << endl;

            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get string value
            if (socket_read_string (priv_sock, (U1 *) var_str, STRINGLEN) != 0)
            {
                perror ("read command string: string value");
                return ((void *) EXIT_FAILURE);
            }

            // store data
            sscanf ((const char *) var_str, "%lli", &var);
            if (data_mem->store_int64 (var_name, var) == 0)
            {
                // write string
                sprintf ((char *) var_str, "%lli", var);

                // DEBUG
                // cout << "DEBUG: l1vm-data: STORE INT64: string value: " << var_str << endl;

                // all OK!
                if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                {
                    perror ("read command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("read command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // store double =======================================================
        if (strcmp (buffer, "STORE DOUBLE") == 0)
        {
            // cout << "STORE DOUBLE"  << endl;

            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get string value
            if (socket_read_string (priv_sock, (U1 *) var_str, STRINGLEN) != 0)
            {
                perror ("read command string: string value");
                return ((void *) EXIT_FAILURE);
            }

            // store data
            sscanf ((const char *) var_str, "%lf", &var_d);
            if (data_mem->store_double (var_name, var_d) == 0)
            {
                // all OK!
                if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                {
                    perror ("read command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("read command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // get byte/string ====================================================
        if (strcmp (buffer, "GET STRING") == 0 || strcmp (buffer, "GET BYTE") == 0)
        {
            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get data
            if (data_mem->get_byte (var_name, var_str) == 0)
            {
                // all OK!

                // write string
                if (socket_write_string (priv_sock, (U1 *) var_str) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // get int64 ==========================================================
        if (strcmp (buffer, "GET INT64") == 0)
        {
            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get data
            if (data_mem->get_int64 (var_name, var) == 0)
            {
                // all OK!

                // write string
                sprintf ((char *) var_str, "%lli", var);

                if (socket_write_string (priv_sock, (U1 *) var_str) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // get double =========================================================
        if (strcmp (buffer, "GET DOUBLE") == 0)
        {
            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get data
            if (data_mem->get_double (var_name, var_d) == 0)
            {
                // all OK!

                // write string
                sprintf ((char *) var_str, "%10.10f", var_d);

                if (socket_write_string (priv_sock, (U1 *) var_str) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }


        // remove byte/string ====================================================
        if (strcmp (buffer, "REMOVE STRING") == 0 || strcmp (buffer, "REMOVE BYTE") == 0)
        {
            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get data
            if (data_mem->remove_byte (var_name, var_str) == 0)
            {
                // all OK!

                // write string
                if (socket_write_string (priv_sock, (U1 *) var_str) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // remove int64 ==========================================================
        if (strcmp (buffer, "REMOVE INT64") == 0)
        {
            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get data
            if (data_mem->remove_int64 (var_name, var) == 0)
            {
                // all OK!

                // write string
                sprintf ((char *) var_str, "%lli", var);

                // DEBUG
                // cout << "DEBUG: l1vm-data: REMOVE INT64: string value: " << var_str << endl;

                if (socket_write_string (priv_sock, (U1 *) var_str) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // remove double =========================================================
        if (strcmp (buffer, "REMOVE DOUBLE") == 0)
        {
            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get data
            if (data_mem->remove_double (var_name, var_d) == 0)
            {
                // all OK!

                // write string
                sprintf ((char *) var_str, "%10.10f", var_d);

                if (socket_write_string (priv_sock, (U1 *) var_str) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR!!
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string ACK");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // get data element info
        // get int64 ==========================================================
        if (strcmp (buffer, "GET INFO") == 0)
        {
            // get string name
            if (socket_read_string (priv_sock, (U1 *) var_name, DATANAME) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get data
            if (data_mem->data_get_info (var_name, info_var_name, info_type) == 0)
            {
                // all ok, write data elemnt real name
                if (socket_write_string (priv_sock, (U1 *) info_var_name) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }

                switch (info_type)
                {
                    case BYTE:
                    case STRING:
                        strcpy ((char *) info_type_str, "STRING");
                        break;

                    case QWORD:
                        strcpy ((char *) info_type_str, "INT64");
                        break;

                    case DOUBLE:
                        strcpy ((char *) info_type_str, "DOUBLE");
                        break;
                }

                // send data element type
                if (socket_write_string (priv_sock, (U1 *) info_type_str) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }

                // send OK
                if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // search for data
        // ====================================================================
        if (strcmp (buffer, "SEARCH DATA") == 0)
        {
            // get data string
            if (socket_read_string (priv_sock, (U1 *) data_str, STRINGLEN) != 0)
            {
                perror ("read command string: string data");
                return ((void *) EXIT_FAILURE);
            }

            if (data_mem->search_data (data_str, data_name) > -1)
            {
                // found data, send name
                if (socket_write_string (priv_sock, (U1 *) data_name) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }

                // send OK
                if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // data not found!
                // ERROR
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            strcpy (buffer, "");    // empty buffer
            continue;
        }

        // search for data and return list of entries
        // ====================================================================
        if (strcmp (buffer, "SEARCH DATA LIST") == 0)
        {
            {
                S8 entries = 0;
                S8 start_index = 0;
                U1 search_loop = 1;
                U1 sendbuf[STRINGLEN];
                U1 lengthstr[STRINGLEN];
                S8 maxdata;
                U1 data_name[STRINGLEN];
                U1 value[STRINGLEN];

                maxdata = data_mem->get_maxdata ();

                // get data string
                if (socket_read_string (priv_sock, (U1 *) data_str, STRINGLEN) != 0)
                {
                    perror ("read command string: string data");
                    return ((void *) EXIT_FAILURE);
                }

                // find out the number of data entries matching 
                while (search_loop == 1)
                {
                    start_index = data_mem->search_data_list (data_str, data_name, start_index);
                    if (start_index > -1)
                    {
                        entries++;
                        if (start_index < maxdata - 1)
                        {
                            start_index++;
                        }
                        else
                        {
                            search_loop = 0;
                        }
                    }
                    else 
                    {
                        // no more entry found, exit loop
                        search_loop = 0;
                    }
                }

                strcpy ((char *) sendbuf, "data list length = ");
                sprintf ((char *) lengthstr, "%lli", entries);
                strcat ((char *) sendbuf, (const char *) lengthstr);

                // send length string
                if (socket_write_string (priv_sock, (U1 *) sendbuf) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }

                // send the found data list
                search_loop = 1;
                start_index = 0;
                while (search_loop == 1)
                {
                    start_index = data_mem->search_data_list (data_str, data_name, start_index);
                    if (start_index > -1)
                    {
                        // cout << "found element: " << start_index << endl;

                        if (data_mem->get_data_name_value (start_index, data_name, value) == 0)
                        {
                            // got data sending it

                            strcpy ((char *) sendbuf, (const char *) data_name);
                            strcat ((char *) sendbuf, " = ");
                            strcat ((char *) sendbuf, (const char *) value);

                            if (socket_write_string (priv_sock, (U1 *) sendbuf) != 0)
                            {
                                perror ("write command string: string");
                                return ((void *) EXIT_FAILURE);
                            }
                        }

                        if (start_index < maxdata - 1)
                        {
                            start_index++;
                        }
                        else
                        {
                            search_loop = 0;
                        }
                    }
                    else 
                    {
                        // no more entry found, exit loop
                        search_loop = 0;

                        // send two newlines to mark data end
                        if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                        {
                            perror ("write command string: string");
                            return ((void *) EXIT_FAILURE);
                        }
                    }
                }
            }
        }

        // search for data and return list of entries
        // ====================================================================
        if (strcmp (buffer, "SEARCH NAME LIST") == 0)
        {
            {
                S8 entries = 0;
                S8 start_index = 0;
                U1 search_loop = 1;
                U1 sendbuf[STRINGLEN];
                U1 lengthstr[STRINGLEN];
                S8 maxdata;
                U1 data_name[STRINGLEN];
                U1 value[STRINGLEN];

                maxdata = data_mem->get_maxdata ();

                // get data string
                if (socket_read_string (priv_sock, (U1 *) data_str, STRINGLEN) != 0)
                {
                    perror ("read command string: string data");
                    return ((void *) EXIT_FAILURE);
                }

                // find out the number of data entries matching 
                while (search_loop == 1)
                {
                    start_index = data_mem->search_element_list (data_str, data_name, start_index);
                    if (start_index > -1)
                    {
                        entries++;
                        if (start_index < maxdata - 1)
                        {
                            start_index++;
                        }
                        else
                        {
                            search_loop = 0;
                        }
                    }
                    else 
                    {
                        // no more entry found, exit loop
                        search_loop = 0;
                    }
                }

                strcpy ((char *) sendbuf, "data list length = ");
                sprintf ((char *) lengthstr, "%lli", entries);
                strcat ((char *) sendbuf, (const char *) lengthstr);

                // send length string
                if (socket_write_string (priv_sock, (U1 *) sendbuf) != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }

                // send the found data list
                search_loop = 1;
                start_index = 0;
                while (search_loop == 1)
                {
                    start_index = data_mem->search_element_list (data_str, data_name, start_index);
                    if (start_index > -1)
                    {
                        // cout << "found element: " << start_index << endl;

                        if (data_mem->get_data_name_value (start_index, data_name, value) == 0)
                        {
                            // got data sending it

                            strcpy ((char *) sendbuf, (const char *) data_name);
                            strcat ((char *) sendbuf, " = ");
                            strcat ((char *) sendbuf, (const char *) value);

                            if (socket_write_string (priv_sock, (U1 *) sendbuf) != 0)
                            {
                                perror ("write command string: string");
                                return ((void *) EXIT_FAILURE);
                            }
                        }

                        if (start_index < maxdata - 1)
                        {
                            start_index++;
                        }
                        else
                        {
                            search_loop = 0;
                        }
                    }
                    else 
                    {
                        // no more entry found, exit loop
                        search_loop = 0;

                        // send two newlines to mark data end
                        if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                        {
                            perror ("write command string: string");
                            return ((void *) EXIT_FAILURE);
                        }
                    }
                }
            }
        }


        // check commands
        if (strcmp (buffer, "LOGOUT") == 0)
        {
            run = 0;
            printf ("> LOGOUT: OK\n");
            continue;
        }

        // save data base  ====================================================
        if (strcmp (buffer, "SAVE") == 0)
        {
            // get string file name
            if (socket_read_string (priv_sock, (U1 *) file_name, DATANAME) != 0)
            {
                perror ("read command string: file name");
                return ((void *) EXIT_FAILURE);
            }
            if (data_mem->save_database (file_name) == 0)
            {
                // send OK
                if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
        }

        // load data base =====================================================
        if (strcmp (buffer, "LOAD") == 0)
        {
            // get string file name
            if (socket_read_string (priv_sock, (U1 *) file_name, DATANAME) != 0)
            {
                perror ("read command string: file name");
                return ((void *) EXIT_FAILURE);
            }
            if (data_mem->load_database (file_name) == 0)
            {
                // send OK
                if (socket_write_string (priv_sock, (U1 *) "OK") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
            else
            {
                // ERROR
                if (socket_write_string (priv_sock, (U1 *) "ERROR") != 0)
                {
                    perror ("write command string: string");
                    return ((void *) EXIT_FAILURE);
                }
            }
        }
    }
    free_socket (priv_sock);
    pthread_exit ((void *) 0);
}

S2 check_client_ip_address (U1 *address_str)
{
    S8 ind;

    for (ind = 0; ind <= max_clientipaddresses; ind++)
    {
        if (strcmp ((const char *) address_str, clientipaddress_str[ind]) == 0)
        {
            // found client IP address in whitelist
            // return OK!
            return (0);
        }
    }
    return (1);     // client address not in whitelist, return error code!!
}

S2 open_server (S8 port)
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof (address);
    S2 curr_sock = -1;

    pthread_t thread_id[MAXSOCKETSCONN];

    // client socket address
    char clientaddr_str[INET_ADDRSTRLEN];

    // set all sockets as free
    init_sockets ();

	// Creating socket file descriptor
	if ((server_fd = socket (AF_INET, SOCK_STREAM, 0)) == 0)
	{
		perror ("socket failed");
		return (EXIT_FAILURE);
	}

	// Forcefully attaching socket to the port
	if (setsockopt (server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof (opt)))
	{
		perror ("setsockopt");
		return (EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons (port);

	// Forcefully attaching socket to the port
	if (bind (server_fd, (struct sockaddr *) &address, sizeof (address)) < 0)
	{
		perror ("bind failed");
		return (EXIT_FAILURE);
	}

	if (listen (server_fd, 3) < 0)
	{
		perror ("listen");
		return (EXIT_FAILURE);
    }

    while ((new_socket = accept (server_fd, (struct sockaddr *) &address, (socklen_t*) &addrlen)))
    {
        struct sockaddr_in pV4Addr = (struct sockaddr_in) address;
        struct in_addr ipAddr = pV4Addr.sin_addr;
        inet_ntop( AF_INET, &ipAddr, clientaddr_str, INET_ADDRSTRLEN );

        cout << "got connection from address: " << clientaddr_str << " : ";
        if (check_client_ip_address ((U1 *) clientaddr_str) != 0)
        {
            cout << " access denied!" << endl;
        }
        else
        {
            cout << " access allowed!" << endl;
            curr_sock = get_free_socket ();
            if (curr_sock > -1)
            {
                pthread_mutex_lock (&data_mutex);
                sockets[curr_sock].serv_conn = new_socket;
                if (pthread_create  (&thread_id[curr_sock], NULL, socket_conn_handler , (void*) &curr_sock) < 0)
                {
                    perror("could not create thread");
                    pthread_mutex_unlock (&data_mutex);
                    return (1);
                }
                pthread_mutex_unlock (&data_mutex);
            }
        }
    }
    return (0);
}


// ============================================================================

void break_handler (void)
{
	/* break - handling
	 *
	 * if user answer is 'y', the engine will shutdown
	 *
	 */

	U1 answ[2];

	printf ("\nexe: break,  exit now (y/n)? ");
	scanf ("%1s", answ);

	if (strcmp ((const char *) answ, "y") == 0 || strcmp ((const char *) answ, "Y") == 0)
	{
	    delete data_mem;
        cout << "freed data store!" << endl;
		exit (0);
	}
}

char *fgets_uni (char *str, int len, FILE *fptr)
{
    int ch, nextch;
    int i = 0, eol = FALSE;
    char *ret;

    ch = fgetc (fptr);
    if (feof (fptr))
    {
        return (NULL);
    }
    while (! feof (fptr) || i == len - 2)
    {
        switch (ch)
        {
            case '\r':
                /* check for '\r\n\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\n')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            case '\n':
                /* check for '\n\r\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\r')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            default:
				str[i] = ch;
				i++;
                
                break;
        }

        if (eol)
        {
            break;
        }

        ch = fgetc (fptr);
    }

    if (feof (fptr))
    {
        str[i] = '\0';
    }
    else
    {
        str[i] = '\0';
    }

    ret = str;
    return (ret);
}

// load config file with legal ip addresses
S2 load_config (U1 *filename)
{
    S8 ind = 0;
    U1 read = 1;
    FILE *file_handle;
    char buf[INET_ADDRSTRLEN];

    file_handle = fopen ((const char *) filename, "r");
    if (file_handle == NULL)
    {
        cout << "ERROR: can't open config file: " << filename << endl;
        return (1);
    }
    while (read)
    {
        if (fgets_uni (buf, INET_ADDRSTRLEN, file_handle) != NULL)
        {
            strcpy (clientipaddress_str[ind], buf);
            cout << "allow client IP address: '" << buf << "'" << endl;
            if (ind < MAX_CLIENT_IP_ADDRESSES - 1)
            {
                ind++;
            }
            else 
            {
                read = 0;
            }
        }
        else
        {
            // EOF
            max_clientipaddresses = ind;
            fclose (file_handle);
            return (0);
        }
    }
    max_clientipaddresses = ind;
    fclose (file_handle);
    return (0);
}

int main (int ac, char *av[])
{
    S8 i;

    // call break_handler on ctrl + C
    signal (SIGINT, (__sighandler_t) break_handler);

    cout << "l1vm-data 1.0.6 (C) 2021 Stefan Pietzonke" << endl;
    cout << "l1vm-data -s <size> -p <port>" << endl;
    cout << "open source version" << endl;

    // cout << "ac: " << ac << endl;

    if (ac > 1)
    {
        for (i = 1; i < ac; i++)
        {
            if (strcmp (av[i], "-s") == 0)
            {
                // cout << "-s i: " << i << endl;

                if (i < ac - 1)
                {
                    size = atoi (av[i + 1]);
                    cout << "size: " << size << endl;
                }
            }
            if (strcmp (av[i], "-p") == 0)
            {
                // cout << "-p: i: " << i << endl;

                if (i < ac - 1)
                {
                    port = atoi (av[i + 1]);
                    cout << "port: " << port << endl;
                }
            }
        }
    }

    if (load_config ((U1 *) "config.txt") != 0)
    {
        cout << "ERROR can't load config file! Can't set allowed client IP addresses!" << endl;
        exit (1);
    }

    // data_store* data_mem = new data_store (size);
	data_mem = new data_store (size, port);
    cout << "allocated: " << size << " elements for data storage" << endl;

	/*
	data_mem->store_int64 ((U1 *) "abcde-test-data1", (S8) 1234567890);
	data_mem->get_int64 ((U1 *) ".*test.*", value);
	cout << "got: " << value << endl;
	data_mem->remove_int64 ((U1 *) ".*test.*", value);
	data_mem->get_int64 ((U1 *) ".*test.*", value2);
	cout << "got: " << value2 << endl;
	*/

	cout << "opening server on port: " << port << " ..." << endl;
	open_server (port);

    delete data_mem;
    cout << "freed data store!" << endl;
    exit (0);
}
