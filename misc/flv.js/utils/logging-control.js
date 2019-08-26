
var LoggingControl = {};

LoggingControl.forceGlobalTag = function (enable) {
    if (typeof enable === 'undefined') {
        return Log.FORCE_GLOBAL_TAG;
    } else {
        Log.FORCE_GLOBAL_TAG = enable;
        LoggingControl._notifyChange();
    }
}

LoggingControl.globalTag = function (tag) {
    if (typeof tag === 'undefined') {
        return Log.GLOBAL_TAG;
    } else {
        Log.GLOBAL_TAG = tag;
        LoggingControl._notifyChange();
    }
}

LoggingControl.enableAll = function (enable) {
    if (typeof enable === 'undefined') {
        return Log.ENABLE_VERBOSE
            && Log.ENABLE_DEBUG
            && Log.ENABLE_INFO
            && Log.ENABLE_WARN
            && Log.ENABLE_ERROR;
    } else {
        Log.ENABLE_VERBOSE = enable;
        Log.ENABLE_DEBUG   = enable;
        Log.ENABLE_INFO    = enable;
        Log.ENABLE_WARN    = enable;
        Log.ENABLE_ERROR   = enable;
        LoggingControl._notifyChange();
    }
}

LoggingControl.enableDebug = function (enable) {
    if (typeof enable === 'undefined') {
        return Log.ENABLE_DEBUG;
    } else {
        Log.ENABLE_DEBUG = enable;
        LoggingControl._notifyChange();
    }
}

LoggingControl.enableVerbose = function (enable) {
    if (typeof enable === 'undefined') {
        return Log.ENABLE_VERBOSE;
    } else {
        Log.ENABLE_VERBOSE = enable;
        LoggingControl._notifyChange();
    }
}

LoggingControl.enableInfo = function (enable) {
    if (typeof enable === 'undefined') {
        return Log.ENABLE_INFO;
    } else {
        Log.ENABLE_INFO = enable;
        LoggingControl._notifyChange();
    }
}

LoggingControl.enableWarn = function (enable) {
    if (typeof enable === 'undefined') {
        return Log.ENABLE_WARN;
    } else {
        Log.ENABLE_WARN = enable;
        LoggingControl._notifyChange();
    }
}

LoggingControl.enableError = function (enable) {
    if (typeof enable === 'undefined') {
        return Log.ENABLE_ERROR;
    } else {
        Log.ENABLE_ERROR = enable;
        LoggingControl._notifyChange();
    }
}

LoggingControl.getConfig = function () {
    return {
        globalTag: Log.GLOBAL_TAG,
        forceGlobalTag: Log.FORCE_GLOBAL_TAG,
        enableVerbose: Log.ENABLE_VERBOSE,
        enableDebug: Log.ENABLE_DEBUG,
        enableInfo: Log.ENABLE_INFO,
        enableWarn: Log.ENABLE_WARN,
        enableError: Log.ENABLE_ERROR,
        enableCallback: Log.ENABLE_CALLBACK
    };
}

LoggingControl.applyConfig = function (config) {
    Log.GLOBAL_TAG = config.globalTag;
    Log.FORCE_GLOBAL_TAG = config.forceGlobalTag;
    Log.ENABLE_VERBOSE = config.enableVerbose;
    Log.ENABLE_DEBUG = config.enableDebug;
    Log.ENABLE_INFO = config.enableInfo;
    Log.ENABLE_WARN = config.enableWarn;
    Log.ENABLE_ERROR = config.enableError;
    Log.ENABLE_CALLBACK = config.enableCallback;
}

LoggingControl._notifyChange = function () {
    let emitter = LoggingControl.emitter;

    if (emitter.listenerCount('change') > 0) {
        let config = LoggingControl.getConfig();
        emitter.emit('change', config);
    }
}

LoggingControl.registerListener = function (listener) {
    LoggingControl.emitter.addListener('change', listener);
}

LoggingControl.removeListener = function (listener) {
    LoggingControl.emitter.removeListener('change', listener);
}

LoggingControl.addLogListener = function (listener) {
    Log.emitter.addListener('log', listener);
    if (Log.emitter.listenerCount('log') > 0) {
        Log.ENABLE_CALLBACK = true;
        LoggingControl._notifyChange();
    }
}

LoggingControl.removeLogListener = function (listener) {
    Log.emitter.removeListener('log', listener);
    if (Log.emitter.listenerCount('log') === 0) {
        Log.ENABLE_CALLBACK = false;
        LoggingControl._notifyChange();
    }
}

LoggingControl.emitter = new EventEmitter();
