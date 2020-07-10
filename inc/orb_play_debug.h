#ifndef __ORB_PLAY_DEBUG_H__
#define __ORB_PLAY_DEBUG_H__

#define AUDIO_PLAY_DEBUG(fmt,args...) printf("[audio_play DEBUG] \033[37m"fmt"\033[0m\n",##args);
#define AUDIO_PLAY_WARNING(fmt,args...) printf("[audio_play WARNING] \033[33m"fmt"\033[0m\n",##args);
#define AUDIO_PLAY_INFO(fmt,args...) printf("[audio_play INFO] \033[34m"fmt"\033[0m\n",##args);
#define AUDIO_PLAY_ERR(fmt,args...) printf("[audio_play ERR][%s %d] \033[31m"fmt"\033[0m\n",__FILE__,__LINE__,##args);

#endif
