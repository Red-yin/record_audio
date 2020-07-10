
#include "orb_play_task.h"
#include "orb_play_debug.h"


orb_play_task_t *orb_play_task_create(orb_ptk_type_e type, orb_audio_stream_e stream,void *param)
{
	orb_play_task_t *task = calloc(1,sizeof(*task));
	if(task == NULL){
		return NULL;
	}

	task->type = type;
	task->stream = stream;


	switch(type){
		case ORB_PLAY_TASK_TYPE_URL:
			if(param == NULL){
				AUDIO_PLAY_ERR("play task create param error! url is null");
				goto err;
			}
			task->url = calloc(1,strlen(param) + 1);
			strcpy(task->url,param);
			break;
		case ORB_PLAY_TASK_TYPE_FILE:
			if(param == NULL){
				AUDIO_PLAY_ERR("play task create param error! file is null");
				goto err;
			}
			task->filename = calloc(1,strlen(param) + 1);
			strcpy(task->filename,param);
			break;
		case ORB_PLAY_TASK_TYPE_PCMFILE:
			if(param == NULL){
				AUDIO_PLAY_ERR("play task create param error! pcmfile struct is null");
				goto err;
			}
			task->pcmfile = param;
			break;
	}

	orb_sem_init(&task->sem,0);
	orb_sem_init(&task->stop_sem,0);

	return task;	

err:
	orb_play_task_destroy(task);
	return NULL;
}


int orb_play_task_destroy(orb_play_task_t *task)
{
	if(task == NULL){
		return -1;
	}

	switch(task->type){
		case ORB_PLAY_TASK_TYPE_URL:
			if(task->url){
				free(task->url);
			}
			break;
		case ORB_PLAY_TASK_TYPE_FILE:
			if(task->filename){
				free(task->filename);
			}
			break;
		case ORB_PLAY_TASK_TYPE_PCMFILE:
			// TODO 
			break;
	}

	if(task->sem){
		orb_sem_destroy(task->sem);
	}

	if(task->stop_sem){
		orb_sem_destroy(task->stop_sem);
		task->stop_sem = NULL;
	}



	return 0;
}

static void *play_task_thread(void *param)
{
	orb_thread_detach(NULL);

	orb_play_task_t *task = param;


	orb_sem_post(task->sem);
	task->status = ORB_PLAY_TASK_STATUS_PLAYING;

	task->pcm = orb_pcm_play_start(task->decode->samplerate,task->decode->sample_bits,task->decode->channels,ORB_PCM_DEVICE_LOCAL);
	if(task->pcm == NULL){
		AUDIO_PLAY_ERR("Pcm device start fail");
	}

	AUDIO_PLAY_INFO("play task start cpos:%f",task->cpos);
	audio_decode_set_pos(task->decode,task->cpos);

	char *data;
	int len;
	while(1) {
		if(task->opt == ORB_PLAY_TASK_OPT_STOP){
			task->opt = ORB_PLAY_TASK_OPT_NONE;
			task->status = ORB_PLAY_TASK_STATUS_STOPPING;
			break;
		}

		if(task->opt == ORB_PLAY_TASK_OPT_PAUSE){
			task->status = ORB_PLAY_TASK_STATUS_PAUSE;
			orb_pcm_play_stop(task->pcm);
			task->pcm = NULL;
			orb_sem_wait(task->sem);
			task->pcm = orb_pcm_play_start(task->decode->samplerate,
					task->decode->sample_bits,task->decode->channels,ORB_PCM_DEVICE_LOCAL);
			task->status = ORB_PLAY_TASK_STATUS_PLAYING;
			task->opt = ORB_PLAY_TASK_OPT_NONE;
			continue;
		}

		len = audio_decode_get_data(task,&data);	
		if(len < 0){
			AUDIO_PLAY_ERR("Decode get data fail");
			break;
		}else if(len == 0){
			task->status = ORB_PLAY_TASK_STATUS_STOPPING;
			break;
		}

		if(orb_pcm_data_put(task->pcm,data,len,1000)){
			AUDIO_PLAY_WARNING("pcm data put fail");
		}
	}

	if(task->pcm){
		orb_pcm_play_stop(task->pcm);
		task->pcm = NULL;
	}

	if(task->decode){
		audio_decode_uninit(task->decode);
		task->decode = NULL;
	}

	if(task->status == ORB_PLAY_TASK_STATUS_STOPPING){
		task->status = ORB_PLAY_TASK_STATUS_DONE;
	}

	if(task->stop_sem){
		orb_sem_post(task->stop_sem);
	}

	AUDIO_PLAY_INFO("task play down");

}

int orb_play_task_start(orb_play_task_t *task)
{
	if(task == NULL){
		return -1;
	}

	if(task->status == ORB_PLAY_TASK_STATUS_STARTING ||
			task->status == ORB_PLAY_TASK_STATUS_PLAYING ||
			task->status == ORB_PLAY_TASK_STATUS_PAUSE){
		AUDIO_PLAY_ERR("task is running, don't start it");
		return -1;
	}


	task->status = ORB_PLAY_TASK_STATUS_STARTING;
	if(task->stop_sem){
		orb_sem_destroy(task->stop_sem);
	}
	orb_sem_init(&task->stop_sem,0);
	task->decode = audio_decode_init(task->url);
	if(task->decode == NULL){
		goto err;
	}

	task->total = audio_decode_total_time_get(task->decode);


	struct thread_param param = {"audio task",OS_PRIORITY_HIGH,1024};
	orb_thread_t pd;
	orb_thread_create(&pd,&param,play_task_thread,task);
	orb_sem_wait(task->sem);

	return 0;
err:
	orb_play_task_destroy(task);
	return -1;

}

int orb_play_task_restart(orb_play_task_t *task)
{
	if(task == NULL){
		return -1;
	}


	if(task->status == ORB_PLAY_TASK_STATUS_PLAYING ||
			task->status == ORB_PLAY_TASK_STATUS_PAUSE){
		orb_play_task_stop(task);
	}
	orb_play_task_wait_done(task,1000);
	task->cpos = 0;
	task->status = ORB_PLAY_TASK_STATUS_IDEL;
	orb_play_task_start(task);
	return 0;
}

int orb_play_task_pause(orb_play_task_t *task)
{
	if(task == NULL){
		return -1;
	}

	if(task->status != ORB_PLAY_TASK_STATUS_PLAYING){
		AUDIO_PLAY_ERR("task is not playing");
		return -1;
	}

	task->opt = ORB_PLAY_TASK_OPT_PAUSE;
	return 0;
}

int orb_play_task_resume(orb_play_task_t *task)
{
	if(task == NULL){
		return -1;
	}

	if(task->status != ORB_PLAY_TASK_STATUS_PAUSE){
		AUDIO_PLAY_ERR("task is not in pause");
		return -1;
	}

	task->opt = ORB_PLAY_TASK_OPT_CONTINUE;
	orb_sem_post(task->sem);
	return 0;
}

int orb_play_task_stop(orb_play_task_t *task)
{
	if(task == NULL){
		return -1;
	}

	if(task->status == ORB_PLAY_TASK_STATUS_DONE ||
			task->status == ORB_PLAY_TASK_STATUS_STOPPING){
		AUDIO_PLAY_ERR("task is not running");
		return -1;
	}

	task->opt = ORB_PLAY_TASK_OPT_STOP;

	if(task->status == ORB_PLAY_TASK_STATUS_PAUSE){
		orb_sem_post(task->sem);
	}

	if(orb_play_task_wait_done(task,10*1000)){
		AUDIO_PLAY_ERR("wait task thread exit error");
	}

	return 0;
}

int orb_play_task_wait_done(orb_play_task_t *task,int timeout)
{
	if(task == NULL){
		return -1;
	}

	if(task->status == ORB_PLAY_TASK_STATUS_DONE ||
			task->status == ORB_PLAY_TASK_STATUS_ERR){
		return 0;
	}
	
	if(task->stop_sem){
		if(orb_sem_wait_timeout(task->stop_sem,timeout)){
			return -1;
		}
	}else{
		return -1;
	}

	orb_sem_post(task->stop_sem);
	return 0;
}
