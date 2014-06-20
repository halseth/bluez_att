#include "curl_proxy.h"
#include "json_helper.h"
#include "thpool.h"

// Global variables
thpool_t* threadpool = NULL;
long TRANSFER_TIMEOUT_MS;

void free_response(response_t *resp);

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

typedef struct
{
	const request_t* request;
	void* (*callback)(response_t*);
} thread_argument;


static void* server_req(void *t)
{
	thread_argument *arg = (thread_argument*) t;
	const request_t *req = arg->request;
	response_t *resp = NULL;

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
		curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TRANSFER_TIMEOUT_MS);

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

		res = curl_easy_perform(curl);
		if (res != CURLE_OK) {
			printf("ERROR: %s\n", curl_easy_strerror(res));
		}

		/* Malloc and populate the struct to be returned with response data */
		resp = malloc(sizeof(response_t));
		resp->http_body = resp_body_string.ptr;
		resp->http_header = resp_header_string.ptr;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(resp->http_status_code));



		curl_slist_free_all(header);
	} else {
		printf("ERROR initializing curl handle.\n");
	}

	curl_easy_cleanup(curl);

	arg->callback(resp);
	free_response(resp);
	free(arg);
	return NULL;
}


void add_server_request(const request_t *req, void (*callback)(const response_t *, request_t *))
{
	if(threadpool == NULL)
	{
		printf("ERROR! Module not initialized\n");
		return;
	}
	thread_argument *arg = malloc(sizeof(thread_argument));
	arg->request = req;
	arg->callback = callback;
	int error = thpool_add_work(threadpool, server_req, arg);
	if(0 != error)
	{
		printf("Couldn't add work to the thread pool %d\n", error);
		free(arg);
	}

}



void init_request(request_t *req)
{
	req->controlpoint = NULL;
	req->http_body = NULL;
	req->http_header = NULL;
	req->uri = NULL;
}


void free_response(response_t *resp)
{
	free(resp->http_body);
	free(resp->http_header);
	free(resp);
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
void print_response(response_t *resp)
{
	printf("HTTP header:\n%s\n", resp->http_header);
	char* body = beautify_json_string(resp->http_body);
	printf("HTTP body:\n%s\n", body);
	free(body);
}


void initialize(int num_threads, long timeout_ms)
{
	TRANSFER_TIMEOUT_MS = timeout_ms;
	threadpool=thpool_init(num_threads);
}

void destroy()
{
	thpool_destroy(threadpool);
}
