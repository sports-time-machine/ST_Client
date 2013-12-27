#pragma once
#include "gl_funcs.h"

namespace stclient{

// �X�|�[�c�^�C���}�V���̒萔�Q
//============================
// 1unit = 2kinects + 1client
static const float
	GROUND_WIDTH       = (4.00f),              // ���j�b�g������̕� (4m)
	GROUND_LEFT        = (-GROUND_WIDTH/2),    // ���[�̃��[�g�� (-2m)
	GROUND_RIGHT       = (+GROUND_WIDTH/2),    // �E�[�̃��[�g�� (+2m)
	GROUND_HEIGHT      = (2.40f),              // ���j�b�g�̍��� (2.4m)
	GROUND_DEPTH       = (2.40f),              // ���j�b�g�̉��s�� (2.4m)
	GROUND_NEAR        = (0.00f),              // ���j�b�g�̈�Ԏ�O (+0m)
	GROUND_FAR         = (GROUND_DEPTH),       // ���j�b�g�̈�ԉ� (+2.4f)
	GROUND_XNEAR       = (0.00f),              // �^��ΏۂƂȂ��O��
	GROUND_XFAR        = (GROUND_DEPTH-0.10f), // �^��ΏۂƂȂ�ŉ��i�ǂ���10cm���͖�������j
	GROUND_TOP         = (GROUND_HEIGHT),      // ���j�b�g�̏�[
	GROUND_BOTTOM      = (0.00f),              // ���j�b�g�̉��[
	GROUND_XTOP        = (GROUND_HEIGHT),      // �^��ΏۂƂȂ�V��
	GROUND_XBOTTOM     = (0.00f);              // �^��ΏۂƂȂ鏰

// �����蔻��
//===========
// ���t�߂̓m�C�Y�������̂ŕG�䂮�炢����̂ݗL��
// ��ʂ̍��E�Ƀ}�[�W����݂��邱�ƂŁA��ʊO�ɂ�������ꍇ�ł����Ă��A
// �l���̒������Ƃ邱�Ƃ��ł���悤�ɂ���
// �l�̌����͂�������50cm�ł���̂ŁA50cm�}�[�W��������ΊT�˖��Ȃ��Ɣ��f����
static const float
	ATARI_MARGIN       = (0.50f),
	ATARI_LEFT         = (GROUND_LEFT  - ATARI_MARGIN),
	ATARI_RIGHT        = (GROUND_RIGHT + ATARI_MARGIN),
	ATARI_BOTTOM       = (0.50f),
	ATARI_TOP          = (GROUND_HEIGHT),
	ATARI_NEAR         = (0.00f),
	ATARI_FAR          = (GROUND_DEPTH-0.20f);  //�Ǎۂ̓m�C�Y�������̂ŏȂ�

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
};

}//namespace stclient
