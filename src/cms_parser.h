#ifndef CMS_PARSER_H
#define CMS_PARSER_H

#include <stdint.h>

typedef struct {
    char *key;
    char *value;
} KEY_VALUE;

#define CMS_FLAG_FORMAT_FOUND     0x01
#define CMS_FLAG_TRACK_FOUND      0x02
#define CMS_FLAG_STOP_WHEN_ERROR  0x04

typedef enum {
    CMS_PART_UNKNOWN = 0,
    CMS_PART_H264_i_FRAME,
    CMS_PART_H264_p_FRAME,
    CMS_PART_AUDIO,
    CMS_PART_JPG,
} CMS_PART_TYPE;


typedef enum {
    CMS_E_HEADER_FIELD = 0,
    CMS_E_HEADER_END,
    CMS_E_PART_HEADER,
    CMS_E_PART_HEADER_FIELD,
    CMS_E_PART_HEADER_END,
    CMS_E_PART_END,
    CMS_E_CHUNK,
    CMS_E_PARSE_FAIL,
} CMS_EVENT;

typedef enum {
    CMS_HEADER = 0,
    CMS_DETECT_BOUNDARY,
    CMS_PART_HEADER,
    CMS_FINISHED,
    CMS_FAIL,
} CMS_STATE;

class CMS_Parser;

typedef int (*cms_callback)(CMS_Parser *parser, CMS_EVENT event, void *data, int size, void *context);

class CMS_Parser {
public:
    uint8_t      flag   = 0;
    unsigned long shift = 0;

    CMS_PART_TYPE part_type   = CMS_PART_UNKNOWN;
    int   part_track  = -1;
    int   part_length = -1;
    long  part_ts     = -1;
    
    CMS_Parser(int buffer_length = 100);
    ~CMS_Parser();
    void reset();
    void parse(char *data, int len);
    int  emit(CMS_EVENT e, void *data = NULL, int size = 0);
    void set_callback(cms_callback cb, void *context);

private:
    char *line_buf         = NULL;
    int       line_buf_size = 0;
    int       line_buf_ptr = 0;
    CMS_STATE state        = CMS_HEADER;

    char cache[100]        = {0}; // for boundary
    char cached_len        = 0;
    
    uint8_t   is_first_part = true;
    void         *callback_context = NULL;
    cms_callback  callback         = NULL;

    char *boundary      = NULL;
    int   boundary_size = 0;
    int   boundary_ptr  = 0;

    int fail(const char *error);
};

#endif
