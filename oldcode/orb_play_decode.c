

#include "libavcodec/avcodec.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"

#include "orb_play_task.h"
#include "orb_play_pcm.h"
#include "orb_play_debug.h"


static int interrupt_cb(void * ctx)
{
	return 0;
}

static const AVIOInterruptCB int_cb = { interrupt_cb, NULL };

static int audio_decode_swr_open(audio_decode_t *decode)
{
	struct SwrContext *swr_ctx = NULL;
	int64_t src_ch_layout = AV_CH_LAYOUT_STEREO; //初始化这样根据不同文件做调整
	int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO; //这里设定ok
	int ret;

	swr_ctx = swr_alloc();
	if (!swr_ctx) {
		AUDIO_PLAY_ERR("Could not allocate resampler context");
		return -1;
	}
	src_ch_layout = (decode->codeContext->channel_layout &&
			decode->codeContext->channels ==
			av_get_channel_layout_nb_channels(decode->codeContext->channel_layout)) ?
		decode->codeContext->channel_layout :
		av_get_default_channel_layout(decode->codeContext->channels);

	if(src_ch_layout < 0){
		swr_free(&swr_ctx);
		return -1;
	}

	switch(decode->channels){
		case 1:
			dst_ch_layout = AV_CH_LAYOUT_MONO;
			break;
		case 2:
			dst_ch_layout = AV_CH_LAYOUT_STEREO;
			break;
		default:
			dst_ch_layout = AV_CH_LAYOUT_STEREO;
			break;
	}
	av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate",       decode->samplerate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", decode->codeContext->sample_fmt, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate",       decode->samplerate, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", decode->format, 0);
	/* initialize the resampling context */
	if ((ret = swr_init(swr_ctx)) < 0) {
		AUDIO_PLAY_ERR("Failed to initialize the resampling context");
		return -1;
	}

	decode->dst_ch_layout = dst_ch_layout;
	decode->swr = swr_ctx;

	return 0;
}

audio_decode_t *audio_decode_init(char *url)
{
	if(url == NULL){
		return NULL;
	}
	audio_decode_t *decode = calloc(1,sizeof(audio_decode_t));

	AVFormatContext *pFormatCtx = NULL;

	static char av_reg_flag = 0;
	if(av_reg_flag == 0){
		av_register_all();
		av_reg_flag = 1;
	}

	if( 0 != avformat_network_init() ) {
		AUDIO_PLAY_DEBUG("avformat_network_init error\n");
		goto error;
	}

	pFormatCtx = avformat_alloc_context();
	pFormatCtx->interrupt_callback = int_cb;

	if( avformat_open_input(&pFormatCtx,url, NULL, NULL) != 0 ) {
		avformat_close_input(&pFormatCtx);
		usleep(1*1000);	
		AUDIO_PLAY_ERR("open file error, exit audio_decode thead*********!!!\n");
		goto error;
	}

	if( avformat_find_stream_info(pFormatCtx, NULL) < 0 ) {
		avformat_close_input(&pFormatCtx);
		usleep(1*1000);
		AUDIO_PLAY_ERR("av_find_stream_info fail, exit audio_decode thead!\n");
		goto error;
	}

	av_dump_format(pFormatCtx, 0,url, 0);

	AVCodecContext *codeContext = NULL;
	for(int i = 0; i < pFormatCtx->nb_streams; i++) {
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			codeContext = pFormatCtx->streams[i]->codec;
			decode->stream = pFormatCtx->streams[i];
			break;
		}
	}
	if(codeContext == NULL){
		avformat_close_input(&pFormatCtx);
		AUDIO_PLAY_ERR("codeContext %d failed.\r\n", pFormatCtx->nb_streams);
		goto error;
	}

	AVCodec *codec = avcodec_find_decoder(codeContext->codec_id);
	if (!codec) {

		AUDIO_PLAY_ERR("find codec for codec_id[%d] fail, file[%s]",pFormatCtx->audio_codec_id,url);
		avcodec_close(codeContext);
		avformat_close_input(&pFormatCtx);
		goto error;
	}

	if ( avcodec_open2(codeContext, codec, NULL) < 0 ) {

		avcodec_close(codeContext);
		avformat_close_input(&pFormatCtx);
		AUDIO_PLAY_ERR("avcodec_open2 failed.\n");
		goto error;
	}

	AVFrame *frame = av_frame_alloc();

	if( (AV_SAMPLE_FMT_S32 == codeContext->sample_fmt) ||
			(AV_SAMPLE_FMT_S32P == codeContext->sample_fmt) )
	{
		decode->sample_bits = 32;
		decode->format = AV_SAMPLE_FMT_S32;
	} else {
		decode->sample_bits = 16;
		decode->format = AV_SAMPLE_FMT_S16;
	}

	AUDIO_PLAY_DEBUG("codeContext->sample_fmt=%d %ld\n", codeContext->sample_fmt,codeContext->bit_rate);
	AUDIO_PLAY_DEBUG("codeContext->channel_layout=%ld\n", codeContext->channel_layout);
	AUDIO_PLAY_DEBUG("codeContext->channels=%d\n", codeContext->channels);
	AUDIO_PLAY_DEBUG("codeContext->sample_rate=%d\n", codeContext->sample_rate);

	decode->codec = codec;
	decode->codeContext = codeContext;
	decode->pFormatCtx = pFormatCtx;
	decode->frame = frame;
	decode->channels = codeContext->channels;
	decode->samplerate = codeContext->sample_rate;
	decode->ppacket = av_packet_alloc();
	decode->buf = malloc(1024*20);
	decode->stream_time_base = av_q2d(decode->stream->time_base);

	if(audio_decode_swr_open(decode)){
		avcodec_close(codeContext);
		avformat_close_input(&pFormatCtx);
		av_frame_free(&frame);
		AUDIO_PLAY_ERR( "audio decode swr open failed.\r\n");
		goto error;
	}

	return decode;
error:
	free(decode);
	return NULL;
}

int audio_decode_set_pos(audio_decode_t *decode, double pos)
{
	int64_t fpos = pos * AV_TIME_BASE;
	av_seek_frame(decode->pFormatCtx,-1,fpos,AVSEEK_FLAG_BACKWARD);
	return 0;
}

int audio_decode_uninit(audio_decode_t *decode)
{
	if(decode == NULL){
		return -1;
	}
	
	if(decode->codeContext){
		avcodec_close(decode->codeContext);
	}

	if(decode->pFormatCtx){
		avformat_close_input(&decode->pFormatCtx);
		avformat_free_context(decode->pFormatCtx);
	}

	if(decode->frame){
		av_frame_free(&decode->frame);
	}

	if(decode->swr){
		swr_free(&decode->swr);
	}

	if(decode->ppacket){
		av_packet_free(&decode->ppacket);
	}

	if(decode->buf){
		free(decode->buf);
	}

	free(decode);
	
	return 0;
}

static int AudioResampling(SwrContext * swr_ctx, AVCodecContext * audio_dec_ctx,AVFrame * pAudioDecodeFrame,
		int out_sample_fmt, int64_t dst_layout ,int out_sample_rate , uint8_t * out_buf)
{
	int data_size = 0;
	int ret = 0;
	int dst_nb_channels = 0;
	int dst_linesize = 0;
	int src_nb_samples = 0;
	int dst_nb_samples = 0;
	int max_dst_nb_samples = 0;
	int resampled_data_size = 0;

	src_nb_samples = pAudioDecodeFrame->nb_samples;
	if (src_nb_samples <= 0) {
		AUDIO_PLAY_ERR("src_nb_samples error!");
		return -1;
	}

	max_dst_nb_samples = dst_nb_samples =
		av_rescale_rnd(src_nb_samples, out_sample_rate, audio_dec_ctx->sample_rate, AV_ROUND_UP);


	if (max_dst_nb_samples <= 0) {
		AUDIO_PLAY_ERR("av_rescale_rnd error");
		return -1;
	}

	dst_nb_channels = av_get_channel_layout_nb_channels(dst_layout);

	dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, audio_dec_ctx->sample_rate) +
			src_nb_samples, out_sample_rate, audio_dec_ctx->sample_rate,AV_ROUND_UP);


	if (dst_nb_samples <= 0) {
		AUDIO_PLAY_ERR("av_rescale_rnd error!");
		return -1;
	}

	if (dst_nb_samples > max_dst_nb_samples) {
		max_dst_nb_samples = dst_nb_samples;
	}

	data_size = av_samples_get_buffer_size(NULL, audio_dec_ctx->channels,
			pAudioDecodeFrame->nb_samples,
			audio_dec_ctx->sample_fmt, 1);

	if (data_size <= 0) {
		AUDIO_PLAY_ERR("av_samples_get_buffer_size error!");
		return -1;
	}
	resampled_data_size = data_size;

	if (swr_ctx) {
		ret = swr_convert(swr_ctx, &out_buf, dst_nb_samples,
				(const uint8_t **)pAudioDecodeFrame->data, pAudioDecodeFrame->nb_samples);
		if (ret <= 0) {
			AUDIO_PLAY_ERR("swr_convert error");
			return -1;
		}

		resampled_data_size = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
				ret, out_sample_fmt, 1);
		if (resampled_data_size <= 0) {
			AUDIO_PLAY_ERR("av_samples_get_buffer_size error");
			return -1;
		}
	} else {
		AUDIO_PLAY_ERR("swr_ctx null error \n");
		return -1;
	}

	return resampled_data_size;
}

int audio_decode_get_data(orb_play_task_t *task, char **data)
{
	audio_decode_t *decode = task->decode;
	AVPacket *ppacket = decode->ppacket;
	int count = 0;
	while(count++ < 3){
		int ret = av_read_frame(decode->pFormatCtx,ppacket);
		if(ret < 0) {
			AUDIO_PLAY_ERR("adplayer exit value = %d!!!!\n",ret);
			return 0;
		}	

		if(ppacket->stream_index != decode->stream->index) {
			AUDIO_PLAY_ERR("error read frame once!");	
			continue;
		}

		if(ppacket->size <=0){
			AUDIO_PLAY_ERR("error! exit player!\n");
			av_free_packet(ppacket);
			return -1;
		}


		int len;
		int got_frame;
		len = avcodec_decode_audio4(decode->codeContext, decode->frame, &got_frame, ppacket);
		if (len < 0 ) {
			AUDIO_PLAY_ERR("Failed to decode a frame!");
			av_free_packet(ppacket);
			return -1;
		}

		if (got_frame) {
			if(decode->codeContext->channels > 2){
				decode->codeContext->channels = 2;
			}
			AVFrame *oframe = decode->frame;

			int data_size = av_samples_get_buffer_size(oframe->linesize,oframe->channels,oframe->nb_samples,AV_SAMPLE_FMT_FLTP,0);

			data_size = AudioResampling(decode->swr, decode->codeContext, decode->frame, 
					decode->format, decode->dst_ch_layout,
					decode->samplerate,(unsigned char*)decode->buf);

			av_free_packet(ppacket);
			if(data_size < 0){
				AUDIO_PLAY_ERR("Resampling fail");
				return -1;
			}
				
			task->cpos = ppacket->pts * decode->stream_time_base;
			//printf("%s %d cpos:%ld ctime:%f duration:%ld\r\n",__func__,__LINE__,task->cpos,time,ppacket->pts);
			*data = decode->buf;
			return data_size;

		}
	}

	return -1;
}

int audio_decode_total_time_get(audio_decode_t *decode)
{
	if(decode == NULL || decode->pFormatCtx == NULL){
		return -1;
	}

	int total = decode->pFormatCtx->duration / AV_TIME_BASE * 1000;
	return total;
}

#if 0
static void *audio_decode_thread(void *param)
{
	AVPacket *ppacket;
	int ret;

	orb_thread_detach(NULL);

	orb_play_task_t *player = param;

	audio_decode_t *decode = player->decode;

	if(decode == NULL){
		return NULL;
	}

	orb_sem_post(decode->sem);
	player->status = ORB_PLAY_TASK_STATUS_PLAYING;

	ppacket = av_packet_alloc();
	while(1) {
		if(player->opt == ORB_PLAY_TASK_OPT_STOP){
			player->status = ORB_PLAY_TASK_STATUS_STOPPING;
			break;
		}

		if(player->opt == ORB_PLAY_TASK_OPT_PAUSE){
			orb_sem_wait(decode->sem);
			continue;
		}

		ret = av_read_frame(decode->pFormatCtx,ppacket);
		if(ret < 0) {
			AUDIO_PLAY_ERR("adplayer exit value = %d!!!!\n",ret);
			break;
		}	

		if(ppacket->stream_index != decode->stream->index) {
			AUDIO_PLAY_ERR("error read frame once!");	
			continue;
		}

		if(ppacket->size <=0){
			AUDIO_PLAY_ERR("error! exit player!\n");
			av_free_packet(ppacket);
			break;
		}


		av_q2d(decode->stream->time_base);// codeContext->time_base
		player->cpos = ppacket->pos;
		int len;
		int got_frame;
		len = avcodec_decode_audio4(decode->codeContext, decode->frame, &got_frame, ppacket);
		if (len < 0 ) {
			AUDIO_PLAY_ERR("Failed to decode a frame!");
			av_free_packet(ppacket);
			player->status = ORB_PLAY_TASK_STATUS_DONE;
			break;
		}

		if (got_frame) {
			if(decode->codeContext->channels > 2){
				decode->codeContext->channels = 2;
			}
			AVFrame *oframe = decode->frame;

			int data_size = av_samples_get_buffer_size(oframe->linesize,oframe->channels,oframe->nb_samples,AV_SAMPLE_FMT_FLTP,0);
			char buf[4096*10];

			data_size = AudioResampling(decode->swr, decode->codeContext, decode->frame, 
					decode->format, decode->dst_ch_layout,
					decode->samplerate,(unsigned char*)buf);

			if(data_size < 0){
				AUDIO_PLAY_ERR("Resampling fail");
				av_free_packet(ppacket);
				break;
			}

			if(orb_pcm_data_put(player->pcm,buf,data_size,100)){
				AUDIO_PLAY_WARNING("data put fail");
			}
			player->cpos = ppacket->pos;
			
		}

		av_free_packet(ppacket);
	}

	av_packet_free(&ppacket);
}
int audio_decode_start(orb_play_task_t *task)
{
	if(task == NULL){
		return -1;
	}

	task->status = ORB_PLAY_TASK_STATUS_STARTING;
	task->decode = audio_decode_init(task->url);
	if(task->decode == NULL){
		AUDIO_PLAY_ERR("decode init fail");
		goto err;
	}

	orb_sem_init(&task->decode->sem,0);

	task->pcm = orb_pcm_play_start(task->decode->samplerate,task->decode->sample_bits,task->decode->channels,ORB_PCM_DEVICE_LOCAL);
	if(task->pcm == NULL){
		AUDIO_PLAY_ERR("Pcm play start fail");
		goto err;
	}

	struct thread_param param = {"audio decode",OS_PRIORITY_HIGH,1024};
	orb_thread_t pd;
	orb_thread_create(&pd,&param,audio_decode_thread,task);

	orb_sem_wait(task->decode->sem);

	return 0;

err:
	task->status = ORB_PLAY_TASK_STATUS_ERR;
	return -1;
}
#endif
