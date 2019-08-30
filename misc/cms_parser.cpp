#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cms_parser.h"


CMS_Parser::CMS_Parser(int buffer_length)
{
    // boundary = strdup("\r\n----asdfasdf\r\n");
    // boundary_size = strlen(boundary);
    line_buf = (char*)malloc(buffer_length);
    line_buf_size = buffer_length;
};
CMS_Parser::~CMS_Parser()
{
    if (boundary) { free(boundary); };
    if (line_buf) { free(line_buf); };
}
int CMS_Parser::fail(const char *error)
{
    emit(CMS_E_PARSE_FAIL, (void*)error, strlen(error));
    if (flag & CMS_FLAG_STOP_WHEN_ERROR) {
        state = CMS_FAIL;
        return true;
    } else {
        state = CMS_DETECT_BOUNDARY;
        return false;
    }
}
void CMS_Parser::reset()
{
    flag   = 0;
    shift = 0;

    part_type   = CMS_PART_UNKNOWN;
    part_track  = -1;
    part_length = -1;
    part_ts     = -1;
    if (boundary) {
        free(boundary);
        boundary = NULL;
    };
    boundary_size = 0;
    boundary_ptr  = 0;
    
    memset(line_buf, 0, line_buf_size);
    line_buf_ptr = 0;
    state        = CMS_HEADER;
    memset(cache, 0, sizeof(cache));
    cached_len        = 0;
    is_first_part = true;
}
void CMS_Parser::parse(char *data, int len)
{
    char *chunk  = data;
    for (int i = 0; i < len; i++) {
        char ch = data[i];
        shift++;
        switch (state) {
            case CMS_FAIL: {
                if (fail("CMS parse stoped.")) {
                    return;
                }
                break;
            };
            case CMS_HEADER: {
                if (ch == '\r') {
                } else if (ch == '\n') {
                    if (line_buf_ptr == 0) {
                        emit(CMS_E_HEADER_END);
                        if (!boundary) {
                            if (fail("Boundary not found.")) {
                                return;
                            }
                        } else if (!(flag & CMS_FLAG_FORMAT_FOUND)) {
                            if (fail("Format not found.")) {
                                return;
                            }
                        } else if (!(flag & CMS_FLAG_TRACK_FOUND)) {
                            if (fail("Track not found.")) {
                                return;
                            }
                        } else {
                            state = CMS_DETECT_BOUNDARY;
                            boundary_ptr = 2;
                            chunk = data + i + 1;
                        }
                    } else {
                        char *p     = line_buf;
                        char *key   = strsep(&p, ":");
                        char *value = p;
                        if (key && value) {
                            if (value[0] == ' ') { value++; };
                            if (strcmp(key, "boundary") == 0) {
                                if (boundary) {
                                    free(boundary);
                                }
                                char buf[100];
                                sprintf(buf, "\r\n--%s\r\n", value);
                                boundary = strdup(buf);
                                boundary_size = strlen(buf);
                            } else if (strcmp(key, "format") == 0
                                       && strcmp(value, "cms") == 0) {
                                flag = flag | CMS_FLAG_FORMAT_FOUND;
                            } else if (strcmp(key, "track") == 0) {
                                flag = flag | CMS_FLAG_TRACK_FOUND;
                            } 
                            KEY_VALUE kv = {
                                .key   = key,
                                .value = value,
                            };
                            emit(CMS_E_HEADER_FIELD, &kv, sizeof(kv));
                        } else {
                            if (fail("Invalid key value.")) {
                                return;
                            }
                        } 
                    }
                    line_buf_ptr = 0;
                } else {
                    if (line_buf_ptr < (line_buf_size - 1)) {
                        line_buf[line_buf_ptr] = ch;
                        line_buf_ptr++;
                        line_buf[line_buf_ptr] = 0;
                    } else {
                        if (fail("Header line too long.")) {
                            return;
                        }
                    }
                }
                break;
            };
            case CMS_DETECT_BOUNDARY: {
                if (!boundary) {
                    break;
                }
                if (ch == boundary[boundary_ptr]) {
                    boundary_ptr++;
                } else {
                    if (cached_len > 0) {
                        emit(CMS_E_CHUNK, cache, cached_len);
                        cached_len = 0;
                    }
                    boundary_ptr = 0;
                }
                if (boundary_ptr == boundary_size) {
                    if (data + i - chunk >= (boundary_size - cached_len)) {
                        int chunk_len = data + i - chunk - (boundary_size - cached_len) + 1;
                        emit(CMS_E_CHUNK, chunk, chunk_len);
                    }
                    cached_len = 0;
                    if (is_first_part) {
                        is_first_part = false;
                    } else {
                        emit(CMS_E_PART_END);
                    }
                    emit(CMS_E_PART_HEADER);

                    state = CMS_PART_HEADER;
                    part_type = CMS_PART_UNKNOWN;
                    part_track = -1;
                    part_length = 0;
                    part_ts = -1;
                    chunk = data + i + 1;
                    boundary_ptr = 0;
                    line_buf_ptr = 0;
                    line_buf[0]  = 0;
                }
                break;
            };
            case CMS_PART_HEADER: {
                if (ch == '\r') {
                } else if (ch == '\n') {
                    if (line_buf_ptr == 0) {
                        emit(CMS_E_PART_HEADER_END);
                        state = CMS_DETECT_BOUNDARY;
                        chunk = data + i + 1;
                    } else {
                        char *p     = line_buf;
                        char *key   = strsep(&p, ":");
                        char *value = p;
                        if (key && value) {
                            if (value[0] == ' ') {
                                value++;
                            }
                            if (strcmp(key, "f") == 0) {
                                if (strcmp(value, "i") == 0) {

                                    part_type = CMS_PART_H264_i_FRAME;
                                } else if (strcmp(value, "p") == 0) {
                                    part_type = CMS_PART_H264_p_FRAME;
                                } else if (strcmp(value, "a") == 0) {
                                    part_type = CMS_PART_AUDIO;
                                } else if (strcmp(value, "j") == 0) {
                                    part_type = CMS_PART_JPG;
                                }
                            } else if (strcmp(key, "ts") == 0) {
                                part_ts = strtol(value, NULL, 10);
                            } else if (strcmp(key, "l") == 0) {
                                part_length = strtol(value, NULL, 10);
                            } else if (strcmp(key, "t") == 0) {
                                part_track = strtol(value, NULL, 10);
                            }
                            KEY_VALUE kv = {
                                .key = key,
                                .value = value,
                            };
                            emit(CMS_E_PART_HEADER_FIELD, &kv, sizeof(kv));
                        } else {
                            if (fail("Part header Invalid key value.")) {
                                return;
                            }
                        }
                    }
                    line_buf_ptr = 0;
                } else {
                    if (line_buf_ptr < (line_buf_size - 1)) {
                        line_buf[line_buf_ptr] = ch;
                        line_buf_ptr++;
                        line_buf[line_buf_ptr] = 0;
                    } else {
                        if (fail("Part header line too long.")) {
                            return;
                        }
                    }
                }
                break;
            };
        }
    }
    if (state == CMS_DETECT_BOUNDARY) {
        if (chunk < (data + len)) {
            if (boundary_ptr > 0) {
                cached_len = boundary_ptr;
                int pos = len - boundary_ptr;
                if (pos >= 0) {
                    memcpy(cache, data + pos, cached_len);
                }
            }
            emit(CMS_E_CHUNK, chunk, data + len - chunk - boundary_ptr);
        }
    }
};
void CMS_Parser::set_callback(cms_callback cb, void *context)
{
    callback         = cb;
    callback_context = context;
};
int CMS_Parser::emit(CMS_EVENT e, void *data, int size)
{
    if (callback != NULL) {
        return callback(this, e, data, size, callback_context);
    } else {
        return -1;
    }
};
