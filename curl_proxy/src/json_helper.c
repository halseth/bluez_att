#include "json_helper.h"


char* create_json_string(char* id, char* value) {
	json_t* json;
//	json = json_pack("{sss[{ssss}]}", "version", "1.0.0", "datastreams", "id", id, "current_value", value);
	json = json_pack("{ssss}", "id", id, "value", value);
	char* msg_json;
	msg_json = json_dumps(json, JSON_INDENT(4));
	return msg_json;
}
