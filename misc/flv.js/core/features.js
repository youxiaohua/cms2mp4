var Features = {};

Features.supportMSEH264Playback = function () {
    return window.MediaSource &&
        window.MediaSource.isTypeSupported('video/mp4; codecs="avc1.42E01E,mp4a.40.2"');
}
Features.supportNetworkStreamIO = function () {
    let ioctl = new IOController({}, createDefaultConfig());
    let loaderType = ioctl.loaderType;
    ioctl.destroy();
    return loaderType == 'fetch-stream-loader' || loaderType == 'xhr-moz-chunked-loader';
}
Features.getNetworkLoaderTypeName = function () {
    let ioctl = new IOController({}, createDefaultConfig());
    let loaderType = ioctl.loaderType;
    ioctl.destroy();
    return loaderType;
}
Features.supportNativeMediaPlayback = function (mimeType) {
    if (Features.videoElement == undefined) {
        Features.videoElement = window.document.createElement('video');
    }
    let canPlay = Features.videoElement.canPlayType(mimeType);
    return canPlay === 'probably' || canPlay == 'maybe';
}
Features.getFeatureList = function () {
    let features = {
        mseFlvPlayback        : false,
        mseLiveFlvPlayback    : false,
        networkStreamIO       : false,
        networkLoaderName     : '',
        nativeMP4H264Playback : false,
        nativeWebmVP8Playback : false,
        nativeWebmVP9Playback : false
    };

    features.mseFlvPlayback = Features.supportMSEH264Playback();
    features.networkStreamIO = Features.supportNetworkStreamIO();
    features.networkLoaderName = Features.getNetworkLoaderTypeName();
    features.mseLiveFlvPlayback = features.mseFlvPlayback && features.networkStreamIO;
    features.nativeMP4H264Playback = Features.supportNativeMediaPlayback('video/mp4; codecs="avc1.42001E, mp4a.40.2"');
    features.nativeWebmVP8Playback = Features.supportNativeMediaPlayback('video/webm; codecs="vp8.0, vorbis"');
    features.nativeWebmVP9Playback = Features.supportNativeMediaPlayback('video/webm; codecs="vp9"');

    return features;
}
