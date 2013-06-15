#pragma once


struct vec4
{
	float v[4];

	vec4()
	{
		v[0] = 0.0f;
		v[1] = 0.0f;
		v[2] = 0.0f;
		v[3] = 0.0f;
	}

	vec4(float a, float b, float c, float d)
	{
		v[0] = a;
		v[1] = b;
		v[2] = c;
		v[3] = d;
	}

	void set(float a, float b, float c, float d)
	{
		v[0] = a;
		v[1] = b;
		v[2] = c;
		v[3] = d;
	}

	float& operator[](int i)
	{
		return v[i];
	}

	float operator[](int i) const
	{
		return v[i];
	}

	void print()
	{
		printf("%f, %f, %f, %f\n",
			v[0],
			v[1],
			v[2],
			v[3]);
	}
};

struct mat4x4
{
	mat4x4()
	{
		v[0].set(1,0,0,0);
		v[1].set(0,1,0,0);
		v[2].set(0,0,1,0);
		v[3].set(0,0,0,1);
	}

	mat4x4(
		float a, float b, float c, float d,
		float e, float f, float g, float h,
		float i, float j, float k, float l,
		float m, float n, float o, float p)
	{
		v[0].set(a,e,i,m);
		v[1].set(b,f,j,n);
		v[2].set(c,g,k,o);
		v[3].set(d,h,l,p);
	}

	vec4 operator*(const vec4& s) const
	{
		vec4 d;
		for (int i=0; i<4; ++i)
		{
			d[i] = v[0][i]*s[0] + v[1][i]*s[1] + v[2][i]*s[2] + v[3][i]*s[3];
		}
		return d;
	}

	mat4x4 operator*(const mat4x4& s) const
	{
		mat4x4 d;
		
		for (int i=0; i<4; ++i)
		{
			for (int j=0; j<4; ++j)
			{
				float f = 0.0f;
				for (int k=0; k<4; ++k)
				{
					f += v[k][i] * s[j][k];
				}
				d[j][i] = f;
			}
		}

		return d;
	}

	vec4& operator[](int i)
	{
		return v[i];
	}

	const vec4& operator[](int i) const
	{
		return v[i];
	}

	vec4 v[4];
};
