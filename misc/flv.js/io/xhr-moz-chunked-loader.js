

// For FireFox browser which supports `xhr.responseType = 'moz-chunked-arraybuffer'`
function MozChunkedLoader(seekHandler, config) {
    BaseLoader.call(this, 'xhr-moz-chunked-loader')
    // super('xhr-moz-chunked-loader');
    this.TAG = 'MozChunkedLoader';

    this._seekHandler = seekHandler;
    this._config = config;
    this._needStash = true;

    this._xhr = null;
    this._requestAbort = false;
    this._contentLength = null;
    this._receivedLength = 0;
}
inherites(MozChunkedLoader, BaseLoader);


MozChunkedLoader.isSupported = function () {
    try {
        let xhr = new XMLHttpRequest();
        // Firefox 37- requires .open() to be called before setting responseType
        xhr.open('GET', 'https://example.com', true);
        xhr.responseType = 'moz-chunked-arraybuffer';
        return (xhr.responseType === 'moz-chunked-arraybuffer');
    } catch (e) {
        Log.w('MozChunkedLoader', e.message);
        return false;
    }
}


MozChunkedLoader.prototype.destroy = function () {
    if (this.isWorking()) {
        this.abort();
    }
    if (this._xhr) {
        this._xhr.onreadystatechange = null;
        this._xhr.onprogress = null;
        this._xhr.onloadend = null;
        this._xhr.onerror = null;
        this._xhr = null;
    }
    // super.destroy();
    BaseLoader.destroy.call(this);
}

MozChunkedLoader.prototype.open = function (dataSource, range) {
    this._dataSource = dataSource;
    this._range = range;

    let sourceURL = dataSource.url;
    if (this._config.reuseRedirectedURL && dataSource.redirectedURL != undefined) {
        sourceURL = dataSource.redirectedURL;
    }

    let seekConfig = this._seekHandler.getConfig(sourceURL, range);
    this._requestURL = seekConfig.url;

    let xhr = this._xhr = new XMLHttpRequest();
    xhr.open('GET', seekConfig.url, true);
    xhr.responseType = 'moz-chunked-arraybuffer';
    xhr.onreadystatechange = this._onReadyStateChange.bind(this);
    xhr.onprogress = this._onProgress.bind(this);
    xhr.onloadend = this._onLoadEnd.bind(this);
    xhr.onerror = this._onXhrError.bind(this);

    // cors is auto detected and enabled by xhr

    // withCredentials is disabled by default
    if (dataSource.withCredentials) {
        xhr.withCredentials = true;
    }

    if (typeof seekConfig.headers === 'object') {
        let headers = seekConfig.headers;

        for (let key in headers) {
            if (headers.hasOwnProperty(key)) {
                xhr.setRequestHeader(key, headers[key]);
            }
        }
    }

    // add additional headers
    if (typeof this._config.headers === 'object') {
        let headers = this._config.headers;

        for (let key in headers) {
            if (headers.hasOwnProperty(key)) {
                xhr.setRequestHeader(key, headers[key]);
            }
        }
    }

    this._status = LoaderStatus.kConnecting;
    xhr.send();
}

MozChunkedLoader.prototype.abort = function () {
    this._requestAbort = true;
    if (this._xhr) {
        this._xhr.abort();
    }
    this._status = LoaderStatus.kComplete;
}

MozChunkedLoader.prototype._onReadyStateChange = function (e) {
    let xhr = e.target;

    if (xhr.readyState === 2) {  // HEADERS_RECEIVED
        if (xhr.responseURL != undefined && xhr.responseURL !== this._requestURL) {
            if (this._onURLRedirect) {
                let redirectedURL = this._seekHandler.removeURLParameters(xhr.responseURL);
                this._onURLRedirect(redirectedURL);
            }
        }

        if (xhr.status !== 0 && (xhr.status < 200 || xhr.status > 299)) {
            this._status = LoaderStatus.kError;
            if (this._onError) {
                this._onError(LoaderErrors.HTTP_STATUS_CODE_INVALID, {code: xhr.status, msg: xhr.statusText});
            } else {
                throw new RuntimeException('MozChunkedLoader: Http code invalid, ' + xhr.status + ' ' + xhr.statusText);
            }
        } else {
            this._status = LoaderStatus.kBuffering;
        }
    }
}

MozChunkedLoader.prototype._onProgress = function (e) {
    if (this._status === LoaderStatus.kError) {
        // Ignore error response
        return;
    }

    if (this._contentLength === null) {
        if (e.total !== null && e.total !== 0) {
            this._contentLength = e.total;
            if (this._onContentLengthKnown) {
                this._onContentLengthKnown(this._contentLength);
            }
        }
    }

    let chunk = e.target.response;
    let byteStart = this._range.from + this._receivedLength;
    this._receivedLength += chunk.byteLength;

    if (this._onDataArrival) {
        this._onDataArrival(chunk, byteStart, this._receivedLength);
    }
}

MozChunkedLoader.prototype._onLoadEnd = function (e) {
    if (this._requestAbort === true) {
        this._requestAbort = false;
        return;
    } else if (this._status === LoaderStatus.kError) {
        return;
    }

    this._status = LoaderStatus.kComplete;
    if (this._onComplete) {
        this._onComplete(this._range.from, this._range.from + this._receivedLength - 1);
    }
}

MozChunkedLoader.prototype._onXhrError = function (e) {
    this._status = LoaderStatus.kError;
    let type = 0;
    let info = null;

    if (this._contentLength && e.loaded < this._contentLength) {
        type = LoaderErrors.EARLY_EOF;
        info = {code: -1, msg: 'Moz-Chunked stream meet Early-Eof'};
    } else {
        type = LoaderErrors.EXCEPTION;
        info = {code: -1, msg: e.constructor.name + ' ' + e.type};
    }

    if (this._onError) {
        this._onError(type, info);
    } else {
        throw new RuntimeException(info.msg);
    }
}
