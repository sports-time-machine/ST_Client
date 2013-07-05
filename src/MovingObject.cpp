#include "StClient.h"

using namespace stclient;

inline float minmaxf(float x, float min, float max)
{
	return (x<min) ? min : (x>max) ? max : x;
}



void MovingObjectImage::init(const string& id)
{
	this->_id = id;
}

void MovingObjectImage::addFrame(int length, const string& filepath)
{
	const int image_id = (int)_images.size();
	mi::Image& image = _images[image_id];
	image.createFromImageA(filepath);
	
	for (int i=0; i<length; ++i)
	{
		_frames.push_back(image_id);
	}
}





MovingObject::MovingObject()
{
	this->_moi = nullptr;
}

void MovingObject::init(const MovingObjectImage& moi)
{
	this->_moi            = &moi;
	this->_stage          = STAGE_RUN1;
	this->_break_rate     = 0.33f;
	this->_distance_meter = 0.5f;
	this->_speed          = 0.0f;
	this->_is_running     = false;
}

int MovingObject::convertRealFrameToVirtualFrame(int real_frame) const
{
	int fr = real_frame % _moi->_frames.size();

	return _moi->_frames[fr];
}

const mi::Image& MovingObject::getFrameImage(int real_frame) const
{
	const int frame = convertRealFrameToVirtualFrame((int)(_speed * real_frame * 45));

	auto itr = _moi->_images.find(frame);
	if (itr==_moi->_images.end())
	{
		static mi::Image null_image;
		return null_image;
	}
	
	return itr->second;
}

void MovingObject::resetDistance()
{
	this->_distance_meter = 0.0f;
	this->_speed = 0.0f;
}

float MovingObject::getDistance() const
{
	switch (this->_stage)
	{
	case STAGE_RUN1:
	case STAGE_BREAK:
		return _distance_meter;
	default:
		return _distance_meter + 28.0f;
	}
}

void MovingObject::updateDistance()
{
	this->_distance_meter += this->_speed;

	bool accelarate = false;
	printf("%f, %f, %d\n",
		_distance_meter,
		_speed,
		_stage);

	// �X�e�[�W�̏I������
	switch (this->_stage)
	{
	case STAGE_RUN1:
		accelarate = true;
		if (this->_distance_meter>=getTurnPosition())
		{
			// �܂�Ԃ��ʒu��ʂ�߂���Ǝ���
			this->_stage = STAGE_BREAK;
		}
		break;
	case STAGE_BREAK:
		this->_speed *= this->_break_rate;
		// �b��0.1m�����ɂȂ�����~�܂����Ɣ��f����
		// 30F�����肾��0.00333m
		if (this->_speed <= 0.1f/30)
		{
			// ��葬�x�����܂Ō��������玟��
			this->_stage = STAGE_RUN2;
			this->_speed = 0.0f;
			this->_distance_meter = 0.0f;
		}
		break;
	case STAGE_RUN2:
		accelarate = true;
		if (this->_distance_meter>=getTurnPosition())
		{
			// �S�[�������玟��
			this->_stage = STAGE_GOAL;
		}
		break;
	default:
		// �X�e�[�W�J�ڂȂ�
		break;
	}

	if (accelarate)
	{
		// �������܂�!!!
		// �g�b�v�X�s�[�h m/s
		// 
		// 2�b��35m/s�܂ŉ��������ꍇ�A
		// �g�b�v�X�s�[�h��5�b�����175m�ƂȂ�
		// ��������25m����ƁA7�b��200m�Ƃ����P�j�A�̋L�^���ɂȂ�
		//const float TOP_SPEED    = 35.0f;
		const float TOP_SPEED    = 25.0f;
		const float ACCEL_SECOND = 2.0f;
		const float ACCEL_FRAMES = ACCEL_SECOND * 60;
		
		const float TOP_SPEED_PER_F = TOP_SPEED / 30.0f;

		this->_speed = minmaxf(
			this->_speed + TOP_SPEED_PER_F/ACCEL_FRAMES,
			0.0f,
			TOP_SPEED_PER_F);
	}
}
