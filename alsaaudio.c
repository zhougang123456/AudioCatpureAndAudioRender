#include <stdio.h>
#define ALSA_PCM_NEW_HW_PARAMS_API  
#include <alsa/asoundlib.h>  
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>
int start_capture_audio() 
{  
	long loops;  
	int rc,i = 0;  
	int size;  
	FILE *fp ;
	snd_pcm_t *handle;  
	snd_pcm_hw_params_t *params;  
	unsigned int val,val2;  
	int dir;  
	snd_pcm_uframes_t frames;  
	char *buffer;  
	if((fp =fopen("sound.wav","w")) < 0)
		printf("open sound.wav fail\n");
	/* Open PCM device for recording (capture). */  
	rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);  
	if (rc < 0) {  
		fprintf(stderr,  "unable to open pcm device: %s/n",  snd_strerror(rc));  
		exit(1);  
	}  
	/* Allocate a hardware parameters object. */  
	snd_pcm_hw_params_alloca(&params);  
	/* Fill it in with default values. */  
	snd_pcm_hw_params_any(handle, params);  
	/* Set the desired hardware parameters. */  
	/* Interleaved mode */  
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);  
	/* Signed 16-bit little-endian format */  
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);  
	/* Two channels (stereo) */  
	snd_pcm_hw_params_set_channels(handle, params, 2);  
	/* 48000 bits/second sampling rate (DVD quality) */  
	val = 48000;  
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);  
	frames = 480 * 3;
	
	/* Set max buffer time 30ms */
	snd_pcm_hw_params_set_buffer_size_near(handle, params, &frames);
	/* Set period size to 480 frames. */  
	frames = 480;  
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);  
	
	/* Write the parameters to the driver */  
	rc = snd_pcm_hw_params(handle, params);  
	if (rc < 0) {  
		fprintf(stderr,  "unable to set hw parameters: %s/n",  
		snd_strerror(rc));  
		exit(1);  
	}  
	/* Use a buffer large enough to hold one period */  
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);  
	size = frames * 4; /* 2 bytes/sample, 2 channels */  
	printf("size = %d\n",size);
	buffer = (char *) malloc(size);  
	/* We want to loop for 5 seconds */  
	snd_pcm_hw_params_get_period_time(params, &val, &dir);  
	loops = 1000;  
	while (--loops > 0) {  

		rc = snd_pcm_readi(handle, buffer, frames); 
		printf("%d read size %d\n",i++, rc); 
		if (rc == -EPIPE) {  
			/* EPIPE means overrun */  
			printf("overrun occurred\n");  
			snd_pcm_prepare(handle);  
		} else if (rc < 0) {  
			printf("error from read: %s\n",  
			snd_strerror(rc));  
		} else if (rc != (int)frames) {  
			printf("short read, read %d frames\n", rc);  
		}  
		rc = fwrite(buffer,1, size,fp);  
		struct timeval tv;
    		struct timezone tz;
    		struct tm  *ptrTime;
    		gettimeofday(&tv, &tz);
   		ptrTime = localtime((const time_t *)(&tv.tv_sec));
    		printf("Time: %ld\n", tv.tv_usec / 1000);
		if (rc != size)  
			printf("short write: wrote %d bytes\n", rc);  
		else 
			printf("fwrite buffer success %d\n", rc);
	}  
	/******************打印参数*********************/
	snd_pcm_hw_params_get_channels(params, &val);  
	printf("channels = %d\n", val);  
	snd_pcm_hw_params_get_rate(params, &val, &dir);  
	printf("rate = %d bps\n", val);  
	snd_pcm_hw_params_get_period_time(params, &val, &dir);  
	printf("period time = %d us\n", val);  
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);  
	printf("period size = %d frames\n", (int)frames);  
	snd_pcm_hw_params_get_buffer_time(params, &val, &dir);  
	printf("buffer time = %d us\n", val);  
	snd_pcm_hw_params_get_buffer_size(params, (snd_pcm_uframes_t *) &val);  
	printf("buffer size = %d frames\n", val);  
	snd_pcm_hw_params_get_periods(params, &val, &dir);  
	printf("periods per buffer = %d frames\n", val);  
	snd_pcm_hw_params_get_rate_numden(params, &val, &val2);  
	printf("exact rate = %d/%d bps\n", val, val2);  
	val = snd_pcm_hw_params_get_sbits(params);  
	printf("significant bits = %d\n", val);  
	//snd_pcm_hw_params_get_tick_time(params,  &val, &dir);  
	printf("tick time = %d us\n", val);  
	val = snd_pcm_hw_params_is_batch(params);  
	printf("is batch = %d\n", val);  
	val = snd_pcm_hw_params_is_block_transfer(params);  
	printf("is block transfer = %d\n", val);  
	val = snd_pcm_hw_params_is_double(params);  
	printf("is double = %d\n", val);  
	val = snd_pcm_hw_params_is_half_duplex(params);  
	printf("is half duplex = %d\n", val);  
	val = snd_pcm_hw_params_is_joint_duplex(params);  
	printf("is joint duplex = %d\n", val);  
	val = snd_pcm_hw_params_can_overrange(params);  
	printf("can overrange = %d\n", val);  
	val = snd_pcm_hw_params_can_mmap_sample_resolution(params);  
	printf("can mmap = %d\n", val);  
	val = snd_pcm_hw_params_can_pause(params);  
	printf("can pause = %d\n", val);  
	val = snd_pcm_hw_params_can_resume(params);  
	printf("can resume = %d\n", val);  
	val = snd_pcm_hw_params_can_sync_start(params);  
	printf("can sync start = %d\n", val);  
	/*******************************************************************/
	snd_pcm_drain(handle);  
	snd_pcm_close(handle); 
	fclose(fp); 
	free(buffer);  
	return 0;
}

int start_render_audio()
{
	long loops;  
	int rc,j = 0;  
	int size;  
	snd_pcm_t *handle;  
	snd_pcm_hw_params_t *params;

	unsigned int val, val2;  
	int dir;  
	snd_pcm_uframes_t frames;  
	char *buffer;  
	FILE *fp ;
	if((fp = fopen("sound.wav","r")) < 0)
		printf("open sound.wav fail\n");
	if(fseek(fp,0,SEEK_SET) < 0)
		printf("put fp start to first error\n ");


	/* Open PCM device for playback. */  
	rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);  
	if (rc < 0) {  
		fprintf(stderr,  "unable to open pcm device: %s/n", snd_strerror(rc));  
		exit(1);  
	}  
	/* Allocate a hardware parameters object. */  
	snd_pcm_hw_params_alloca(&params);  
	/* Fill it in with default values. */  
	snd_pcm_hw_params_any(handle, params);  
	/* Set the desired hardware parameters. */  
	/* Interleaved mode */  
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);  
	/* Signed 16-bit little-endian format */  
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);  
	/* Two channels (stereo) */  
	snd_pcm_hw_params_set_channels(handle, params, 2);  
	/* 48000 bits/second sampling rate (DVD quality) */  
	val = 48000;  
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);  
	frames = 480 * 3;
	/* Set max buffer time 30ms */
	snd_pcm_hw_params_set_buffer_size_near(handle, params, &frames);
	/* Set period size to 480 frames. */  
	frames = 480;  //设置的值没有反应
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir); 
	
	rc = snd_pcm_hw_params(handle, params);  
	if (rc < 0) {  
		fprintf(stderr,  "unable to set hw parameters: %s/n",  snd_strerror(rc));  
		exit(1);  
	}  

	/* Use a buffer large enough to hold one period */  
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);  
	size = frames * 4; /* 2 bytes/sample, 2 channels */  
	buffer = (char *) malloc(size);  

	loops = 1;  
	
	while (loops > 0) {  

    		rc = snd_pcm_wait (handle, 10);

		if (rc < 0) {  
			printf("warit err\n"); 
		} else {
			int frames_to_deliver = snd_pcm_avail_update(handle);
			printf("frames_to_deliver = %d\n", frames_to_deliver); 
 			if (frames_to_deliver < frames) {	
				continue;
			}

			rc = fread(buffer,1, size,fp); 
      
			printf("%d\n",j++); 
			if (rc == 0) {  
				fprintf(stderr, "end of file on input\n");  
				break;  
			} else if (rc != size) {  
				fprintf(stderr,  "short read: read %d bytes\n", rc);  
			}  
  			struct timeval tv;
    			struct timezone tz;
    			struct tm  *ptrTime;
    			gettimeofday(&tv, &tz);
   			ptrTime = localtime((const time_t *)(&tv.tv_sec));
    			printf("Time: %ld\n", tv.tv_usec / 1000);
			rc = snd_pcm_writei(handle, buffer, frames);  
		}

		if (rc == -EAGAIN) {
			continue;
	        }
		else if (rc == -EPIPE) {  
			/* EPIPE means underrun */  
			fprintf(stderr, "underrun occurred\n");  
			snd_pcm_prepare(handle);  
		} else if (rc < 0) {  
			fprintf(stderr,  "error from writei: %s\n",  
			snd_strerror(rc));  
		}  
	}  
	/******************打印参数*********************/
	snd_pcm_hw_params_get_channels(params, &val);  
	printf("channels = %d\n", val);  
	snd_pcm_hw_params_get_rate(params, &val, &dir);  
	printf("rate = %d bps\n", val);  
	snd_pcm_hw_params_get_period_time(params, &val, &dir);  
	printf("period time = %d us\n", val);  
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);  
	printf("period size = %d frames\n", (int)frames);  
	snd_pcm_hw_params_get_buffer_time(params, &val, &dir);  
	printf("buffer time = %d us\n", val);  
	snd_pcm_hw_params_get_buffer_size(params, (snd_pcm_uframes_t *) &val);  
	printf("buffer size = %d frames\n", val);  
	snd_pcm_hw_params_get_periods(params, &val, &dir);  
	printf("periods per buffer = %d frames\n", val);  
	snd_pcm_hw_params_get_rate_numden(params, &val, &val2);  
	printf("exact rate = %d/%d bps\n", val, val2);  
	val = snd_pcm_hw_params_get_sbits(params);  
	printf("significant bits = %d\n", val);  
	//snd_pcm_hw_params_get_tick_time(params,  &val, &dir);  
	printf("tick time = %d us\n", val);  
	val = snd_pcm_hw_params_is_batch(params);  
	printf("is batch = %d\n", val);  
	val = snd_pcm_hw_params_is_block_transfer(params);  
	printf("is block transfer = %d\n", val);  
	val = snd_pcm_hw_params_is_double(params);  
	printf("is double = %d\n", val);  
	val = snd_pcm_hw_params_is_half_duplex(params);  
	printf("is half duplex = %d\n", val);  
	val = snd_pcm_hw_params_is_joint_duplex(params);  
	printf("is joint duplex = %d\n", val);  
	val = snd_pcm_hw_params_can_overrange(params);  
	printf("can overrange = %d\n", val);  
	val = snd_pcm_hw_params_can_mmap_sample_resolution(params);  
	printf("can mmap = %d\n", val);  
	val = snd_pcm_hw_params_can_pause(params);  
	printf("can pause = %d\n", val);  
	val = snd_pcm_hw_params_can_resume(params);  
	printf("can resume = %d\n", val);  
	val = snd_pcm_hw_params_can_sync_start(params);  
	printf("can sync start = %d\n", val);  
	/*******************************************************************/
	snd_pcm_drain(handle);  
	snd_pcm_close(handle);  
	free(buffer);
	fclose(fp);
	return 0;
}

void set_audio_volome(bool is_src) {
	int unmute, chn;
	snd_mixer_t* mixer;
	snd_mixer_elem_t* master_element;
	snd_mixer_open(&mixer, 0);
	snd_mixer_attach(mixer, "default");
	snd_mixer_selem_register(mixer, NULL, NULL);
	snd_mixer_load(mixer);
	// get the first one element, the master
	master_element = snd_mixer_first_elem(mixer);
	if (!master_element) {
		snd_mixer_close(mixer);
		return;
	}

	if (is_src) {			
		if (!snd_mixer_selem_has_capture_volume(master_element)) {
			printf("no capture audio volume\n");  
			snd_mixer_close(mixer);
			return;
		}
		snd_mixer_selem_set_capture_volume_range(master_element, 0, 100);
		int volume = 100;

		snd_mixer_selem_set_capture_volume(master_element, SND_MIXER_SCHN_FRONT_LEFT, volume);
		if (!snd_mixer_selem_is_capture_mono(master_element)) {
			snd_mixer_selem_set_capture_volume(master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume);
		}
	} else {
		if (!snd_mixer_selem_has_playback_volume(master_element)) {
			printf("no playback audio volume\n");  
			return;
		}
		// set the range of volume 0-100
		snd_mixer_selem_set_playback_volume_range(master_element, 0, 100);
		int volume = 50;
		snd_mixer_selem_set_playback_volume(master_element, SND_MIXER_SCHN_FRONT_LEFT, volume);
		if (!snd_mixer_selem_is_playback_mono(master_element)) {
			snd_mixer_selem_set_playback_volume(master_element, SND_MIXER_SCHN_FRONT_RIGHT, volume);
		}
	}
	snd_mixer_close(mixer);
}

void set_audio_mute(bool is_src) {
	int chn;
	snd_mixer_t* mixer;
	snd_mixer_elem_t* master_element;
	snd_mixer_open(&mixer, 0);
	snd_mixer_attach(mixer, "default");
	snd_mixer_selem_register(mixer, NULL, NULL);
	snd_mixer_load(mixer);
	// get the first one element, the master
	master_element = snd_mixer_first_elem(mixer);
	if (!master_element) {
		snd_mixer_close(mixer);
		return;
	}
	if (is_src) {
		// set mute
		if (!snd_mixer_selem_has_capture_switch(master_element)) {
			printf("no capture audio mute\n");  
			snd_mixer_close(mixer);
		}
		int unmute;
		snd_mixer_selem_get_capture_switch(master_element, 0, &unmute);
		printf("capture audio mute %d \n", unmute);  
		for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
			snd_mixer_selem_set_capture_switch(master_element, chn, 0);
		}
	} else {
		if (!snd_mixer_selem_has_playback_switch(master_element)) {
			printf("no playback audio mute\n");  
		}
		// set mute
		for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
			snd_mixer_selem_set_playback_switch(master_element, chn, 0);
		}
		// set unmute
		for (chn = 0; chn <= SND_MIXER_SCHN_LAST; chn++) {
			snd_mixer_selem_set_playback_switch(master_element, chn, 1);
		}
	}
	snd_mixer_close(mixer);
}
int main() {
	//start_capture_audio();
	set_audio_mute(false);
	set_audio_volome(true);
	start_render_audio();
}
