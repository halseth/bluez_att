#include "curl_proxy.h"
#include "json_helper.h"
#include <pthread.h>

static char* xively_feed_uri = "https://api.xively.com/v2/feeds/2006458513.json";
static char* xively_feed_header =
		"X-ApiKey:aalkBaiFALoKopSXAVMv3DRqcMTagC9ooHI5CgdEfZPG5AHO";

struct string
{
	char* ptr;
	size_t len;
};

void init_string(struct string *s)
{
	s->len = 0;
	s->ptr = malloc(s->len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	size_t new_len = s->len + size * nmemb;
	s->ptr = realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr + s->len, ptr, size * nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size * nmemb;
}

struct callback_argument
{
	request_t* request;
	void (*callback)(request_t*);
};


static void* server_req(struct callback_argument *arg)
{
	request_t *req = arg->request;
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if (curl)
	{ // TODO: Add error checking/return value
		struct string resp_body_string, resp_header_string;
		init_string(&resp_body_string);
		init_string(&resp_header_string);

		// TODO make header parser
		struct curl_slist *header = NULL;
		if(req->http_header != NULL)
		{
			header = curl_slist_append(header, req->http_header);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
		}

		/* Set the URI of the server the request is being sent to */
		curl_easy_setopt(curl, CURLOPT_URL, req->uri);

		/* Sets where the response body and header is stored */
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_body_string);

		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp_header_string);

		/* HTTP request body is set in postfields */
		if (req->http_body != NULL)
		{
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->http_body);
		}

		/* Here the type of request (GET, PUT) is set */
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req->controlpoint);

		printf("Now performing %s request\n", req->controlpoint);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			printf("ERROR\n");
		}

		/* Populate the struct to be returned with response data */
		free(req->http_body);
		req->http_body = resp_body_string.ptr;
		req->http_header = resp_header_string.ptr;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(req->http_status_code));
		curl_slist_free_all(header);
	}

	curl_easy_cleanup(curl);

	printf("Returned:\n");
	//print_request(req);
	arg->callback(req);
	// TODO Remember to remove this when the other module is ready!
	free(req->http_body);
	free(req->http_header);
	free(arg);
	printf("Callback over\n");
	return NULL;
}


pthread_t add_server_request(request_t *req, void (*callback)(request_t*))
{
	struct callback_argument *arg = malloc(sizeof(struct callback_argument));
	arg->request = req;
	arg->callback = callback;
	pthread_t thread_id;
	int error = pthread_create(&thread_id, NULL, server_req, arg);
	if(0 != error)
	{
		printf("Couldn't run thread with id %u, errno %d\n", thread_id, error);
	}
	else
	{
		printf("Thread with id %u started\n", thread_id);
	}

	return thread_id;
}



void init_request(request_t *req)
{
	// TODO: Memory management
	req->controlpoint = NULL;
	req->http_body = NULL;
	req->http_header = NULL;
	req->uri = NULL;
	req->http_status_code = 0;
}
void free_request(request_t *req)
{
	//TODO: This does not work, because strings in req is not always dynamic
	//free(req->controlpoint);
	//free(req->http_body);
	//free(req->http_header);
	//free(req->uri);
}

void print_request(request_t *req)
{
	printf("URI:\n%s\n", req->uri);
	printf("HTTP header:\n%s\n", req->http_header);
	char* body = beautify_json_string(req->http_body);
	printf("HTTP body:\n%s\n", body);
	free(body);
	printf("Control point:\n%s\n", req->controlpoint);
}

/**
 * DUMMY function
 */

void callback_func(request_t *req)
{
	printf("Request returned with: \n");
	print_request(req);
}

int main(void)
{
	pthread_t t1, t2;

	request_t get_req;
	init_request(&get_req);
	get_req.uri = xively_feed_uri;
	get_req.http_header = xively_feed_header;
	get_req.controlpoint = "GET";

	printf("Now issuing this GET request:\n");
	print_request(&get_req);
	t1 = add_server_request(&get_req, callback_func);


	request_t put_req;
	init_request(&put_req);

	put_req.uri = xively_feed_uri;
	put_req.http_header = xively_feed_header;
	put_req.controlpoint = "PUT";
	put_req.http_body = create_json_string("1211", "211");

	printf("Now issuing this PUT request:\n");
	print_request(&put_req);
	t2 = 0;
	t2 = add_server_request(&put_req, callback_func);

	if(pthread_join(t1, NULL) || pthread_join(t2, NULL)) {

		fprintf(stderr, "Error joining thread\n");
		return 2;

	}

	printf("Main exit\n");
	return 0;
}
