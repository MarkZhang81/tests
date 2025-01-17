#ifndef CM_EXAMPLES_PARAMS_H
#define CM_EXAMPLES_PARAMS_H

#define LOG_LEVEL 1

/* Arguments below need to be set properly */

/* Registered to SM */
#define CM_EXAMPLE_IB_SERVICE_ID "20913455"	/* 0x13f1d2f */
#define CM_EXAMPLE_IB_SERVICE_NAME "MyService"

#define CM_EXAMPLE_CLIENT_GID "fe80:0000:0000:0000:0c42:a103:0002:e69b"
// #define CM_EXAMPLE_SERVER_GID "fe80:0000:0000:0000:0c42:a103:0002:e823"


#define CM_EXAMPLE_CLIENT_IP "192.168.0.5"
#define CM_EXAMPLE_SERVER_IP "192.168.0.6"

/* XXX: For resolve IB service test, thie port must be the low-16 bit
 *      of ID of the registered service id
 */
#define CM_EXAMPLE_SERVER_PORT 7471
#define CM_EXAMPLE_SERVER_PORT_STR "7471"

#endif
