#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <stdlib.h>

/*
 * Initialize the module by making the worker threadpool
 */
void initialize(int num_threads, long connection_timeout_ms, long transfer_timout_ms);

/*
 * Will destroy all work assigned to this module.
 */
void destroy();

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
} request_t;

/*
 * Initialize the request struct
 */
void init_request(request_t *req);

typedef struct
{
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
	 * HTTP status. The status returned by the server.
	 */
	long http_status_code;
} response_t;

void free_response(response_t *resp);

/*
 * Do a server request (PUT or GET). Response are handled to the callback.
 */
void add_server_request(const request_t *req, void (*callback)(response_t *));
