// protos string.c
size_t strlen_safe (const char * str, int maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
void convtabs (U1 *str);
S2 strip_end_commas (U1 *str);

// data.c
extern "C" S2 store_byte_c (U1 *name, U1 *string);
extern "C" S2 store_int64_c (U1 *name, S8 value);
extern "C" S2 store_double_c (U1 *name, F8 value);

extern "C" S2 get_byte_c (U1 *name, U1 *string);
extern "C" S2 get_int64_c (U1 *name, S8 value);
extern "C" S2 get_double_c (U1 *name, F8 value);

extern "C" S2 remove_byte_c (U1 *name, U1 *string);
extern "C" S2 remove_int64_c (U1 *name, S8 value);
extern "C" S2 remove_double_c (U1 *name, F8 value);

// C++
S2 store_byte (U1 *name, U1 *string);
S2 store_int64 (U1 *name, S8 value);
S2 store_double (U1 *name, F8 value);

S2 get_byte (U1 *name, U1 *string);
S2 get_int64 (U1 *name, S8 value);
S2 get_double (U1 *name, F8 value);

S2 remove_byte (U1 *name, U1 *string);
S2 remove_int64 (U1 *name, S8 value);
S2 remove_double (U1 *name, F8 value);


// net.c
S2 open_server (void);

// register.c
extern "C"
{
    S2 check_registered (U1 *code_filename);
}
