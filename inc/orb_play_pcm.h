/********************************************************
 *	file	orb_play_pcm.h
 *		audio_manager pcm播放数据结构和接口定义头文件
 *
 *	
 *	author	zhuxunhu
 *	date	2020.06.17
 *
 * ******************************************************/
#ifndef __ORB_PLAY_PCM_H__
#define __ORB_PLAY_PCM_H__
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include "orb_queue.h"

typedef struct pcm_data{
	char	*data;
	int		len;
	char	channels;
}pcm_data_t;

typedef enum{
	ORB_PCM_DEVICE_LOCAL,
	ORB_PCM_DEVICE_BLUETOOTH,
}orb_pcm_device_e;

enum{
	PCM_OPT_NONE,
	PCM_OPT_STOP,
};

typedef struct orb_pcm{
	unsigned char 	opt:1;
	unsigned char	stopped:1;
	int				samplerate;
	char			bits;
	char			channels;
	orb_pcm_device_e	device_type;
	void				*device_data;
}orb_pcm_t;

/********************************************************
 *	function	orb_pcm_play_start
 *		pcm播放启动
 *
 *	@samlerate	采样率
 *	@bits		采样深度
 *	@channels	通道数
 *	@device		输出设备
 *
 *	return 
 *		成功返回 pcm播放句柄
 *		失败返回 NULL
 * ******************************************************/
orb_pcm_t *orb_pcm_play_start(int samplerate, int bits, int channels, orb_pcm_device_e device);

/********************************************************
 *	function	orb_pcm_data_put
 *		发送音频数据给PCM播放器
 *
 *	@pcm	播放器句柄
 *	@data	音频数据
 *	@len	音频数据长度
 *	@timeout	超时时间,单位ms
 *
 *	return
 *		成功返回 0
 *		失败返回 -1
 *
 * ******************************************************/
int orb_pcm_data_put(orb_pcm_t *pcm, char *data, int len, int timeout);

/********************************************************
 *	function	orb_pcm_play_stop
 *		pcm播放停止
 *
 *	@pcm	播放器句柄
 *
 *	return
 *		成功返回 0
 *		失败返回 -1
 * ******************************************************/
int orb_pcm_play_stop(orb_pcm_t *pcm);

#endif

