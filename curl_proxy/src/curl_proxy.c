#include "curl_proxy.h"
#include "json_helper.h"

CURL *curl;
CURLcode res;

static char* xively_feed_uri = "https://api.xively.com/v2/feeds/2006458513.json";
static char* xively_feed_header =
		"X-ApiKey:aalkBaiFALoKopSXAVMv3DRqcMTagC9ooHI5CgdEfZPG5AHO";

struct peripheral {
	int peripheral_handle;
	char* http_header;
	char* server_uri;
	char* http_body;
	char* control_point;
	int http_status_code;
};

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

void get_from_server(struct peripheral *peripheral) {
	struct string resp_string;
	init_string(&resp_string);

	struct curl_slist *header = NULL;
	// TODO make header parser
	header = curl_slist_append(header, peripheral->http_header);

	curl_easy_setopt(curl, CURLOPT_URL, peripheral->server_uri);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, peripheral->control_point);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_string);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

	printf("performing\n");
	res = curl_easy_perform(curl);

	printf("Respons:\n%s\n", resp_string.ptr);
	free(resp_string.ptr);

	if (res != CURLE_OK) {
		printf("ERROR\n");
	}
}

void put_to_server(struct peripheral* peri) {
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, peri->http_body);
	curl_easy_setopt(curl, CURLOPT_URL, peri->server_uri);
	struct curl_slist *header = NULL;
	header = curl_slist_append(header, peri->http_header);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
	res = curl_easy_perform(curl);
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
	printf("HTTP body: %s\n", req.http_body);

	/* Here the type of request (GET, PUT) is set */
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.controlpoint);

	printf("Now performing the %s request\n", req.controlpoint);
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
	req->controlpoint = empty_string();
	req->http_body = empty_string();
	req->http_header = empty_string();
	req->http_status_code = 0;
}

int main(void) {

	curl = curl_easy_init();
	if (curl) {
		/*struct peripheral peri;
		peri.server_uri = xively_feed_uri;
		peri.http_header = xively_feed_header;
		peri.control_point = "GET";
		get_from_server(&peri);
		curl_easy_reset(curl);

		char* msg_json = create_json_string("1211", "1337");
		printf("Sending body:\n%s \n", msg_json);
		peri.http_body = msg_json;
		put_to_server(&peri);
*/

		request_t request;
		init_request(&request);
		request.uri = xively_feed_uri;
		request.http_header = xively_feed_header;
		request.controlpoint = "GET";

		printf("Now issuing a GET request\n");
		request = server_req(request);
		printf("Returned:\n");
		printf("Status code:\n%ld\n", request.http_status_code);
		printf("Header:\n%s\n", request.http_header);
		printf("Body:\n%s", request.http_body);



	}

	curl_easy_cleanup(curl);
	return 0;
}
