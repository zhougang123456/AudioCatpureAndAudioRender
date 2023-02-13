#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <endpointvolume.h>
#include "stdio.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "libswresample/swresample.h"
#ifdef __cplusplus
}
#endif
#define REFTIMES_PER_SEC       (1000000)
#define REFTIMES_PER_MILLISEC  (10000)

#define SAFE_RELEASE(punk)  \
	if ((punk) != NULL)  \
				{ (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID   IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID   IID_IAudioClient = __uuidof(IAudioClient);
const IID   IID_IAudioEndpointVolume = __uuidof(IAudioEndpointVolume);
const IID   IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID   IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

#define min(a,b)            (((a) < (b)) ? (a) : (b))

#define AUDIO_CHANNELS AV_CH_LAYOUT_STEREO
#define AUDIO_FORMAT  AV_SAMPLE_FMT_S16
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_FRAME_SAMPLES 480
#define AUDIO_FRAME_SIZE 1920

static void dump_raw(BYTE* data, int size)
{
	static FILE* file = NULL;
	if (file == NULL)
	{
		char Buf[128];
		sprintf(Buf, "E:\\output.raw");
		file = fopen(Buf, "wb");
	}
	if (file != NULL) {
		fwrite(data, size, 1, file);
	}
}

static AVSampleFormat convert_avformat(WORD wBitsPerSample)
{
	switch (wBitsPerSample)
	{
	case 32:
		return AV_SAMPLE_FMT_FLT;
	case 16:
		return AV_SAMPLE_FMT_S16;
	case 8:
		return AV_SAMPLE_FMT_U8;
	default:
		printf("invalid format\n");
		return AV_SAMPLE_FMT_NONE;
	}
}

static int64_t convert_avchannels(WORD nChannels)
{
	switch (nChannels)
	{
	case 1:
		return AV_CH_LAYOUT_MONO;
	case 2:
		return AV_CH_LAYOUT_STEREO;
	default:
		printf("invalid channels\n");
		return -1;
	}
}

static bool stillPlaying = true;

void audio_capture_start()
{
	HRESULT hr;

	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDevice* pDevice = NULL;
	IAudioClient* pAudioClient = NULL;
	IAudioEndpointVolume* pAudioEndpointVolume = NULL;
	IAudioCaptureClient* pCaptureClient = NULL;
	WAVEFORMATEX* pwfx = NULL;

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	UINT32         bufferFrameCount;
	UINT32         numFramesAvailable;

	BYTE* pData;
	UINT32         packetLength = 0;
	DWORD          flags;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// 首先枚举你的音频设备
	// 你可以在这个时候获取到你机器上所有可用的设备，并指定你需要用到的那个设置
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator,
		NULL,
		CLSCTX_ALL,
		IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);

	hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice); // 采集麦克风

	hr = pDevice->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
	// 创建一个管理对象，通过它可以获取到你需要的一切数据
	hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	// 获取支持的音频格式
	hr = pAudioClient->GetMixFormat(&pwfx);

	int nFrameSize = (pwfx->wBitsPerSample / 8) * pwfx->nChannels;

	// 初始化管理对象，在这里，你可以指定它的最大缓冲区长度，这个很重要，
	// 应用程序控制数据块的大小以及延时长短都靠这里的初始化，具体参数大家看看文档解释
	hr = pAudioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
		hnsRequestedDuration,
		0,
		pwfx,
		NULL);

	REFERENCE_TIME hnsStreamLatency;
	hr = pAudioClient->GetStreamLatency(&hnsStreamLatency);

	REFERENCE_TIME hnsDefaultDevicePeriod;
	REFERENCE_TIME hnsMinimumDevicePeriod;
	hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, &hnsMinimumDevicePeriod);

	hr = pAudioClient->GetBufferSize(&bufferFrameCount);

	HANDLE hAudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	hr = pAudioClient->SetEventHandle(hAudioSamplesReadyEvent);

	hr = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);

	hr = pAudioClient->Start();  // Start recording.

	// 设置录音静音
	//hr = pAudioEndpointVolume->SetMute(TRUE, NULL);
	// 设置录音音量
	hr = pAudioEndpointVolume->SetMasterVolumeLevelScalar(1.0f, &GUID_NULL);
	
	SwrContext* swr = swr_alloc_set_opts(NULL,
									     AUDIO_CHANNELS, 
										 AUDIO_FORMAT,
										 AUDIO_SAMPLE_RATE,
										 convert_avchannels(pwfx->nChannels),
										 convert_avformat(pwfx->wBitsPerSample), 
										 pwfx->nSamplesPerSec,
									     0, NULL);

	if (swr_init(swr) < 0)
	{
		printf("swr init failed\n");
		return;
	}

	stillPlaying = TRUE;
	uint8_t* outbuf = (uint8_t*)av_malloc(AUDIO_FRAME_SIZE * 2);
	int dst_nb_samples;
	int real_nb_samples;
	// Each loop fills about half of the shared buffer.
	while (stillPlaying)
	{
		DWORD waitResult = WaitForSingleObject(hAudioSamplesReadyEvent, INFINITE);
		switch (waitResult)
		{
			case WAIT_OBJECT_0 + 0:     // _AudioSamplesReadyEvent
			{
				hr = pCaptureClient->GetNextPacketSize(&packetLength);
				while (packetLength != 0)
				{
					// Get the available data in the shared buffer.
					// 锁定缓冲区，获取数据
					hr = pCaptureClient->GetBuffer(&pData,
												   &numFramesAvailable,
												   &flags, NULL, NULL);

					dst_nb_samples = av_rescale_rnd(swr_get_delay(swr, pwfx->nSamplesPerSec) + numFramesAvailable,
						AUDIO_SAMPLE_RATE, pwfx->nSamplesPerSec, AVRounding(1));

					if (numFramesAvailable != 0)
					{
						//  The flags on capture tell us information about the data.
						//  We only really care about the silent flag since we want to put frames of silence into the buffer
						//  when we receive silence.  We rely on the fact that a logical bit 0 is silence for both float and int formats.
						if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
						{
							//  Fill 0s from the capture buffer to the output buffer.
							memset(pData, 0, numFramesAvailable * nFrameSize);
						}

						real_nb_samples = swr_convert(swr, &outbuf, dst_nb_samples, (const uint8_t**)&pData, numFramesAvailable);
						dump_raw(outbuf, real_nb_samples * 4);
					}

					SYSTEMTIME time;
					GetLocalTime(&time);
					printf("record ms %d\n", time.wMilliseconds);

					hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
					hr = pCaptureClient->GetNextPacketSize(&packetLength);

					UINT32 ui32NumPaddingFrames;
					hr = pAudioClient->GetCurrentPadding(&ui32NumPaddingFrames);
				}
			}
			break;
		} 
	} 
	real_nb_samples = swr_convert(swr, &outbuf, dst_nb_samples, NULL, 0);
	dump_raw(outbuf, real_nb_samples * 4);

	hr = pAudioClient->Stop();  // Stop recording.

	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDevice)
	SAFE_RELEASE(pAudioClient)
	SAFE_RELEASE(pCaptureClient)
	SAFE_RELEASE(pAudioEndpointVolume)

	CoUninitialize();
	swr_free(&swr);
	av_free(outbuf);
	printf("stop capture audio\n");
}
void audio_capture_stop()
{
	stillPlaying = FALSE;
}

void audio_render_start()
{
	HRESULT hr;

	IMMDeviceEnumerator* pEnumerator = NULL;
	IMMDevice* pDevice = NULL;
	IAudioClient* pAudioClient = NULL;
	IAudioEndpointVolume* pAudioEndpointVolume = NULL;
	IAudioRenderClient* pRenderClient = NULL;
	WAVEFORMATEX* pwfx = NULL;

	REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
	UINT32         bufferFrameCount;
	UINT32         numFramesAvailable;

	BYTE* pData;
	UINT32         packetLength = 0;
	DWORD          flags;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// 首先枚举你的音频设备
	// 你可以在这个时候获取到你机器上所有可用的设备，并指定你需要用到的那个设置
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator,
						  NULL,
						  CLSCTX_ALL,
						  IID_IMMDeviceEnumerator,
						  (void**)&pEnumerator);

	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice); // 扬声器播放

	hr = pDevice->Activate(IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
	// 创建一个管理对象，通过它可以获取到你需要的一切数据
	hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	// 获取支持的音频格式
	hr = pAudioClient->GetMixFormat(&pwfx);

	int frameSamples = pwfx->nSamplesPerSec / 100;
	int sampleSize = (pwfx->wBitsPerSample / 8) * pwfx->nChannels;
	int frameSize = sampleSize * frameSamples;

	// 初始化管理对象，在这里，你可以指定它的最大缓冲区长度，这个很重要，
	// 应用程序控制数据块的大小以及延时长短都靠这里的初始化，具体参数大家看看文档解释
	hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
								  AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
								  hnsRequestedDuration,
								  0,
								  pwfx,
								  NULL);

	REFERENCE_TIME hnsStreamLatency;
	hr = pAudioClient->GetStreamLatency(&hnsStreamLatency);

	REFERENCE_TIME hnsDefaultDevicePeriod;
	REFERENCE_TIME hnsMinimumDevicePeriod;
	hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, &hnsMinimumDevicePeriod);

	hr = pAudioClient->GetBufferSize(&bufferFrameCount);

	HANDLE hAudioSamplesReadyEvent = CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	if (hAudioSamplesReadyEvent == NULL) {
		return;
	}

	hr = pAudioClient->SetEventHandle(hAudioSamplesReadyEvent);

	hr = pAudioClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);

	hr = pAudioClient->Start();  // Start recording.

	//hr = pAudioEndpointVolume->SetMute(TRUE, NULL);

	//hr = pAudioEndpointVolume->SetMasterVolumeLevelScalar(1.0f, &GUID_NULL);

	SwrContext* swr = swr_alloc_set_opts(NULL,
										 convert_avchannels(pwfx->nChannels),
										 convert_avformat(pwfx->wBitsPerSample),
										 pwfx->nSamplesPerSec,
										 AUDIO_CHANNELS,
										 AUDIO_FORMAT,
										 AUDIO_SAMPLE_RATE,
										 0, NULL);
	if (swr_init(swr) < 0)
	{
		printf("swr init failed\n");
		return;
	}

	int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr, pwfx->nSamplesPerSec) + AUDIO_FRAME_SAMPLES,
										pwfx->nSamplesPerSec, AUDIO_SAMPLE_RATE, AVRounding(1));
	int real_nb_samples;
	stillPlaying = TRUE;
	uint8_t* inbuf = (uint8_t*)av_malloc(AUDIO_FRAME_SIZE);
	uint8_t* outbuf = (uint8_t*)av_malloc(frameSize);
	FILE* f = fopen("E:\\output.raw", "rb+");

	// Each loop fills about half of the shared buffer.
	while (stillPlaying)
	{  
		DWORD waitResult = WaitForSingleObject(hAudioSamplesReadyEvent, INFINITE);
		switch (waitResult)
		{
			case WAIT_OBJECT_0 + 0:     // _AudioSamplesReadyEvent
			{
				// Get the available data in the shared buffer.
				// 锁定缓冲区，获取数据
				hr = pRenderClient->GetBuffer(frameSamples, &pData);
				int read_bytes = fread(inbuf, 1, AUDIO_FRAME_SIZE, f);
				if (read_bytes <= 0) {
					return;
				}
				real_nb_samples = swr_convert(swr, &outbuf, dst_nb_samples, (const uint8_t**)&inbuf, AUDIO_FRAME_SAMPLES);
				memcpy(pData, outbuf, sampleSize * real_nb_samples);
				hr = pRenderClient->ReleaseBuffer(real_nb_samples, 0);

				SYSTEMTIME time;
				GetLocalTime(&time);
				printf("playback ms %d\n", time.wMilliseconds);

				UINT32 ui32NumPaddingFrames;
				hr = pAudioClient->GetCurrentPadding(&ui32NumPaddingFrames);
			}
			break;
		}
	}

	hr = pAudioClient->Stop();  // Stop rendering.

	CoTaskMemFree(pwfx);
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pDevice)
	SAFE_RELEASE(pAudioClient)
	SAFE_RELEASE(pRenderClient)

	CoUninitialize();
	swr_free(&swr);
	av_free(outbuf);
	printf("stop render audio\n");
}

int main()
{
	audio_capture_start();
	//audio_render_start();
}