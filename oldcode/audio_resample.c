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

int audio_resample_init()
{
	//two methods for initialize struct SwrContext
#if 0
	struct SwrContext* swrCtx = swr_alloc();
	av_opt_set_channel_layout(swrCtx, "in_channel_layout", AV_CH_LAYOUT_5POINT1, 0);
	av_opt_set_channel_layou(swrCtx, "out_channel_layout", AV_CH_LAYOUT_STEREO, 0);
	av_opt_set_int(swrCtx, "in_sample_rate", 48000, 0);
	av_opt_set_int(swrCtx, "out_sample_rate", 44100, 0);
	av_opt_set_sample_fmt(swrCtx, "in_sample_fmt", AV_SAMPLE_FMT_FLPT, 0);
	av_opt_set_sample_fmt(swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
#else
	SwrContext *swrCtx = swr_alloc_set_opts(NULL,  // we're allocating a new context
                      AV_CH_LAYOUT_STEREO,  // out_ch_layout
                      AV_SAMPLE_FMT_S16,    // out_sample_fmt
                      44100,                // out_sample_rate
                      AV_CH_LAYOUT_5POINT1, // in_ch_layout
                      AV_SAMPLE_FMT_FLTP,   // in_sample_fmt
                      48000,                // in_sample_rate
                      0,                    // log_offset
                      NULL);                // log_ctx
#endif
	swr_init(swrCtx);

	return 0;
}

int audio_resample_test(SwrContext *swrCtx, 	const uint8_t **input, int in_samples)
{
	uint8_t *output;
/*The delay between input and output, can at any time be found by using swr_get_delay().
 * av_rescale_rnd() calculate out sample number according to the sample rate of input and output
 */
    int out_samples = av_rescale_rnd(swr_get_delay(swrCtx, 48000) +
                                     in_samples, 44100, 48000, AV_ROUND_UP);
    av_samples_alloc(&output, NULL, 2, out_samples,
                     AV_SAMPLE_FMT_S16, 0);
    out_samples = swr_convert(swrCtx, &output, out_samples,
                                     input, in_samples);
    handle_output(output, out_samples);
    av_freep(&output);
}
