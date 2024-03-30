#ifndef HTCW_JSON_HPP
#define HTCW_JSON_HPP
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <io_stream.hpp>
#include <io_lex_source.hpp>
namespace json {
using stream = io::stream;
enum struct json_node_type {
    error = -3,
    end_document = -2,
    initial = -1,
    value = 0,
    field = 1,
    array = 2,
    end_array = 3,
    object = 4,
    end_object = 5,
    value_part = 6,
    end_value_part = 7
};
enum struct json_value_type {
    none = 0,
    null = 1,
    boolean = 2,
    integer = 3,
    real = 4
};
enum struct json_error {
    none = 0,
    unterminated_object,
    unterminated_array,
    unterminated_string,
    unterminated_element,
    illegal_literal,
    illegal_character,
    field_too_long,
    field_missing_value
};
namespace {
    // implement std::move to limit dependencies on the STL, which may not be there
    template< class T > struct remove_reference      { typedef T type; };
    template< class T > struct remove_reference<T&>  { typedef T type; };
    template< class T > struct remove_reference<T&&> { typedef T type; };
    template <typename T>
    typename remove_reference<T>::type&& member_move(T&& arg) {
        return static_cast<typename remove_reference<T>::type&&>(arg);
    }
}
template<size_t CaptureSize=1024>
class json_reader_ex {
public:
    constexpr static const size_t capture_size = CaptureSize;
private:
    using ls_type = io::lex_source<CaptureSize>;
    ls_type m_source;
    int m_state;
    unsigned int m_depth;
    int m_error;
    int m_lex_state;
    bool m_lex_neg;
    int m_lex_sub;
    int32_t m_lex_accum;
    double m_real;
    long long m_int;
    json_value_type m_value_type;
    bool m_raw_strings;
    void do_move(json_reader_ex& rhs) {
        m_source = member_move(rhs.m_source);
        m_state = rhs.m_state;
        rhs.m_state = (int)json_node_type::initial;
        m_depth = rhs.m_depth;
        rhs.m_depth = 0;
        m_error = rhs.m_error;
        rhs.m_error = 0;
        m_lex_state = rhs.m_lex_state;
        m_lex_neg = rhs.m_lex_state;
        m_lex_sub = rhs.m_lex_sub;
        m_lex_accum = rhs.m_lex_accum;
        m_value_type = rhs.m_value_type;
        m_raw_strings = rhs.m_raw_strings;
    }
    json_reader_ex(const json_reader_ex& rhs)=delete;
    json_reader_ex& operator=(const json_reader_ex& rhs)=delete;
    void skip_whitespace() {
        while(m_source.current()==' ' ||
                m_source.current()=='\t' || m_source.current()=='\n') {
            if(!m_source.advance()) {
                break;
            }
        }
    }
    static uint8_t from_hex_char(int32_t hex) {
        if (':' > hex && '/' < hex)
            return (uint8_t)(hex - '0');
        if ('G' > hex && '@' < hex)
            return (uint8_t)(hex - '7'); // 'A'-10
        return (uint8_t)(hex - 'W'); // 'a'-10
    }
    static bool is_hex_char(int32_t hex) {
        return (':' > hex && '/' < hex) ||
            ('G' > hex && '@' < hex) ||
            ('g' > hex && '`' < hex);
    }
    bool lex_number() {
        switch(m_lex_state) {
            case 0:
                m_lex_neg = false;
                m_lex_sub = 0;
                m_lex_accum = 0;
                m_real = 0.0;
                m_int = 0;
                m_value_type = json_value_type::integer;
                if(m_source.current()=='0') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 2;
                    return true;
                }
                if(m_source.current()=='-') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_neg = true;
                    m_lex_state = 1;
                    return true;
                }
                if(m_source.current()>='1' && m_source.current()<='9') {
                    m_int=m_source.current()-'0';
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 8;
                    m_real = m_int;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 1:
                if(m_source.current()=='0') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 2;
                    return true;
                }
                if(m_source.current()>='1' && m_source.current()<='9') {
                    m_int=m_source.current()-'0';
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 8;
                    m_real = m_int;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 2:
                if(m_source.current()>='0' && m_source.current()<='9') {
                    m_int=m_source.current()-'0';
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 4;
                    m_real = m_int;
                    return true;
                }
                if(m_source.current()=='.') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 3;
                    m_lex_sub = 1; // frac part
                    return true;
                }
                if(m_source.current()=='E' || m_source.current()=='e') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 5;
                    m_lex_sub = 2;
                    return true;
                }
                if(m_lex_neg) {
                    m_real*=-1;
                    m_int*=-1;
                }
                // is already int
                // no more data
                return false;
            case 3:
                if(m_source.current()>='0' && m_source.current()<='9') {
                    double f = m_source.current()-'0';
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 4;
                    int i = m_lex_accum+1;
                    while(i--) {
                        f*=.1;
                    }
                    ++m_lex_accum;
                    m_real+=f;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 4:
                if(m_source.current()>='0' && m_source.current()<='9') {
                    if(m_lex_sub==1) {
                        double f = m_source.current()-'0';
                        int i = m_lex_accum+1;
                        while(i--) {
                            f*=.1;
                        }
                        ++m_lex_accum;
                        m_real+=f;
                    } else {
                        m_int*=10;
                        m_int += (m_source.current()-'0');
                        m_real = m_int;
                    }
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 4;
                    return true;
                }
                if(m_source.current()=='E' || m_source.current()=='e') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 5;
                    m_lex_sub = 2;
                    m_lex_accum = 0;
                    return true;
                }
                if(m_lex_neg) {
                    m_real*=-1;
                    m_int*=-1;
                }
                if(m_lex_sub==0) {
                    m_value_type = json_value_type::integer;
                } else {
                    m_value_type = json_value_type::real;
                }
                return false;
            case 5:
                if(m_source.current()>='0' && m_source.current()<='9') {
                    m_lex_accum*=10;
                    m_lex_accum += (m_source.current()-'0');
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 7;
                    
                    return true;
                }
                if(m_source.current()=='-') {
                    m_lex_state = 6;
                    m_lex_sub = 3;
                    return true;
                }
                if(m_source.current()=='+') {
                    m_lex_state = 6;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 6:
                if(m_source.current()>='0' && m_source.current()<='9') {
                    m_lex_accum*=10;
                    m_lex_accum += (m_source.current()-'0');
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 7;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 7:
                if(m_source.current()>='0' && m_source.current()<='9') {
                    m_lex_accum*=10;
                    m_lex_accum += (m_source.current()-'0');
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 7;
                    return true;
                }
                m_real*=pow(10,m_lex_accum);
                m_int*=pow(10,m_lex_accum);
                if(m_lex_neg) {
                    m_real*=-1;
                    m_int*=-1;
                }
                m_value_type = json_value_type::real;
                return false;
            case 8:
                if(m_source.current()>='0' && m_source.current()<='9') {
                    m_int*=10;
                    m_int += (m_source.current()-'0');
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 8;
                    m_real = m_int;
                    return true;
                }
                if(m_source.current()=='.') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_sub=1;
                    m_lex_state = 3;
                    return true;
                }
                if(m_source.current()=='E' || m_source.current()=='e') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 5;
                    return true;
                }
                if(m_lex_neg) {
                    m_real*=-1;
                    m_int*=-1;
                }
                m_value_type = json_value_type::real;
                return false;
            default:
                m_error = (int)json_error::illegal_literal;
                return false;
        }
        
    }
    bool lex_boolean() {
        switch(m_lex_state-9) {
            case 0:
                if(m_source.current()=='f') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_int = 0;
                    m_lex_state = 1+9;
                    return true;
                }
                if(m_source.current()=='t') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_int = 1;
                    m_lex_state = 6+9;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 1:
                if(m_source.current()=='a') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 2+9;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 2:
                if(m_source.current()=='l') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 3+9;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 3:
                if(m_source.current()=='s') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 4+9;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 4:
                if(m_source.current()=='e') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 5+9;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 5:
                m_value_type = json_value_type::boolean;
                return false;
            case 6:
                if(m_source.current()=='r') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 7+9;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 7:
                if(m_source.current()=='u') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 4+9;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            default:
                m_error = (int)json_error::illegal_literal;
                return false;
        }
    }
    bool lex_null() {
        switch(m_lex_state-16) {
            case 0:
                if(m_source.current()=='n') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 1+16;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 1:
                if(m_source.current()=='u') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 2+16;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 2:
                if(m_source.current()=='l') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 3+16;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 3:
                if(m_source.current()=='l') {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    m_lex_state = 4+16;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 4:
                m_value_type = json_value_type::null;
                return false;
            default:
                m_error = (int)json_error::illegal_literal;
                return false;
        }
    }
    bool lex_string() {
        switch(m_lex_state-21) {
            case 0:
                if(m_source.current()=='\"') {
                    if(m_raw_strings) {
                        m_source.capture(m_source.current());
                    }
                    m_source.advance();
                    m_lex_state = 1+21;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 1:
                if(!m_source.eof() && (m_source.current()!='\"' && m_source.current()!='\n' && m_source.current()!='\\')) {
                    m_source.capture(m_source.current());
                    m_source.advance();
                    // m_lex_state = 1+21;
                    return true;
                }
                if(m_source.current()=='\"') {
                    if(m_raw_strings) {
                        m_source.capture(m_source.current());
                    }
                    m_source.advance();
                    m_lex_state = 2+21;
                    return true;
                }
                if(m_source.current()=='\\') {
                    if(m_raw_strings) {
                        m_source.capture(m_source.current());
                    }
                    m_source.advance();
                    m_lex_state = 3+21;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 2:
                m_value_type = json_value_type::none;
                return false;
            case 3:
            
                switch(m_source.current()) {
                    case '\"':
                    case '/':
                    case '\\':
                        m_source.capture(m_source.current());
                        m_source.advance();
                        break;
                    case 'b':
                        if(m_raw_strings) {
                            m_source.capture(m_source.current());
                        } else {
                            m_source.capture('\b');
                        }
                        m_source.advance();
                        break;
                    case 'f':
                        if(m_raw_strings) {
                            m_source.capture(m_source.current());
                        } else {
                            m_source.capture('\f');
                        }
                        m_source.advance();
                        break;
                    case 'n':
                        if(m_raw_strings) {
                            m_source.capture(m_source.current());
                        } else {
                            m_source.capture('\n');
                        }
                        m_source.advance();
                        break;
                    case 'r':
                        if(m_raw_strings) {
                            m_source.capture(m_source.current());
                        } else {
                            m_source.capture('\r');
                        }
                        m_source.advance();
                        break;
                    case 't':
                        if(m_raw_strings) {
                            m_source.capture(m_source.current());
                        } else {
                            m_source.capture('\t');
                        }
                        m_source.advance();
                        break;
                    case 'u':
                        if(m_raw_strings) {
                            m_source.capture(m_source.current());
                        }
                        m_lex_accum = 0;
                        m_source.advance();
                        m_lex_state = 4+21;
                        return true;
                    default:
                        m_error = (int)json_error::illegal_literal;
                        return false;
                    
                }
            
                m_lex_state = 1+21;
                return true;
            case 4:
            case 5:
            case 6:
                if(is_hex_char(m_source.current())) {
                    if(m_raw_strings) {
                        m_source.capture(m_source.current());
                    } 
                    m_lex_accum*=0x10;
                    m_lex_accum |= from_hex_char(m_source.current());
                    m_source.advance();
                    ++m_lex_state;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            case 7:
                if(is_hex_char(m_source.current())) {
                    if(m_raw_strings) {
                            m_source.capture(m_source.current());
                    } 
                    m_lex_accum*=0x10;
                    m_lex_accum |= from_hex_char(m_source.current());
                    if(!m_raw_strings) {
                        m_source.capture(m_lex_accum);
                    }
                    m_source.advance();
                    m_lex_state=1;
                    return true;
                }
                m_error = (int)json_error::illegal_literal;
                return false;
            default:
                m_error = (int)json_error::illegal_literal;
                return false;
        }
    }
    bool skip_if_comma() {
        if(m_source.more() && ','==m_source.current()) {
            m_source.advance();
            skip_whitespace();
            if(m_source.eof()) {
                m_error = (int)json_error::unterminated_element;
                return false;
            }
        }
        return true;
    }

    bool read_any_open() {
        skip_whitespace();
        switch(m_source.current()) {
            case '[':
                if(!m_source.advance()) {
                    m_error = (int)json_error::unterminated_array;
                    return false;
                }
                skip_whitespace();
                if(!m_source.more()) {
                    m_error =  (int)json_error::unterminated_array;
                    return false;
                }
                m_state = (int)json_node_type::array;
                return true;
            case '{':
                if(!m_source.advance()) {
                    m_error = (int)json_error::unterminated_object;
                    return false;
                }
                skip_whitespace();
                if(!m_source.more()) {
                    m_error =  (int)json_error::unterminated_object;
                    return false;
                }
                m_state = (int)json_node_type::object;
                ++m_depth;
                return true;
            case '-':
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9': {
                m_source.clear_capture();
                m_lex_state = 0;
                bool more = false;
                while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_number()));
                if(more) {
                    m_state = (int)json_node_type::value_part;
                    return true;
                }
                else {
                    if(m_error==0) {
                        m_state = (int)json_node_type::value;
                        return true;
                    }
                }
                return false;
            }
            case 't':
            case 'f': {
                m_source.clear_capture();
                m_lex_state = 9;
                bool more = false;
                while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_boolean()));
                if(more) {
                    m_state = (int)json_node_type::value_part;
                    return true;
                }
                else {
                    if(m_error==0) {
                        m_state = (int)json_node_type::value;
                        return true;
                    }
                }
                return false;
            }
            case 'n': {
                m_source.clear_capture();
                m_lex_state = 16;
                bool more = false;
                while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_null()));
                if(more) {
                    m_state = (int)json_node_type::value_part;
                    return true;
                }
                else {
                    if(m_error==0) {
                        m_state = (int)json_node_type::value;
                        return true;
                    }
                }
                return false;
            }
            case '\"': {
                m_source.clear_capture();
                m_lex_state = 21;
                bool more = false;
                while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_string()));
                skip_whitespace();
                bool field = m_source.current()==':';
                if(more) {
                    if(field) {
                        m_error = (int)json_error::field_too_long;
                        return false;
                    }
                    m_state = (int)json_node_type::value_part;
                    return true;
                }
                else {
                    if(m_error==0) {
                        if(field) {
                            m_source.advance();
                            m_state = (int)json_node_type::field;
                        } else {
                            m_state = (int)json_node_type::value;
                        }
                        return true;
                    }
                }
                return false;
            }
        }
        return false;
    }
    bool read_field_or_end_object() {
        skip_whitespace();
        switch(m_source.current()) {
            case '}':
                --m_depth;
                m_source.advance();
                skip_whitespace();
                m_state = (int)json_node_type::end_object;
                return true;
            case '\"':
                m_source.clear_capture();
                m_lex_state = 21;
                bool more = false;
                while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_string()));
                if(more) {
                    m_error=(int)json_error::field_too_long;
                    return false;
                }
                else {
                    if(m_error!=0) { 
                        return false;
                    }
                    skip_whitespace();
                    if(m_source.current()!=':') {
                        m_error=(int)json_error::field_missing_value;
                        return false;
                    }
                    m_source.advance();
                    m_state = (int)json_node_type::field;
                    return true;
                
                }
                return false;
        }
        return false;
    }
    bool read_any() {
        if(!m_source.more()) {
            return false;
        }
        skip_whitespace();
        switch(m_source.current()) {
            case ']':
                m_source.advance();
                m_state = (int)json_node_type::end_array;
                return true;
            case '}':
                m_source.advance();
                if(m_depth==0) {
                    m_error = (int)json_error::illegal_character;
                    return false;
                }
                --m_depth;
                m_state = (int)json_node_type::end_object;
                return true;
        }
        if(!skip_if_comma()) {
            return false;
        }
        return read_any_open();
    }
    bool read_value_or_end_array() {
            skip_whitespace();
            switch(m_source.current()) {
                case ']':
                    m_source.advance();
                    skip_whitespace();
                    m_state = (int)json_node_type::end_array;
                    return true;
                default:
                    if(!read_any_open())
                        return false;
                    return true;
            }
        }
public:    
    json_reader_ex(stream& input) : m_source(&input),m_state((int)json_node_type::initial),m_depth(0), m_error(0),m_raw_strings(false) {

    }
    json_reader_ex(json_reader_ex&& rhs) {
        do_move(rhs);
    }
    json_reader_ex& operator=(json_reader_ex&& rhs) {
        do_move(rhs);
        return *this;
    }
    json_node_type node_type() const {
        return (json_node_type)m_state;
    }
    json_value_type value_type() const {
        if(m_state == (int)json_node_type::value ||
            /*m_state == (int)json_node_type::value_part ||*/ // don't have the whole value yet
            m_state == (int)json_node_type::end_value_part) {
            return (json_value_type)m_value_type;
        }
        return json_value_type::none;
    }
    long long value_int() const {
        json_value_type vt = value_type();
        if(vt==json_value_type::integer || vt==json_value_type::real) {
            return m_int;
        }
        if(vt==json_value_type::boolean) {
            return m_int!=0;
        }
        return 0;
    }
    double value_real() const {
        json_value_type vt = value_type();
        if(vt==json_value_type::integer || vt==json_value_type::real) {
            return m_real;
        }
        if(vt==json_value_type::boolean) {
            return m_int!=0;
        }
        return 0.0;
    }
    bool value_bool() const {
        json_value_type vt = value_type();
        if(vt==json_value_type::boolean || vt==json_value_type::integer || vt==json_value_type::real) {
            return m_int!=0;
        }
        return false;
    }
    const char* value() const {
        return m_source.const_capture_buffer();
    }
    bool is_value() const {
        return m_state == (int)json_node_type::value ||
            m_state == (int)json_node_type::value_part ||
            m_state == (int)json_node_type::end_value_part;
    }
    bool raw_strings() const {
        return m_raw_strings;
    }
    void raw_strings(bool value) {
        m_raw_strings = value;
    }
    unsigned int depth() const {
        return m_depth;
    }
    bool read() {
        if(m_error!=0) {
            return false;
        }
        if(!m_source.ensure_started() || !m_source.more()) {
            m_state = (int)json_node_type::end_document;
            return false;
        }
        switch((json_node_type)m_state) {
            case json_node_type::error:
            case json_node_type::end_document:
                return false;
            case json_node_type::initial:
                m_depth = 0;
                skip_whitespace();
                if(!read_any_open()) {
                    return false;
                }
                break;
            case json_node_type::value_part:
                // 0, 9, 16, 21
                if(m_lex_state>=21) {
                    m_source.clear_capture();
                    bool more = false;
                    while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_string()));
                    if(more) {
                        m_state = (int)json_node_type::value_part;
                        return true;
                    }
                    else {
                        if(m_error==0) {
                            m_state = (int)json_node_type::end_value_part;
                            return true;
                        }
                    }
                    return false;
                }
                else if(m_lex_state>=16) {
                    m_source.clear_capture();
                    bool more = false;
                    while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_null()));
                    if(more) {
                        m_state = (int)json_node_type::value_part;
                        return true;
                    }
                    else {
                        if(m_error==0) {
                            m_state = (int)json_node_type::end_value_part;
                            return true;
                        }
                    }
                    return false;
                }
                else if(m_lex_state>=9) {
                    m_source.clear_capture();
                    bool more = false;
                    while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_boolean()));
                    if(more) {
                        m_state = (int)json_node_type::value_part;
                        return true;
                    }
                    else {
                        if(m_error==0) {
                            m_state = (int)json_node_type::end_value_part;
                            return true;
                        }
                    }
                    return false;
                } else {
                    m_source.clear_capture();
                    bool more = false;
                    while(m_source.capture_size()<m_source.capture_capacity()-3 && (more=lex_number()));
                    if(more) {
                        m_state = (int)json_node_type::value_part;
                        return true;
                    }
                    else {
                        if(m_error==0) {
                            m_state = (int)json_node_type::end_value_part;
                            return true;
                        }
                    }
                    return false;
                }
            case json_node_type::value:
            case json_node_type::end_value_part:
            case json_node_type::end_array:
            case json_node_type::end_object:
                if(!read_any()) {
                    return false;
                }
                break;
            case json_node_type::array:
                if(!read_value_or_end_array()) {
                    return false;
                }
                break;
            case json_node_type::object:
                if(!read_field_or_end_object()) {
                    return false;
                }
                break;
            case json_node_type::field:
                if(!read_any_open()) {
                    return false;
                }
                break;
        }   
        return true;    
    }

};
using json_reader = json_reader_ex<1024>;
}

#endif // HTCW_JSON_HPP
