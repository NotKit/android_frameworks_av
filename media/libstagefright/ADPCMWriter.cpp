/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein
 * is confidential and proprietary to MediaTek Inc. and/or its licensors.
 * Without the prior written permission of MediaTek inc. and/or its licensors,
 * any reproduction, modification, use or disclosure of MediaTek Software,
 * and information contained herein, in whole or in part, shall be strictly prohibited.
 *
 * MediaTek Inc. (C) 2010. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN MEDIATEK SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE MEDIATEK SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 * MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek Software")
 * have been modified by MediaTek Inc. All revisions are subject to any receiver's
 * applicable license agreements with MediaTek Inc.
 */

/*
 * Copyright (C) 2010 The Android Open Source Project
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
 #define MTK_LOG_ENABLE 1
 #define LOG_TAG "ADPCMWriter"
 #include <media/stagefright/ADPCMWriter.h>
 #include <media/stagefright/MediaBuffer.h>
 #include <media/stagefright/foundation/ADebug.h>
 #include <media/stagefright/MediaDefs.h>
 #include <media/stagefright/MediaErrors.h>
 #include <media/stagefright/MediaSource.h>
 #include <media/stagefright/MetaData.h>
 #include <media/mediarecorder.h>
 #include <sys/prctl.h>
 #include <pthread.h>
 #include <sys/resource.h>
 #include <utils/Log.h>



namespace android {
 ADPCMWriter::ADPCMWriter(const char* filePath)
    : mFile(fopen(filePath, "wb"))
    , mInitChecked(NULL == mFile ? ADPCM_FALSE : ADPCM_TRUE)
    , mStarted(ADPCM_FALSE)
    , mStoped(ADPCM_TRUE)
    , mPaused(ADPCM_FALSE)
    , mResumed(ADPCM_FALSE)
    , mReachedEOS(ADPCM_FALSE)
    , mEstimatedDurationUs(0LL)
    , mEstimatedSizeBytes(0LL)
    , mRecordFileSize(0LL)
    , mAudioFormat(MS_ADPCM)
    , mSamplesPerBlock(0)
 {
     ALOGD("ADPCMWriter construct,filePath is %s", filePath);
    memset(&mWaveHeader, 0, sizeof(mWaveHeader));
 }

  ADPCMWriter::ADPCMWriter(int fd)
    : mFile(fdopen(fd, "wb"))
    , mInitChecked(NULL == mFile ? ADPCM_FALSE : ADPCM_TRUE)
    , mStarted(ADPCM_FALSE)
    , mStoped(ADPCM_TRUE)
    , mPaused(ADPCM_FALSE)
    , mResumed(ADPCM_FALSE)
    , mReachedEOS(ADPCM_FALSE)
    , mEstimatedDurationUs(0LL)
    , mEstimatedSizeBytes(0LL)
    , mRecordFileSize(0LL)
    , mAudioFormat(MS_ADPCM)
    , mSamplesPerBlock(0)
 {
     ALOGD("ADPCMWriter construct,fd is %d", fd);
    memset(&mWaveHeader, 0, sizeof(mWaveHeader));
 }

 ADPCMWriter::~ADPCMWriter()
 {
     ALOGD("ADPCMWriter Deconstruct");
    if (mStarted)
    {
       stop();
    }

    if (mFile != NULL)
    {
        fclose(mFile);
        mFile = NULL;
    }
 }

 ADPCM_BOOL ADPCMWriter::initCheck()
 {
    return mInitChecked;
 }

 status_t ADPCMWriter::addSource(const sp<IMediaSource> &source)
 {
     ALOGD("addSource +++");

    if(ADPCM_FALSE == mInitChecked)
    {
        ALOGD("Init Check Failed !!!");
        return UNKNOWN_ERROR;
    }
    if(source == NULL)
    {
        ALOGD("Not Vailed Source !!!");
        return UNKNOWN_ERROR;
    }
    mSource = source;

    sp<MetaData> mMetaData;
    mMetaData = mSource->getFormat();

    //get params from meta data
    char* mimeType = NULL;
    uint32_t type;
    CHECK(mMetaData->findCString(kKeyMIMEType, (const char **)&mimeType));
    CHECK(mMetaData->findInt32(kKeyChannelCount, (int32_t *)&mWaveHeader.num_channels));
    CHECK(mMetaData->findInt32(kKeySampleRate, (int32_t *)&mWaveHeader.sample_rate));
    CHECK(mMetaData->findInt32(kKeyBlockAlign, (int32_t *)&mWaveHeader.block_align));
    CHECK(mMetaData->findInt32(kKeyBitsPerSample, (int32_t *)&mWaveHeader.bits_per_sample));
    CHECK(mMetaData->findData(kKeyExtraDataPointer, &type, (const void **)&mWaveHeader.pextraData, (size_t *)&mWaveHeader.extra_data_size));
    mSamplesPerBlock = *((uint8_t *)mWaveHeader.pextraData + 1) << 8 | *((uint8_t *)mWaveHeader.pextraData);

    ALOGD("ADPCM Writer mime type is %s", mimeType);
    ALOGD("ADPCM Writer mWaveHeader.num_channels is %d", mWaveHeader.num_channels);
    ALOGD("ADPCM Writer mWaveHeader.sample_rate is %d", mWaveHeader.sample_rate);
    ALOGD("ADPCM Writer mWaveHeader.block_align is %d", mWaveHeader.block_align);
    ALOGD("ADPCM Writer mWaveHeader.bits_per_sample is %d", mWaveHeader.bits_per_sample);
    ALOGD("ADPCM Writer mWaveHeader.extra_data_size is %d", mWaveHeader.extra_data_size);
    ALOGD("ADPCM Writer mSamplesPerBlock is %d", mSamplesPerBlock);

    ALOGD("sizeof short int = %zu, int = %zu, long int = %zu, long long int = %zu",
            sizeof(short int), sizeof(int), sizeof(long int), sizeof(long long int));

    //write them to file
    mWaveHeader.riff_id = CHUNK_ID_RIFF;
    mWaveHeader.riff_sz = 0;
    mWaveHeader.riff_fmt= CHUNK_ID_WAVE;

    mWaveHeader.fmt_id =  CHUNK_ID_FMT;
    mWaveHeader.fmt_sz =  0;
    mWaveHeader.audio_format = !strcasecmp(mimeType, MEDIA_MIMETYPE_AUDIO_MS_ADPCM) ? 2 : 17;
    mWaveHeader.byte_rate = 0;

    writeDataToFile((char *)&mWaveHeader, 12 /*RIFF Header*/+26 /*FMT CHUNK*/);
    writeDataToFile((char *)mWaveHeader.pextraData, mWaveHeader.extra_data_size);


    mWaveHeader.fact_id = CHUNK_ID_FACT;
    mWaveHeader.fact_sz = 4;
    mWaveHeader.samples_per_channel = 0;
    mWaveHeader.data_id = CHUNK_ID_DATA;
    mWaveHeader.data_sz = 0;
    writeDataToFile((char *)&mWaveHeader.fact_id, 12 /*FACT Header*/+8 /*DATA CHUNK Header*/);


    return OK;
 }

 status_t ADPCMWriter::start(MetaData * /*params*/)
 {
     ALOGD("ADPCM start +++");
    if(ADPCM_TRUE != mInitChecked)
    {
        ALOGE("Not Init Before ADPCM Writer Start !!!");
        return UNKNOWN_ERROR;
    }

    if(mSource == NULL)
    {
        ALOGE("Media Source Is NULL !!!");
        return UNKNOWN_ERROR;
    }

    if(mStarted && mPaused)
    {
        mPaused = ADPCM_FALSE;
//      mResumed= ADPCM_TRUE;
        return OK;
    }
    else if(mStarted)
    {
        ALOGD("ADPCM Writer is already start !!!");
        return OK;
    }

    ALOGD("ADPCM OmxCodec start +++");
    status_t ret = mSource->start();
    if(OK != ret)
    {
        ALOGE("Media Source Start Failed !!!");
        return UNKNOWN_ERROR;
    }

    ALOGD("ADPCM start render thread+++");
    mRecordFileSize = 0;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    mReachedEOS = ADPCM_FALSE;
    mStoped = ADPCM_FALSE;

    pthread_create(&mThread, &attr, threadWrapper, this);
    pthread_attr_destroy(&attr);

    mStarted = ADPCM_TRUE;
    return OK;
 }

 status_t ADPCMWriter::pause()
 {
     ALOGD("ADPCM pause !!!");
    if(ADPCM_FALSE == mStarted)
    {
        ALOGD("ADPCM Writer not start !!!");
        return OK;
    }
    mPaused = ADPCM_TRUE;

    return OK;
 }

 status_t ADPCMWriter::stop()
 {
     ALOGD("ADPCM stop !!!");
    if(ADPCM_FALSE == mStarted)
    {
        ALOGD("ADPCM Writer not start !!!");
        return OK;
    }
    mStoped = ADPCM_TRUE;
    mPaused= ADPCM_FALSE;

    void* ret1;
    ALOGD("pthread_join +++ !!!");
    pthread_join(mThread, &ret1);
    status_t err = static_cast<status_t>(reinterpret_cast<uintptr_t>(ret1));
    if(OK != err)
    {
        ALOGE("Pthread_join ret error !!!");
        return err;
    }
    ALOGD("pthread_join --- !!!");

    status_t ret2;
    ALOGD("mSource->stop +++ !!!");
    ret2 = mSource->stop();
    ALOGD("mSource->stop --- !!!");

    mStarted = ADPCM_FALSE;
    return ret2;
 }

 //static
 void* ADPCMWriter::threadWrapper(void* me)
 {
    return (void *)(uintptr_t)static_cast<ADPCMWriter *>(me)->threadFunc();
 }

 status_t ADPCMWriter::threadFunc()
 {
     ALOGV("Entry threadFunc +++");
    //ADPCM_U64 timeGapUs = 0;
    //ADPCM_U64 maxTimeStampUs = 0;
    //ADPCM_U64 timeStampUs = 0;
    ADPCM_U64 blockDurationUs = 0;
    //ADPCM_U32 dataSize = 0;
    ADPCM_U64 fileDataSize = 0;
    ADPCM_BOOL stoppedPrematurely;
    status_t ret;

    prctl(PR_SET_NAME, (unsigned long)"ADPCMWriter", 0, 0, 0);
    MediaBuffer *mBuffer;

    while(mStoped == ADPCM_FALSE)
    {
        ALOGV("mSource->read +++ !!!");
        ret = mSource->read(&mBuffer);
        ALOGV("mSource->read --- !!!");
        if(OK != ret)
        {
            ALOGE("mSource->read Err !!!");
            if(NULL != mBuffer)
            {
                mBuffer->release();
                mBuffer = NULL;
            }
            break;
        }

        if(mPaused == ADPCM_TRUE)
        {
            ALOGD("ADPCM Writer Pause !!!");
            mBuffer->release();
            mBuffer = NULL;
            continue;
        }

        ALOGV("mEstimatedSizeBytes is %llu", mEstimatedSizeBytes);
        mEstimatedSizeBytes += mBuffer->range_length();
        if(reachedFileSizeLimit())
        {
            mBuffer->release();
            mBuffer = NULL;
            notify(MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED, 0);
            break;
        }

        ALOGV("mEstimatedDurationUs is %.2f s", mEstimatedDurationUs / 1E6);
        blockDurationUs = 1000000LL * mSamplesPerBlock / mWaveHeader.sample_rate;
        mEstimatedDurationUs = (mEstimatedSizeBytes / mWaveHeader.block_align) * blockDurationUs;
        if(reachedFileDurationLimit())
        {
            mBuffer->release();
            mBuffer = NULL;
            notify(MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_INFO_MAX_DURATION_REACHED, 0);
            break;
        }

        ssize_t n = fwrite(
                (const uint8_t *)mBuffer->data() + mBuffer->range_offset(),
                1,
                mBuffer->range_length(),
                mFile);

        if (n < (ssize_t)mBuffer->range_length()) {
            mBuffer->release();
            mBuffer = NULL;

            break;
        }
        else
        {
            fileDataSize += n;
        }

        // XXX: How to tell it is stopped prematurely?
        if (stoppedPrematurely) {
            stoppedPrematurely = ADPCM_FALSE;
        }

        mBuffer->release();
        mBuffer = NULL;

        ALOGV("mStop is %d", mStoped == ADPCM_TRUE ? 1 : 0);
    }

    ALOGD("Out of while loop, thread is ready to exit !!!");
    if (stoppedPrematurely) {
        notify(MEDIA_RECORDER_EVENT_INFO, MEDIA_RECORDER_TRACK_INFO_COMPLETION_STATUS, UNKNOWN_ERROR);
    }

    //write related data to file header
    //write wave chunk size
    moveFilePointer(4, SEEK_SET);
    ALOGD("value1 is %llu", 4 + 26 + mWaveHeader.extra_data_size + 20 + fileDataSize);
    writeDWordToFile(4 + 26 + mWaveHeader.extra_data_size + 20 + fileDataSize);

    //write fmt chunk size
    moveFilePointer(8, SEEK_CUR);
    ALOGD("value2 is %d", 16 + 2 + mWaveHeader.extra_data_size);
    writeDWordToFile(16 + 2 + mWaveHeader.extra_data_size);


    //write fmt bitRate
    moveFilePointer(8, SEEK_CUR);
    ALOGD("value3 is %d", (int)(fileDataSize * 100000LL / mEstimatedDurationUs) << 3);
    writeDWordToFile((int)(fileDataSize * 100000LL / mEstimatedDurationUs) << 3);

/*
	//write fact samples per channel
	moveFilePointer(2 + 6 + mWaveHeader.extra_data_size + 8, SEEK_CUR);
	SLOGD("value4 is %d", fileDataSize / mWaveHeader.block_align * mSamplesPerBlock);
	writeDWordToFile(fileDataSize / mWaveHeader.block_align * mSamplesPerBlock);
*/
    //write data size
    moveFilePointer(12+26+mWaveHeader.extra_data_size+12+4, SEEK_SET);
    ALOGD("value5 is %llu", fileDataSize);
    writeDWordToFile(fileDataSize);

    fflush(mFile);
    fclose(mFile);
    mFile = NULL;
    mReachedEOS = ADPCM_TRUE;
    if (ret == ERROR_END_OF_STREAM)
    {
        return OK;
    }
    return ret;
 }

 bool ADPCMWriter::reachedEOS()
 {
    return mReachedEOS == ADPCM_TRUE ? true : false;
 }

 ADPCM_BOOL ADPCMWriter::reachedFileSizeLimit()
 {
    if(mMaxFileSizeLimitBytes ==0)  //if set 0, file size is not limitted
        return ADPCM_FALSE;

     return mEstimatedSizeBytes >= (ADPCM_U64)mMaxFileSizeLimitBytes ? ADPCM_TRUE : ADPCM_FALSE;
 }

  ADPCM_BOOL ADPCMWriter::reachedFileDurationLimit()
 {
    if(mMaxFileDurationLimitUs ==0)  //if set 0, file duration is not limitted
        return ADPCM_FALSE;

     return mEstimatedDurationUs >= (ADPCM_U64)mMaxFileDurationLimitUs ? ADPCM_TRUE : ADPCM_FALSE;
 }

 void ADPCMWriter::writeWordToFile(short data)
 {
    fwrite(&data, sizeof(data), 1, mFile);
 }

 void ADPCMWriter::writeDWordToFile(int data)
 {
    fwrite(&data, sizeof(data), 1, mFile);
 }

 void ADPCMWriter::writeDataToFile(char* data, ADPCM_U32 length)
 {
    if(fwrite(data, length, 1, mFile) != 1)
    {
        ALOGE("ADPCMWriter::writeDataToFile error !!!");
        return ;
    }
 }

 void ADPCMWriter::moveFilePointer(long offset, int fromwhere)
 {
    long off = 0;
    if(fseek(mFile, offset, fromwhere) != 0)
    {
        ALOGE("ADPCMWriter::moveFilePointer fseek error !!!");
        return ;
    }
    else
    {
        if((off = ftell(mFile)) == -1)
        {
            ALOGE("ADPCMWriter::moveFilePointer ftell error !!!");
            return ;
        }
        else
            ALOGD("offset from file header is %ld", off);
    }
 }

 }//end of namespace android