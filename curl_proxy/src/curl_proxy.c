#include "curl_proxy.h"
#include "json_helper.h"

CURL *curl;
CURLcode res;

static char* xively_feed_uri = "https://api.xively.com/v2/feeds/2006458513.json";
static char* xively_feed_header =
		"X-ApiKey:aalkBaiFALoKopSXAVMv3DRqcMTagC9ooHI5CgdEfZPG5AHO";

struct string {
	char* ptr;
	size_t len;
};

void init_string(struct string *s) {
	s->len = 0;
	s->ptr = malloc(s->len + 1);
	if (s->ptr == NULL) {
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
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

request_t server_req(request_t req)
{
	struct string resp_body_string, resp_header_string;
	init_string(&resp_body_string);
	init_string(&resp_header_string);

	/* Make sure the curl handle is empty */
	curl_easy_reset(curl);

	// TODO make header parser
	struct curl_slist *header = NULL;
	header = curl_slist_append(header, req.http_header);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

	/* Set the URI of the server the request is being sent to */
	curl_easy_setopt(curl, CURLOPT_URL, req.uri);

	/* Sets where the response body and header is stored */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_body_string);

	curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writefunc);
	curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp_header_string);

	/* HTTP request body is set in postfields */
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.http_body);

	/* Here the type of request (GET, PUT) is set */
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.controlpoint);

	printf("Now performing %s request\n", req.controlpoint);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		printf("ERROR\n");
	}

	/* Populate the struct to be returned with response data */
	req.http_body = resp_body_string.ptr;
	req.http_header = resp_header_string.ptr;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &req.http_status_code);

	return req;
}

char* empty_string()
{
	char* str = malloc(sizeof(char));
	str[0] = '\0';
	return str;
}

void init_request(request_t *req)
{
	// TODO: Memory management
	req->controlpoint = empty_string();
	req->http_body = empty_string();
	req->http_header = empty_string();
	req->uri = empty_string();
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

int main(void) {

	curl = curl_easy_init();
	if (curl) {
		char* stt = empty_string();
		free(stt);
		request_t get_req;
		init_request(&get_req);
		get_req.uri = xively_feed_uri;
		get_req.http_header = xively_feed_header;
		get_req.controlpoint = "GET";

		printf("Now issuing this GET request:\n");
		print_request(&get_req);
		get_req = server_req(get_req);
		printf("Returned:\n");
		print_request(&get_req);

		request_t put_req;
		init_request(&put_req);

		put_req.uri = xively_feed_uri;
		put_req.http_header = xively_feed_header;
		put_req.controlpoint = "PUT";
		put_req.http_body = create_json_string("1211", "211");

		printf("Now issuing this PUT request:\n");
		print_request(&put_req);
		put_req = server_req(put_req);
		printf("Returned:\n");
		print_request(&put_req);
	}

	curl_easy_cleanup(curl);
	return 0;
}
