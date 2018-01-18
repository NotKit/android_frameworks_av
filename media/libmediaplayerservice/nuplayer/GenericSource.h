/*
* Copyright (C) 2014 MediaTek Inc.
* Modification based on code covered by the mentioned copyright
* and/or permission notice(s).
*/
/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GENERIC_SOURCE_H_

#define GENERIC_SOURCE_H_

#include "NuPlayer.h"
#include "NuPlayerSource.h"

#include "ATSParser.h"

#include <media/mediaplayer.h>

#ifdef MTK_AOSP_ENHANCEMENT
#include <media/stagefright/MediaExtractor.h>
#endif

namespace android {

class DecryptHandle;
class DrmManagerClient;
struct AnotherPacketSource;
struct ARTSPController;
class DataSource;
class IDataSource;
struct IMediaHTTPService;
struct MediaSource;
class MediaBuffer;
struct NuCachedSource2;
class WVMExtractor;

struct NuPlayer::GenericSource : public NuPlayer::Source {
    GenericSource(const sp<AMessage> &notify, bool uidValid, uid_t uid);

    status_t setDataSource(
            const sp<IMediaHTTPService> &httpService,
            const char *url,
            const KeyedVector<String8, String8> *headers);

    status_t setDataSource(int fd, int64_t offset, int64_t length);

    status_t setDataSource(const sp<DataSource>& dataSource);

    virtual void prepareAsync();

    virtual void start();
    virtual void stop();
    virtual void pause();
    virtual void resume();

    virtual void disconnect();

    virtual status_t feedMoreTSData();

    virtual sp<MetaData> getFileFormatMeta() const;

    virtual status_t dequeueAccessUnit(bool audio, sp<ABuffer> *accessUnit);

    virtual status_t getDuration(int64_t *durationUs);
    virtual size_t getTrackCount() const;
    virtual sp<AMessage> getTrackInfo(size_t trackIndex) const;
    virtual ssize_t getSelectedTrack(media_track_type type) const;
    virtual status_t selectTrack(size_t trackIndex, bool select, int64_t timeUs);
    virtual status_t seekTo(int64_t seekTimeUs);

    virtual status_t setBuffers(bool audio, Vector<MediaBuffer *> &buffers);

    virtual bool isStreaming() const;

    virtual void setOffloadAudio(bool offload);

protected:
    virtual ~GenericSource();

    virtual void onMessageReceived(const sp<AMessage> &msg);

    virtual sp<MetaData> getFormatMeta(bool audio);

private:
    enum {
        kWhatPrepareAsync,
        kWhatFetchSubtitleData,
        kWhatFetchTimedTextData,
        kWhatSendSubtitleData,
        kWhatSendGlobalTimedTextData,
        kWhatSendTimedTextData,
        kWhatChangeAVSource,
        kWhatPollBuffering,
        kWhatGetFormat,
        kWhatGetSelectedTrack,
        kWhatSelectTrack,
        kWhatSeek,
        kWhatReadBuffer,
        kWhatStopWidevine,
        kWhatStart,
        kWhatResume,
        kWhatSecureDecodersInstantiated,
    };

    struct Track {
        size_t mIndex;
        sp<IMediaSource> mSource;
        sp<AnotherPacketSource> mPackets;
#ifdef MTK_AOSP_ENHANCEMENT
        bool isEOS;
#endif
    };

    // Helper to monitor buffering status. The polling happens every second.
    // When necessary, it will send out buffering events to the player.
    struct BufferingMonitor : public AHandler {
    public:
        BufferingMonitor(const sp<AMessage> &notify);

        // Set up state.
        void prepare(const sp<NuCachedSource2> &cachedSource,
                const sp<WVMExtractor> &wvmExtractor,
                int64_t durationUs,
                int64_t bitrate,
                bool isStreaming);
        // Stop and reset buffering monitor.
        void stop();
        // Cancel the current monitor task.
        void cancelPollBuffering();
        // Restart the monitor task.
        void restartPollBuffering();
        // Stop buffering task and send out corresponding events.
        void stopBufferingIfNecessary();
        // Make sure data source is getting data.
        void ensureCacheIsFetching();
        // Update media time of just extracted buffer from data source.
        void updateQueuedTime(bool isAudio, int64_t timeUs);

        // Set the offload mode.
        void setOffloadAudio(bool offload);
        // Update media time of last dequeued buffer which is sent to the decoder.
        void updateDequeuedBufferTime(int64_t mediaUs);
#ifdef MTK_AOSP_ENHANCEMENT
        // dequeueAccessUnit() call it to check buffering status more soon
        void onPollBuffering();
        bool isBuffering();
        bool getOffloadAudio(){return mOffloadAudio;}
#endif

    protected:
        virtual ~BufferingMonitor();
        virtual void onMessageReceived(const sp<AMessage> &msg);

    private:
        enum {
            kWhatPollBuffering,
        };

        sp<AMessage> mNotify;

        sp<NuCachedSource2> mCachedSource;
        sp<WVMExtractor> mWVMExtractor;
        int64_t mDurationUs;
        int64_t mBitrate;
        bool mIsStreaming;

        int64_t mAudioTimeUs;
        int64_t mVideoTimeUs;
        int32_t mPollBufferingGeneration;
        bool mPrepareBuffering;
        bool mBuffering;
        int32_t mPrevBufferPercentage;

        mutable Mutex mLock;

        bool mOffloadAudio;
        int64_t mFirstDequeuedBufferRealUs;
        int64_t mFirstDequeuedBufferMediaUs;
        int64_t mlastDequeuedBufferMediaUs;
#ifdef MTK_AOSP_ENHANCEMENT
        int mLastNotifyPercent;
        bool mCacheErrorNotify;
#endif

        void prepare_l(const sp<NuCachedSource2> &cachedSource,
                const sp<WVMExtractor> &wvmExtractor,
                int64_t durationUs,
                int64_t bitrate,
                bool isStreaming);
        void cancelPollBuffering_l();
        void notifyBufferingUpdate_l(int32_t percentage);
        void startBufferingIfNecessary_l();
        void stopBufferingIfNecessary_l();
        void sendCacheStats_l();
        void ensureCacheIsFetching_l();
        int64_t getLastReadPosition_l();
#ifdef MTK_AOSP_ENHANCEMENT
        void onPollBuffering_l(bool shouldNotify = true);
#else
        void onPollBuffering_l();
#endif
        void schedulePollBuffering_l();
    };

    Vector<sp<IMediaSource> > mSources;
    Track mAudioTrack;
    int64_t mAudioTimeUs;
    int64_t mAudioLastDequeueTimeUs;
    Track mVideoTrack;
    int64_t mVideoTimeUs;
    int64_t mVideoLastDequeueTimeUs;
    Track mSubtitleTrack;
    Track mTimedTextTrack;

    int32_t mFetchSubtitleDataGeneration;
    int32_t mFetchTimedTextDataGeneration;
    int64_t mDurationUs;
    bool mAudioIsVorbis;
    bool mIsWidevine;
    bool mIsSecure;
    bool mIsStreaming;
    bool mUIDValid;
    uid_t mUID;
    sp<IMediaHTTPService> mHTTPService;
    AString mUri;
    KeyedVector<String8, String8> mUriHeaders;
    int mFd;
    int64_t mOffset;
    int64_t mLength;

    sp<DataSource> mDataSource;
    sp<NuCachedSource2> mCachedSource;
    sp<DataSource> mHttpSource;
    sp<WVMExtractor> mWVMExtractor;
    sp<MetaData> mFileMeta;
    DrmManagerClient *mDrmManagerClient;
    sp<DecryptHandle> mDecryptHandle;
    bool mStarted;
    bool mStopRead;
    int64_t mBitrate;
    sp<BufferingMonitor> mBufferingMonitor;
    uint32_t mPendingReadBufferTypes;
    sp<ABuffer> mGlobalTimedText;

    mutable Mutex mReadBufferLock;
    mutable Mutex mDisconnectLock;

    sp<ALooper> mLooper;
    sp<ALooper> mBufferingMonitorLooper;

    void resetDataSource();

    status_t initFromDataSource();
    void checkDrmStatus(const sp<DataSource>& dataSource);
    int64_t getLastReadPosition();
    void setDrmPlaybackStatusIfNeeded(int playbackStatus, int64_t position);

    void notifyPreparedAndCleanup(status_t err);
    void onSecureDecodersInstantiated(status_t err);
    void finishPrepareAsync();
    status_t startSources();

    void onGetFormatMeta(sp<AMessage> msg) const;
    sp<MetaData> doGetFormatMeta(bool audio) const;

    void onGetSelectedTrack(sp<AMessage> msg) const;
    ssize_t doGetSelectedTrack(media_track_type type) const;

    void onSelectTrack(sp<AMessage> msg);
    status_t doSelectTrack(size_t trackIndex, bool select, int64_t timeUs);

    void onSeek(sp<AMessage> msg);
    status_t doSeek(int64_t seekTimeUs);

    void onPrepareAsync();

    void fetchTextData(
            uint32_t what, media_track_type type,
            int32_t curGen, sp<AnotherPacketSource> packets, sp<AMessage> msg);

    void sendGlobalTextData(
            uint32_t what,
            int32_t curGen, sp<AMessage> msg);

    void sendTextData(
            uint32_t what, media_track_type type,
            int32_t curGen, sp<AnotherPacketSource> packets, sp<AMessage> msg);

    sp<ABuffer> mediaBufferToABuffer(
            MediaBuffer *mbuf,
            media_track_type trackType,
            int64_t seekTimeUs,
            int64_t *actualTimeUs = NULL);

    void postReadBuffer(media_track_type trackType);
    void onReadBuffer(sp<AMessage> msg);
    void readBuffer(
            media_track_type trackType,
            int64_t seekTimeUs = -1ll, int64_t *actualTimeUs = NULL, bool formatChange = false);

    void queueDiscontinuityIfNeeded(
            bool seeking, bool formatChange, media_track_type trackType, Track *track);

    DISALLOW_EVIL_CONSTRUCTORS(GenericSource);
#ifdef MTK_AOSP_ENHANCEMENT
public:
    bool mIsCurrentComplete;   // OMA DRM v1 implementation
    String8 mDrmProcName;
    void setDRMClientInfo(const Parcel *request);
    virtual status_t initCheck() const;
    virtual status_t getFinalStatus() const;
    virtual bool hasVideo();
    virtual void setParams(const sp<MetaData>& meta);
private:
    void onPollBuffering2();
    void notifySeekDone(status_t err);
    bool getCachedDuration(int64_t *durationUs, bool *eos);
    bool getBitrate(int64_t *bitrate);

    typedef void (*callback_t)(void *observer, int64_t durationUs);
    static void updateAudioDuration(void *observer, int64_t durationUs);
    void notifyDurationUpdate(int64_t duration);
    status_t initFromDataSource_checkLocalSdp(const sp<IMediaExtractor> extractor);
    bool isTS();
    bool isASF();
    void  BufferingDataForTsVideo(media_track_type trackType, bool shouldBuffering);
    status_t checkNetWorkErrorIfNeed();
    void notifySizeForHttp();
    void consumeRightIfNeed();
    void resetCacheHttp();
    void addMetaKeyIfNeed(void *format);
    void changeMaxBuffersInNeed(size_t *maxBuffers);
    void handleReadEOS(bool seeking, Track *track);
    void init();
    void checkDrmStatus2(const sp<DataSource>& dataSource);
    void consumeRight2();
    void addMetaKeyMbIfNeed(
            MediaBuffer* mb,
            media_track_type trackType,
            int64_t seekTimeUs,
            int64_t timeUs,
            sp<AMessage> meta);
    sp<MetaData> addMetaKeySdp() const;
    sp<MetaData> getFormatMetaForHttp(bool audio);
    status_t checkCachedIfNecessary();
    String8 mRtspUri;
    // sp<ASessionDescription> mSessionDesc;
    sp<RefBase> mSessionDesc;
    status_t mInitCheck;
    int64_t mSeekTimeUs;
    sp<MetaData> mSDPFormatMeta;          // for sdp local file getFormatMeta -add by Jiapeng Yin
    int mFDforSniff;
    bool mIsRequiresSecureBuffer;
    bool mAudioCanChangeMaxBuffer;
    int mSeekingCount;
    mutable Mutex mSeekingLock;
    int mIsMtkMusic;

    bool mIs3gppSource;
    int64_t mFirstAudioSampleOffset;  // only for mpeg4
    int64_t mFirstVideoSampleOffset;  // only for mpeg4

    bool mIsPlayReady;
    mutable Mutex mSourceLock;  // add for ts, ALPS03065445
#endif
};

}  // namespace android

#endif  // GENERIC_SOURCE_H_
