// Transmuxing (IO, Demuxing, Remuxing) controller, with multipart support
function TransmuxingController(mediaDataSource, config) {
    this.TAG = 'TransmuxingController';
    this._emitter = new EventEmitter();

    this._config = config;

    // treat single part media as multipart media, which has only one segment
    if (!mediaDataSource.segments) {
        mediaDataSource.segments = [{
            duration: mediaDataSource.duration,
            filesize: mediaDataSource.filesize,
            url: mediaDataSource.url
        }];
    }

    // fill in default IO params if not exists
    if (typeof mediaDataSource.cors !== 'boolean') {
        mediaDataSource.cors = true;
    }
    if (typeof mediaDataSource.withCredentials !== 'boolean') {
        mediaDataSource.withCredentials = false;
    }

    this._mediaDataSource = mediaDataSource;
    this._currentSegmentIndex = 0;
    let totalDuration = 0;

    this._mediaDataSource.segments.forEach((segment) => {
        // timestampBase for each segment, and calculate total duration
        segment.timestampBase = totalDuration;
        totalDuration += segment.duration;
        // params needed by IOController
        segment.cors = mediaDataSource.cors;
        segment.withCredentials = mediaDataSource.withCredentials;
        // referrer policy control, if exist
        if (config.referrerPolicy) {
            segment.referrerPolicy = config.referrerPolicy;
        }
    });

    if (!isNaN(totalDuration) && this._mediaDataSource.duration !== totalDuration) {
        this._mediaDataSource.duration = totalDuration;
    }

    this._mediaInfo = null;
    this._demuxer = null;
    this._remuxer = null;
    this._ioctl = null;

    this._pendingSeekTime = null;
    this._pendingResolveSeekPoint = null;

    this._statisticsReporter = null;
}

TransmuxingController.prototype.destroy = function () {
    this._mediaInfo = null;
    this._mediaDataSource = null;

    if (this._statisticsReporter) {
        this._disableStatisticsReporter();
    }
    if (this._ioctl) {
        this._ioctl.destroy();
        this._ioctl = null;
    }
    if (this._demuxer) {
        this._demuxer.destroy();
        this._demuxer = null;
    }
    if (this._remuxer) {
        this._remuxer.destroy();
        this._remuxer = null;
    }

    this._emitter.removeAllListeners();
    this._emitter = null;
}

TransmuxingController.prototype.on = function (event, listener) {
    this._emitter.addListener(event, listener);
}

TransmuxingController.prototype.off = function (event, listener) {
    this._emitter.removeListener(event, listener);
}

TransmuxingController.prototype.start = function () {
    this._loadSegment(0);
    this._enableStatisticsReporter();
}

TransmuxingController.prototype._loadSegment = function (segmentIndex, optionalFrom) {
    this._currentSegmentIndex = segmentIndex;
    let dataSource = this._mediaDataSource.segments[segmentIndex];

    let ioctl = this._ioctl = new IOController(dataSource, this._config, segmentIndex);
    ioctl.onError(this._onIOException.bind(this));
    ioctl.onSeeked(this._onIOSeeked.bind(this));
    ioctl.onComplete(this._onIOComplete.bind(this));
    ioctl.onRedirect(this._onIORedirect.bind(this));
    ioctl.onRecoveredEarlyEof( this._onIORecoveredEarlyEof.bind(this));

    if (optionalFrom) {
        this._demuxer.bindDataSource(this._ioctl);
    } else {
        ioctl.onDataArrival(this._onInitChunkArrival.bind(this));
    }

    ioctl.open(optionalFrom);
}

TransmuxingController.prototype.stop = function () {
    this._internalAbort();
    this._disableStatisticsReporter();
}

functhoi_internalAbort = function () {
    if (this._ioctl) {
        this._ioctl.destroy();
        this._ioctl = null;
    }
}

TransmuxingController.prototype.pause = function () {  // take a rest
    if (this._ioctl && this._ioctl.isWorking()) {
        this._ioctl.pause();
        this._disableStatisticsReporter();
    }
}

TransmuxingController.prototype.resume = function () {
    if (this._ioctl && this._ioctl.isPaused()) {
        this._ioctl.resume();
        this._enableStatisticsReporter();
    }
}

TransmuxingController.prototype.seek = function (milliseconds) {
    if (this._mediaInfo == null || !this._mediaInfo.isSeekable()) {
        return;
    }

    let targetSegmentIndex = this._searchSegmentIndexContains(milliseconds);

    if (targetSegmentIndex === this._currentSegmentIndex) {
        // intra-segment seeking
        let segmentInfo = this._mediaInfo.segments[targetSegmentIndex];

        if (segmentInfo == undefined) {
            // current segment loading started, but mediainfo hasn't received yet
            // wait for the metadata loaded, then seek to expected position
            this._pendingSeekTime = milliseconds;
        } else {
            let keyframe = segmentInfo.getNearestKeyframe(milliseconds);
            this._remuxer.seek(keyframe.milliseconds);
            this._ioctl.seek(keyframe.fileposition);
            // Will be resolved in _onRemuxerMediaSegmentArrival()
            this._pendingResolveSeekPoint = keyframe.milliseconds;
        }
    } else {
        // cross-segment seeking
        let targetSegmentInfo = this._mediaInfo.segments[targetSegmentIndex];

        if (targetSegmentInfo == undefined) {
            // target segment hasn't been loaded. We need metadata then seek to expected time
            this._pendingSeekTime = milliseconds;
            this._internalAbort();
            this._remuxer.seek();
            this._remuxer.insertDiscontinuity();
            this._loadSegment(targetSegmentIndex);
            // Here we wait for the metadata loaded, then seek to expected position
        } else {
            // We have target segment's metadata, direct seek to target position
            let keyframe = targetSegmentInfo.getNearestKeyframe(milliseconds);
            this._internalAbort();
            this._remuxer.seek(milliseconds);
            this._remuxer.insertDiscontinuity();
            this._demuxer.resetMediaInfo();
            this._demuxer.timestampBase(this._mediaDataSource.segments[targetSegmentIndex].timestampBase);
            this._loadSegment(targetSegmentIndex, keyframe.fileposition);
            this._pendingResolveSeekPoint = keyframe.milliseconds;
            this._reportSegmentMediaInfo(targetSegmentIndex);
        }
    }

    this._enableStatisticsReporter();
}

TransmuxingController.prototype._searchSegmentIndexContains = function (milliseconds) {
    let segments = this._mediaDataSource.segments;
    let idx = segments.length - 1;

    for (let i = 0; i < segments.length; i++) {
        if (milliseconds < segments[i].timestampBase()) {
            idx = i - 1;
            break;
        }
    }
    return idx;
}

TransmuxingController.prototype._onInitChunkArrival = function (data, byteStart) {
    let probeData = null;
    let consumed = 0;

    if (byteStart > 0) {
        // IOController seeked immediately after opened, byteStart > 0 callback may received
        this._demuxer.bindDataSource(this._ioctl);
        this._demuxer.timestampBase(this._mediaDataSource.segments[this._currentSegmentIndex].timestampBase);

        consumed = this._demuxer.parseChunks(data, byteStart);
    } else if ((probeData = FLVDemuxer.probe(data)).match) {
        // Always create new FLVDemuxer
        this._demuxer = new FLVDemuxer(probeData, this._config);

        if (!this._remuxer) {
            this._remuxer = new MP4Remuxer(this._config);
        }

        let mds = this._mediaDataSource;
        if (mds.duration != undefined && !isNaN(mds.duration)) {
            this._demuxer.overridedDuration(mds.duration);
        }
        if (typeof mds.hasAudio === 'boolean') {
            this._demuxer.overridedHasAudio(mds.hasAudio);
        }
        if (typeof mds.hasVideo === 'boolean') {
            this._demuxer.overridedHasVideo(mds.hasVideo);
        }

        this._demuxer.timestampBase(mds.segments[this._currentSegmentIndex].timestampBase);

        this._demuxer.onError(this._onDemuxException.bind(this));
        this._demuxer.onMediaInfo(this._onMediaInfo.bind(this));
        this._demuxer.onMetaDataArrived(this._onMetaDataArrived.bind(this));
        this._demuxer.onScriptDataArrived(this._onScriptDataArrived.bind(this));

        this._remuxer.bindDataSource(this._demuxer.bindDataSource(this._ioctl));

        this._remuxer.onInitSegment(this._onRemuxerInitSegmentArrival.bind(this));
        this._remuxer.onMediaSegment(this._onRemuxerMediaSegmentArrival.bind(this));

        consumed = this._demuxer.parseChunks(data, byteStart);
    } else if ((probeData = CMSDemuxer.probe(data)).match) {
        this._demuxer = new CMSDemuxer(probeData, this._config);

        if (!this._remuxer) {
            this._remuxer = new MP4Remuxer(this._config);
        }

        let mds = this._mediaDataSource;
        if (mds.duration != undefined && !isNaN(mds.duration)) {
            this._demuxer.overridedDuration(mds.duration);
        }
        if (typeof mds.hasAudio === 'boolean') {
            this._demuxer.overridedHasAudio(mds.hasAudio);
        }
        if (typeof mds.hasVideo === 'boolean') {
            this._demuxer.overridedHasVideo(mds.hasVideo);
        }

        this._demuxer.timestampBase(mds.segments[this._currentSegmentIndex].timestampBase);

        this._demuxer.onError(this._onDemuxException.bind(this));
        this._demuxer.onMediaInfo(this._onMediaInfo.bind(this));
        this._demuxer.onMetaDataArrived(this._onMetaDataArrived.bind(this));
        this._demuxer.onScriptDataArrived(this._onScriptDataArrived.bind(this));

        this._remuxer.bindDataSource(this._demuxer.bindDataSource(this._ioctl));

        this._remuxer.onInitSegment(this._onRemuxerInitSegmentArrival.bind(this));
        this._remuxer.onMediaSegment(this._onRemuxerMediaSegmentArrival.bind(this));

        consumed = this._demuxer.parseChunks(data, byteStart);
    } else {
        probeData = null;
        Log.e(this.TAG, 'Non-FLV, Unsupported media type!');
        Promise.resolve().then(() => {
            this._internalAbort();
        });
        this._emitter.emit(TransmuxingEvents.DEMUX_ERROR, DemuxErrors.FORMAT_UNSUPPORTED, 'Non-FLV, Unsupported media type');

        consumed = 0;
    }

    return consumed;
}

TransmuxingController.prototype._onMediaInfo = function (mediaInfo) {
    if (this._mediaInfo == null) {
        // Store first segment's mediainfo as global mediaInfo
        this._mediaInfo = Object.assign({}, mediaInfo);
        this._mediaInfo.keyframesIndex = null;
        this._mediaInfo.segments = [];
        this._mediaInfo.segmentCount = this._mediaDataSource.segments.length;
        Object.setPrototypeOf(this._mediaInfo, MediaInfo.prototype);
    }

    let segmentInfo = Object.assign({}, mediaInfo);
    Object.setPrototypeOf(segmentInfo, MediaInfo.prototype);
    this._mediaInfo.segments[this._currentSegmentIndex] = segmentInfo;

    // notify mediaInfo update
    this._reportSegmentMediaInfo(this._currentSegmentIndex);

    if (this._pendingSeekTime != null) {
        Promise.resolve().then(() => {
            let target = this._pendingSeekTime;
            this._pendingSeekTime = null;
            this.seek(target);
        });
    }
}

TransmuxingController.prototype._onMetaDataArrived = function (metadata) {
    this._emitter.emit(TransmuxingEvents.METADATA_ARRIVED, metadata);
}

TransmuxingController.prototype._onScriptDataArrived = function (data) {
    this._emitter.emit(TransmuxingEvents.SCRIPTDATA_ARRIVED, data);
}

TransmuxingController.prototype._onIOSeeked = function () {
    this._remuxer.insertDiscontinuity();
}

TransmuxingController.prototype._onIOComplete = function (extraData) {
    let segmentIndex = extraData;
    let nextSegmentIndex = segmentIndex + 1;

    if (nextSegmentIndex < this._mediaDataSource.segments.length) {
        this._internalAbort();
        this._remuxer.flushStashedSamples();
        this._loadSegment(nextSegmentIndex);
    } else {
        this._remuxer.flushStashedSamples();
        this._emitter.emit(TransmuxingEvents.LOADING_COMPLETE);
        this._disableStatisticsReporter();
    }
}

TransmuxingController.prototype._onIORedirect = function (redirectedURL) {
    let segmentIndex = this._ioctl.extraData();
    this._mediaDataSource.segments[segmentIndex].redirectedURL = redirectedURL;
}

TransmuxingController.prototype._onIORecoveredEarlyEof = function () {
    this._emitter.emit(TransmuxingEvents.RECOVERED_EARLY_EOF);
}

TransmuxingController.prototype._onIOException = function (type, info) {
    Log.e(this.TAG, `IOException: type = ${type}, code = ${info.code}, msg = ${info.msg}`);
    this._emitter.emit(TransmuxingEvents.IO_ERROR, type, info);
    this._disableStatisticsReporter();
}

TransmuxingController.prototype._onDemuxException = function (type, info) {
    Log.e(this.TAG, `DemuxException: type = ${type}, info = ${info}`);
    this._emitter.emit(TransmuxingEvents.DEMUX_ERROR, type, info);
}

TransmuxingController.prototype._onRemuxerInitSegmentArrival = function (type, initSegment) {
    this._emitter.emit(TransmuxingEvents.INIT_SEGMENT, type, initSegment);
}

TransmuxingController.prototype._onRemuxerMediaSegmentArrival = function (type, mediaSegment) {
    if (this._pendingSeekTime != null) {
        // Media segments after new-segment cross-seeking should be dropped.
        return;
    }
    this._emitter.emit(TransmuxingEvents.MEDIA_SEGMENT, type, mediaSegment);

    // Resolve pending seekPoint
    if (this._pendingResolveSeekPoint != null && type === 'video') {
        let syncPoints = mediaSegment.info.syncPoints;
        let seekpoint = this._pendingResolveSeekPoint;
        this._pendingResolveSeekPoint = null;

        // Safari: Pass PTS for recommend_seekpoint
        if (Browser.safari && syncPoints.length > 0 && syncPoints[0].originalDts === seekpoint) {
            seekpoint = syncPoints[0].pts;
        }
        // else: use original DTS (keyframe.milliseconds)

        this._emitter.emit(TransmuxingEvents.RECOMMEND_SEEKPOINT, seekpoint);
    }
}

TransmuxingController.prototype._enableStatisticsReporter = function () {
    if (this._statisticsReporter == null) {
        this._statisticsReporter = self.setInterval(
            this._reportStatisticsInfo.bind(this),
            this._config.statisticsInfoReportInterval);
    }
}

TransmuxingController.prototype._disableStatisticsReporter = function () {
    if (this._statisticsReporter) {
        self.clearInterval(this._statisticsReporter);
        this._statisticsReporter = null;
    }
}

TransmuxingController.prototype._reportSegmentMediaInfo = function (segmentIndex) {
    let segmentInfo = this._mediaInfo.segments[segmentIndex];
    let exportInfo = Object.assign({}, segmentInfo);

    exportInfo.duration = this._mediaInfo.duration;
    exportInfo.segmentCount = this._mediaInfo.segmentCount;
    delete exportInfo.segments;
    delete exportInfo.keyframesIndex;

    this._emitter.emit(TransmuxingEvents.MEDIA_INFO, exportInfo);
}

TransmuxingController.prototype._reportStatisticsInfo = function () {
    let info = {};

    info.url = this._ioctl.currentURL;
    info.hasRedirect = this._ioctl.hasRedirect;
    if (info.hasRedirect) {
        info.redirectedURL = this._ioctl.currentRedirectedURL;
    }

    info.speed = this._ioctl.currentSpeed;
    info.loaderType = this._ioctl.loaderType;
    info.currentSegmentIndex = this._currentSegmentIndex;
    info.totalSegmentCount = this._mediaDataSource.segments.length;

    this._emitter.emit(TransmuxingEvents.STATISTICS_INFO, info);
}
