#pragma once


struct Config
{
	int near_threshold;
	int far_threshold;
};
struct ClientConfig
{
	int client_number;
};

#ifdef THIS_IS_MAIN
#define EXTERN /*nop*/
#else
#define EXTERN extern
#endif


EXTERN Config       config;
EXTERN ClientConfig client_config;
