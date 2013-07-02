// UNUSED

#if 0
struct RgbaTex
{
	RGBA_raw* vram;
	uint tex;
	uint width;
	uint height;
	uint ram_width, ram_height;
	uint pitch;     // f(2^X, width)

	RgbaTex();
	virtual ~RgbaTex();

	void create(int w, int h);
};

RgbaTex::RgbaTex()
{
	vram = nullptr;
	tex = 0;
	width = 0;
	height = 0;
	ram_width = 0;
	ram_height = 0;
	pitch = 0;
}

RgbaTex::~RgbaTex()
{
	if (vram!=nullptr) delete[] vram;
}

void RgbaTex::create(int w, int h)
{
	const int TEXTURE_SIZE = 512;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	width      = w;
	height     = h;
	ram_width  = MIN_CHUNKS_SIZE(w, TEXTURE_SIZE);
	ram_height = MIN_CHUNKS_SIZE(h, TEXTURE_SIZE);
	pitch      = ram_width;
	vram = new RGBA_raw[ram_width * ram_height];

	fprintf(stderr, "RgbaTex: texture %d, %dx%d creted.\n",
		tex,
		w, h);
}

void DrawRgbaTex_Build(const RgbaTex& img)
{
	gl::Texture(true);
	glBindTexture(GL_TEXTURE_2D, img.tex);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		img.ram_width, img.ram_height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, img.vram);
}

void DrawRgbaTex_Draw(const RgbaTex& img, int dx, int dy, int dw, int dh)
{
	const int x1 = dx;
	const int y1 = dy;
	const int x2 = dx + dw;
	const int y2 = dy + dh;
	const float u1 = 0.0f;
	const float v1 = 0.0f;
	const float u2 = 1.0f * img.width  / img.ram_width;
	const float v2 = 1.0f * img.height / img.ram_height;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, img.tex);
	glColor4f(1,1,1,1);
	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1); glVertex2f(x1, y1);
	glTexCoord2f(u2, v1); glVertex2f(x2, y1);
	glTexCoord2f(u2, v2); glVertex2f(x2, y2);
	glTexCoord2f(u1, v2); glVertex2f(x1, y2);
	glEnd();
}

void DrawRgbaTex(const RgbaTex& img, int dx, int dy, int dw, int dh)
{
	DrawRgbaTex_Build(img);
	DrawRgbaTex_Draw(img, dx,dy,dw,dh);
}
#endif
