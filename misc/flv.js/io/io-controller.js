/**
 * DataSource: {
 *     url: string,
 *     filesize: number,
 *     cors: boolean,
 *     withCredentials: boolean
 * }
 * 
 */

// Manage IO Loaders
function IOController(dataSource, config, extraData) {
    this.TAG = 'IOController';

    this._config = config;
    this._extraData = extraData;

    this._stashInitialSize = 1024 * 384;  // default initial size: 384KB
    if (config.stashInitialSize != undefined && config.stashInitialSize > 0) {
        // apply from config
        this._stashInitialSize = config.stashInitialSize;
    }

    this._stashUsed = 0;
    this._stashSize = this._stashInitialSize;
    this._bufferSize = 1024 * 1024 * 3;  // initial size: 3MB
    this._stashBuffer = new ArrayBuffer(this._bufferSize);
    this._stashByteStart = 0;
    this._enableStash = true;
    if (config.enableStashBuffer === false) {
        this._enableStash = false;
    }

    this._loader = null;
    this._loaderClass = null;
    this._seekHandler = null;

    this._dataSource = dataSource;
    this._isWebSocketURL = /wss?:\/\/(.+?)/.test(dataSource.url);
    this._refTotalLength = dataSource.filesize ? dataSource.filesize : null;
    this._totalLength = this._refTotalLength;
    this._fullRequestFlag = false;
    this._currentRange = null;
    this._redirectedURL = null;

    this._speedNormalized = 0;
    this._speedSampler = new SpeedSampler();
    this._speedNormalizeList = [64, 128, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096];

    this._isEarlyEofReconnecting = false;

    this._paused = false;
    this._resumeFrom = 0;

    this._onDataArrival = null;
    this._onSeeked = null;
    this._onError = null;
    this._onComplete = null;
    this._onRedirect = null;
    this._onRecoveredEarlyEof = null;

    this._selectSeekHandler();
    this._selectLoader();
    this._createLoader();
}

IOController.prototype.destroy = function () {
    if (this._loader.isWorking()) {
        this._loader.abort();
    }
    this._loader.destroy();
    this._loader = null;
    this._loaderClass = null;
    this._dataSource = null;
    this._stashBuffer = null;
    this._stashUsed = this._stashSize = this._bufferSize = this._stashByteStart = 0;
    this._currentRange = null;
    this._speedSampler = null;

    this._isEarlyEofReconnecting = false;

    this._onDataArrival = null;
    this._onSeeked = null;
    this._onError = null;
    this._onComplete = null;
    this._onRedirect = null;
    this._onRecoveredEarlyEof = null;

    this._extraData = null;
}

IOController.prototype.isWorking = function () {
    return this._loader && this._loader.isWorking() && !this._paused;
}

IOController.prototype.isPaused = function () {
    return this._paused;
}

IOController.prototype.status = function () {
    return this._loader.status;
}

IOController.prototype.extraData = function (data) {
    if (typeof data === 'undefined') {
        return this._extraData;
    } else {
        this._extraData = data;
    }
}

// prototype: function onDataArrival(chunks: ArrayBuffer, byteStart: number): number
IOController.prototype.onDataArrival = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onDataArrival;
    } else {
        this._onDataArrival = callback;
    }
}

IOController.prototype.onSeeked = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onSeeked;
    } else {
        this._onSeeked = callback;
    }
}

// prototype: function onError(type: number, info: {code: number, msg: string}): void
IOController.prototype.onError = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onError;
    } else {
        this._onError = callback;
    }
}

IOController.prototype.onComplete = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onComplete;
    } else {
        this._onComplete = callback;
    }
}

IOController.prototype.onRedirect = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onRedirect;
    } else {
        this._onRedirect = callback;
    }
}

IOController.prototype.onRecoveredEarlyEof = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onRecoveredEarlyEof;
    } else {
        this._onRecoveredEarlyEof = callback;
    }
}

IOController.prototype.currentURL = function () {
    return this._dataSource.url;
}

IOController.prototype.hasRedirect = function () {
    return (this._redirectedURL != null || this._dataSource.redirectedURL != undefined);
}

IOController.prototype.currentRedirectedURL = function () {
    return this._redirectedURL || this._dataSource.redirectedURL;
}

// in KB/s
IOController.prototype.currentSpeed = function () {
    if (this._loaderClass === RangeLoader) {
        // SpeedSampler is inaccuracy if loader is RangeLoader
        return this._loader.currentSpeed;
    }
    return this._speedSampler.lastSecondKBps;
}

IOController.prototype.loaderType = function () {
    return this._loader.type;
}

IOController.prototype._selectSeekHandler = function () {
    let config = this._config;

    if (config.seekType === 'range') {
        this._seekHandler = new RangeSeekHandler(this._config.rangeLoadZeroStart);
    } else if (config.seekType === 'param') {
        let paramStart = config.seekParamStart || 'bstart';
        let paramEnd = config.seekParamEnd || 'bend';

        this._seekHandler = new ParamSeekHandler(paramStart, paramEnd);
    } else if (config.seekType === 'custom') {
        if (typeof config.customSeekHandler !== 'function') {
            throw new InvalidArgumentException('Custom seekType specified in config but invalid customSeekHandler!');
        }
        this._seekHandler = new config.customSeekHandler();
    } else {
        throw new InvalidArgumentException(`Invalid seekType in config: ${config.seekType}`);
    }
}

IOController.prototype._selectLoader = function () {
    if (this._config.customLoader != null) {
        this._loaderClass = this._config.customLoader;
    } else if (this._isWebSocketURL) {
        this._loaderClass = WebSocketLoader;
    } else if (FetchStreamLoader.isSupported()) {
        this._loaderClass = FetchStreamLoader;
    } else if (MozChunkedLoader.isSupported()) {
        this._loaderClass = MozChunkedLoader;
    } else if (RangeLoader.isSupported()) {
        this._loaderClass = RangeLoader;
    } else {
        throw new RuntimeException('Your browser doesn\'t support xhr with arraybuffer responseType!');
    }
}

IOController.prototype._createLoader = function () {
    this._loader = new this._loaderClass(this._seekHandler, this._config);
    if (this._loader.needStashBuffer === false) {
        this._enableStash = false;
    }
    this._loader.onContentLengthKnown(this._onContentLengthKnown.bind(this));
    this._loader.onURLRedirect(this._onURLRedirect.bind(this));
    this._loader.onDataArrival(this._onLoaderChunkArrival.bind(this));
    this._loader.onComplete(this._onLoaderComplete.bind(this));
    this._loader.onError(this._onLoaderError.bind(this));
}

IOController.prototype.open = function (optionalFrom) {
    this._currentRange = {from: 0, to: -1};
    if (optionalFrom) {
        this._currentRange.from = optionalFrom;
    }

    this._speedSampler.reset();
    if (!optionalFrom) {
        this._fullRequestFlag = true;
    }

    this._loader.open(this._dataSource, Object.assign({}, this._currentRange));
}

IOController.prototype.abort = function () {
    this._loader.abort();

    if (this._paused) {
        this._paused = false;
        this._resumeFrom = 0;
    }
}

IOController.prototype.pause = function () {
    if (this.isWorking()) {
        this._loader.abort();

        if (this._stashUsed !== 0) {
            this._resumeFrom = this._stashByteStart;
            this._currentRange.to = this._stashByteStart - 1;
        } else {
            this._resumeFrom = this._currentRange.to + 1;
        }
        this._stashUsed = 0;
        this._stashByteStart = 0;
        this._paused = true;
    }
}

IOController.prototype.resume = function () {
    if (this._paused) {
        this._paused = false;
        let bytes = this._resumeFrom;
        this._resumeFrom = 0;
        this._internalSeek(bytes, true);
    }
}

IOController.prototype.seek = function (bytes) {
    this._paused = false;
    this._stashUsed = 0;
    this._stashByteStart = 0;
    this._internalSeek(bytes, true);
}

/**
 * When seeking request is from media seeking, unconsumed stash data should be dropped
 * However, stash data shouldn't be dropped if seeking requested from http reconnection
 *
 * @dropUnconsumed: Ignore and discard all unconsumed data in stash buffer
 */
IOController.prototype._internalSeek = function (bytes, dropUnconsumed) {
    if (this._loader.isWorking()) {
        this._loader.abort();
    }

    // dispatch & flush stash buffer before seek
    this._flushStashBuffer(dropUnconsumed);

    this._loader.destroy();
    this._loader = null;

    let requestRange = {from: bytes, to: -1};
    this._currentRange = {from: requestRange.from, to: -1};

    this._speedSampler.reset();
    this._stashSize = this._stashInitialSize;
    this._createLoader();
    this._loader.open(this._dataSource, requestRange);

    if (this._onSeeked) {
        this._onSeeked();
    }
}

IOController.prototype.updateUrl = function (url) {
    if (!url || typeof url !== 'string' || url.length === 0) {
        throw new InvalidArgumentException('Url must be a non-empty string!');
    }

    this._dataSource.url = url;

    // TODO: replace with new url
}

IOController.prototype._expandBuffer = function (expectedBytes) {
    let bufferNewSize = this._stashSize;
    while (bufferNewSize + 1024 * 1024 * 1 < expectedBytes) {
        bufferNewSize *= 2;
    }

    bufferNewSize += 1024 * 1024 * 1;  // bufferSize = stashSize + 1MB
    if (bufferNewSize === this._bufferSize) {
        return;
    }

    let newBuffer = new ArrayBuffer(bufferNewSize);

    if (this._stashUsed > 0) {  // copy existing data into new buffer
        let stashOldArray = new Uint8Array(this._stashBuffer, 0, this._stashUsed);
        let stashNewArray = new Uint8Array(newBuffer, 0, bufferNewSize);
        stashNewArray.set(stashOldArray, 0);
    }

    this._stashBuffer = newBuffer;
    this._bufferSize = bufferNewSize;
}

IOController.prototype._normalizeSpeed = function (input) {
    let list = this._speedNormalizeList;
    let last = list.length - 1;
    let mid = 0;
    let lbound = 0;
    let ubound = last;

    if (input < list[0]) {
        return list[0];
    }

    // binary search
    while (lbound <= ubound) {
        mid = lbound + Math.floor((ubound - lbound) / 2);
        if (mid === last || (input >= list[mid] && input < list[mid + 1])) {
            return list[mid];
        } else if (list[mid] < input) {
            lbound = mid + 1;
        } else {
            ubound = mid - 1;
        }
    }
}

IOController.prototype._adjustStashSize = function (normalized) {
    let stashSizeKB = 0;

    if (this._config.isLive) {
        // live stream: always use single normalized speed for size of stashSizeKB
        stashSizeKB = normalized;
    } else {
        if (normalized < 512) {
            stashSizeKB = normalized;
        } else if (normalized >= 512 && normalized <= 1024) {
            stashSizeKB = Math.floor(normalized * 1.5);
        } else {
            stashSizeKB = normalized * 2;
        }
    }

    if (stashSizeKB > 8192) {
        stashSizeKB = 8192;
    }

    let bufferSize = stashSizeKB * 1024 + 1024 * 1024 * 1;  // stashSize + 1MB
    if (this._bufferSize < bufferSize) {
        this._expandBuffer(bufferSize);
    }
    this._stashSize = stashSizeKB * 1024;
}

IOController.prototype._dispatchChunks = function (chunks, byteStart) {
    this._currentRange.to = byteStart + chunks.byteLength - 1;
    return this._onDataArrival(chunks, byteStart);
}

IOController.prototype._onURLRedirect = function (redirectedURL) {
    this._redirectedURL = redirectedURL;
    if (this._onRedirect) {
        this._onRedirect(redirectedURL);
    }
}

IOController.prototype._onContentLengthKnown = function (contentLength) {
    if (contentLength && this._fullRequestFlag) {
        this._totalLength = contentLength;
        this._fullRequestFlag = false;
    }
}

IOController.prototype._onLoaderChunkArrival = function (chunk, byteStart, receivedLength) {
    if (!this._onDataArrival) {
        throw new IllegalStateException('IOController: No existing consumer (onDataArrival) callback!');
    }
    if (this._paused) {
        return;
    }
    if (this._isEarlyEofReconnecting) {
        // Auto-reconnect for EarlyEof succeed, notify to upper-layer by callback
        this._isEarlyEofReconnecting = false;
        if (this._onRecoveredEarlyEof) {
            this._onRecoveredEarlyEof();
        }
    }

    this._speedSampler.addBytes(chunk.byteLength);

    // adjust stash buffer size according to network speed dynamically
    let KBps = this._speedSampler.lastSecondKBps;
    if (KBps !== 0) {
        let normalized = this._normalizeSpeed(KBps);
        if (this._speedNormalized !== normalized) {
            this._speedNormalized = normalized;
            this._adjustStashSize(normalized);
        }
    }

    if (!this._enableStash) {  // disable stash
        if (this._stashUsed === 0) {
            // dispatch chunk directly to consumer;
            // check ret value (consumed bytes) and stash unconsumed to stashBuffer
            let consumed = this._dispatchChunks(chunk, byteStart);
            if (consumed < chunk.byteLength) {  // unconsumed data remain.
                let remain = chunk.byteLength - consumed;
                if (remain > this._bufferSize) {
                    this._expandBuffer(remain);
                }
                let stashArray = new Uint8Array(this._stashBuffer, 0, this._bufferSize);
                stashArray.set(new Uint8Array(chunk, consumed), 0);
                this._stashUsed += remain;
                this._stashByteStart = byteStart + consumed;
            }
        } else {
            // else: Merge chunk into stashBuffer, and dispatch stashBuffer to consumer.
            if (this._stashUsed + chunk.byteLength > this._bufferSize) {
                this._expandBuffer(this._stashUsed + chunk.byteLength);
            }
            let stashArray = new Uint8Array(this._stashBuffer, 0, this._bufferSize);
            stashArray.set(new Uint8Array(chunk), this._stashUsed);
            this._stashUsed += chunk.byteLength;
            let consumed = this._dispatchChunks(this._stashBuffer.slice(0, this._stashUsed), this._stashByteStart);
            if (consumed < this._stashUsed && consumed > 0) {  // unconsumed data remain
                let remainArray = new Uint8Array(this._stashBuffer, consumed);
                stashArray.set(remainArray, 0);
            }
            this._stashUsed -= consumed;
            this._stashByteStart += consumed;
        }
    } else {  // enable stash
        if (this._stashUsed === 0 && this._stashByteStart === 0) {  // seeked? or init chunk?
            // This is the first chunk after seek action
            this._stashByteStart = byteStart;
        }
        if (this._stashUsed + chunk.byteLength <= this._stashSize) {
            // just stash
            let stashArray = new Uint8Array(this._stashBuffer, 0, this._stashSize);
            stashArray.set(new Uint8Array(chunk), this._stashUsed);
            this._stashUsed += chunk.byteLength;
        } else {  // stashUsed + chunkSize > stashSize, size limit exceeded
            let stashArray = new Uint8Array(this._stashBuffer, 0, this._bufferSize);
            if (this._stashUsed > 0) {  // There're stash datas in buffer
                // dispatch the whole stashBuffer, and stash remain data
                // then append chunk to stashBuffer (stash)
                let buffer = this._stashBuffer.slice(0, this._stashUsed);
                let consumed = this._dispatchChunks(buffer, this._stashByteStart);
                if (consumed < buffer.byteLength) {
                    if (consumed > 0) {
                        let remainArray = new Uint8Array(buffer, consumed);
                        stashArray.set(remainArray, 0);
                        this._stashUsed = remainArray.byteLength;
                        this._stashByteStart += consumed;
                    }
                } else {
                    this._stashUsed = 0;
                    this._stashByteStart += consumed;
                }
                if (this._stashUsed + chunk.byteLength > this._bufferSize) {
                    this._expandBuffer(this._stashUsed + chunk.byteLength);
                    stashArray = new Uint8Array(this._stashBuffer, 0, this._bufferSize);
                }
                stashArray.set(new Uint8Array(chunk), this._stashUsed);
                this._stashUsed += chunk.byteLength;
            } else {  // stash buffer empty, but chunkSize > stashSize (oh, holy shit)
                // dispatch chunk directly and stash remain data
                let consumed = this._dispatchChunks(chunk, byteStart);
                if (consumed < chunk.byteLength) {
                    let remain = chunk.byteLength - consumed;
                    if (remain > this._bufferSize) {
                        this._expandBuffer(remain);
                        stashArray = new Uint8Array(this._stashBuffer, 0, this._bufferSize);
                    }
                    stashArray.set(new Uint8Array(chunk, consumed), 0);
                    this._stashUsed += remain;
                    this._stashByteStart = byteStart + consumed;
                }
            }
        }
    }
}

IOController.prototype._flushStashBuffer = function (dropUnconsumed) {
    if (this._stashUsed > 0) {
        let buffer = this._stashBuffer.slice(0, this._stashUsed);
        let consumed = this._dispatchChunks(buffer, this._stashByteStart);
        let remain = buffer.byteLength - consumed;

        if (consumed < buffer.byteLength) {
            if (dropUnconsumed) {
                Log.w(this.TAG, `${remain} bytes unconsumed data remain when flush buffer, dropped`);
            } else {
                if (consumed > 0) {
                    let stashArray = new Uint8Array(this._stashBuffer, 0, this._bufferSize);
                    let remainArray = new Uint8Array(buffer, consumed);
                    stashArray.set(remainArray, 0);
                    this._stashUsed = remainArray.byteLength;
                    this._stashByteStart += consumed;
                }
                return 0;
            }
        }
        this._stashUsed = 0;
        this._stashByteStart = 0;
        return remain;
    }
    return 0;
}

IOController.prototype._onLoaderComplete = function (from, to) {
    // Force-flush stash buffer, and drop unconsumed data
    this._flushStashBuffer(true);

    if (this._onComplete) {
        this._onComplete(this._extraData);
    }
}

IOController.prototype._onLoaderError = function (type, data) {
    Log.e(this.TAG, `Loader error, code = ${data.code}, msg = ${data.msg}`);

    this._flushStashBuffer(false);

    if (this._isEarlyEofReconnecting) {
        // Auto-reconnect for EarlyEof failed, throw UnrecoverableEarlyEof error to upper-layer
        this._isEarlyEofReconnecting = false;
        type = LoaderErrors.UNRECOVERABLE_EARLY_EOF;
    }

    switch (type) {
    case LoaderErrors.EARLY_EOF: {
        if (!this._config.isLive) {
            // Do internal http reconnect if not live stream
            if (this._totalLength) {
                let nextFrom = this._currentRange.to + 1;
                if (nextFrom < this._totalLength) {
                    Log.w(this.TAG, 'Connection lost, trying reconnect...');
                    this._isEarlyEofReconnecting = true;
                    this._internalSeek(nextFrom, false);
                }
                return;
            }
            // else: We don't know totalLength, throw UnrecoverableEarlyEof
        }
        // live stream: throw UnrecoverableEarlyEof error to upper-layer
        type = LoaderErrors.UNRECOVERABLE_EARLY_EOF;
        break;
    }
    case LoaderErrors.UNRECOVERABLE_EARLY_EOF:
    case LoaderErrors.CONNECTING_TIMEOUT:
    case LoaderErrors.HTTP_STATUS_CODE_INVALID:
    case LoaderErrors.EXCEPTION:
        break;
    }

    if (this._onError) {
        this._onError(type, data);
    } else {
        throw new RuntimeException('IOException: ' + data.msg);
    }
}

