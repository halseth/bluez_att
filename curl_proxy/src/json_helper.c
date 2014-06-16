#include "json_helper.h"


char* create_json_string(char* id, char* value) {
	json_t* json;
	json = json_pack("{sss[{ssss}]}", "version", "1.0.0", "datastreams", "id", id, "current_value", value);
	char* msg_json;
	msg_json = json_dumps(json, JSON_INDENT(4));
	return msg_json;
}

char* beautify_json_string(char* str)
{
	if(strlen(str) == 0)
	{
		char* empty = malloc(sizeof(char));
		empty[0] = '\0';
		return empty;
	}
	json_error_t error;
	json_t* json = json_loads(str, NULL, &error);
	if(!json)
	{
		printf("Error loading JSON\n");
		return NULL;
	}
	char* msg_json;
	msg_json = json_dumps(json, JSON_INDENT(4));
	return msg_json;
}
