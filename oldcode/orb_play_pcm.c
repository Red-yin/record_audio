#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include "orb_play_pcm.h"
#include "orb_play_debug.h"
#include "platform.h"

void pcm_data_destroy(pcm_data_t *data)
{
	if(data){
		if(data->data){
			free(data->data);
		}
		free(data);
	}
}

static void orb_pcm_destroy(orb_pcm_t *pcm)
{
	if(pcm == NULL){
		return;
	}

	if(pcm->device_type == ORB_PCM_DEVICE_LOCAL){
		if(pcm->device_data){
			snd_pcm_drain(pcm->device_data);
			snd_pcm_close(pcm->device_data);
			pcm->device_data = NULL;
		}
	}

	free(pcm);
}

static int local_pcm_device_init(orb_pcm_t *pcm)
{
	int sample_bits = pcm->bits;
	int samplerate = pcm->samplerate;
	int channels = pcm->channels;

	snd_pcm_t *playback_handle = NULL;
	snd_pcm_hw_params_t *hw_params = NULL;
	snd_pcm_sw_params_t *sw_params = NULL;
	int i;
	int err;
	int dir;
	int pcm_fmt;
	snd_pcm_sframes_t period_size = 640*2; //1536;
	snd_pcm_sframes_t buffer_size = 640*10; //1536*4;
	snd_pcm_uframes_t boundary;

	AUDIO_PLAY_DEBUG(" sample_bits:%d samplerate:%d channels:%d",sample_bits,samplerate,channels);

	for(i=0; i<5; i++) {
		if((err = snd_pcm_open(&playback_handle,  "plug:dmix", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
			usleep(200*1000);
			AUDIO_PLAY_ERR("play music open alsa plug:dmix fail %d time\n", i);		  
		} else {
			AUDIO_PLAY_DEBUG("play music open alsa %d time plug:dmix ok\n", i);
			break;
		}

	}
	if(err <0){
		AUDIO_PLAY_ERR("play music open alsa fail");
		return -1;
	}


#if 1
	//fcntl(get_pcm_handle(playback_handle),F_SETFD,FD_CLOEXEC);

	if((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
		AUDIO_PLAY_ERR("cannot allocate hardware parameter structure (%s)\n",snd_strerror(err));
		goto error;
	}

	if((err = snd_pcm_hw_params_any(playback_handle, hw_params)) < 0) {
		AUDIO_PLAY_ERR("cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
		goto error;
	}

	if((err = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		AUDIO_PLAY_ERR("cannot allocate hardware parameter structure (%s)\n",snd_strerror(err));
		goto error;
	}

	if(16 == sample_bits)
		pcm_fmt = SND_PCM_FORMAT_S16_LE;
	else
		pcm_fmt = SND_PCM_FORMAT_S24_LE;

	if((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, pcm_fmt)) < 0) {
		AUDIO_PLAY_ERR("cannot allocate hardware parameter structure (%s)\n",snd_strerror(err));
		goto error;
	}

	if((err = snd_pcm_hw_params_set_rate(playback_handle, hw_params,samplerate, 0)) < 0) {
		AUDIO_PLAY_ERR("cannot set sample rate %d, (%s)\n",samplerate, snd_strerror(err));
		goto error;
	}


	if((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params,channels)) <0) {
		AUDIO_PLAY_ERR("cannot set channel count (%s)\n", snd_strerror(err));
		goto error;
	}

	//printf("adplayer  set channels    =%d  \n\n",channels);

	err = snd_pcm_hw_params_set_buffer_size_near(playback_handle, hw_params, &buffer_size);
	if (err < 0) {
		AUDIO_PLAY_ERR("Unable to set buffer size %ld for playback: %s\n", buffer_size, snd_strerror(err));
		goto error;
	}

	err = snd_pcm_hw_params_set_period_size_near(playback_handle, hw_params, &period_size, &dir);
	if (err < 0) {
		AUDIO_PLAY_ERR("Unable to set period size %ld for playback: %s\n", period_size, snd_strerror(err));
		goto error;
	}

	if((err = snd_pcm_hw_params(playback_handle, hw_params)) < 0) {
		AUDIO_PLAY_ERR("cannot set hw parameters (%s)\n", snd_strerror(err));
		goto error;
	}

	snd_pcm_hw_params_free(hw_params);

#if 0//added for underrun test begin 160128  
	snd_pcm_sw_params_alloca(&sw_params);

	if ((err = snd_pcm_sw_params_current(playback_handle, sw_params)) < 0) {
		AUDIO_PLAY_ERR("audioplayer sw params current failed, errorcode = %s\n",snd_strerror(err));
		goto error;  
	}    

	if ((err = snd_pcm_sw_params_get_boundary(sw_params, &boundary))<0) {
		AUDIO_PLAY_ERR("audioplayer sw params get boundary failed, errorcode = %s\n",snd_strerror(err));
		goto error;     
	}

	if ((err = snd_pcm_sw_params_set_silence_threshold(playback_handle, sw_params, 0) )<0) {
		AUDIO_PLAY_ERR("audioplayer sw params set silence threshold failed, errorcode = %s\n", snd_strerror(err));
		goto error;     
	}

	if ((err = snd_pcm_sw_params_set_silence_size(playback_handle, sw_params, boundary) )<0) {
		AUDIO_PLAY_ERR("audioplayer sw params set silence size failed, errorcode = %s\n", snd_strerror(err));
		goto error;     
	}

	if((err = snd_pcm_sw_params(playback_handle, sw_params)) < 0) {
		AUDIO_PLAY_ERR("cannot set sw parameters (%s)\n", snd_strerror(err));
		goto error;
	}    
#endif//added for underrun test end 160128 
#endif

	pcm->device_data = playback_handle;
	return 0;


error:
	if(hw_params){
		snd_pcm_hw_params_free(hw_params);
	}

	snd_pcm_drain(playback_handle);
	snd_pcm_close(playback_handle);
	return -1;
}

int orb_pcm_data_put(orb_pcm_t *pcm, char *data, int len, int timeout)
{
	if(pcm == NULL || data == NULL){
		return -1;
	}
	snd_pcm_writei(pcm->device_data,data,len/pcm->channels/2);
	return 0;
}

orb_pcm_t *orb_pcm_play_start(int samplerate, int bits, int channels, orb_pcm_device_e device)
{
	orb_pcm_t *pcm = calloc(1,sizeof(*pcm));
	pcm->samplerate = samplerate;
	pcm->bits = bits;
	pcm->channels = channels;
	pcm->device_type = device;

	if(device == ORB_PCM_DEVICE_LOCAL){
		if(local_pcm_device_init(pcm)){
			AUDIO_PLAY_ERR("Local play device init fail");
			goto err;
		}
	}

	return pcm;

err:
	return NULL;
}


int orb_pcm_play_stop(orb_pcm_t *pcm)
{
	if(pcm == NULL){
		return -1;
	}

	pcm->opt = PCM_OPT_STOP;

	if(pcm->device_data){
		snd_pcm_drain(pcm->device_data);
		snd_pcm_close(pcm->device_data);
		pcm->device_data = NULL;
	}

	orb_pcm_destroy(pcm);
	return 0;

}

