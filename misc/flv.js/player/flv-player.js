function FlvPlayer(mediaDataSource, config) {
    this.TAG = 'FlvPlayer';
    this._type = 'FlvPlayer';
    this._emitter = new EventEmitter();

    this._config = createDefaultConfig();
    if (typeof config === 'object') {
        Object.assign(this._config, config);
    }

    if (mediaDataSource.type.toLowerCase() !== 'flv' && mediaDataSource.type.toLowerCase() !== 'cms') {
        throw new InvalidArgumentException('FlvPlayer requires an flv MediaDataSource input!');
    }

    if (mediaDataSource.isLive === true) {
        this._config.isLive = true;
    }

    this.e = {
        onvLoadedMetadata: this._onvLoadedMetadata.bind(this),
        onvSeeking: this._onvSeeking.bind(this),
        onvMouseOver: this._onvMouseOver.bind(this),
        onvMouseMove: this._onvMouseMove.bind(this),
        onvMouseOut: this._onvMouseOut.bind(this),
        onvCanPlay: this._onvCanPlay.bind(this),
        onvStalled: this._onvStalled.bind(this),
        onvProgress: this._onvProgress.bind(this)
    };

    if (self.performance && self.performance.now) {
        this._now = self.performance.now.bind(self.performance);
    } else {
        this._now = Date.now;
    }

    this._pendingSeekTime = null;  // in seconds
    this._requestSetTime = false;
    this._seekpointRecord = null;
    this._progressChecker = null;

    this._mediaDataSource = mediaDataSource;
    this._mediaElement = null;
    this._msectl = null;
    this._transmuxer = null;

    this._mseSourceOpened = false;
    this._hasPendingLoad = false;
    this._receivedCanPlay = false;

    this._mediaInfo = null;
    this._statisticsInfo = null;

    let chromeNeedIDRFix = (Browser.chrome &&
                            (Browser.version.major < 50 ||
                             (Browser.version.major === 50 && Browser.version.build < 2661)));
    this._alwaysSeekKeyframe = (chromeNeedIDRFix || Browser.msedge || Browser.msie) ? true : false;

    if (this._alwaysSeekKeyframe) {
        this._config.accurateSeek = false;
    }
}

FlvPlayer.prototype.destroy = function() {
    if (this._progressChecker != null) {
        window.clearInterval(this._progressChecker);
        this._progressChecker = null;
    }
    if (this._transmuxer) {
        this.unload();
    }
    if (this._mediaElement) {
        this.detachMediaElement();
    }
    this.e = null;
    this._mediaDataSource = null;

    this._emitter.removeAllListeners();
    this._emitter = null;
}

FlvPlayer.prototype.on = function(event, listener) {
    if (event === PlayerEvents.MEDIA_INFO) {
        if (this._mediaInfo != null) {
            Promise.resolve().then(() => {
                this._emitter.emit(PlayerEvents.MEDIA_INFO, this.mediaInfo);
            });
        }
    } else if (event === PlayerEvents.STATISTICS_INFO) {
        if (this._statisticsInfo != null) {
            Promise.resolve().then(() => {
                this._emitter.emit(PlayerEvents.STATISTICS_INFO, this.statisticsInfo);
            });
        }
    }
    this._emitter.addListener(event, listener);
}

FlvPlayer.prototype.off = function(event, listener) {
    this._emitter.removeListener(event, listener);
}

FlvPlayer.prototype.attachMediaElement = function(mediaElement) {
    this._mediaElement = mediaElement;
    mediaElement.addEventListener('loadedmetadata', this.e.onvLoadedMetadata);
    mediaElement.addEventListener('seeking', this.e.onvSeeking);
    mediaElement.addEventListener('mousemove', this.e.onvMouseMove);
    mediaElement.addEventListener('mouseover', this.e.onvMouseOver);
    mediaElement.addEventListener('mouseout', this.e.onvMouseOut);
    mediaElement.addEventListener('canplay', this.e.onvCanPlay);
    mediaElement.addEventListener('stalled', this.e.onvStalled);
    mediaElement.addEventListener('progress', this.e.onvProgress);

    this._msectl = new MSEController(this._config);

    this._msectl.on(MSEEvents.UPDATE_END, this._onmseUpdateEnd.bind(this));
    this._msectl.on(MSEEvents.BUFFER_FULL, this._onmseBufferFull.bind(this));
    this._msectl.on(MSEEvents.SOURCE_OPEN, () => {
        this._mseSourceOpened = true;
        if (this._hasPendingLoad) {
            this._hasPendingLoad = false;
            this.load();
        }
    });
    this._msectl.on(MSEEvents.ERROR, (info) => {
        this._emitter.emit(PlayerEvents.ERROR,
                           ErrorTypes.MEDIA_ERROR,
                           ErrorDetails.MEDIA_MSE_ERROR,
                           info
                          );
    });

    this._msectl.attachMediaElement(mediaElement);

    if (this._pendingSeekTime != null) {
        try {
            mediaElement.currentTime = this._pendingSeekTime;
            this._pendingSeekTime = null;
        } catch (e) {
            // IE11 may throw InvalidStateError if readyState === 0
            // We can defer set currentTime operation after loadedmetadata
        }
    }
}

FlvPlayer.prototype.detachMediaElement = function() {
    if (this._mediaElement) {
        this._msectl.detachMediaElement();
        this._mediaElement.removeEventListener('loadedmetadata', this.e.onvLoadedMetadata);
        this._mediaElement.removeEventListener('seeking', this.e.onvSeeking);
        this._mediaElement.removeEventListener('mousemove', this.e.onvMouseMove);
        this._mediaElement.removeEventListener('mouseover', this.e.onvMouseOver);
        this._mediaElement.removeEventListener('mouseout', this.e.onvMouseOut);
        this._mediaElement.removeEventListener('canplay', this.e.onvCanPlay);
        this._mediaElement.removeEventListener('stalled', this.e.onvStalled);
        this._mediaElement.removeEventListener('progress', this.e.onvProgress);
        this._mediaElement = null;
    }
    if (this._msectl) {
        this._msectl.destroy();
        this._msectl = null;
    }
}

FlvPlayer.prototype.load = function() {
    if (!this._mediaElement) {
        throw new IllegalStateException('HTMLMediaElement must be attached before load()!');
    }
    if (this._transmuxer) {
        throw new IllegalStateException('FlvPlayer.load() has been called, please call unload() first!');
    }
    if (this._hasPendingLoad) {
        return;
    }

    if (this._config.deferLoadAfterSourceOpen && this._mseSourceOpened === false) {
        this._hasPendingLoad = true;
        return;
    }

    if (this._mediaElement.readyState > 0) {
        this._requestSetTime = true;
        // IE11 may throw InvalidStateError if readyState === 0
        this._mediaElement.currentTime = 0;
    }

    this._transmuxer = new Transmuxer(this._mediaDataSource, this._config);

    this._transmuxer.on(TransmuxingEvents.INIT_SEGMENT, (type, is) => {
        this._msectl.appendInitSegment(is);
    });
    this._transmuxer.on(TransmuxingEvents.MEDIA_SEGMENT, (type, ms) => {
        this._msectl.appendMediaSegment(ms);

        // lazyLoad check
        if (this._config.lazyLoad && !this._config.isLive) {
            let currentTime = this._mediaElement.currentTime;
            if (ms.info.endDts >= (currentTime + this._config.lazyLoadMaxDuration) * 1000) {
                if (this._progressChecker == null) {
                    Log.v(this.TAG, 'Maximum buffering duration exceeded, suspend transmuxing task');
                    this._suspendTransmuxer();
                }
            }
        }
    });
    this._transmuxer.on(TransmuxingEvents.LOADING_COMPLETE, () => {
        this._msectl.endOfStream();
        this._emitter.emit(PlayerEvents.LOADING_COMPLETE);
    });
    this._transmuxer.on(TransmuxingEvents.RECOVERED_EARLY_EOF, () => {
        this._emitter.emit(PlayerEvents.RECOVERED_EARLY_EOF);
    });
    this._transmuxer.on(TransmuxingEvents.IO_ERROR, (detail, info) => {
        this._emitter.emit(PlayerEvents.ERROR, ErrorTypes.NETWORK_ERROR, detail, info);
    });
    this._transmuxer.on(TransmuxingEvents.DEMUX_ERROR, (detail, info) => {
        this._emitter.emit(PlayerEvents.ERROR, ErrorTypes.MEDIA_ERROR, detail, {code: -1, msg: info});
    });
    this._transmuxer.on(TransmuxingEvents.MEDIA_INFO, (mediaInfo) => {
        this._mediaInfo = mediaInfo;
        this._emitter.emit(PlayerEvents.MEDIA_INFO, Object.assign({}, mediaInfo));
    });
    this._transmuxer.on(TransmuxingEvents.METADATA_ARRIVED, (metadata) => {
        this._emitter.emit(PlayerEvents.METADATA_ARRIVED, metadata);
    });
    this._transmuxer.on(TransmuxingEvents.SCRIPTDATA_ARRIVED, (data) => {
        this._emitter.emit(PlayerEvents.SCRIPTDATA_ARRIVED, data);
    });
    this._transmuxer.on(TransmuxingEvents.STATISTICS_INFO, (statInfo) => {
        this._statisticsInfo = this._fillStatisticsInfo(statInfo);
        this._emitter.emit(PlayerEvents.STATISTICS_INFO, Object.assign({}, this._statisticsInfo));
    });
    this._transmuxer.on(TransmuxingEvents.RECOMMEND_SEEKPOINT, (milliseconds) => {
        if (this._mediaElement && !this._config.accurateSeek) {
            this._requestSetTime = true;
            this._mediaElement.currentTime = milliseconds / 1000;
        }
    });

    this._transmuxer.open();
}

FlvPlayer.prototype.unload = function () {
    if (this._mediaElement) {
        this._mediaElement.pause();
    }
    if (this._msectl) {
        this._msectl.seek(0);
    }
    if (this._transmuxer) {
        this._transmuxer.close();
        this._transmuxer.destroy();
        this._transmuxer = null;
    }
}

FlvPlayer.prototype.play = function() {
    return this._mediaElement.play();
}

FlvPlayer.prototype.pause = function() {
    this._mediaElement.pause();
}

FlvPlayer.prototype.type = function() {
    return this._type;
}

FlvPlayer.prototype.buffered = function() {
    return this._mediaElement.buffered;
}

FlvPlayer.prototype.duration = function() {
    return this._mediaElement.duration;
}

FlvPlayer.prototype.volume = function(value) {
    if (typeof value === 'undefined') {
        return this._mediaElement.volume;
    } else {
        this._mediaElement.volume = value;
    }
}
FlvPlayer.prototype.muted = function (value) {
    if (typeof value === 'undefined') {
        return this._mediaElement.muted;
    } else {
        this._mediaElement.muted = muted;
    }
}
FlvPlayer.prototype.currentTime = function (seconds) {
    if (typeof seconds === 'undefined') {
        if (this._mediaElement) {
            return this._mediaElement.currentTime;
        }
        return 0;
    } else {
        if (this._mediaElement) {
            this._internalSeek(seconds);
        } else {
            this._pendingSeekTime = seconds;
        }
    }
}
FlvPlayer.prototype.mediaInfo = function() {
    return Object.assign({}, this._mediaInfo);
}

FlvPlayer.prototype.statisticsInfo = function() {
    if (this._statisticsInfo == null) {
        this._statisticsInfo = {};
    }
    this._statisticsInfo = this._fillStatisticsInfo(this._statisticsInfo);
    return Object.assign({}, this._statisticsInfo);
}

FlvPlayer.prototype._fillStatisticsInfo = function (statInfo) {
    statInfo.playerType = this._type;

    if (!(this._mediaElement instanceof HTMLVideoElement)) {
        return statInfo;
    }

    let hasQualityInfo = true;
    let decoded = 0;
    let dropped = 0;

    if (this._mediaElement.getVideoPlaybackQuality) {
        let quality = this._mediaElement.getVideoPlaybackQuality();
        decoded = quality.totalVideoFrames;
        dropped = quality.droppedVideoFrames;
    } else if (this._mediaElement.webkitDecodedFrameCount != undefined) {
        decoded = this._mediaElement.webkitDecodedFrameCount;
        dropped = this._mediaElement.webkitDroppedFrameCount;
    } else {
        hasQualityInfo = false;
    }

    if (hasQualityInfo) {
        statInfo.decodedFrames = decoded;
        statInfo.droppedFrames = dropped;
    }

    return statInfo;
}

FlvPlayer.prototype._onmseUpdateEnd = function () {
    if (!this._config.lazyLoad || this._config.isLive) {
        return;
    }
    let buffered = this._mediaElement.buffered;
    let currentTime = this._mediaElement.currentTime;
    let currentRangeStart = 0;
    let currentRangeEnd = 0;

    for (let i = 0; i < buffered.length; i++) {
        let start = buffered.start(i);
        let end = buffered.end(i);
        if (start <= currentTime && currentTime < end) {
            currentRangeStart = start;
            currentRangeEnd = end;
            break;
        }
    }

    if (currentRangeEnd >= currentTime + this._config.lazyLoadMaxDuration && this._progressChecker == null) {
        Log.v(this.TAG, 'Maximum buffering duration exceeded, suspend transmuxing task');
        this._suspendTransmuxer();
    }
}

FlvPlayer.prototype._onmseBufferFull = function () {
    Log.v(this.TAG, 'MSE SourceBuffer is full, suspend transmuxing task');
    if (this._progressChecker == null) {
        this._suspendTransmuxer();
    }
}

FlvPlayer.prototype._suspendTransmuxer = function () {
    if (this._transmuxer) {
        this._transmuxer.pause();

        if (this._progressChecker == null) {
            this._progressChecker = window.setInterval(this._checkProgressAndResume.bind(this), 1000);
        }
    }
}

FlvPlayer.prototype._checkProgressAndResume = function () {
    let currentTime = this._mediaElement.currentTime;
    let buffered = this._mediaElement.buffered;

    let needResume = false;

    for (let i = 0; i < buffered.length; i++) {
        let from = buffered.start(i);
        let to = buffered.end(i);
        if (currentTime >= from && currentTime < to) {
            if (currentTime >= to - this._config.lazyLoadRecoverDuration) {
                needResume = true;
            }
            break;
        }
    }

    if (needResume) {
        window.clearInterval(this._progressChecker);
        this._progressChecker = null;
        if (needResume) {
            Log.v(this.TAG, 'Continue loading from paused position');
            this._transmuxer.resume();
        }
    }
}

FlvPlayer.prototype._isTimepointBuffered = function (seconds) {
    let buffered = this._mediaElement.buffered;

    for (let i = 0; i < buffered.length; i++) {
        let from = buffered.start(i);
        let to = buffered.end(i);
        if (seconds >= from && seconds < to) {
            return true;
        }
    }
    return false;
}

FlvPlayer.prototype._internalSeek = function (seconds) {
    let directSeek = this._isTimepointBuffered(seconds);

    let directSeekBegin = false;
    let directSeekBeginTime = 0;

    if (seconds < 1.0 && this._mediaElement.buffered.length > 0) {
        let videoBeginTime = this._mediaElement.buffered.start(0);
        if ((videoBeginTime < 1.0 && seconds < videoBeginTime) || Browser.safari) {
            directSeekBegin = true;
            // also workaround for Safari: Seek to 0 may cause video stuck, use 0.1 to avoid
            directSeekBeginTime = Browser.safari ? 0.1 : videoBeginTime;
        }
    }

    if (directSeekBegin) {  // seek to video begin, set currentTime directly if beginPTS buffered
        this._requestSetTime = true;
        this._mediaElement.currentTime = directSeekBeginTime;
    }  else if (directSeek) {  // buffered position
        if (!this._alwaysSeekKeyframe) {
            this._requestSetTime = true;
            this._mediaElement.currentTime = seconds;
        } else {
            let idr = this._msectl.getNearestKeyframe(Math.floor(seconds * 1000));
            this._requestSetTime = true;
            if (idr != null) {
                this._mediaElement.currentTime = idr.dts / 1000;
            } else {
                this._mediaElement.currentTime = seconds;
            }
        }
        if (this._progressChecker != null) {
            this._checkProgressAndResume();
        }
    } else {
        if (this._progressChecker != null) {
            window.clearInterval(this._progressChecker);
            this._progressChecker = null;
        }
        this._msectl.seek(seconds);
        this._transmuxer.seek(Math.floor(seconds * 1000));  // in milliseconds
        // no need to set mediaElement.currentTime if non-accurateSeek,
        // just wait for the recommend_seekpoint callback
        if (this._config.accurateSeek) {
            this._requestSetTime = true;
            this._mediaElement.currentTime = seconds;
        }
    }
}

FlvPlayer.prototype._checkAndApplyUnbufferedSeekpoint = function () {
    if (this._seekpointRecord) {
        if (this._seekpointRecord.recordTime <= this._now() - 100) {
            let target = this._mediaElement.currentTime;
            this._seekpointRecord = null;
            if (!this._isTimepointBuffered(target)) {
                if (this._progressChecker != null) {
                    window.clearTimeout(this._progressChecker);
                    this._progressChecker = null;
                }
                // .currentTime is consists with .buffered timestamp
                // Chrome/Edge use DTS, while FireFox/Safari use PTS
                this._msectl.seek(target);
                this._transmuxer.seek(Math.floor(target * 1000));
                // set currentTime if accurateSeek, or wait for recommend_seekpoint callback
                if (this._config.accurateSeek) {
                    this._requestSetTime = true;
                    this._mediaElement.currentTime = target;
                }
            }
        } else {
            window.setTimeout(this._checkAndApplyUnbufferedSeekpoint.bind(this), 50);
        }
    }
}

FlvPlayer.prototype._checkAndResumeStuckPlayback = function (stalled) {
    let media = this._mediaElement;
    if (stalled || !this._receivedCanPlay || media.readyState < 2) {  // HAVE_CURRENT_DATA
        let buffered = media.buffered;
        if (buffered.length > 0 && media.currentTime < buffered.start(0)) {
            Log.w(this.TAG, `Playback seems stuck at ${media.currentTime}, seek to ${buffered.start(0)}`);
            this._requestSetTime = true;
            this._mediaElement.currentTime = buffered.start(0);
            this._mediaElement.removeEventListener('progress', this.e.onvProgress);
        }
    } else {
        // Playback didn't stuck, remove progress event listener
        this._mediaElement.removeEventListener('progress', this.e.onvProgress);
    }
}

FlvPlayer.prototype._onvLoadedMetadata = function (e) {
    console.log("FlvPlayer.prototype._onvLoadedMetadata");
    if (this._pendingSeekTime != null) {
        this._mediaElement.currentTime = this._pendingSeekTime;
        this._pendingSeekTime = null;
    }
}

FlvPlayer.prototype._onvMouseOver = function (e) {
    console.log('MouseOver', e.timeStamp);
};
FlvPlayer.prototype._onvMouseMove = function (e) {
    console.log('MouseMove', e.timeStamp, e);
};
FlvPlayer.prototype._onvMouseOut = function (e) {
    console.log('MouseOut', e.timeStamp);
};
FlvPlayer.prototype._onvSeeking = function (e) {  // handle seeking request from browser's progress bar
    console.log(e.timeStamp);
    let target = this._mediaElement.currentTime;
    let buffered = this._mediaElement.buffered;

    if (this._requestSetTime) {
        this._requestSetTime = false;
        return;
    }

    if (target < 1.0 && buffered.length > 0) {
        // seek to video begin, set currentTime directly if beginPTS buffered
        let videoBeginTime = buffered.start(0);
        if ((videoBeginTime < 1.0 && target < videoBeginTime) || Browser.safari) {
            this._requestSetTime = true;
            // also workaround for Safari: Seek to 0 may cause video stuck, use 0.1 to avoid
            this._mediaElement.currentTime = Browser.safari ? 0.1 : videoBeginTime;
            return;
        }
    }

    if (this._isTimepointBuffered(target)) {
        if (this._alwaysSeekKeyframe) {
            let idr = this._msectl.getNearestKeyframe(Math.floor(target * 1000));
            if (idr != null) {
                this._requestSetTime = true;
                this._mediaElement.currentTime = idr.dts / 1000;
            }
        }
        if (this._progressChecker != null) {
            this._checkProgressAndResume();
        }
        return;
    }

    this._seekpointRecord = {
        seekPoint: target,
        recordTime: this._now()
    };
    window.setTimeout(this._checkAndApplyUnbufferedSeekpoint.bind(this), 50);
}

FlvPlayer.prototype._onvCanPlay = function (e) {
    console.log("FlvPlayer.prototype._onvCanPlay");
    this._receivedCanPlay = true;
    this._mediaElement.removeEventListener('canplay', this.e.onvCanPlay);
}

FlvPlayer.prototype._onvStalled = function (e) {
    console.log("FlvPlayer.prototype._onvStalled");
    this._checkAndResumeStuckPlayback(true);
}

FlvPlayer.prototype._onvProgress = function (e) {
    console.log("FlvPlayer.prototype._onvProgress");
    this._checkAndResumeStuckPlayback();
}
