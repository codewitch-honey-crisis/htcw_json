#include <Arduino.h>
#include <SPIFFS.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <string.h>
#include <json.hpp>
using namespace io;
using namespace json;
void indent(int spaces) {
    while(spaces--) Serial.print(' ');
}
void setup() {
    Serial.begin(115200);
    SPIFFS.begin(false);
    File f_in = SPIFFS.open("/data.json","rb");
    file_stream stm(f_in);
    json_reader reader(stm);
    // don't de-escape and dequote field names or string values:
    reader.raw_strings(true);
    bool first_part=true;
    int tabs = 0;
    bool skip_read = false;
    while(skip_read || reader.read()) {
        skip_read = false;
        switch(reader.node_type()) {
            case json_node_type::array:
                indent(tabs++);
                Serial.println("[");
                break;
            case json_node_type::end_array:
                indent(--tabs);
                Serial.println("]");
                break;
            case json_node_type::object:
                indent(tabs++);
                Serial.println("{");
                break;
            case json_node_type::end_object:
                indent(--tabs);
                Serial.println("}");
                break;
            case json_node_type::field:
                indent(tabs);
                Serial.printf("%s: ",reader.value());
                while(reader.read() && reader.is_value()) {
                    Serial.printf("%s",reader.value());
                }
                Serial.println("");
                skip_read = true;
                break;
            case json_node_type::value:
                indent(tabs);
                Serial.printf("%s\r\n",reader.value());
                break;
            case json_node_type::value_part:
                if(first_part) {
                    indent(tabs);
                    first_part = false;
                }
                Serial.printf("%s",reader.value());
                break;
            case json_node_type::end_value_part:
                Serial.printf("%s,\r\n",reader.value());               
                first_part = true;
                break;      
        }

    }
}
void loop() {

}