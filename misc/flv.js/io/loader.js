var LoaderStatus = {
    kIdle       : 0,
    kConnecting : 1,
    kBuffering  : 2,
    kError      : 3,
    kComplete   : 4
};

var LoaderErrors = {
    OK                       : 'OK',
    EXCEPTION                : 'Exception',
    HTTP_STATUS_CODE_INVALID : 'HttpStatusCodeInvalid',
    CONNECTING_TIMEOUT       : 'ConnectingTimeout',
    EARLY_EOF                : 'EarlyEof',
    UNRECOVERABLE_EARLY_EOF  : 'UnrecoverableEarlyEof'
};

/* Loader has callbacks which have following prototypes:
 *     function onContentLengthKnown(contentLength: number): void
 *     function onURLRedirect(url: string): void
 *     function onDataArrival(chunk: ArrayBuffer, byteStart: number, receivedLength: number): void
 *     function onError(errorType: number, errorInfo: {code: number, msg: string}): void
 *     function onComplete(rangeFrom: number, rangeTo: number): void
 */
function BaseLoader(typeName) {
    this._type = typeName || 'undefined';
    this._status = LoaderStatus.kIdle;
    this._needStash = false;
    // callbacks
    this._onContentLengthKnown = null;
    this._onURLRedirect = null;
    this._onDataArrival = null;
    this._onError = null;
    this._onComplete = null;
}

BaseLoader.prototype.destroy = function () {
    this._status = LoaderStatus.kIdle;
    this._onContentLengthKnown = null;
    this._onURLRedirect = null;
    this._onDataArrival = null;
    this._onError = null;
    this._onComplete = null;
}

BaseLoader.prototype.isWorking = function () {
    return this._status === LoaderStatus.kConnecting || this._status === LoaderStatus.kBuffering;
}

BaseLoader.prototype.type = function () {
    return this._type;
}

BaseLoader.prototype.status = function () {
    return this._status;
}

BaseLoader.prototype.needStashBuffer = function () {
    return this._needStash;
}

BaseLoader.prototype.onContentLengthKnown = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onContentLengthKnown;
    } else {
        this._onContentLengthKnown = callback;
    }
}

BaseLoader.prototype.onURLRedirect = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onURLRedirect;
    } else {
        this._onURLRedirect = callback;
    }
}

BaseLoader.prototype.onDataArrival = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onDataArrival;
    } else {
        this._onDataArrival = callback;
    }
}

BaseLoader.prototype.onError = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onError;
    } else {
        this._onError = callback;
    }
}

BaseLoader.prototype.onComplete = function (callback) {
    if (typeof callback === 'undefined') {
        return this._onComplete;
    } else {
        this._onComplete = callback;
    }
}

// pure virtual
BaseLoader.prototype.open = function (dataSource, range) {
    throw new NotImplementedException('Unimplemented abstract function!');
}

BaseLoader.prototype.abort = function () {
    throw new NotImplementedException('Unimplemented abstract function!');
}
