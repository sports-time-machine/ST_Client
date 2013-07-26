#include "StClient.h"

using namespace stclient;

inline float minmaxf(float x, float min, float max)
{
	return (x<min) ? min : (x>max) ? max : x;
}


MovingObjectImage::MovingObjectImage()
{
	MoiInitStruct zero = {};

	this->_id  = "(non-id-string)";
	this->_mis = zero;
}

void MovingObjectImage::init(const string& id, const MoiInitStruct& mis)
{
	this->_id  = id;
	this->_mis = mis;
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

void MovingObject::initRunParam()
{
	this->_stage = STAGE_RUN1;
	this->_speed = 0.0f;
	this->_distance_meter = 0.5f;
}

void MovingObject::init()
{
	this->_moi = nullptr;
	this->initRunParam();
}

void MovingObject::init(const MovingObjectImage& moi)
{
	this->_moi = &moi;
	this->initRunParam();
}

int MovingObject::convertRealFrameToVirtualFrame(int real_frame) const
{
	if (_moi->_frames.empty())
	{
		return 0;
	}

	int fr = real_frame % _moi->_frames.size();
	return _moi->_frames[fr];
}

const mi::Image& MovingObject::getFrameImage(int real_frame) const
{
	const int x = (int)(_speed * real_frame * getMoi().getAnimSpeed());
	const int frame = convertRealFrameToVirtualFrame(x);

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
	// Ignore
	if (this->_moi==nullptr)
		return;

	this->_distance_meter += this->_speed;

	bool accelarate = false;
#if 0
	printf("%f, %f, %d\n",
		_distance_meter,
		_speed,
		_stage);
#endif

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
		this->_speed *= this->_moi->getBreakRate();
		// �b��0.1m�����ɂȂ�����~�܂����Ɣ��f����
		// 30F�����肾��0.00333m
		if (this->_speed <= 0.1f/MOVIE_FPS)
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
		// ���������̕��͋C�ł�
		// 30m/s��108km/h
		const float TOP_SPEED       = this->_moi->getTopSpeed();
		const float ACCEL_SECOND    = this->_moi->getAccelSecond();
		const float ACCEL_FRAMES    = ACCEL_SECOND * MOVIE_FPS;
		const float TOP_SPEED_PER_F = TOP_SPEED / MOVIE_FPS;

		this->_speed = minmaxf(
			this->_speed + TOP_SPEED_PER_F/ACCEL_FRAMES,
			0.0f,
			TOP_SPEED_PER_F);
	}
}
