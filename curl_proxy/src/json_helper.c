#include "json_helper.h"


char* create_json_string(char* id, char* value) {
	json_t* json;
	json = json_pack("{sss[{ssss}]}", "version", "1.0.0", "datastreams", "id", id, "current_value", value);
	char* msg_json;
	msg_json = json_dumps(json, JSON_INDENT(4));
	json_delete(json);
	return msg_json;
}

char* beautify_json_string(char* str)
{
	if(str == NULL || strlen(str) == 0)
	{
		return NULL;
	}
	json_error_t error;
	json_t* json = json_loads(str, NULL, &error);
	if(!json)
	{
		printf("Error loading JSON\n");
		json_delete(json);
		return NULL;
	}
	char* msg_json;
	msg_json = json_dumps(json, JSON_INDENT(4));
	json_delete(json);
	return msg_json;
}
