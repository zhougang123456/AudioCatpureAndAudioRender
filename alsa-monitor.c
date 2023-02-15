#include <stdio.h>   
#include <unistd.h>   
#include <sys/select.h>   
#include <errno.h>   
#include <sys/inotify.h>   
#include <alsa/asoundlib.h>

int device_num = 0;

static void update_devices(int device_count)
{	
	if (device_count > device_num) {	
		printf("add device\n");
	} else if (device_count < device_num) {
		printf("remove device\n");
	}
	device_num = device_count;
}	

static void probe_devices(void)
{
  snd_ctl_t *handle;
  int card, dev;
  snd_ctl_card_info_t *info;
  snd_pcm_info_t *pcminfo;

  int i;
  int streams[] = { SND_PCM_STREAM_CAPTURE, SND_PCM_STREAM_PLAYBACK };
  snd_pcm_stream_t stream;

  snd_ctl_card_info_malloc (&info);
  snd_pcm_info_malloc (&pcminfo);

  for (i = 0; i < 1; i++) {
    card = -1;
    stream = (snd_pcm_stream_t)streams[i];

    if (snd_card_next (&card) < 0 || card < 0) {
      /* no soundcard found */
      printf("No soundcard found\n");
      goto beach;
    }

    int device_count = 0;
    while (card >= 0) {
      
      char name[32];

      sprintf (name, "hw:%d", card);
      if (snd_ctl_open (&handle, name, 0) < 0)
        goto next_card;

      if (snd_ctl_card_info (handle, info) < 0) {
        snd_ctl_close (handle);
        goto next_card;
      }

      dev = -1;
      while (1) {
        //GstDevice *device;
        snd_ctl_pcm_next_device (handle, &dev);

        if (dev < 0)
          break;
        snd_pcm_info_set_device (pcminfo, dev);
        snd_pcm_info_set_subdevice (pcminfo, 0);
        snd_pcm_info_set_stream (pcminfo, stream);
        if (snd_ctl_pcm_info (handle, pcminfo) < 0) {
          continue;
        }
	
	device_count++;
	
	//printf("stream: %d card: %d dev: %d\n", stream, card, dev);
      }
      snd_ctl_close (handle);
    next_card:
      if (snd_card_next (&card) < 0) {
        break;
      }
    }
    update_devices(device_count);
  }

beach:
  snd_ctl_card_info_free (info);
  snd_pcm_info_free (pcminfo);

}

static void inotify_event_handler(struct inotify_event *event)      //从buf中取出一个事件。  
{   
	 if (event->mask & IN_CREATE) {
		printf("create event->mask: 0x%08x\n", event->mask); 
		printf("event->name: %s\n", event->name);
		sleep(1);	
		probe_devices();		
	 } else if (event->mask & IN_DELETE){
		printf("delete event->mask: 0x%08x\n", event->mask); 
		printf("event->name: %s\n", event->name);	
		probe_devices();	
         } else if (event->mask & IN_MODIFY){
		printf("change event->mask: 0x%08x\n", event->mask); 
		printf("event->name: %s\n", event->name);	
		probe_devices();	
         }
}   
  
int  main(int argc, char **argv)   
{   
  
  probe_devices();
	
  unsigned char buf[1024] = {0};   
  struct inotify_event *event = NULL;              
  int fd = inotify_init1(IN_NONBLOCK);               //初始化
  int wd = inotify_add_watch(fd, "/dev/snd/", IN_CREATE | IN_DELETE | IN_MODIFY);                  //监控指定文件。
  
  for (;;) 

  {   
       fd_set fds;   
       FD_ZERO(&fds);                
       FD_SET(fd, &fds);   
       if (select(fd + 1, &fds, NULL, NULL, NULL) > 0)                //监控fd的事件。当有事件发生时，返回值>0

       {   
           int len, index = 0;   
           while (((len = read(fd, &buf, sizeof(buf))) < 0) && (errno == EINTR));       //没有读取到事件。
           while (index < len) 

           {   
                  event = (struct inotify_event *)(buf + index);                      
                 	 	inotify_event_handler(event);                                             //获取事件。
                  index += sizeof(struct inotify_event) + event->len;             //移动index指向下一个事件。
           }   
       }   
  }   
  
  inotify_rm_watch(fd, wd);              //删除对指定文件的监控。
  
  return 0;   
}  

