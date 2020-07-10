#include "libavcodec/avcodec.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include <unistd.h>


int audio_decode_init(const char *path)
{
	int ret = 0;
	av_register_all();
	ret = avformat_network_init();
	printf("[%s %d] ret: %d\n", __func__, __LINE__, ret);
	
	AVFormatContext *pFormatCtx = NULL;
	ret = avformat_open_input(&pFormatCtx, path, NULL, NULL);
	printf("[%s %d] ret: %d\n", __func__, __LINE__, ret);
	if(ret < 0){
		printf("%s is not exist\n", path);
		return -1;
	}
	pFormatCtx->debug = 1;
	ret = avformat_find_stream_info(pFormatCtx, NULL);
	printf("[%s %d] ret: %d, duration: %ld, bit rate: %ld, file: %s\n", __func__, __LINE__, ret, pFormatCtx->duration, pFormatCtx->bit_rate, pFormatCtx->filename);
	int audioStreamIndex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if(audioStreamIndex < 0){
		printf("[%s %d] audio stream index: %d\n", __func__, __LINE__, audioStreamIndex);
		return -1;
	}
	AVCodecContext *pCodecCtx = pFormatCtx->streams[audioStreamIndex]->codec;
	AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	ret = avcodec_open2(pCodecCtx, pCodec,NULL);
	printf("[%s %d] ret: %d\n", __func__, __LINE__, ret);
	av_dump_format(pFormatCtx,0,path,0);

	AVPacket *packet=(AVPacket *)av_malloc(sizeof(AVPacket));
	int len = 0;
	int got_frame = 0;// 1: packet decode to frame ok; 0: packet decode to frame failed;

	AVFrame *frame = av_frame_alloc();
	while(1){
		ret = av_read_frame(pFormatCtx, packet);
		printf("audio read ret: %d\n", ret);
		if(ret < 0)
			break;
		printf("packet index: %d, stream index: %d\n", packet->stream_index, audioStreamIndex);

		//packet decode to frame
		len = avcodec_decode_audio4(pCodecCtx, frame, &got_frame, packet);
		printf("current frame sample number: %d\n", frame->nb_samples);

		//sleep(1);
	}

	return ret;
}

int main(int argc, char **argv)
{
	if(argc < 2)
		return -1;
	printf("decode: %s\n", argv[1]);
	audio_decode_init(argv[1]);

	return 0;
}
