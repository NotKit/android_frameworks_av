/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright 2012, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PLAYBACK_SESSION_H_

#define PLAYBACK_SESSION_H_

#include "MediaSender.h"
#include "VideoFormats.h"
#include "WifiDisplaySource.h"

#include <utils/String16.h>

namespace android {

struct ABuffer;
struct IHDCP;
class IGraphicBufferProducer;
struct MediaPuller;
struct MediaSource;
struct MediaSender;
struct NuMediaExtractor;

#ifdef MTK_AOSP_ENHANCEMENT
struct Serializer;
struct MediaCodec;
#endif
// Encapsulates the state of an RTP/RTCP session in the context of wifi
// display.
struct WifiDisplaySource::PlaybackSession : public AHandler {
    PlaybackSession(
            const String16 &opPackageName,
            const sp<ANetworkSession> &netSession,
            const sp<AMessage> &notify,
            const struct in_addr &interfaceAddr,
            const sp<IHDCP> &hdcp,
            uid_t  ClientUid,
            pid_t  ClientPid,
            const char *path = NULL);

    status_t init(
            const char *clientIP,
            int32_t clientRtp,
            RTPSender::TransportMode rtpMode,
            int32_t clientRtcp,
            RTPSender::TransportMode rtcpMode,
            bool enableAudio,
            bool usePCMAudio,
            bool enableVideo,
            VideoFormats::ResolutionType videoResolutionType,
            size_t videoResolutionIndex,
            VideoFormats::ProfileType videoProfileType,
            VideoFormats::LevelType videoLevelType);

    void destroyAsync();

    int32_t getRTPPort() const;

    int64_t getLastLifesignUs() const;
    void updateLiveness();

    status_t play();
    status_t finishPlay();
    status_t pause();

    sp<IGraphicBufferProducer> getSurfaceTexture();

    void requestIDRFrame();

    enum {
        kWhatSessionDead,
        kWhatBinaryData,
        kWhatSessionEstablished,
        kWhatSessionDestroyed,
    };

protected:
    virtual void onMessageReceived(const sp<AMessage> &msg);
    virtual ~PlaybackSession();

private:
    struct Track;

    enum {
        kWhatMediaPullerNotify,
        kWhatConverterNotify,
        kWhatTrackNotify,
        kWhatUpdateSurface,
        kWhatPause,
        kWhatResume,
        kWhatMediaSenderNotify,
        kWhatPullExtractorSample,
    };

    String16 mOpPackageName;

    sp<ANetworkSession> mNetSession;
    sp<AMessage> mNotify;
    in_addr mInterfaceAddr;
    sp<IHDCP> mHDCP;
    AString mMediaPath;

    sp<MediaSender> mMediaSender;
    int32_t mLocalRTPPort;

    bool mWeAreDead;
    bool mPaused;

    int64_t mLastLifesignUs;

    sp<IGraphicBufferProducer> mProducer;

    KeyedVector<size_t, sp<Track> > mTracks;
    ssize_t mVideoTrackIndex;

    int64_t mPrevTimeUs;

    sp<NuMediaExtractor> mExtractor;
    KeyedVector<size_t, size_t> mExtractorTrackToInternalTrack;
    bool mPullExtractorPending;
    int32_t mPullExtractorGeneration;
    int64_t mFirstSampleTimeRealUs;
    int64_t mFirstSampleTimeUs;

    status_t setupMediaPacketizer(bool enableAudio, bool enableVideo);

    status_t setupPacketizer(
            bool enableAudio,
            bool usePCMAudio,
            bool enableVideo,
            VideoFormats::ResolutionType videoResolutionType,
            size_t videoResolutionIndex,
            VideoFormats::ProfileType videoProfileType,
            VideoFormats::LevelType videoLevelType);

    status_t addSource(
            bool isVideo,
            const sp<MediaSource> &source,
            bool isRepeaterSource,
            bool usePCMAudio,
            unsigned profileIdc,
            unsigned levelIdc,
            unsigned contraintSet,
            size_t *numInputBuffers);

    status_t addVideoSource(
            VideoFormats::ResolutionType videoResolutionType,
            size_t videoResolutionIndex,
            VideoFormats::ProfileType videoProfileType,
            VideoFormats::LevelType videoLevelType);

    status_t addAudioSource(bool usePCMAudio);

    status_t onMediaSenderInitialized();

    void notifySessionDead();

    void schedulePullExtractor();
    void onPullExtractor();

    void onSinkFeedback(const sp<AMessage> &msg);

    DISALLOW_EVIL_CONSTRUCTORS(PlaybackSession);
#ifdef MTK_AOSP_ENHANCEMENT
public:
    status_t setBitrateControl(int32_t bitrate);
    void setSliceMode(int32_t useSliceMode);
    status_t forceBlackScreen(bool blackNow);
    void setMiracastMode(bool MiracastEnable = false);

private:
    int32_t mUseSliceMode;

    VideoFormats::ResolutionType mVideoResolutionType;
    size_t mVideoResolutionIndex;
    int32_t mTestFakeVideoPath;
    bool mMiracastEnable;
    bool drop_dummy;
    uid_t mClientUid;
    pid_t mClientPid;
    void init_pre(bool* enableAudio,bool* enableVideo);
    void delete_pro();
    void addSource_video_ext(sp<AMessage> format,int32_t profileIdc,int32_t levelIdc);

#endif
};

}  // namespace android

#endif  // PLAYBACK_SESSION_H_

