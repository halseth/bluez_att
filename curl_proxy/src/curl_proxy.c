#include <stdio.h>
#include <unistd.h>
#include <curl/curl.h>
#include <stdlib.h>


static char* server_url = "192.168.14.89";
static long server_port = 1337;

struct peripheral
{
	int peripheral_handle;
	char* http_header;
	char* server_uri;
	char* http_body;
	char* control_point;
	int http_status_code;
};

struct string {
  char *ptr;
  size_t len;
};

void init_string(struct string *s) {
  s->len = 0;
  s->ptr = malloc(s->len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }
  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size*nmemb;
}

void peripheral_did_write_to_header(struct peripheral *peripheral, char* val)
{
	peripheral->http_header;
}

void peripheral_did_write_to_uri(struct peripheral *peripheral, char* val)
{
	peripheral->server_uri = val;
}

void peripheral_did_write_to_body(struct peripheral *peripheral, char* val)
{
	peripheral->http_body = val;
}

void peripheral_did_write_to_controlpoint(struct peripheral *peripheral, char* val)
{

}


int main(void)
{
	CURL *curl;
	CURLcode res;

	struct string resp_string;



	curl = curl_easy_init();
	while(curl)
	{
		init_string(&resp_string);
		curl_easy_setopt(curl, CURLOPT_URL, "example.com");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_string);
		printf("performing\n");
		res = curl_easy_perform(curl);
		printf("%s", resp_string.ptr);
		free(resp_string.ptr);
		if(res != CURLE_OK)
		{
			printf("ERROR\n");
			return 1;
		}
	}
	curl_easy_cleanup(curl);
	return 0;
}
