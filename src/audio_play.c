#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <stdio.h>

#define ALSA_RECORD_RATE 48000
#define ALSA_RECORD_CHANNEL 4
#define ALSA_RECORD_FORMAT SND_PCM_FORMAT_S16_LE 

//SND_PCM_FORMAT_U8,
static char *device = "default";            /* playback device */
//static char *device = "plughw:0,0";         /* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;    /* sample format */
static unsigned int rate = 48000;           /* stream rate */
static unsigned int channels = 1;           /* count of channels */
static unsigned int buffer_time = 500000;       /* ring buffer length in us */
static unsigned int period_time = 100000;       /* period time in us */
static double freq = 440;               /* sinusoidal wave frequency in Hz */
static int verbose = 0;                 /* verbose flag */
static int resample = 1;                /* enable alsa-lib resampling */
static int period_event = 0;                /* produce poll event after each period */


unsigned char buffer[32*1024];              /* some random data */

int set_hwparams(snd_pcm_t *handle,
            snd_pcm_hw_params_t *params,
            snd_pcm_access_t access)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;
    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0) {
        printf("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        printf("Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
    if (err < 0) {
        printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
    if (err < 0) {
        printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}
int main(void)
{
	int err;
	unsigned int i, j;
	snd_pcm_t *handle;
	snd_pcm_sframes_t frames;
	for (i = 0; i < sizeof(buffer); i++)
		buffer[i] = random() & 0xff;
	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
#if 1
	snd_pcm_hw_params_t *hwparams;
	snd_pcm_hw_params_alloca(&hwparams);
	set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
#else
	if ((err = snd_pcm_set_params(handle,
					SND_PCM_FORMAT_S16_LE, 
					SND_PCM_ACCESS_RW_INTERLEAVED,
					1,
					48000,
					1,
					50000)) < 0) {   /* 0.5sec */
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
#endif
	snd_pcm_uframes_t bufferSize, periodSize;
	if ((err = snd_pcm_get_params(handle, &bufferSize, &periodSize )) < 0) {
		printf("Pcm get params error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	printf("bufferSize: %d, periodSize: %d\n", bufferSize, periodSize);
	int writenum = 0;
	for (i = 0; i < sizeof(buffer); i++) {
		printf("i: %d, left: %d\n", i, sizeof(buffer)-i);
		writenum = i+periodSize < sizeof(buffer)? periodSize: sizeof(buffer)-i-6;
		printf("%d\n", writenum);
		frames = snd_pcm_writei(handle, &buffer[i], writenum);
		if (frames < 0)
			frames = snd_pcm_recover(handle, frames, 0);
		if (frames < 0) {
			printf("snd_pcm_writei failed: %s\n", snd_strerror(frames));
			break;
		}
		printf("==%d==\n", __LINE__);
		if (frames > 0 && frames < (long)writenum)
			printf("Short write (expected %li, wrote %li)\n", (long)sizeof(buffer), frames);
		i += frames;
	}
	/* pass the remaining samples, otherwise they're dropped in close */
	err = snd_pcm_drain(handle);
	if (err < 0)
		printf("snd_pcm_drain failed: %s\n", snd_strerror(err));
	snd_pcm_close(handle);
	return 0;
}
#if 0
int record()
{
	long loops;
	int rc;
	int size;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	int dir;
	snd_pcm_uframes_t frames;
	char *buffer;

	/* Open PCM device for recording (capture). */
	rc = snd_pcm_open(&handle, "default",
			SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		fprintf(stderr,	"unable to open pcm device: %s\n",snd_strerror(rc));
		exit(1);
	}

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(handle, params,
			SND_PCM_ACCESS_RW_INTERLEAVED);

	/* Signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(handle, params,
			SND_PCM_FORMAT_S16_LE);

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(handle, params, 2);

	/* 44100 bits/second sampling rate (CD quality) */
	val = 44100;
	snd_pcm_hw_params_set_rate_near(handle, params,
			&val, &dir);

	/* Set period size to 32 frames. */
	frames = 32;
	snd_pcm_hw_params_set_period_size_near(handle,
			params, &frames, &dir);

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		fprintf(stderr,
				"unable to set hw parameters: %s\n",
				snd_strerror(rc));
		exit(1);
	}

	/* Use a buffer large enough to hold one period */
	snd_pcm_hw_params_get_period_size(params,
			&frames, &dir);
	size = frames * 4; /* 2 bytes/sample, 2 channels */
	buffer = (char *) malloc(size);

	/* We want to loop for 5 seconds */
	snd_pcm_hw_params_get_period_time(params,
			&val, &dir);
	loops = 5000000 / val;

	while (loops > 0) {
		loops--;
		rc = snd_pcm_readi(handle, buffer, frames);
		if (rc == -EPIPE) {
			/* EPIPE means overrun */
			fprintf(stderr, "overrun occurred\n");
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			fprintf(stderr,
					"error from read: %s\n",
					snd_strerror(rc));
		} else if (rc != (int)frames) {
			fprintf(stderr, "short read, read %d frames\n", rc);
		}
		rc = write(1, buffer, size);
		if (rc != size)
			fprintf(stderr,
					"short write: wrote %d bytes\n", rc);
	}

	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(buffer);

	return 0;
}

#endif
