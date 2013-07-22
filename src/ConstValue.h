#pragma once
#include "gl_funcs.h"

namespace stclient{

static const float
	GROUND_WIDTH       = (4.00f),
	GROUND_LEFT        = (-GROUND_WIDTH/2),
	GROUND_RIGHT       = (+GROUND_WIDTH/2),
	GROUND_HEIGHT      = (2.40f),
	GROUND_DEPTH       = (2.40f),
	GROUND_NEAR        = (0.00f),
	GROUND_FAR         = (GROUND_DEPTH),
	GROUND_TOP         = (GROUND_HEIGHT),
	GROUND_BOTTOM      = (0.00f);

// ���t�߂̓m�C�Y�������̂ŕG�䂮�炢����̂ݗL��
// ��ʂ̍��E�Ƀ}�[�W����݂��邱�ƂŁA��ʊO�ɂ�������ꍇ�ł����Ă��A
// �l���̒������Ƃ邱�Ƃ��ł���悤�ɂ���
// �l�̌����͂�������50cm�ł���̂ŁA50cm�}�[�W��������ΊT�˖��Ȃ��Ɣ��f����
static const float
	ATARI_MARGIN       = (0.50f),
	ATARI_LEFT         = (GROUND_LEFT  - ATARI_MARGIN),
	ATARI_RIGHT        = (GROUND_RIGHT + ATARI_MARGIN),
	ATARI_BOTTOM       = (0.50f),
	ATARI_TOP          = (GROUND_HEIGHT);

static const float
	LOOKAT_EYE_DEPTH   = (4.0f),
	IDLE_IMAGE_Z       = (5.0f);

static mgl::glRGBA
	TIMEMACHINE_ORANGE(240,160,80, 160);

enum NonameEnum
{
	// ����ȊO�ɂ��Ă�Config.cpp��Config::Config()�ɋL�q���Ă���܂�
	INITIAL_WIN_SIZE_X   = 640,
	INITIAL_WIN_SIZE_Y   = 480,
	MOVIE_FPS            = 30,
	MIN_VOXEL_INC        = 16,
	MAX_VOXEL_INC        = 1024,
	ATARI_INC            = 20,
	MAX_PICT_NUMBER      = 10,        // PICT num�R�}���h�ő����s�N�`���̐�
	AUTO_CF_THRESHOULD   = 800,       // ������������臒l
};

}//namespace stclient
