


function CMS()
{
    this.CMS_FLAG_FORMAT_FOUND     = 0x01;
    this.CMS_FLAG_TRACK_FOUND      = 0x02;
    this.CMS_FLAG_STOP_WHEN_ERROR  = 0x04;

    this.CMS_PART_UNKNOWN       = 'na';
    this.CMS_PART_H264_i_FRAME  = 'i';
    this.CMS_PART_H264_p_FRAME  = 'p';
    this.CMS_PART_AUDIO         = 'a';
    this.CMS_PART_JPG           = 'j';


    this.CMS_E_HEADER_FIELD      = 0;
    this.CMS_E_HEADER_END        = 1;
    this.CMS_E_PART_HEADER       = 2;
    this.CMS_E_PART_HEADER_FIELD = 3;
    this.CMS_E_PART_HEADER_END   = 4;
    this.CMS_E_PART_END          = 5;
    this.CMS_E_CHUNK             = 6;
    this.CMS_E_PARSE_FAIL        = 7;


    
    this.CMS_HEADER          = 0;
    this.CMS_DETECT_BOUNDARY = 1;
    this.CMS_PART_HEADER     = 2;
    this.CMS_FINISHED        = 3;
    this.CMS_FAIL            = 4;

    this.flag   = 0;
    this.shift = 0;

    this.line_buf     = new Uint8Array(2000);
    this.line_buf_ptr = 0;
    this.state        = this.CMS_HEADER;

    this.cache      = new Uint8Array(100); // for boundary
    this.cached_len = 0;
    
    this.is_first_part = true;
    this.callback_context = null;
    this.callback         = null;

    this.boundary      = null;
    this.boundary_size = 0;
    this.boundary_ptr  = 0;

    // boundary = strdup("\r\n----asdfasdf\r\n");
    // boundary.length = strlen(boundary);
    this._header = {
        hasAudio : false,
        hasVideo : false
    };
    this.parts = [];
    this.part = {
        header : {},
        shift  : 0,
        chunks : []
    };
};

CMS.prototype.fail = function(error)
{
    this.emit(this.CMS_E_PARSE_FAIL, error);
    if (this.flag & this.CMS_FLAG_STOP_WHEN_ERROR) {
        this.state = this.CMS_FAIL;
        return true;
    } else {
        this.state = this.CMS_DETECT_BOUNDARY;
        return false;
    }
}
CMS.prototype.parse = function (data)
{
    var chunk  = 0;
    for (var i = 0; i < data.length; i++) {
        var ch = data[i];
        ch = String.fromCharCode(ch);
        this.shift++;
        switch (this.state) {
        case this.CMS_FAIL: {
            if (this.fail("CMS parse stoped.")) {
                return;
            }
            break;
        };
        case this.CMS_HEADER: {
            if (ch === '\r') {
            } else if (ch === '\n') {
                if (this.line_buf_ptr === 0) {
                    this.emit(this.CMS_E_HEADER_END);
                    if (!this.boundary) {
                        if (this.fail("Boundary not found.")) {
                            return;
                        }
                    } else if (!(this.flag & this.CMS_FLAG_FORMAT_FOUND)) {
                        if (this.fail("Format not found.")) {
                            return;
                        }
                    } else if (!(this.flag & this.CMS_FLAG_TRACK_FOUND)) {
                        if (this.fail("Track not found.")) {
                            return;
                        }
                    } else {
                        this.state = this.CMS_DETECT_BOUNDARY;
                        this.boundary_ptr = 2;
                        chunk = i + 1;
                    }
                } else {
                    var s = String.fromCharCode.apply(null, this.line_buf.slice(0, this.line_buf_ptr));
                    s     = s.split(":");
                    var key   = s.shift().trim();
                    var value = s.join(":").trim();
                    // console.log("cms header:", key, '=', value);
                    if (key && value) {
                        if (key === "boundary") {
                            this.boundary = "\r\n--" + value + "\r\n";
                        } else if (key === "format" && value === "cms") {
                            this.flag = this.flag | this.CMS_FLAG_FORMAT_FOUND;
                        } else if (key === "track") {
                            this.flag = this.flag | this.CMS_FLAG_TRACK_FOUND;
                        } 
                        var kv = {
                            key : key,
                            value : value
                        };
                        this.emit(this.CMS_E_HEADER_FIELD, kv);
                    } else {
                        if (this.fail("Invalid key value.")) {
                            return;
                        }
                    }
                }
                this.line_buf_ptr = 0;
                this.line_buf.fill(0);
            } else {
                this.line_buf[this.line_buf_ptr] = ch.charCodeAt(0);
                this.line_buf_ptr++;
            }
            break;
        };
        case this.CMS_DETECT_BOUNDARY: {
            if (!this.boundary) {
                break;
            }
            if (ch == this.boundary[this.boundary_ptr]) {
                this.boundary_ptr++;
            } else if (this.boundary_ptr == 1 && ch == this.boundary[this.boundary_ptr - 1]) {
                
            } else {
                if (this.cached_len > 0) {
                    this.emit(this.CMS_E_CHUNK, this.cache.slice(0, this.cached_len));
                    this.cached_len = 0;
                }
                this.boundary_ptr = 0;
            }
            if (this.boundary_ptr === this.boundary.length) {
                if (i - chunk >= (this.boundary.length - this.cached_len)) {
                    var chunk_len = i - chunk - (this.boundary.length - this.cached_len) + 1;
                    this.emit(this.CMS_E_CHUNK, data.slice(chunk, chunk + chunk_len));
                }
                this.cached_len = 0;
                if (this.is_first_part) {
                    this.is_first_part = false;
                } else {
                    this.emit(this.CMS_E_PART_END);
                }
                this.emit(this.CMS_E_PART_HEADER);

                this.state = this.CMS_PART_HEADER;
                this.part = {
                    header: {
                        type   : this.CMS_PART_UNKNOWN,
                        track  : -1,
                        length : 0,
                        ts     : -1
                    },
                    shift : this.shift,
                    chunks : []
                }
                chunk = i + 1;
                this.boundary_ptr = 0;
                this.line_buf_ptr = 0;
                this.line_buf[0]  = 0;
            }
            break;
        };
        case this.CMS_PART_HEADER: {
            if (ch == '\r') {
            } else if (ch == '\n') {
                if (this.line_buf_ptr == 0) {
                    this.emit(this.CMS_E_PART_HEADER_END);
                    this.state = this.CMS_DETECT_BOUNDARY;
                    chunk = i + 1;
                } else {
                    var s     = String.fromCharCode.apply(null, this.line_buf.slice(0, this.line_buf_ptr)).split(":");
                    var key   = s.shift().trim();
                    var value = s.join(":").trim();
                    // console.log("part header:", key, '=', value);
                    if (key && value) {
                        if (key === "f") {
                            if (value === "i") {
                                this.part.header.type = this.CMS_PART_H264_i_FRAME;
                            } else if (value === "p") {
                                this.part.header.type = this.CMS_PART_H264_p_FRAME;
                            } else if (value === "a") {
                                this.part.header.type = this.CMS_PART_AUDIO;
                            } else if (value === "j") {
                                this.part.header.type = this.CMS_PART_JPG;
                            } else {
                                console.error("Unknown frame type:", value);
                            }
                        } else if (key === "ts") {
                            this.part.header.ts = parseInt(value, 10);
                        } else if (key === "l") {
                            this.part.header.length = parseInt(value, 10);
                        } else if (key === "t") {
                            this.part.header.track = parseInt(value, 10);
                        }
                        var kv = {
                            key : key,
                            value : value
                        };
                        this.emit(this.CMS_E_PART_HEADER_FIELD, kv);
                    } else {
                        if (this.fail("Part header Invalid key value.")) {
                            return;
                        }
                    }
                }
                this.line_buf_ptr = 0;
                this.line_buf.fill(0);
            } else {
                if (this.line_buf_ptr < this.line_buf.length - 1) {
                    this.line_buf[this.line_buf_ptr] = ch.charCodeAt(0);
                    this.line_buf_ptr++;
                } else {
                    if (this.fail("Part header line too long.")) {
                        return;
                    }
                }
            }
            
            break;
        };
        }
    }
    if (this.state == this.CMS_DETECT_BOUNDARY) {
        if (chunk < data.length) {
            if (this.boundary_ptr > 0) {
                this.cached_len = this.boundary_ptr;
                var pos = data.length - this.boundary_ptr;
                if (pos >= 0) {
                    this.cache = data.slice(pos, this.cached_len);
                }
            }
            this.emit(this.CMS_E_CHUNK, data.slice(chunk, chunk + data.length - chunk - this.boundary_ptr));
        }
    }
};
CMS.prototype.set_callback = function (cb, context)
{
    this.callback         = cb;
    this.callback_context = context;
};
CMS.prototype.emit = function (e, data, size)
{
    if (this.callback) {
        return this.callback(this, e, data, size, this.callback_context);
    } else {
        this.cache_data(e, data);
    }
};

CMS.prototype.cache_data = function (e, data)
{
    switch (e) {
    case this.CMS_E_HEADER_FIELD : {
        var key   = data.key;
        var value = data.value;
        this._header[key] = value;
        if (key === 'track') {
            var tracks = value.split(';');
            this._header.tracks = [];
            for (var t = 0; t < tracks.length; t++) {
                var fields = tracks[t].split(',');
                var track = {};
                for (var j = 0; j < fields.length; j++) {
                    var f = fields[j].split('=');
                    var key = f.shift();
                    var value = f.join('=');
                    track[key] = value;
                    if (key === 'codec') {
                        if (value === 'alaw') {
                            this._header.hasAudio = true;
                        } else if (value === 'h264') {
                            this._header.hasVideo = true;
                        }
                    }
                }
                this._header.tracks.push(track);
            }
        }
        break;
    };
    case this.CMS_E_HEADER_END        : {
        this.header = this._header;
       break;
    };
    case this.CMS_E_PART_HEADER       : {
        break;
    };
    case this.CMS_E_PART_HEADER_FIELD : {
        break;
    };
    case this.CMS_E_PART_HEADER_END   : {
        break;
    };
    case this.CMS_E_PART_END          : {
        var chunks = this.part.chunks;
        var size = 0;
        for (var i = 0; i < chunks.length; i++) {
            size = size + chunks[i].length;
        }
        this.part.data = new ArrayBuffer(size);
        var data = new Uint8Array(this.part.data);
        var shift = 0;
        for (var i = 0; i < chunks.length; i++) {
            data.set(chunks[i], shift);
            shift = shift + chunks[i].length;
        }
        this.parts.push(this.part);
        break;
    };
    case this.CMS_E_CHUNK             : {
        this.part.chunks.push(data);
        break;
    };
    case this.CMS_E_PARSE_FAIL        : {
        break;
    };
        
    }
};

// var cms = new CMS();

// var fs = require('fs');
// var f = fs.createReadStream("/home/gxy/work/jingyun/cms/tmp/ttt.cms");
// f.on('end', function (err) {
//     console.log("end");
// });
// f.on('error', function (err) {
//     console.error(err);
// });
// f.on('data', function (chunk) {
//     cms.parse(chunk);
//     if (cms.header) {
//         console.log(cms.header);
//         cms.header = null;
//     }
//     var parts = cms.parts;
//     while (parts.length > 0) {
//         var part = parts.shift();
//         console.log(part.header);
//         var chunks = part.chunks;
//         var len = 0;
//         for (var i = 0; i < chunks.length; i++) {
//             len = len + chunks[i].length;
//         }
//         if (len !== part.header.length) {
//             console.log("mismatch: len=%d part.len=%d", len, part.header.length);
//         }
//     }
// });

