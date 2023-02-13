//
//  
//  osxaudio
//
//  Created by zhougang on 2023/2/2.
//

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#import <AVFoundation/AVFoundation.h>

#include <time.h>
#include <sys/time.h>
FILE* fd;
AudioUnit audioUnit;

static OSStatus recordingCallback(void* inRefCon,
                                  AudioUnitRenderActionFlags *ioActionFlags,
                                  const AudioTimeStamp *inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList *ioData) {
    // 这里就可以对数据进行操作了
    uint8_t* buffer = (uint8_t*)malloc(inNumberFrames * 4);
    AudioBufferList audioBufferList;
    audioBufferList.mNumberBuffers = 1;
    audioBufferList.mBuffers[0].mDataByteSize = inNumberFrames * 4;
    audioBufferList.mBuffers[0].mNumberChannels = 2;
    audioBufferList.mBuffers[0].mData = buffer;
    AudioUnitRender(audioUnit, ioActionFlags, inTimeStamp, inBusNumber,  inNumberFrames, &audioBufferList);
    fwrite(buffer, 1, inNumberFrames * 4, fd);
    struct timeval tv;
    struct timezone tz;
    struct tim *ptrTime;
    gettimeofday(&tv, &tz);
    ptrTime = localtime((const time_t*)(&tv.tv_sec));
    NSLog(@"time %i", tv.tv_usec / 1000);
    return 0;
}

static OSStatus playbackCallback(void *inRefCon,
                                 AudioUnitRenderActionFlags *ioActionFlags,
                                 const AudioTimeStamp *inTimeStamp,
                                 UInt32 inBusNumber,
                                 UInt32 inNumberFrames,
                                 AudioBufferList *ioData) {
    // 这里就可以对数据进行操作了
    uint8_t* buffer = (uint8_t*)malloc(inNumberFrames * 4);
    fread(buffer, 1, inNumberFrames * 4, fd);
    AudioBuffer inBuffer = ioData->mBuffers[0];
    memcpy(inBuffer.mData, buffer, inNumberFrames * 4);
    inBuffer.mDataByteSize = inNumberFrames * 4;
    struct timeval tv;
    struct timezone tz;
    struct tim *ptrTime;
    gettimeofday(&tv, &tz);
    ptrTime = localtime((const time_t*)(&tv.tv_sec));
    NSLog(@"time %i", tv.tv_usec / 1000);
    return 0;
}

static void start_capture_audio() {
    OSStatus status;
    
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    
    
    status = AudioComponentInstanceNew(inputComponent, &audioUnit);

    UInt32 size = 0;
    
    AudioStreamBasicDescription audioFormat = {0};
    audioFormat.mSampleRate = 48000; // 这里是对应的采样率
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mChannelsPerFrame = 2;
    audioFormat.mBitsPerChannel = 16;
    audioFormat.mBytesPerPacket = 4;
    audioFormat.mBytesPerFrame = 4;

    // 申请格式
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  1,
                                  &audioFormat,
                                  sizeof(audioFormat));

    int enableIO = 1;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input,
                                  1,
                                  &enableIO,
                                  sizeof(enableIO));
    enableIO = 0;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Output,
                                  0,
                                  &enableIO,
                                  sizeof(enableIO));
    
    // 设置回调函数
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = recordingCallback; // 回调函数
    callbackStruct.inputProcRefCon = &audioUnit;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global,
                                  1,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    
    UInt32 preferredBufferSize = ((10 * audioFormat.mSampleRate) / 1000); // in bytes
    size = sizeof (preferredBufferSize);

    // Mac OS 设置
    status = AudioUnitSetProperty (audioUnit,
                                   kAudioDevicePropertyBufferFrameSize,
                                   kAudioUnitScope_Global,
                                   0,
                                   &preferredBufferSize,
                                   size);
    
    status = AudioUnitGetProperty (audioUnit,
                                   kAudioDevicePropertyBufferFrameSize,
                                   kAudioUnitScope_Global,
                                   0,
                                   &preferredBufferSize,
                                   &size);
    
    // 检查
    size = sizeof(audioFormat);
    status = AudioUnitGetProperty( audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output,
                                  1,
                                  &audioFormat,
                                  &size);


    // 初始化
    status = AudioUnitInitialize(audioUnit);
    
    status = AudioOutputUnitStart(audioUnit);
    
    if (status < 0) {
        NSLog(@"audio unit start fail");
        return;
    }
}

static void start_render_audio() {
    OSStatus status;

    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent inputComponent = AudioComponentFindNext(NULL, &desc);
    
    AudioUnit audioUnit;
    status = AudioComponentInstanceNew(inputComponent, &audioUnit);

    UInt32 size = 0;
    
    AudioStreamBasicDescription audioFormat = {0};
    audioFormat.mSampleRate = 48000; // 这里是对应的采样率
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger;
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mChannelsPerFrame = 2;
    audioFormat.mBitsPerChannel = 16;
    audioFormat.mBytesPerPacket = 4;
    audioFormat.mBytesPerFrame = 4;

    // 申请格式
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &audioFormat,
                                  sizeof(audioFormat));

    

    // 设置回调函数
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = playbackCallback; // 回调函数
    callbackStruct.inputProcRefCon = NULL;
    status = AudioUnitSetProperty(audioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Global,
                                  0,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    
    UInt32 preferredBufferSize = (( 10 * audioFormat.mSampleRate) / 1000); // in bytes
    size = sizeof (preferredBufferSize);

    // Mac OS 设置
    status = AudioUnitSetProperty (audioUnit,
                                   kAudioDevicePropertyBufferFrameSize,
                                   kAudioUnitScope_Global,
                                   0,
                                   &preferredBufferSize,
                                   size);
    
    status = AudioUnitGetProperty (audioUnit,
                                   kAudioDevicePropertyBufferFrameSize,
                                   kAudioUnitScope_Global,
                                   0,
                                   &preferredBufferSize,
                                   &size);
    
    // 检查
    size = sizeof(audioFormat);
    status = AudioUnitGetProperty( audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &audioFormat,
                                  &size);


    // 初始化
    status = AudioUnitInitialize(audioUnit);
    
    status = AudioOutputUnitStart(audioUnit);
}

int main(int argc, const char * argv[]) {
    fd = fopen("/Users/zhougang/osxaudio/osxaudio/sound.wav", "rb+");
    if (@available(macOS 10.14, *)) {
        
        AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        NSLog(@"mac audio : %i", authStatus);
    }
    //start_render_audio();
    start_capture_audio();
    while (1) {
        
    }
    return 0;
}
