function NativePlayer(mediaDataSource, config) {
    this.TAG = 'NativePlayer';
    this._type = 'NativePlayer';
    this._emitter = new EventEmitter();

    this._config = createDefaultConfig();
    if (typeof config === 'object') {
        Object.assign(this._config, config);
    }

    if (mediaDataSource.type.toLowerCase() === 'flv') {
        throw new InvalidArgumentException('NativePlayer does\'t support flv MediaDataSource input!');
    }
    if (mediaDataSource.hasOwnProperty('segments')) {
        throw new InvalidArgumentException(`NativePlayer(${mediaDataSource.type}) doesn't support multipart playback!`);
    }

    this.e = {
        onvLoadedMetadata: this._onvLoadedMetadata.bind(this)
    };

    this._pendingSeekTime = null;
    this._statisticsReporter = null;

    this._mediaDataSource = mediaDataSource;
    this._mediaElement = null;
}

NativePlayer.prototype.destroy = function () {
    if (this._mediaElement) {
        this.unload();
        this.detachMediaElement();
    }
    this.e = null;
    this._mediaDataSource = null;
    this._emitter.removeAllListeners();
    this._emitter = null;
}

NativePlayer.prototype.on = function (event, listener) {
    if (event === PlayerEvents.MEDIA_INFO) {
        if (this._mediaElement != null && this._mediaElement.readyState !== 0) {  // HAVE_NOTHING
            Promise.resolve().then(() => {
                this._emitter.emit(PlayerEvents.MEDIA_INFO, this.mediaInfo);
            });
        }
    } else if (event === PlayerEvents.STATISTICS_INFO) {
        if (this._mediaElement != null && this._mediaElement.readyState !== 0) {
            Promise.resolve().then(() => {
                this._emitter.emit(PlayerEvents.STATISTICS_INFO, this.statisticsInfo);
            });
        }
    }
    this._emitter.addListener(event, listener);
}

NativePlayer.prototype.off = function (event, listener) {
    this._emitter.removeListener(event, listener);
}

NativePlayer.prototype.attachMediaElement = function (mediaElement) {
    this._mediaElement = mediaElement;
    mediaElement.addEventListener('loadedmetadata', this.e.onvLoadedMetadata);

    if (this._pendingSeekTime != null) {
        try {
            mediaElement.currentTime = this._pendingSeekTime;
            this._pendingSeekTime = null;
        } catch (e) {
            // IE11 may throw InvalidStateError if readyState === 0
            // Defer set currentTime operation after loadedmetadata
        }
    }
}

NativePlayer.prototype.detachMediaElement = function() {
    if (this._mediaElement) {
        this._mediaElement.src = '';
        this._mediaElement.removeAttribute('src');
        this._mediaElement.removeEventListener('loadedmetadata', this.e.onvLoadedMetadata);
        this._mediaElement = null;
    }
    if (this._statisticsReporter != null) {
        window.clearInterval(this._statisticsReporter);
        this._statisticsReporter = null;
    }
}

NativePlayer.prototype.load = function () {
    if (!this._mediaElement) {
        throw new IllegalStateException('HTMLMediaElement must be attached before load()!');
    }
    this._mediaElement.src = this._mediaDataSource.url;

    if (this._mediaElement.readyState > 0) {
        this._mediaElement.currentTime = 0;
    }

    this._mediaElement.preload = 'auto';
    this._mediaElement.load();
    this._statisticsReporter = window.setInterval(
        this._reportStatisticsInfo.bind(this),
        this._config.statisticsInfoReportInterval);
}

NativePlayer.prototype.unload = function () {
    if (this._mediaElement) {
        this._mediaElement.src = '';
        this._mediaElement.removeAttribute('src');
    }
    if (this._statisticsReporter != null) {
        window.clearInterval(this._statisticsReporter);
        this._statisticsReporter = null;
    }
}

NativePlayer.prototype.play = function () {
    return this._mediaElement.play();
}

NativePlayer.prototype.pause = function () {
    this._mediaElement.pause();
}

NativePlayer.prototype.type = function() {
    return this._type;
}

NativePlayer.prototype.buffered = function () {
    return this._mediaElement.buffered;
}

NativePlayer.prototype.duration = function () {
    return this._mediaElement.duration;
}

NativePlayer.prototype.volume = function (value) {
    if (typeof value === 'undefined') {
        return this._mediaElement.volume;
    } else {
        this._mediaElement.volume = value;
    }
}

NativePlayer.prototype.muted = function (value) {
    if (typeof value === 'undefined') {
        return this._mediaElement.muted;
    } else {
        this._mediaElement.muted = muted;
    }
}

NativePlayer.prototype.currentTime = function (seconds) {
    if (typeof seconds === 'undefined') {
        if (this._mediaElement) {
            return this._mediaElement.currentTime;
        }
        return 0;
    } else {
        if (this._mediaElement) {
            this._mediaElement.currentTime = seconds;
        } else {
            this._pendingSeekTime = seconds;
        }
    }
}

NativePlayer.prototype.mediaInfo = function () {
    let mediaPrefix = (this._mediaElement instanceof HTMLAudioElement) ? 'audio/' : 'video/';
    let info = {
        mimeType: mediaPrefix + this._mediaDataSource.type
    };
    if (this._mediaElement) {
        info.duration = Math.floor(this._mediaElement.duration * 1000);
        if (this._mediaElement instanceof HTMLVideoElement) {
            info.width = this._mediaElement.videoWidth;
            info.height = this._mediaElement.videoHeight;
        }
    }
    return info;
}

NativePlayer.prototype.statisticsInfo = function () {
    let info = {
        playerType: this._type,
        url: this._mediaDataSource.url
    };

    if (!(this._mediaElement instanceof HTMLVideoElement)) {
        return info;
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
        info.decodedFrames = decoded;
        info.droppedFrames = dropped;
    }
    
    return info;
}

NativePlayer.prototype._onvLoadedMetadata = function (e) {
    if (this._pendingSeekTime != null) {
        this._mediaElement.currentTime = this._pendingSeekTime;
        this._pendingSeekTime = null;
    }
    this._emitter.emit(PlayerEvents.MEDIA_INFO, this.mediaInfo);
}

NativePlayer.prototype._reportStatisticsInfo = function () {
    this._emitter.emit(PlayerEvents.STATISTICS_INFO, this.statisticsInfo);
}
