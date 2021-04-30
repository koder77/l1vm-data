// main.cpp
//
// to connect manualy to data bank:
// nc localhost 2020

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

// crypto stuff
#include <openssl/sha.h>

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

#define DATANAME 128

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

    // data info
    S2 data_get_info (U1 *name, U1 *realname, S8 &type);
    S2 find_element_realname (U1 *name, S8 &type, U1 *realname);

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
        delete data;
    }
};

// declare global pointer
data_store* data_mem;

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

    size = strlen_safe ((const char *) string, 512);

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

// wrapper for C ==============================================================

extern "C" S2 store_byte_c (U1 *name, U1 *string)
{
    return (data_mem->store_byte (name, string));
}

extern "C" S2 store_int64_c (U1 *name, S8 value)
{
    return (data_mem->store_int64 (name, value));
}

extern "C" S2 store_double_c (U1 *name, F8 value)
{
    return (data_mem->store_double (name, value));
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

// wrapper for C ==============================================================

extern "C" S2 get_byte_c (U1 *name, U1 *string)
{
    return (data_mem->get_byte (name, string));
}

extern "C" S2 get_int64_c (U1 *name, S8 value)
{
    return (data_mem->get_int64 (name, value));
}

extern "C" S2 get_double_c (U1 *name, F8 value)
{
    return (data_mem->get_double (name, value));
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


// wrapper for C ==============================================================

extern "C" S2 remove_byte_c (U1 *name, U1 *string)
{
    return (data_mem->remove_byte (name, string));
}

extern "C" S2 remove_int64_c (U1 *name, S8 value)
{
    return (data_mem->remove_int64 (name, value));
}

extern "C" S2 remove_double_c (U1 *name, F8 value)
{
    return (data_mem->remove_double (name, value));
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

    char buffer[1024] = {0};
    U1 var_name[512];
    S8 var = 0;
    F8 var_d = 0.0;
    U1 var_str[512];

    U1 info_var_name[512];
    S8 info_type;
    U1 info_type_str[512];

    U1 run = 1;

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
        if (socket_read_string (priv_sock, (U1 *) buffer, 1024) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get string value
            if (socket_read_string (priv_sock, (U1 *) var_str, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get string value
            if (socket_read_string (priv_sock, (U1 *) var_str, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
            {
                perror ("read command string: string name");
                return ((void *) EXIT_FAILURE);
            }

            // get string value
            if (socket_read_string (priv_sock, (U1 *) var_str, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
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
            if (socket_read_string (priv_sock, (U1 *) var_name, 512) != 0)
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

        // check commands
        if (strcmp (buffer, "LOGOUT") == 0)
        {
            run = 0;
            printf ("> LOGOUT: OK\n");
            continue;
        }
    }
    free_socket (priv_sock);
    pthread_exit ((void *) 0);
}

S2 open_server (S8 port)
{
	int server_fd, new_socket;
	struct sockaddr_in address;
	int opt = 1;
	int addrlen = sizeof (address);
    S2 curr_sock = -1;

    pthread_t thread_id[MAXSOCKETSCONN];

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
        curr_sock = get_free_socket ();
        if (curr_sock > -1)
        {
            pthread_mutex_lock (&data_mutex);
            sockets[curr_sock].serv_conn = new_socket;
            if (pthread_create  (&thread_id[curr_sock], NULL, socket_conn_handler , (void*) &curr_sock) < 0)
            {
                perror("could not create thread");
                return (1);
            }
            pthread_mutex_unlock (&data_mutex);
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

int main (int ac, char *av[])
{
    S8 size = 200;
    S8 port = 2020;
    S8 i;

    // call break_handler on ctrl + C
    signal (SIGINT, (__sighandler_t) break_handler);

    cout << "l1vm-data 1.0.3 (C) 2021 Stefan Pietzonke" << endl;
    cout << "l1vm-data -s <size> -p <port>" << endl;
    cout << "open source version" << endl;

    if (ac > 1)
    {
        for (i = 1; i < ac; i++)
        {
            if (strcmp (av[i], "-s") == 0)
            {
                size = atoi (av[i + 1]);
                cout << "size: " << size << endl;
            }
            if (strcmp (av[i], "-p") == 0)
            {
                port = atoi (av[i + 1]);
                cout << "port: " << port << endl;
            }
        }
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
