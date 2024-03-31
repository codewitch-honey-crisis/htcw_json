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
char name[2048];
char description[8192];
void read_episodes(json_reader_base& reader) {
    int ad = 0;
    if(reader.read() && reader.node_type()==json_node_type::array) {
        ad = reader.depth();
        while(true) {
            if(reader.depth()==ad && reader.node_type()==json_node_type::end_array) {
                break;
            }
            if(reader.read()&&reader.node_type()==json_node_type::object) {
                
                int od = reader.depth();
                int season = -1;
                int episode = -1;
                while(reader.read() && reader.depth()>=od) {
                    if(reader.depth()==od && reader.node_type()==json_node_type::field) {
                        if(0==strcmp("episode_number",reader.value()) && reader.read() && reader.node_type()==json_node_type::value) {
                            episode = reader.value_int();
                        }
                        if(0==strcmp("season_number",reader.value()) && reader.read() && reader.node_type()==json_node_type::value) {
                            season = reader.value_int();
                        }
                        if(0==strcmp("name",reader.value())) {
                            name[0]=0;
                            while(reader.read() && reader.is_value()) {
                                strcat(name,reader.value());
                            }
                        }
                        if(0==strcmp("overview",reader.value())) {
                            description[0]=0;
                            while(reader.read() && reader.is_value()) {
                                strcat(description,reader.value());
                            }
                        }
                    }
                }
                if(season>-1 && episode>-1 && name[0]) {
                    printf("S%02dE%02d %s\r\n",season,episode,name);
                    if(description[0]) {
                        printf("\t%s\r\n",description);
                    }
                    puts("");
                }
            }
        }
    }
}
void read_series(json_reader_base& reader) {
    while(reader.read()) {
        switch(reader.node_type()) {
            case json_node_type::field:
                if(0==strcmp("episodes",reader.value())) {
                    read_episodes(reader);
                }
                break;
            default:
                break;      
        }
    }
}

void setup() {
    Serial.begin(115200);
    SPIFFS.begin(false);
    File f_in = SPIFFS.open("/data.json","rb");
    file_stream stm(f_in);
    json_reader reader(stm);
    read_series(reader);
#if 0
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
#endif
}
void loop() {

}