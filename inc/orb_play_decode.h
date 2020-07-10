#ifndef __ORB_PLAY_DECODE_H__
#define __ORB_PLAY_DECODE_H__

#include "libavcodec/avcodec.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"

#include "platform.h"

struct orb_play_task;

typedef struct audio_decode_info{
	char				type;

	AVFormatContext		*pFormatCtx;
	AVCodec				*codec;
	AVCodecContext		*codeContext;
	AVFrame				*frame;
	AVStream			*stream;
	struct SwrContext	*swr;
	AVPacket			*ppacket;
	char				*buf;

	double				stream_time_base;

	int sample_bits;
	int samplerate;
	int channels;
	int format;
	int64_t dst_ch_layout;

}audio_decode_t;

/********************************************************
 *	function	audio_decode_start
 *		初始化解码器并且启动解码线程
 *
 * ******************************************************/
int audio_decode_start(struct orb_play_task *task);

audio_decode_t *audio_decode_init(char *url);

int audio_decode_uninit(audio_decode_t *decode);

int audio_decode_get_data(struct orb_play_task *task, char **data);

int audio_decode_set_pos(audio_decode_t *decode, double pos);

/********************************************************
 *	function	audio_decode_total_time_get
 *		获取文件的总时长 单位ms
 *
 * ******************************************************/
int audio_decode_total_time_get(audio_decode_t *decode);
#endif
