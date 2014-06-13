#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <jansson.h>

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

void peripheral_did_write_to_header(struct peripheral *peripheral, char* val) {
	peripheral->http_header;
}

void peripheral_did_write_to_uri(struct peripheral *peripheral, char* val) {
	peripheral->server_uri = val;
}

void peripheral_did_write_to_body(struct peripheral *peripheral, char* val) {
	peripheral->http_body = val;
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

	printf("Respons:\n%s", resp_string.ptr);
	free(resp_string.ptr);

	if (res != CURLE_OK) {
		printf("ERROR\n");
	}
}

void peripheral_did_write_to_controlpoint(struct peripheral *peripheral,
		char* val) {
	peripheral->control_point = val;
	get_from_server(peripheral);
}

char* create_json_string(char* id, char* value) {
	json_t* json;
	json = json_pack("{sss[{ssss}]}", "version", "1.0.0", "datastreams", "id",
			id, "current_value", value);
//	json = json_pack("{ssss}", "id", id, "value", value);
	char* msg_json;
	msg_json = json_dumps(json, JSON_INDENT(4));
	return msg_json;
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

int main(void) {

	curl = curl_easy_init();
	if (curl) {
		struct peripheral peri;
		peri.server_uri = xively_feed_uri;
		peri.http_header = xively_feed_header;
		peri.control_point = "GET";
		get_from_server(&peri);
		curl_easy_reset(curl);

		char* msg_json = create_json_string("1211", "1337");
		printf("Sending body:\n%s \n", msg_json);
		peri.http_body = msg_json;
		put_to_server(&peri);

	}

	curl_easy_cleanup(curl);
	return 0;
}
