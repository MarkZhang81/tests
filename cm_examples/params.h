#ifndef CM_EXAMPLES_PARAMS_H
#define CM_EXAMPLES_PARAMS_H

#define LOG_LEVEL 1

/* Arguments below need to be set properly */

#define CM_EXAMPLE_CLIENT_IP "192.168.0.5"
#define CM_EXAMPLE_SERVER_IP "192.168.0.6"

#define CM_EXAMPLE_SERVER_PORT 7471
#define CM_EXAMPLE_SERVER_PORT_STR "7471"

/* Registered to SM */
#define CM_EXAMPLE_IB_SERVICE_ID "20913455"	/* 0x13f1d2f */
#define CM_EXAMPLE_IB_SERVICE_NAME "MyService"

/* port_space and port is defined in service_id:
 *   port_space = (sid >> 16) & 0xffffffff;
 *   port = sid & 0xffff;
 */
#define SERVER_ID_to_PORT_SPACE(sid) ((unsigned int)((sid >> 16) & 0xffffffff))
#define SERVER_ID_to_PORT(sid) ((unsigned int)(sid & 0xffff))


#define CM_EXAMPLE_CLIENT_GID "fe80:0000:0000:0000:0c42:a103:0002:e69b"
// #define CM_EXAMPLE_SERVER_GID "fe80:0000:0000:0000:0c42:a103:0002:e823"

#endif
