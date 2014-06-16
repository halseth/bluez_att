#include <jansson.h>

/*
 * Creates json-string fro Xively server.
 */

char* create_json_string(char* id, char* value);

/*
 * Convert json-string to string nicely indented
 */
char* beautify_json_string(char* str);
