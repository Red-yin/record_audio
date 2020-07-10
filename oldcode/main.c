#include <stdio.h>
#include "orb_cli.h"
int main(int argc, char **argv)
{
	
	
	orb_cli_init();
	orb_task_test_cli_init();
	pcm_play_test_init();
	
	while(1){
		sleep(10);
	}
}
