#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <stdlib.h>

typedef struct
{
	/*
	 * The server request URI. E.g: https://api.xively.com/v2/feeds/2006458513
	 */
	char* uri;

	/*
	 * HTTP header. E.g.: X-ApiKey:aalkBaiFALoKopSXAVMv3DRqcMTagC9ooHI5CgdEfZPG5AHO
	 */
	char* http_header;

	/*
	 * HTTP body. E.g. json data:
	 * 	{
     *		"id": "1211",
     *		"current_value": "1337"
     *	}
	 */
	char* http_body;

	/*
	 * Controlpoint. Used to determine the request type.
	 * E.g: "GET", "PUT".
	 */
	char* controlpoint;

	/*
	 * HTTP status. The status returned by the server.
	 */
	long http_status_code;
} request_t;

/*
 * Initialize the request struct
 */
void init_request(request_t *req);

/*
 * Do a server request (PUT or GET). Response are found in return struct's data fields.
 */
request_t server_req(request_t req);
