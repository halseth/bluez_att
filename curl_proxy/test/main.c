/*
 * main.c
 *
 *  Created on: Jun 16, 2014
 *      Author: didrik
 */

#include "unity/unity_fixture.h";

static void runAllTests() {
	RUN_TEST_GROUP(proxy);
}

int main(int argc, char* argv[]) {
	return UnityMain(argc, argv, runAllTests);
}
