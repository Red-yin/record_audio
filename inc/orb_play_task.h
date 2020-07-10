/********************************************************
 *	file	orb_play_task.h
 *		audio_manager play_task 数据结构和接口定义头文件	
 *
 *
 *	author	zhuxunhu
 *	date	2020.06.17
 *
 * ******************************************************/
#ifndef __ORB_PLAY_TASK_H__
#define __ORB_PLAY_TASK_H__

#include "orb_play_pcm.h"
#include "orb_play_decode.h"

/********************************************************
 *	enum	orb_audio_stream_e
 *		音频流类型
 *
 * ******************************************************/
typedef enum{
	ORB_AUDIO_STREAM_MUSIC,		// 音乐播放流 
	ORB_AUDIO_STREAM_TONE,		// 提示音流
	ORB_AUDIO_STREAM_SYSTEM		// 系统流（触摸反馈音等）
}orb_audio_stream_e;

typedef enum {
	ORB_PLAY_TASK_TYPE_URL,
	ORB_PLAY_TASK_TYPE_FILE,
	ORB_PLAY_TASK_TYPE_PCMFILE,
}orb_ptk_type_e;

typedef enum{
	ORB_PLAY_TASK_OPT_NONE,
	ORB_PLAY_TASK_OPT_PAUSE,
	ORB_PLAY_TASK_OPT_CONTINUE,
	ORB_PLAY_TASK_OPT_STOP,
}orb_ptk_opt_e;

typedef enum{
	ORB_PLAY_TASK_STATUS_IDEL,
	ORB_PLAY_TASK_STATUS_STARTING,
	ORB_PLAY_TASK_STATUS_PLAYING,
	ORB_PLAY_TASK_STATUS_PAUSE,
	ORB_PLAY_TASK_STATUS_STOPPING,
	ORB_PLAY_TASK_STATUS_DONE,
	ORB_PLAY_TASK_STATUS_ERR,
}orb_ptk_status_e;

typedef struct pcmfile{
	short	samplerate;
	char	bits;
	char	*filename;
}pcmfile_t;


typedef struct orb_play_task{
	orb_ptk_type_e			type;	// 播放任务类型
	orb_audio_stream_e		stream;	// 播放任务使用的音频流
	orb_ptk_status_e		status; // 播放任务状态
	orb_ptk_opt_e			opt;	// 播放任务控制字段
	double					cpos;	// 当前播放位置单位秒s
	double					total;  // 播放任务总时长,单位s
	union{
		char *url;
		char *filename;
		pcmfile_t *pcmfile;
	};
	
	audio_decode_t *decode;

	orb_sem_t sem;
	orb_sem_t stop_sem;			// task停止同步信号量

	orb_pcm_t *pcm;				// pcm播放句柄

}orb_play_task_t;

/********************************************************
 *	function	orb_play_task_create
 *		创建播放任务
 *
 *	@type		播放类型
 *	@stream		播放使用的流
 *	@param		播放参数,参数类型和type相关。 
 *
 *	return
 *		成功返回 task指针
 *		失败返回 NULL 
 *		
 * ******************************************************/
orb_play_task_t *orb_play_task_create(orb_ptk_type_e type, orb_audio_stream_e stream,void *param);

/********************************************************
 *	function	orb_play_task_start
 *		启动播放任务
 *	@task	播放任务句柄
 *
 *	return
 *		成功返回 0
 *		失败返回 -1
 * ******************************************************/
int orb_play_task_start(orb_play_task_t *task);

/********************************************************
 *	function	orb_play_task_restart
 *		重新启动播放任务
 *	@task	播放任务句柄
 *
 *	return
 *		成功返回 0
 *		失败返回 -1
 * ******************************************************/
int orb_play_task_restart(orb_play_task_t *task);

/********************************************************
 *	function	orb_play_task_pause
 *		播放任务暂停
 *
 *	@task	播放任务句柄
 *
 *		
 *	return
 *		成功返回 0
 *		失败返回 -1
 *
 * ******************************************************/
int orb_play_task_pause(orb_play_task_t *task);

/********************************************************
 *	function	orb_play_task_resume
 *		播放任务 继续播放
 *
 *	@task	播放任务句柄
 *
 *	return
 *		成功返回 0
 *		失败返回 -1
 *
 * ******************************************************/
int orb_play_task_resume(orb_play_task_t *task);

/********************************************************
 *	function	orb_play_task_stop
 *		停止播放任务
 *
 *	@task	播放任务句柄
 *
 *	return
 *		成功返回 0
 *		失败返回 -1
 *
 * ******************************************************/
int orb_play_task_stop(orb_play_task_t *task);

/********************************************************
 *	function	orb_play_task_destroy
 *		销毁播放任务结构体
 *
 *	@task  播放任务
 *
 *
 *	return
 *		成功返回 0
 *		失败返回 -1
 * ******************************************************/
int orb_play_task_destroy(orb_play_task_t *task);

/********************************************************
 *	function	orb_play_task_wait_done
 *		
 * ******************************************************/
int orb_play_task_wait_done(orb_play_task_t *task,int timeout);

#endif

