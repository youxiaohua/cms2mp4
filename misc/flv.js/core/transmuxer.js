function Transmuxer(mediaDataSource, config) {
    this.TAG = 'Transmuxer';
    this._emitter = new EventEmitter();

    if (config.enableWorker && typeof (Worker) !== 'undefined') {
        try {
            let work = require('webworkify');
            this._worker = work(TransmuxingWorker);
            this._workerDestroying = false;
            this._worker.addEventListener('message', this._onWorkerMessage.bind(this));
            this._worker.postMessage({cmd: 'init', param: [mediaDataSource, config]});
            this.e = {
                onLoggingConfigChanged: this._onLoggingConfigChanged.bind(this)
            };
            LoggingControl.registerListener(this.e.onLoggingConfigChanged);
            this._worker.postMessage({cmd: 'logging_config', param: LoggingControl.getConfig()});
        } catch (error) {
            Log.e(this.TAG, 'Error while initialize transmuxing worker, fallback to inline transmuxing');
            this._worker = null;
            this._controller = new TransmuxingController(mediaDataSource, config);
        }
    } else {
        this._controller = new TransmuxingController(mediaDataSource, config);
    }

    if (this._controller) {
        let ctl = this._controller;
        ctl.on(TransmuxingEvents.IO_ERROR, this._onIOError.bind(this));
        ctl.on(TransmuxingEvents.DEMUX_ERROR, this._onDemuxError.bind(this));
        ctl.on(TransmuxingEvents.INIT_SEGMENT, this._onInitSegment.bind(this));
        ctl.on(TransmuxingEvents.MEDIA_SEGMENT, this._onMediaSegment.bind(this));
        ctl.on(TransmuxingEvents.LOADING_COMPLETE, this._onLoadingComplete.bind(this));
        ctl.on(TransmuxingEvents.RECOVERED_EARLY_EOF, this._onRecoveredEarlyEof.bind(this));
        ctl.on(TransmuxingEvents.MEDIA_INFO, this._onMediaInfo.bind(this));
        ctl.on(TransmuxingEvents.METADATA_ARRIVED, this._onMetaDataArrived.bind(this));
        ctl.on(TransmuxingEvents.SCRIPTDATA_ARRIVED, this._onScriptDataArrived.bind(this));
        ctl.on(TransmuxingEvents.STATISTICS_INFO, this._onStatisticsInfo.bind(this));
        ctl.on(TransmuxingEvents.RECOMMEND_SEEKPOINT, this._onRecommendSeekpoint.bind(this));
    }
}

Transmuxer.prototype.destroy = function () {
    if (this._worker) {
        if (!this._workerDestroying) {
            this._workerDestroying = true;
            this._worker.postMessage({cmd: 'destroy'});
            LoggingControl.removeListener(this.e.onLoggingConfigChanged);
            this.e = null;
        }
    } else {
        this._controller.destroy();
        this._controller = null;
    }
    this._emitter.removeAllListeners();
    this._emitter = null;
}

Transmuxer.prototype.on = function (event, listener) {
    this._emitter.addListener(event, listener);
}

Transmuxer.prototype.off = function (event, listener) {
    this._emitter.removeListener(event, listener);
}

Transmuxer.prototype.hasWorker = function () {
    return this._worker != null;
}

Transmuxer.prototype.open = function () {
    if (this._worker) {
        this._worker.postMessage({cmd: 'start'});
    } else {
        this._controller.start();
    }
}

Transmuxer.prototype.close = function () {
    if (this._worker) {
        this._worker.postMessage({cmd: 'stop'});
    } else {
        this._controller.stop();
    }
}

Transmuxer.prototype.seek = function (milliseconds) {
    if (this._worker) {
        this._worker.postMessage({cmd: 'seek', param: milliseconds});
    } else {
        this._controller.seek(milliseconds);
    }
}

Transmuxer.prototype.pause = function () {
    if (this._worker) {
        this._worker.postMessage({cmd: 'pause'});
    } else {
        this._controller.pause();
    }
}

Transmuxer.prototype.resume = function () {
    if (this._worker) {
        this._worker.postMessage({cmd: 'resume'});
    } else {
        this._controller.resume();
    }
}

Transmuxer.prototype._onInitSegment = function (type, initSegment) {
    // do async invoke
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.INIT_SEGMENT, type, initSegment);
    });
}

Transmuxer.prototype._onMediaSegment = function (type, mediaSegment) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.MEDIA_SEGMENT, type, mediaSegment);
    });
}

Transmuxer.prototype._onLoadingComplete = function () {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.LOADING_COMPLETE);
    });
}

Transmuxer.prototype._onRecoveredEarlyEof = function () {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.RECOVERED_EARLY_EOF);
    });
}

Transmuxer.prototype._onMediaInfo = function (mediaInfo) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.MEDIA_INFO, mediaInfo);
    });
}

Transmuxer.prototype._onMetaDataArrived = function (metadata) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.METADATA_ARRIVED, metadata);
    });
}

Transmuxer.prototype._onScriptDataArrived = function (data) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.SCRIPTDATA_ARRIVED, data);
    });
}

Transmuxer.prototype._onStatisticsInfo = function (statisticsInfo) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.STATISTICS_INFO, statisticsInfo);
    });
}

Transmuxer.prototype._onIOError = function (type, info) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.IO_ERROR, type, info);
    });
}

Transmuxer.prototype._onDemuxError = function (type, info) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.DEMUX_ERROR, type, info);
    });
}

Transmuxer.prototype._onRecommendSeekpoint = function (milliseconds) {
    Promise.resolve().then(() => {
        this._emitter.emit(TransmuxingEvents.RECOMMEND_SEEKPOINT, milliseconds);
    });
}

Transmuxer.prototype._onLoggingConfigChanged = function (config) {
    if (this._worker) {
        this._worker.postMessage({cmd: 'logging_config', param: config});
    }
}

Transmuxer.prototype._onWorkerMessage = function (e) {
    let message = e.data;
    let data = message.data;

    if (message.msg === 'destroyed' || this._workerDestroying) {
        this._workerDestroying = false;
        this._worker.terminate();
        this._worker = null;
        return;
    }

    switch (message.msg) {
    case TransmuxingEvents.INIT_SEGMENT:
    case TransmuxingEvents.MEDIA_SEGMENT:
        this._emitter.emit(message.msg, data.type, data.data);
        break;
    case TransmuxingEvents.LOADING_COMPLETE:
    case TransmuxingEvents.RECOVERED_EARLY_EOF:
        this._emitter.emit(message.msg);
        break;
    case TransmuxingEvents.MEDIA_INFO:
        Object.setPrototypeOf(data, MediaInfo.prototype);
        this._emitter.emit(message.msg, data);
        break;
    case TransmuxingEvents.METADATA_ARRIVED:
    case TransmuxingEvents.SCRIPTDATA_ARRIVED:
    case TransmuxingEvents.STATISTICS_INFO:
        this._emitter.emit(message.msg, data);
        break;
    case TransmuxingEvents.IO_ERROR:
    case TransmuxingEvents.DEMUX_ERROR:
        this._emitter.emit(message.msg, data.type, data.info);
        break;
    case TransmuxingEvents.RECOMMEND_SEEKPOINT:
        this._emitter.emit(message.msg, data);
        break;
    case 'logcat_callback':
        Log.emitter.emit('log', data.type, data.logcat);
        break;
    default:
        break;
    }
}
