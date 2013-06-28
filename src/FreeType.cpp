#include "freetype.h"
#pragma warning(disable: 4244)
#pragma warning(disable: 4996)//unsafe functions
#pragma comment(lib, "freetype2412_x32.lib")

namespace freetype {

///This function gets the first power of 2 >= the
///int that we pass it.
inline int next_p2(int x)
{
	int rval=1;
	while (rval<x){
		rval<<=1;
	}
	return rval;
}

///Create a display list coresponding to the give character.
void make_dlist(FT_Face face, int ch, GLuint list_base, GLuint* tex_base)
{
	//The first thing we do is get FreeType to render our character
	//into a bitmap.  This actually requires a couple of FreeType commands:

	//Load the Glyph for our character.
	if (FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), FT_LOAD_DEFAULT))
		throw std::runtime_error("FT_Load_Glyph failed");

	//Move the face's glyph into a Glyph object.
	FT_Glyph glyph;
	if (FT_Get_Glyph( face->glyph, &glyph ))
		throw std::runtime_error("FT_Get_Glyph failed");

	//Convert the glyph to a bitmap.
	FT_Glyph_To_Bitmap( &glyph, ft_render_mode_normal, 0, 1 );
	FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;

	//This reference will make accessing the bitmap easier
	FT_Bitmap& bitmap=bitmap_glyph->bitmap;

	//Use our helper function to get the widths of
	//the bitmap data that we will need in order to create
	//our texture.
	const int width  = next_p2( bitmap.width );
	const int height = next_p2( bitmap.rows );

	//Allocate memory for the texture data.
	GLubyte* expanded_data = new GLubyte[ 2 * width * height];

	//Here we fill in the data for the expanded bitmap.
	//Notice that we are using two channel bitmap (one for
	//luminocity and one for alpha), but we assign
	//both luminocity and alpha to the value that we
	//find in the FreeType bitmap. 
	//We use the ?: operator so that value which we use
	//will be 0 if we are in the padding zone, and whatever
	//is the the Freetype bitmap otherwise.
	const float gamma = 1.0f;
	const float G = 1 / gamma;
	for(int j=0; j <height;j++) {
		for(int i=0; i < width; i++){
			GLubyte data = 
				(i>=bitmap.width || j>=bitmap.rows) ?
					0 : bitmap.buffer[i + bitmap.width*j];

			// gamma
			float d = data/255.0;
			data = (GLubyte)(powf(d, G) * 255.0);

			expanded_data[2*(i+j*width)]   = data;
			expanded_data[2*(i+j*width)+1] = data;
		}
	}


	//Now we just setup some texture paramaters.
	glBindTexture(GL_TEXTURE_2D, tex_base[ch]);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

	//Here we actually create the texture itself, notice
	//that we are using GL_LUMINANCE_ALPHA to indicate that
	//we are using 2 channel data.
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, width, height,
		0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, expanded_data );

	//With the texture created, we don't need to expanded data anymore
	delete [] expanded_data;

	//So now we can create the display list
	glNewList(list_base+ch, GL_COMPILE);

	glBindTexture(GL_TEXTURE_2D, tex_base[ch]);

	glPushMatrix();

	//first we need to move over a little so that
	//the character has the right amount of space
	//between it and the one before it.
	glTranslatef(bitmap_glyph->left,0,0);

	//Now we move down a little in the case that the
	//bitmap extends past the bottom of the line 
	//(this is only true for characters like 'g' or 'y'.
	glTranslatef(0,bitmap_glyph->top-bitmap.rows,0);

	//Now we need to account for the fact that many of
	//our textures are filled with empty padding space.
	//We figure what portion of the texture is used by 
	//the actual character and store that information in 
	//the x and y variables, then when we draw the
	//quad, we will only reference the parts of the texture
	//that we contain the character itself.
	float	x=(float)bitmap.width / (float)width,
			y=(float)bitmap.rows / (float)height;

	//Here we draw the texturemaped quads.
	//The bitmap that we got from FreeType was not 
	//oriented quite like we would like it to be,
	//so we need to link the texture to the quad
	//so that the result will be properly aligned.
	glBegin(GL_QUADS);
	glTexCoord2d(0,0); glVertex2f(0,bitmap.rows);
	glTexCoord2d(0,y); glVertex2f(0,0);
	glTexCoord2d(x,y); glVertex2f(bitmap.width,0);
	glTexCoord2d(x,0); glVertex2f(bitmap.width,bitmap.rows);
	glEnd();
	glPopMatrix();
	glTranslatef(face->glyph->advance.x >> 6 ,0,0);


	//increment the raster position as if we were a bitmap font.
	//(only needed if you want to calculate text length)
	//glBitmap(0,0,0,0,face->glyph->advance.x >> 6,0,NULL);

	//Finnish the display list
	glEndList();
}



void font_data::init(const std::string& fname, unsigned int h)
{
	//Create and initilize a freetype font library.
	FT_Library library;
	if (FT_Init_FreeType(&library)) 
		throw std::runtime_error("FT_Init_FreeType failed");

	//The object in which Freetype holds information on a given
	//font is called a "face".
	FT_Face face;

	//This is where we load in the font information from the file.
	//Of all the places where the code might die, this is the most likely,
	//as FT_New_Face will die if the font file does not exist or is somehow broken.
	{
		int res = FT_New_Face(library, fname.c_str(), 0, &face); 
		if (res!=0)
			throw std::runtime_error("FT_New_Face failed (there is probably a problem with your font file)");
	}

	//For some twisted reason, Freetype measures font size
	//in terms of 1/64ths of pixels.  Thus, to make a font
	//h pixels high, we need to request a size of h*64.
	//(h << 6 is just a prettier way of writting h*64)
	const int RESO = 96;
	FT_Set_Char_Size(face, h<<6, h<<6, RESO, RESO);


	this->h=h;

	//Allocate some memory to store the texture ids.
	textures = new GLuint[128];
	list_base=glGenLists(128);
	glGenTextures( 128, textures );

	//This is where we actually create each of the fonts display lists.
	for (int i=0; i<128; ++i)
	{
		make_dlist(face,i,list_base,textures);
	}

	//We don't need the face information now that the display
	//lists have been created, so we free the assosiated resources.
	FT_Done_Face(face);

	//Ditto for the library.
	FT_Done_FreeType(library);
}

void font_data::clean() {
	glDeleteLists(list_base,128);
	glDeleteTextures(128,textures);
	delete [] textures;
}

/// A fairly straight forward function that pushes
/// a projection matrix that will make object world 
/// coordinates identical to window coordinates.
static inline int pushScreenCoordinateMatrix()
{
	glPushAttrib(GL_TRANSFORM_BIT);
		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();

		glOrtho(viewport[0],viewport[2],viewport[1],viewport[3],-1,1);
	glPopAttrib();
	return viewport[3] - viewport[1];
}

/// Pops the projection matrix without changing the current
/// MatrixMode.
static inline void pop_projection_matrix()
{
	glPushAttrib(GL_TRANSFORM_BIT);
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
	glPopAttrib();
}

///Much like Nehe's glPrint function, but modified to work
///with freetype fonts.
void print(const font_data &ft_font, float x, float y, const char *fmt, ...) 
{
	// We want a coordinate system where things coresponding to window pixels.
	const int window_height = pushScreenCoordinateMatrix();					
	
	GLuint font=ft_font.list_base;
	float h=ft_font.h/.63f;						//We make the height about 1.5* that of
	
	char text[256];								// Holds Our String
	if (fmt == NULL) {									// If There's No Text
		*text=0;											// Do Nothing
	} else {
		va_list ap;										// Pointer To List Of Arguments
		va_start(ap, fmt);									// Parses The String For Variables
			vsprintf(text, fmt, ap);						// And Converts Symbols To Actual Numbers
		va_end(ap);											// Results Are Stored In Text
	}


	//Here is some code to split the text that we have been
	//given into a set of lines.  
	//This could be made much neater by using
	//a regular expression library such as the one avliable from
	//boost.org (I've only done it out by hand to avoid complicating
	//this tutorial with unnecessary library dependencies).
	const char *start_line=text;
	vector<string> lines;
	const char *c;
	for(c=text;*c;c++) {
		if(*c=='\n') {
			string line;
			for(const char *n=start_line;n<c;n++) line.append(1,*n);
			lines.push_back(line);
			start_line=c+1;
		}
	}
	if(start_line) {
		string line;
		for(const char *n=start_line;n<c;n++) line.append(1,*n);
		lines.push_back(line);
	}

	glPushAttrib(GL_LIST_BIT | GL_CURRENT_BIT  | GL_ENABLE_BIT | GL_TRANSFORM_BIT);	
	glMatrixMode(GL_MODELVIEW);
	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	

	glListBase(font);

	float modelview_matrix[16];	
	glGetFloatv(GL_MODELVIEW_MATRIX, modelview_matrix);

	for (unsigned int i=0;i<lines.size();i++)
	{
		glPushMatrix();
			glLoadIdentity();
			glTranslatef(x,window_height-1-y-h*i,0);
			glMultMatrixf(modelview_matrix);
			glCallLists(lines[i].length(), GL_UNSIGNED_BYTE, lines[i].c_str());
		glPopMatrix();
	}

	glPopAttrib();		

	pop_projection_matrix();
}

}
