/*
 * basic_test.c
 *
 *  Created on: Jun 17, 2014
 *      Author: didrik
 */

//test harness include
#include "unity/unity_fixture.h"
#include "../src/curl_proxy.h"

/**
 * Timeout in seconds
 */
#define TIMEOUT 1

static char* MOCKY_BODY = "{ \"hello\": \"world\" }";
static char* MOCKY_200 = "http://www.mocky.io/v2/53a02fa1a7039ffd173fcbc1";
static char* MOCKY_404 = "http://www.mocky.io/v2/53a02c75a7039fc6173fcbbf";
static char* MOCKY_301 = "http://www.mocky.io/v2/53a02cc0a7039fc9173fcbc0";
int CALLBACK_CALLED = 0;


void CALLBACK_200(response_t *resp)
{
	printf("IN CALLBACK200\n");
	TEST_ASSERT_EQUAL_STRING(MOCKY_BODY, resp->http_body);
}

void CALLBACK_404(response_t *resp)
{
	printf("IN CALLBACK404\n");
}
void CALLBACK_301(response_t *resp)
{
	printf("IN CALLBACK301\n");
}

void CALLBACK(response_t *resp)
{
	printf("IN CALLBACK TEST\n");
	CALLBACK_CALLED = 1;
}

request_t init_req(char* uri, char* header, char*cp)
{
	request_t req;
	init_request(&req);
	req.uri = uri;
	req.http_header = header;
	req.controlpoint = cp;

	return req;
}

TEST_GROUP(proxy);

//Define file scope data accessible to test group members prior to TEST_SETUP.
TEST_SETUP(proxy)
{
	//initialization steps are executed before each TEST

}

TEST_TEAR_DOWN(proxy)
{
	//clean up steps are executed after each TEST

}

TEST(proxy, getTest)
{
	request_t get_req;
	get_req = init_req(MOCKY_200, "", "GET");
	add_server_request(&get_req, CALLBACK_200);
	sleep(TIMEOUT);

}

//There can be many tests in a TEST_GROUP
TEST(proxy, putTest)
{
	request_t put_req;
	put_req = init_req(MOCKY_200, "", "PUT");
	add_server_request(&put_req, CALLBACK_200);
	sleep(TIMEOUT);

}


TEST(proxy, test404Test)
{
	request_t put_req;
	put_req = init_req(MOCKY_404, "", "PUT");
	add_server_request(&put_req, CALLBACK_404);
	sleep(TIMEOUT);

}


TEST(proxy, callbackTest)
{
	request_t req;

	// 200
	CALLBACK_CALLED = 0;
	req = init_req(MOCKY_200, "", "GET");
	add_server_request(&req, CALLBACK);
	sleep(TIMEOUT);
	TEST_ASSERT_EQUAL_INT(1, CALLBACK_CALLED);

	CALLBACK_CALLED = 0;
	req = init_req(MOCKY_200, "", "PUT");
	add_server_request(&req, CALLBACK);
	sleep(TIMEOUT);
	TEST_ASSERT_EQUAL_INT(1, CALLBACK_CALLED);

	// 301
	CALLBACK_CALLED = 0;
	req = init_req(MOCKY_301, "", "GET");
	add_server_request(&req, CALLBACK);
	sleep(TIMEOUT);
	TEST_ASSERT_EQUAL_INT(1, CALLBACK_CALLED);

	CALLBACK_CALLED = 0;
	req = init_req(MOCKY_301, "", "PUT");
	add_server_request(&req, CALLBACK);
	sleep(TIMEOUT);
	TEST_ASSERT_EQUAL_INT(1, CALLBACK_CALLED);

	// 404
	CALLBACK_CALLED = 0;
	req = init_req(MOCKY_404, "", "GET");
	add_server_request(&req, CALLBACK);
	sleep(TIMEOUT);
	TEST_ASSERT_EQUAL_INT(1, CALLBACK_CALLED);

	CALLBACK_CALLED = 0;
	req = init_req(MOCKY_404, "", "PUT");
	add_server_request(&req, CALLBACK);
	sleep(TIMEOUT);
	TEST_ASSERT_EQUAL_INT(1, CALLBACK_CALLED);






}

//Each group has a TEST_GROUP_RUNNER
TEST_GROUP_RUNNER(proxy)
{
	//Each TEST has a corresponding RUN_TEST_CASE
	initialize(10, 1000);
	RUN_TEST_CASE(proxy, getTest);
	RUN_TEST_CASE(proxy, putTest);
	RUN_TEST_CASE(proxy, test404Test);
	RUN_TEST_CASE(proxy, callbackTest);
	destroy();

}

