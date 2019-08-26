function Swap16(src) {
    return (((src >>> 8) & 0xFF) |
            ((src & 0xFF) << 8));
}

function Swap32(src) {
    return (((src & 0xFF000000) >>> 24) |
            ((src & 0x00FF0000) >>> 8)  |
            ((src & 0x0000FF00) << 8)   |
            ((src & 0x000000FF) << 24));
}

function ReadBig32(array, index) {
    return ((array[index] << 24)     |
            (array[index + 1] << 16) |
            (array[index + 2] << 8)  |
            (array[index + 3]));
}


function CMSDemuxer(probeData, config) {
    this.cms = new CMS();

    this.mp3enc = new lamejs.Mp3Encoder(1, 8000, 128);
    
    this.TAG = 'CMSDemuxer';

    this._config = config;

    this._onError = null;
    this._onMediaInfo = null;
    this._onMetaDataArrived = null;
    this._onScriptDataArrived = null;
    this._onTrackMetadata = null;
    this._onDataAvailable = null;

    this._dataOffset = probeData.dataOffset;
    this._firstParse = true;
    this._dispatch = false;

    this._hasAudio = probeData.hasAudioTrack;
    this._hasVideo = probeData.hasVideoTrack;

    this._hasAudioFlagOverrided = false;
    this._hasVideoFlagOverrided = false;

    this._audioInitialMetadataDispatched = false;
    this._videoInitialMetadataDispatched = false;

    this._mediaInfo = new MediaInfo();
    this._mediaInfo.hasAudio = this._hasAudio;
    this._mediaInfo.hasVideo = this._hasVideo;
    this._metadata = null;
    this._audioMetadata = null;
    this._videoMetadata = null;

    this._naluLengthSize = 4;
    this._timestampBase = 0;  // int32, in milliseconds
    this._timescale = 1000;
    this._duration = 9052204;  // int32, in milliseconds
    this._durationOverrided = false;
    this._referenceFrameRate = {
        fixed: true,
        fps: 23.976,
        fps_num: 23976,
        fps_den: 1000
    };

    this._cmsSoundRateTable = [5500, 11025, 22050, 44100, 48000];

    this._mpegSamplingRates = [
        96000, 88200, 64000, 48000, 44100, 32000,
        24000, 22050, 16000, 12000, 11025, 8000, 7350
    ];

    this._mpegAudioV10SampleRateTable = [44100, 48000, 32000, 0];
    this._mpegAudioV20SampleRateTable = [22050, 24000, 16000, 0];
    this._mpegAudioV25SampleRateTable = [11025, 12000, 8000,  0];

    this._mpegAudioL1BitRateTable = [0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, -1];
    this._mpegAudioL2BitRateTable = [0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384, -1];
    this._mpegAudioL3BitRateTable = [0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, -1];

    this._videoTrack = {type: 'video', id: 1, sequenceNumber: 0, samples: [], length: 0};
    this._audioTrack = {type: 'audio', id: 2, sequenceNumber: 0, samples: [], length: 0};

    this._littleEndian = (function () {
        let buf = new ArrayBuffer(2);
        (new DataView(buf)).setInt16(0, 256, true);  // little-endian write
        return (new Int16Array(buf))[0] === 256;  // platform-spec read, if equal then LE
    })();
}
CMSDemuxer.probe = function (buffer) {
    let data = new Uint8Array(buffer);
    var s = String.fromCharCode.apply(null, data);
    var offset = s.indexOf("\r\n\r\n");
    let mismatch = {match: false};

    if (offset < 0) {
        return mismatch;
    }
    var hasAudio = false;
    var hasVideo = false;
    
    var lines = s.substr(0, offset).split("\r\n");
    var headers = {};
    for (var i = 0; i < lines.length; i++) {
        var f = lines[i].split(':');
        var key = f.shift();
        var value = f.join(':');
        headers[key] = value;
    }
    if (headers.track) {
        var tracks = headers.track.split(';');
        headers.tracks = [];
        for (var i = 0; i < tracks.length; i++) {
            var fields = tracks[i].split(',');
            var track = {};
            for (var j = 0; j < fields.length; j++) {
                var f = fields[j].split('=');
                var key = f.shift();
                var value = f.join('=');
                track[key] = value;
                if (key === 'codec') {
                    if (value === 'alaw') {
                        hasAudio = true;
                    } else if (value === 'h264') {
                        hasVideo = true;
                    }
                }
            }
            headers.tracks.push(track);
        }
    }
    if (!hasAudio && !hasVideo) {
        return mismatch;
    }

    return {
        match: true,
        consumed: offset,
        dataOffset: offset,
        hasAudioTrack: hasAudio,
        hasVideoTrack: hasVideo
    };
}



CMSDemuxer.prototype.destroy = function () {
    this._mediaInfo = null;
    this._metadata = null;
    this._audioMetadata = null;
    this._videoMetadata = null;
    this._videoTrack = null;
    this._audioTrack = null;

    this._onError = null;
    this._onMediaInfo = null;
    this._onMetaDataArrived = null;
    this._onScriptDataArrived = null;
    this._onTrackMetadata = null;
    this._onDataAvailable = null;
}

CMSDemuxer.prototype.bindDataSource = function (loader) {
    loader.onDataArrival = this.parseChunks.bind(this);
    return this;
}

// prototype: function(type: string, metadata: any): void
CMSDemuxer.prototype.onTrackMetadata = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onTrackMetadata;
    } else {
        this._onTrackMetadata = callback;
    }
}

// prototype: function(mediaInfo: MediaInfo): void
CMSDemuxer.prototype.onMediaInfo = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onMediaInfo;
    } else {
        this._onMediaInfo = callback;
    }
}

CMSDemuxer.prototype.onMetaDataArrived = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onMetaDataArrived;
    } else {
        this._onMetaDataArrived = callback;
    }
}

CMSDemuxer.prototype.onScriptDataArrived = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onScriptDataArrived;
    } else {
        this._onScriptDataArrived = callback;
    }
}

// prototype: function(type: number, info: string): void
CMSDemuxer.prototype.onError = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onError;
    } else {
        this._onError = callback;
    }
}

// prototype: function(videoTrack: any, audioTrack: any): void
CMSDemuxer.prototype.onDataAvailable = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onDataAvailable;
    } else {
        this._onDataAvailable = callback;
    }
}

// timestamp base for output samples, must be in milliseconds
CMSDemuxer.prototype.timestampBase = function (base) {
    if (typeof base === 'undefined') {
        return this._timestampBase;
    } else {
        this._timestampBase = base;
    }
}

CMSDemuxer.prototype.overridedDuration = function (duration) {
    if (typeof duration === 'undefined') {
        return this._duration;
    } else {
        // Force-override media duration. Must be in milliseconds, int32
        this._durationOverrided = true;
        this._duration = duration;
        this._mediaInfo.duration = duration;
    }
}

// Force-override audio track present flag, boolean
CMSDemuxer.prototype.overridedHasAudio = function (hasAudio) {
    this._hasAudioFlagOverrided = true;
    this._hasAudio = hasAudio;
    this._mediaInfo.hasAudio = hasAudio;
}

// Force-override video track present flag, boolean
CMSDemuxer.prototype.overridedHasVideo = function (hasVideo) {
    this._hasVideoFlagOverrided = true;
    this._hasVideo = hasVideo;
    this._mediaInfo.hasVideo = hasVideo;
}

CMSDemuxer.prototype.resetMediaInfo = function () {
    this._mediaInfo = new MediaInfo();
}

CMSDemuxer.prototype._isInitialMetadataDispatched = function () {
    if (this._hasAudio && this._hasVideo) {  // both audio & video
        return this._audioInitialMetadataDispatched && this._videoInitialMetadataDispatched;
    }
    if (this._hasAudio && !this._hasVideo) {  // audio only
        return this._audioInitialMetadataDispatched;
    }
    if (!this._hasAudio && this._hasVideo) {  // video only
        return this._videoInitialMetadataDispatched;
    }
    return false;
}

// function parseChunks(chunk: ArrayBuffer, byteStart: number): number;
CMSDemuxer.prototype.parseChunks = function (chunk, byteStart) {
    if (!this._onError || !this._onMediaInfo || !this._onTrackMetadata || !this._onDataAvailable) {
        throw new IllegalStateException('Cms: onError & onMediaInfo & onTrackMetadata & onDataAvailable callback must be specified');
    }
    let data = new Uint8Array(chunk);
    this.cms.parse(data);
    while ((part = this.cms.parts.shift())) {
        this._dispatch = true;
        switch (part.header.type) {
        case this.cms.CMS_PART_AUDIO:  // Audio
            this._parseAudioData(part);
            break;
        case this.cms.CMS_PART_H264_i_FRAME:  // Video
        case this.cms.CMS_PART_H264_p_FRAME:  // Video
            this._parseVideoData(part);
            break;
        case 18:  // ScriptDataObject
            this._parseScriptData(chunk, dataOffset, dataSize);
            break;
        }

    }

    // dispatch parsed frames to consumer (typically, the remuxer)
    if (this._isInitialMetadataDispatched()) {
        if (this._dispatch && (this._audioTrack.length || this._videoTrack.length)) {
            this._onDataAvailable(this._audioTrack, this._videoTrack);
        }
    }

    return chunk.byteLength;
}

CMSDemuxer.prototype._parseScriptData = function () {
    this._metadata = {};
    this._hasAudio = true;
    this._mediaInfo.hasAudio = this._hasAudio;
    this._hasVideo = true;
    this._mediaInfo.hasVideo = this._hasVideo;
    this._mediaInfo.audioDataRate = 64;
    this._mediaInfo.videoDataRate = 1000000;
    this._mediaInfo.width = 1080;
    this._mediaInfo.height = 720;
    let duration = Math.floor(9052204 * this._timescale);
    this._duration = duration;
    this._mediaInfo.duration = duration;

    let fps_num = Math.floor(15 * 1000);
    if (fps_num > 0) {
        let fps = fps_num / 1000;
        this._referenceFrameRate.fixed = true;
        this._referenceFrameRate.fps = fps;
        this._referenceFrameRate.fps_num = fps_num;
        this._referenceFrameRate.fps_den = 1000;
        this._mediaInfo.fps = fps;
    }

    this._mediaInfo.hasKeyframesIndex = false;
    this._dispatch = false;
    this._mediaInfo.metadata = onMetaData;
    if (this._mediaInfo.isComplete()) {
        this._onMediaInfo(this._mediaInfo);
    }

    if (Object.keys(scriptData).length > 0) {
        if (this._onScriptDataArrived) {
            this._onScriptDataArrived(Object.assign({}, scriptData));
        }
    }
}

CMSDemuxer.prototype._parseKeyframesIndex = function (keyframes) {
    let times = [];
    let filepositions = [];

    // ignore first keyframe which is actually AVC Sequence Header (AVCDecoderConfigurationRecord)
    for (let i = 1; i < keyframes.times.length; i++) {
        let time = this._timestampBase + Math.floor(keyframes.times[i] * 1000);
        times.push(time);
        filepositions.push(keyframes.filepositions[i]);
    }

    return {
        times: times,
        filepositions: filepositions
    };
}

CMSDemuxer.prototype._parseAudioData = function (part) {
    if (!part) {
        Log.w(this.TAG, 'Cms: Invalid audio packet, missing SoundData payload!');
        return;
    }

    if (this._hasAudioFlagOverrided === true && this._hasAudio === false) {
        // If hasAudio: false indicated explicitly in MediaDataSource,
        // Ignore all the audio packets
        return;
    }

    var pcm16buf = new ArrayBuffer(part.data.byteLength * 2);
    var pcm16 = new Int16Array(pcm16buf)
    var data = new  Uint8Array(part.data)
    for (var i = 0; i < part.data.byteLength; i++) {
        pcm16[i] = alaw2linear(data[i]);
    };
    var mp3buf = this.mp3enc.encodeBuffer(pcm16);
    if (mp3buf.length === 0) {
        return;
    }
    let meta = this._audioMetadata;
    let track = this._audioTrack;

    if (!meta) {
        if (this._hasAudio === false && this._hasAudioFlagOverrided === false) {
            this._hasAudio = true;
            this._mediaInfo.hasAudio = true;
        }

        // initial metadata
        meta = this._audioMetadata = {};
        meta.type = 'audio';
        meta.id = track.id;
        meta.timescale = this._timescale;
        meta.duration = this._duration;
        meta.channelCount = 1;
    }  

    if (!meta.codec) {
        // We need metadata for mp3 audio track, extract info from frame header
        var _misc = this._parseMP3AudioData(mp3buf, true);
        if (_misc == undefined) {
            return;
        }
        meta.audioSampleRate = 8000;
        meta.channelCount = 1;
        meta.codec = 'mp3';
        meta.originalCodec = 'mp3';
        // The decode result of an mp3 sample is 1152 PCM samples
        meta.refSampleDuration = 1152 / 8000 * meta.timescale;
        this._audioInitialMetadataDispatched = true;
        this._onTrackMetadata('audio', meta);

        let mi = this._mediaInfo;
        mi.audioCodec = meta.codec;
        mi.audioSampleRate = meta.audioSampleRate;
        mi.audioChannelCount = meta.channelCount;
        mi.audioDataRate = _misc.bitRate;
        if (mi.hasVideo) {
            if (mi.videoCodec != null) {
                mi.mimeType = 'video/x-cms; codecs="' + mi.videoCodec + ',' + mi.audioCodec + '"';
            }
        } else {
            mi.mimeType = 'video/x-cms; codecs="' + mi.audioCodec + '"';
        }
        if (mi.isComplete()) {
            this._onMediaInfo(mi);
        }
    }
    // This packet is always a valid audio packet, extract it

    let dts = this._timestampBase + part.header.ts;
    let mp3Sample = {unit: mp3buf, length: mp3buf.byteLength, dts: dts, pts: dts};
    track.samples.push(mp3Sample);
    track.length += mp3buf.byteLength;
}

CMSDemuxer.prototype._parseAACAudioData = function (arrayBuffer, dataOffset, dataSize) {
    if (dataSize <= 1) {
        Log.w(this.TAG, 'Cms: Invalid AAC packet, missing AACPacketType or/and Data!');
        return;
    }

    let result = {};
    let array = new Uint8Array(arrayBuffer, dataOffset, dataSize);

    result.packetType = array[0];

    if (array[0] === 0) {
        result.data = this._parseAACAudioSpecificConfig(arrayBuffer, dataOffset + 1, dataSize - 1);
    } else {
        result.data = array.subarray(1);
    }

    return result;
}

CMSDemuxer.prototype._parseAACAudioSpecificConfig = function (arrayBuffer, dataOffset, dataSize) {
    let array = new Uint8Array(arrayBuffer, dataOffset, dataSize);
    let config = null;

    /* Audio Object Type:
       0: Null
       1: AAC Main
       2: AAC LC
       3: AAC SSR (Scalable Sample Rate)
       4: AAC LTP (Long Term Prediction)
       5: HE-AAC / SBR (Spectral Band Replication)
       6: AAC Scalable
    */

    let audioObjectType = 0;
    let originalAudioObjectType = 0;
    let audioExtensionObjectType = null;
    let samplingIndex = 0;
    let extensionSamplingIndex = null;

    // 5 bits
    audioObjectType = originalAudioObjectType = array[0] >>> 3;
    // 4 bits
    samplingIndex = ((array[0] & 0x07) << 1) | (array[1] >>> 7);
    if (samplingIndex < 0 || samplingIndex >= this._mpegSamplingRates.length) {
        this._onError(DemuxErrors.FORMAT_ERROR, 'Cms: AAC invalid sampling frequency index!');
        return;
    }

    let samplingFrequence = this._mpegSamplingRates[samplingIndex];

    // 4 bits
    let channelConfig = (array[1] & 0x78) >>> 3;
    if (channelConfig < 0 || channelConfig >= 8) {
        this._onError(DemuxErrors.FORMAT_ERROR, 'Cms: AAC invalid channel configuration');
        return;
    }

    if (audioObjectType === 5) {  // HE-AAC?
        // 4 bits
        extensionSamplingIndex = ((array[1] & 0x07) << 1) | (array[2] >>> 7);
        // 5 bits
        audioExtensionObjectType = (array[2] & 0x7C) >>> 2;
    }

    // workarounds for various browsers
    let userAgent = self.navigator.userAgent.toLowerCase();

    if (userAgent.indexOf('firefox') !== -1) {
        // firefox: use SBR (HE-AAC) if freq less than 24kHz
        if (samplingIndex >= 6) {
            audioObjectType = 5;
            config = new Array(4);
            extensionSamplingIndex = samplingIndex - 3;
        } else {  // use LC-AAC
            audioObjectType = 2;
            config = new Array(2);
            extensionSamplingIndex = samplingIndex;
        }
    } else if (userAgent.indexOf('android') !== -1) {
        // android: always use LC-AAC
        audioObjectType = 2;
        config = new Array(2);
        extensionSamplingIndex = samplingIndex;
    } else {
        // for other browsers, e.g. chrome...
        // Always use HE-AAC to make it easier to switch aac codec profile
        audioObjectType = 5;
        extensionSamplingIndex = samplingIndex;
        config = new Array(4);

        if (samplingIndex >= 6) {
            extensionSamplingIndex = samplingIndex - 3;
        } else if (channelConfig === 1) {  // Mono channel
            audioObjectType = 2;
            config = new Array(2);
            extensionSamplingIndex = samplingIndex;
        }
    }

    config[0]  = audioObjectType << 3;
    config[0] |= (samplingIndex & 0x0F) >>> 1;
    config[1]  = (samplingIndex & 0x0F) << 7;
    config[1] |= (channelConfig & 0x0F) << 3;
    if (audioObjectType === 5) {
        config[1] |= ((extensionSamplingIndex & 0x0F) >>> 1);
        config[2]  = (extensionSamplingIndex & 0x01) << 7;
        // extended audio object type: force to 2 (LC-AAC)
        config[2] |= (2 << 2);
        config[3]  = 0;
    }

    return {
        config: config,
        samplingRate: samplingFrequence,
        channelCount: channelConfig,
        codec: 'mp4a.40.' + audioObjectType,
        originalCodec: 'mp4a.40.' + originalAudioObjectType
    };
}

CMSDemuxer.prototype._parseMP3AudioData = function (mp3buf, requestHeader) {
    let le = this._littleEndian;
    let array = new Uint8Array(mp3buf);
    let result = null;

    if (requestHeader) {
        if (array[0] !== 0xFF) {
            return;
        }
        let ver = (array[1] >>> 3) & 0x03;
        let layer = (array[1] & 0x06) >> 1;

        let bitrate_index = (array[2] & 0xF0) >>> 4;
        let sampling_freq_index = (array[2] & 0x0C) >>> 2;

        let channel_mode = (array[3] >>> 6) & 0x03;
        let channel_count = channel_mode !== 3 ? 2 : 1;

        let sample_rate = 0;
        let bit_rate = 0;
        let object_type = 34;  // Layer-3, listed in MPEG-4 Audio Object Types

        let codec = 'mp3';

        switch (ver) {
        case 0:  // MPEG 2.5
            sample_rate = this._mpegAudioV25SampleRateTable[sampling_freq_index];
            break;
        case 2:  // MPEG 2
            sample_rate = this._mpegAudioV20SampleRateTable[sampling_freq_index];
            break;
        case 3:  // MPEG 1
            sample_rate = this._mpegAudioV10SampleRateTable[sampling_freq_index];
            break;
        }

        switch (layer) {
        case 1:  // Layer 3
            object_type = 34;
            if (bitrate_index < this._mpegAudioL3BitRateTable.length) {
                bit_rate = this._mpegAudioL3BitRateTable[bitrate_index];
            }
            break;
        case 2:  // Layer 2
            object_type = 33;
            if (bitrate_index < this._mpegAudioL2BitRateTable.length) {
                bit_rate = this._mpegAudioL2BitRateTable[bitrate_index];
            }
            break;
        case 3:  // Layer 1
            object_type = 32;
            if (bitrate_index < this._mpegAudioL1BitRateTable.length) {
                bit_rate = this._mpegAudioL1BitRateTable[bitrate_index];
            }
            break;
        }

        result = {
            bitRate: bit_rate,
            samplingRate: sample_rate,
            channelCount: channel_count,
            codec: codec,
            originalCodec: codec
        };
    } else {
        result = array;
    }

    return result;
}

CMSDemuxer.prototype._parseVideoData = function (part) {
    if (!part) {
        Log.w(this.TAG, 'Cms: Invalid video packet, missing VideoData payload!');
        return;
    }

    if (this._hasVideoFlagOverrided === true && this._hasVideo === false) {
        // If hasVideo: false indicated explicitly in MediaDataSource,
        // Ignore all the video packets
        return;
    }

    this._parseAVCVideoPacket(part);
}

CMSDemuxer.prototype._parseAVCVideoPacket = function (part) {
    if (!part) {
        Log.w(this.TAG, 'Cms: Invalid AVC packet, missing AVCPacketType or/and CompositionTime');
        return;
    }
    if (this._firstParse) {
        this._firstParse = false;
        var _v = new Uint8Array(part.data);
        var sps_data        = new Uint8Array(255);
        var sps_data_index  = 1;
        var sps_length      = 0;
        var sps_length_index= sps_data_index - 1;
        let sps_count       = 0;
        var pps_data        = new Uint8Array(255);
        var pps_data_index  = 1;
        var pps_length      = 0;
        var pps_length_index= pps_data_index - 1;
        let pps_count       = 0;
        let nal_unit_type   = 0;

        var i = 0;
        for ( i = 0; i < _v.byteLength; i++) {
            let data = _v[i];
            if ( ( 0 ==  data) && ( _v.byteLength - i > 5 ) ) {
                if ( ( 0 == _v[i+1] ) && ( 0 == _v[i+2] ) && ( 1 == _v[i+3] ) ) {
                    data           = _v[i+4];
                    nal_unit_type  = data ;

                    if ( sps_count > 0 ) {
                        sps_data[sps_length_index] = sps_length & 0xFF;
                    }
                    if ( pps_count > 0 ) {
                        pps_data[pps_length_index] = pps_length & 0xFF;
                    }

                    if ( 0x67 == nal_unit_type ) {
                        sps_count++;
                        sps_data_index += 2;
                        sps_length_index= sps_data_index - 1;
                        sps_length      = 0;
                    } else if ( 0x68 == nal_unit_type ) {
                        pps_count++;
                        pps_data_index += 2;
                        pps_length_index= pps_data_index - 1;
                        pps_length      = 0;
                    } else if ( 0x65 == nal_unit_type ) {
                        break;
                    }
                    i+=4;
                }
            }
            if ( 0x67 == nal_unit_type ) {
                sps_data[sps_data_index++] = data;
                sps_length++;
            } else if ( 0x68 == nal_unit_type ) {
                pps_data[pps_data_index++] = data;
                pps_length++;
            }
        }
        sps_data[0] = sps_count & 0xFF;
        pps_data[0] = pps_count & 0xFF;

        var AVC_Dec_Conf_size   = 5 + sps_data_index + pps_data_index;
        var AVC_Dec_Conf        = new ArrayBuffer(AVC_Dec_Conf_size);
        var AVC_Dec_Conf_v      = new Uint8Array(AVC_Dec_Conf);
        AVC_Dec_Conf_v.set([1, 100, 0, 30, 255], 0);
        AVC_Dec_Conf_v.set(sps_data.subarray(0, sps_data_index), 5);
        AVC_Dec_Conf_v.set(pps_data.subarray(0, pps_data_index), 5 + sps_data_index );
        
        this._parseAVCDecoderConfigurationRecord(AVC_Dec_Conf, 0, AVC_Dec_Conf_size);
    }
    this._parseAVCVideoData(part);
}

CMSDemuxer.prototype._parseAVCDecoderConfigurationRecord = function (arrayBuffer, dataOffset, dataSize) {
    if (dataSize < 7) {
        Log.w(this.TAG, 'Cms: Invalid AVCDecoderConfigurationRecord, lack of data!');
        return;
    }

    let meta = this._videoMetadata;
    let track = this._videoTrack;
    let le = this._littleEndian;
    let v = new DataView(arrayBuffer, dataOffset, dataSize);

    if (!meta) {
        if (this._hasVideo === false && this._hasVideoFlagOverrided === false) {
            this._hasVideo = true;
            this._mediaInfo.hasVideo = true;
        }

        meta = this._videoMetadata = {};
        meta.type = 'video';
        meta.id = track.id;
        meta.timescale = this._timescale;
        meta.duration = this._duration;
    } else {
        if (typeof meta.avcc !== 'undefined') {
            Log.w(this.TAG, 'Found another AVCDecoderConfigurationRecord!');
        }
    }

    let version = v.getUint8(0);  // configurationVersion
    let avcProfile = v.getUint8(1);  // avcProfileIndication
    let profileCompatibility = v.getUint8(2);  // profile_compatibility
    let avcLevel = v.getUint8(3);  // AVCLevelIndication

    if (version !== 1 || avcProfile === 0) {
        this._onError(DemuxErrors.FORMAT_ERROR, 'Cms: Invalid AVCDecoderConfigurationRecord');
        return;
    }

    this._naluLengthSize = (v.getUint8(4) & 3) + 1;  // lengthSizeMinusOne
    if (this._naluLengthSize !== 3 && this._naluLengthSize !== 4) {  // holy shit!!!
        this._onError(DemuxErrors.FORMAT_ERROR, `Cms: Strange NaluLengthSizeMinusOne: ${this._naluLengthSize - 1}`);
        return;
    }

    let spsCount = v.getUint8(5) & 31;  // numOfSequenceParameterSets
    if (spsCount === 0) {
        this._onError(DemuxErrors.FORMAT_ERROR, 'Cms: Invalid AVCDecoderConfigurationRecord: No SPS');
        return;
    } else if (spsCount > 1) {
        Log.w(this.TAG, `Cms: Strange AVCDecoderConfigurationRecord: SPS Count = ${spsCount}`);
    }

    let offset = 6;

    for (let i = 0; i < spsCount; i++) {
        let len = v.getUint16(offset, !le);  // sequenceParameterSetLength
        offset += 2;

        if (len === 0) {
            continue;
        }

        // Notice: Nalu without startcode header (00 00 00 01)
        let sps = new Uint8Array(arrayBuffer, dataOffset + offset, len);
        offset += len;

        let config = SPSParser.parseSPS(sps);
        if (i !== 0) {
            // ignore other sps's config
            continue;
        }

        meta.codecWidth = config.codec_size.width;
        meta.codecHeight = config.codec_size.height;
        meta.presentWidth = config.present_size.width;
        meta.presentHeight = config.present_size.height;

        meta.profile = config.profile_string;
        meta.level = config.level_string;
        meta.bitDepth = config.bit_depth;
        meta.chromaFormat = config.chroma_format;
        meta.sarRatio = config.sar_ratio;
        meta.frameRate = config.frame_rate;

        if (config.frame_rate.fixed === false ||
            config.frame_rate.fps_num === 0 ||
            config.frame_rate.fps_den === 0) {
            meta.frameRate = this._referenceFrameRate;
        }

        let fps_den = meta.frameRate.fps_den;
        let fps_num = meta.frameRate.fps_num;
        meta.refSampleDuration = meta.timescale * (fps_den / fps_num);

        let codecArray = sps.subarray(1, 4);
        let codecString = 'avc1.';
        for (let j = 0; j < 3; j++) {
            let h = codecArray[j].toString(16);
            if (h.length < 2) {
                h = '0' + h;
            }
            codecString += h;
        }
        meta.codec = codecString;

        let mi = this._mediaInfo;
        mi.width        = meta.codecWidth;
        mi.height       = meta.codecHeight;
        mi.fps          = meta.frameRate.fps;
        mi.profile      = meta.profile;
        mi.level        = meta.level;
        mi.refFrames    = config.ref_frames;
        mi.chromaFormat = config.chroma_format_string;
        mi.sarNum       = meta.sarRatio.width;
        mi.sarDen       = meta.sarRatio.height;
        mi.videoCodec   = codecString;

        if (mi.hasAudio) {
            if (mi.audioCodec != null) {
                mi.mimeType = 'video/x-cms; codecs="' + mi.videoCodec + ',' + mi.audioCodec + '"';
            }
        } else {
            mi.mimeType = 'video/x-cms; codecs="' + mi.videoCodec + '"';
        }
        if (mi.isComplete()) {
            this._onMediaInfo(mi);
        }
    }

    let ppsCount = v.getUint8(offset);  // numOfPictureParameterSets
    if (ppsCount === 0) {
        this._onError(DemuxErrors.FORMAT_ERROR, 'Cms: Invalid AVCDecoderConfigurationRecord: No PPS');
        return;
    } else if (ppsCount > 1) {
        Log.w(this.TAG, `Cms: Strange AVCDecoderConfigurationRecord: PPS Count = ${ppsCount}`);
    }

    offset++;

    for (let i = 0; i < ppsCount; i++) {
        let len = v.getUint16(offset, !le);  // pictureParameterSetLength
        offset += 2;

        if (len === 0) {
            continue;
        }

        // pps is useless for extracting video information
        offset += len;
    }

    meta.avcc = new Uint8Array(dataSize);
    meta.avcc.set(new Uint8Array(arrayBuffer, dataOffset, dataSize), 0);
    Log.v(this.TAG, 'Parsed AVCDecoderConfigurationRecord');

    if (this._isInitialMetadataDispatched()) {
        // flush parsed frames
        if (this._dispatch && (this._audioTrack.length || this._videoTrack.length)) {
            this._onDataAvailable(this._audioTrack, this._videoTrack);
        }
    } else {
        this._videoInitialMetadataDispatched = true;
    }
    // notify new metadata
    this._dispatch = false;
    this._onTrackMetadata('video', meta);
}

CMSDemuxer.prototype._parseAVCVideoData = function (part) {
    let le = this._littleEndian;
    let v = new DataView(part.data);

    let units = [], length = 0;

    let offset = 0;
    const lengthSize = this._naluLengthSize;
    let dts = this._timestampBase + part.header.ts;
    let keyframe = (part.header.type === this.cms.CMS_PART_H264_i_FRAME);  // from CMS Frame Type constants

    while (offset < part.data.byteLength) {
        if (offset + 4 >= part.data.byteLength) {
            Log.w(this.TAG, `Malformed Nalu near timestamp ${dts}, offset = ${offset}, dataSize = ${dataSize}`);
            break;  // data not enough for next Nalu
        }
        var naluSize = part.data.byteLength - lengthSize; //
        var unitType = v.getUint8(offset + 4) & 0x1F;
        if ( (unitType === 5) || (unitType === 7) || (unitType === 8)){
            // IDR
            keyframe = true;
        }

        let data = new Uint8Array(part.data,  offset, lengthSize + naluSize);
        data[0] = naluSize >>> 24 & 0xFF; 
        data[1] = naluSize >>> 16 & 0xFF;
        data[2] = naluSize >>> 8 & 0xFF;
        data[3] = naluSize & 0xFF;

        let unit = {type: unitType, data: data};
        units.push(unit);
        length += data.byteLength;

        offset += lengthSize + naluSize;
    }
    var cts = 0;
    if (units.length) {
        let track = this._videoTrack;
        let avcSample = {
            units: units,
            length: length,
            isKeyframe: keyframe,
            dts: dts,
            cts: cts,
            pts: (dts + cts)
        };
        if (keyframe) {
            avcSample.fileposition = part.shift;
        }
        track.samples.push(avcSample);
        track.length += length;
    }
}
