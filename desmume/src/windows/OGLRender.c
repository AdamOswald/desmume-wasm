/*  
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl\gl.h>
#include <gl\glext.h>
#include <math.h>
#include <stdlib.h>
#include "..\debug.h"
#include "..\MMU.h"
#include "..\bits.h"
#include "..\matrix.h"
#include "OGLRender.h"


#define fix2float(v)    (((float)((s32)(v))) / (float)(1<<12))
#define fix10_2float(v) (((float)((s32)(v))) / (float)(1<<9))

static unsigned char  GPU_screen3D[256*256*4]={0};
// Accelerationg tables
static float*		float16table = NULL;
static float*		float10Table = NULL;
static float*		float10RelTable = NULL;
static float*		normalTable = NULL;
static int			numVertex = 0;

// Matrix stack handling
static MatrixStack	mtxStack[4];
static float		mtxCurrent [4][16];
static float		mtxTemporal[16];

// Indexes for matrix loading/multiplication
static char ML4x4ind = 0;
static char ML4x3_c = 0, ML4x3_l = 0;
static char MM4x4ind = 0;
static char MM4x3_c = 0, MM4x3_l = 0;
static char MM3x3_c = 0, MM3x3_l = 0;

// Data for vertex submission
static float	coord[3] = {0.0, 0.0, 0.0};
static char		coordind = 0;

// Data for basic transforms
static float	trans[3] = {0.0, 0.0, 0.0};
static char		transind = 0;

static float	scale[3] = {0.0, 0.0, 0.0};
static char		scaleind = 0;

static const unsigned short polyType[4] = {GL_TRIANGLES, GL_QUADS, GL_TRIANGLE_STRIP, GL_QUAD_STRIP};
static const unsigned short map3d_cull[4] = {GL_FRONT_AND_BACK, GL_FRONT, GL_BACK, 0};
static const int texEnv[4] = { GL_MODULATE, GL_DECAL, GL_MODULATE, GL_MODULATE };
static const int depthFunc[2] = { GL_LESS, GL_EQUAL };

static unsigned short matrixMode[2] = {GL_PROJECTION, GL_MODELVIEW};
static short mode = 0;

static int colorAlpha=0;
static unsigned int depthFuncMode=0;
static unsigned int lightMask=0;
static unsigned int envMode=0;
static unsigned int cullingMask=0;
static unsigned int oglTextureID=0;
static unsigned char texMAP[1024*2048*4], texMAP2[2048*2048*4];
static float invTexWidth  = 1.f;
static float invTexHeight = 1.f;
static float fogColor[4] = {0.f};
static float fogOffset = 0.f;

static unsigned long clCmd = 0;
static unsigned long clInd = 0;
static unsigned long clInd2 = 0;
static int alphaDepthWrite = 0;
static int colorRGB[4] = { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};
static int texCoordinateTransform = 0;
static int t=0, s=0;
static char beginCalled = 0;
static unsigned int vtxFormat;
static unsigned int textureFormat=0, texturePalette=0;
static unsigned int lastTextureFormat=0, lastTexturePalette=0;

// This was used to disable/enable certain stuff, as some is partially broken
//extern char disableBlending, disableLighting, wireframeMode, disableTexturing;
extern u32 nbframe;
extern HDC  hdc;

char NDS_glInit(void)
{
	HDC						oglDC = NULL;
	HDC *					tmpDC = NULL;
	HBITMAP 				oglBMP = NULL;
	HGLRC					hRC = NULL;
	int						pixelFormat, i;
	PIXELFORMATDESCRIPTOR	pfd;
	HBITMAP					hBmp;
	BITMAP					bmpInfo;

	tmpDC = &hdc;
	oglDC = CreateCompatibleDC(hdc);
	oglBMP = CreateCompatibleBitmap(hdc, 256, 192);
	SelectObject(oglDC, oglBMP);

	hBmp=(HBITMAP)GetCurrentObject(oglDC,OBJ_BITMAP);
	GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmpInfo);

	memset(&pfd,0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags =  PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL;// | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = (BYTE)bmpInfo.bmBitsPixel;
	pfd.cDepthBits = 32;
	pfd.iLayerType = PFD_MAIN_PLANE ;

	pixelFormat = ChoosePixelFormat(oglDC, &pfd);
	if (pixelFormat == 0) 
		return 0;

	if(!SetPixelFormat(oglDC, pixelFormat, &pfd)) 
		return 0;

	hRC = wglCreateContext(oglDC);
	if (!hRC) 
		return 0;

	if(!wglMakeCurrent(oglDC, hRC))
		return 0;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glColor3f (1.f, 1.f, 1.f);

	glGenTextures (1, &oglTextureID);

	glViewport(0, 0, 256, 192);

	if (glGetError() != GL_NO_ERROR)
		return 0;

	float16table = (float*) malloc (sizeof(float)*65536);
	for (i = 0; i < 65536; i++)
	{
		float16table[i] = fix2float((signed short)i);
	}

	float10RelTable = (float*) malloc (sizeof(float)*1024);
	for (i = 0; i < 1024; i++)
	{
		float10RelTable[i] = ((signed short)(i<<6)) / (float)(1<<18);
	}

	float10Table = (float*) malloc (sizeof(float)*1024);
	for (i = 0; i < 1024; i++)
	{
		float10Table[i] = ((signed short)(i<<6)) / (float)(1<<12);
	}

	normalTable = (float*) malloc (sizeof(float)*1024);
	for (i = 0; i < 1024; i++)
	{
		normalTable[i] = ((signed short)(i<<6)) / (float)(1<<16);
	}

	MatrixStackSetMaxSize(&mtxStack[0], 1);		// Projection stack
	MatrixStackSetMaxSize(&mtxStack[1], 31);	// Coordinate stack
	MatrixStackSetMaxSize(&mtxStack[2], 31);	// Directional stack
	MatrixStackSetMaxSize(&mtxStack[3], 1);		// Texture stack

	MatrixInit (mtxCurrent[0]);
	MatrixInit (mtxCurrent[1]);
	MatrixInit (mtxCurrent[2]);
	MatrixInit (mtxCurrent[3]);
	MatrixInit (mtxTemporal);

	return 1;
}

void NDS_glViewPort(unsigned long v)
{
	glViewport( (v&0xFF), ((v>>8)&0xFF), ((v>>16)&0xFF), (v>>24));
}

void NDS_glClearColor(unsigned long v)
{
	glClearColor(	((float)(v&0x1F))/31.0f,
					((float)((v>>5)&0x1F))/31.0f, 
					((float)((v>>10)&0x1F))/31.0f, 
					((float)((v>>16)&0x1F))/31.0f);
}

void NDS_glFogColor(unsigned long v)
{
	fogColor[0] = ((float)((v    )&0x1F))/31.0f;
	fogColor[1] = ((float)((v>> 5)&0x1F))/31.0f;
	fogColor[2] = ((float)((v>>10)&0x1F))/31.0f;
	fogColor[3] = ((float)((v>>16)&0x1F))/31.0f;
}

void NDS_glFogOffset (unsigned long v)
{
	fogOffset = (float)(v&0xffff);
}

void NDS_glClDepth()
{
	glClear(GL_DEPTH_BUFFER_BIT);
}

void NDS_glClearDepth(unsigned long v)
{
	u32 depth24b;

	v		&= 0x7FFFF;
	depth24b = (v*0x200)+((v+1)/0x8000);

	glClearDepth(depth24b / ((float)(1<<24)));
}

void NDS_glMatrixMode(unsigned long v)
{
	if (beginCalled)
	{
		glEnd();
	}

	/*
	if (matmode[mode] == GL_TEXTURE)
	{
		glGetFloatv(GL_TEXTURE_MATRIX, textureMatrix);

		glLoadIdentity ();
	}
	*/

	mode = (short)(v&3);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

void SetMatrix (void)
{
	if (mode < 2)
	{
		glEnd();
		glMatrixMode (matrixMode[mode]);
		glLoadMatrixf(mtxCurrent[mode]);
		glBegin (vtxFormat);
	}
	else if (mode == 2)
	{
		glEnd();
		glMatrixMode (matrixMode[1]);
		glLoadMatrixf(mtxCurrent[1]);
		glBegin (vtxFormat);
	}
}

void NDS_glLoadIdentity (void)
{
	MatrixIdentity (mtxCurrent[mode]);

	if (mode == 2)
		MatrixIdentity (mtxCurrent[1]);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glLoadMatrix4x4(signed long v)
{
	mtxCurrent[mode][ML4x4ind] = fix2float(v);

	++ML4x4ind;
	if(ML4x4ind<16) return;
	//MATRIX_LOAD4x4[15]*=2;  // petite triche pour faire fonctionner Meteos

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

	ML4x4ind = 0;

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glLoadMatrix4x3(signed long v)
{
	mtxCurrent[mode][(ML4x3_l<<2)+ML4x3_c] = fix2float(v);

	++ML4x3_c;
	if(ML4x3_c<3) return;

	ML4x3_c = 0;
	++ML4x3_l;
	if(ML4x3_l<4) return;
	ML4x3_l = 0;

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glStoreMatrix(unsigned long v)
{
	MatrixStackLoadMatrix (&mtxStack[mode], v&31, mtxCurrent[mode]);
}

void NDS_glRestoreMatrix(unsigned long v)
{
	//LOG ("glRestoreMatrix - param:%d - mode:%d", v, mode);

	MatrixCopy (mtxCurrent[mode], MatrixStackGetPos(&mtxStack[mode], v&31));
/*
	if (mode == 2)
		mtxCurrent[1].Set (mtxCurrent[2].Get());

	if (beginCalled)
	{
		SetMatrix ();
	}
*/
}

void NDS_glPushMatrix (void)
{
	MatrixStackPushMatrix (&mtxStack[mode], mtxCurrent[mode]);
}

void NDS_glPopMatrix(signed long i)
{
	MatrixCopy (mtxCurrent[mode], MatrixStackPopMatrix (&mtxStack[mode], i));

	if (mode == 2)
		MatrixCopy (mtxCurrent[1], mtxCurrent[2]);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glTranslate(signed long v)
{
	trans[transind] = fix2float(v);

	++transind;

	if(transind<3)
		return;

	transind = 0;

	MatrixTranslate (mtxCurrent[mode], trans);

	if (mode == 2)
		MatrixTranslate (mtxCurrent[1], trans);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glScale(signed long v)
{
	scale[scaleind] = fix2float(v); //(((signed long)v>>16)) / (float)(1<<9);

	++scaleind;

	if(scaleind<3) 
		return;

	scaleind = 0;

	MatrixScale (mtxCurrent[mode], scale);

	if (mode == 2)
		MatrixScale (mtxCurrent[1], scale);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glMultMatrix3x3(signed long v)
{
	mtxTemporal[(MM3x3_l<<2)+MM3x3_c] = fix2float(v);

	++MM3x3_c;
	if(MM3x3_c<3) return;

	MM3x3_c = 0;
	++MM3x3_l;
	if(MM3x3_l<3) return;
	MM3x3_l = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);

	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	MatrixIdentity (mtxTemporal);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glMultMatrix4x3(signed long v)
{
	mtxTemporal[(MM4x3_l<<2)+MM4x3_c] = fix2float(v);

	++MM4x3_c;
	if(MM4x3_c<3) return;

	MM4x3_c = 0;
	++MM4x3_l;
	if(MM4x3_l<4) return;
	MM4x3_l = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);
	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	MatrixIdentity (mtxTemporal);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

void NDS_glMultMatrix4x4(signed long v)
{
	mtxTemporal[MM4x4ind] = fix2float(v);

	++MM4x4ind;
	if(MM4x4ind<16) return;

	MM4x4ind = 0;

	MatrixMultiply (mtxCurrent[mode], mtxTemporal);
	if (mode == 2)
		MatrixMultiply (mtxCurrent[1], mtxTemporal);

	MatrixIdentity (mtxTemporal);

	if (beginCalled)
	{
		SetMatrix ();
	}
}

static __inline void SetupTexture (unsigned int format, unsigned int palette)
{
	if(format == 0)// || disableTexturing) 
		return;
	else
	{
		unsigned short *pal = NULL;
		unsigned int sizeX = (1<<(((format>>20)&0x7)+3));
		unsigned int sizeY = (1<<(((format>>23)&0x7)+3));
		unsigned int mode = (unsigned short)((format>>26)&0x7);
		unsigned char * adr = (unsigned char *)(ARM9Mem.ARM9_LCD + ((format&0xFFFF)<<3));
		unsigned short param = (unsigned short)((format>>30)&0xF);
		unsigned short param2 = (unsigned short)((format>>16)&0xF);
		unsigned int imageSize = sizeX*sizeY;
		unsigned int paletteSize = 0;
		unsigned int palZeroTransparent = (1-((format>>29)&1))*255; // shash: CONVERT THIS TO A TABLE :)
		unsigned int x=0, y=0;

		if (mode == 0)
			glDisable (GL_TEXTURE_2D);
		else
			glEnable (GL_TEXTURE_2D);

		switch(mode)
		{
			case 1:
			{
				paletteSize = 256;
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				break;
			}
			case 2:
			{
				paletteSize = 4;
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<3));
				imageSize >>= 2;
				break;
			}
			case 3:
			{
				paletteSize = 16;
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				imageSize >>= 1;
				break;
			}
			case 4:
			{
				paletteSize = 256;
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				break;
			}
			case 5:
			{
				paletteSize = 0;
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				break;
			}
			case 6:
			{
				paletteSize = 256;
				pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
				break;
			}
			case 7:
			{
				paletteSize = 0;
				break;
			}
		}

		//if (!textureCache.IsCached((u8*)pal, paletteSize, adr, imageSize, mode, palZeroTransparent))
		{
			unsigned char * dst = texMAP;// + sizeX*sizeY*3;

			switch(mode)
			{
				case 1:
				{
					for(x = 0; x < imageSize; x++, dst += 4)
					{
						unsigned short c = pal[adr[x]&31], alpha = (adr[x]>>5);
						dst[0] = (unsigned char)((c & 0x1F)<<3);
						dst[1] = (unsigned char)((c & 0x3E0)>>2);
						dst[2] = (unsigned char)((c & 0x7C00)>>7);
						dst[3] = ((alpha<<2)+(alpha>>1))<<3;
					}

					break;
				}
				case 2:
				{
					for(x = 0; x < imageSize; ++x)
					{
						unsigned short c = pal[(adr[x])&0x3];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = ((adr[x]&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;

						c = pal[((adr[x])>>2)&0x3];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (((adr[x]>>2)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;

						c = pal[((adr[x])>>4)&0x3];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (((adr[x]>>4)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;

						c = pal[(adr[x])>>6];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (((adr[x]>>6)&3) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;
					}
				}
				break;
				case 3:
				{
					for(x = 0; x < imageSize; x++)
					{
						unsigned short c = pal[adr[x]&0xF];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (((adr[x])&0xF) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;

						c = pal[((adr[x])>>4)];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (((adr[x]>>4)&0xF) == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;
					}
				}
				break;

				case 4:
				{
					for(x = 0; x < imageSize; ++x)
					{
						unsigned short c = pal[adr[x]];
						dst[0] = (unsigned char)((c & 0x1F)<<3);
						dst[1] = (unsigned char)((c & 0x3E0)>>2);
						dst[2] = (unsigned char)((c & 0x7C00)>>7);
						dst[3] = (adr[x] == 0) ? palZeroTransparent : 255;//(c>>15)*255;
						dst += 4;
					}
				}
				break;

				case 5:
				{
					// UNOPTIMIZED
					unsigned short * pal = (unsigned short *)(ARM9Mem.texPalSlot[0] + (texturePalette<<4));
					unsigned short * slot1 = (unsigned short*)((unsigned char *)(ARM9Mem.ARM9_LCD + ((format&0xFFFF)<<3)/2 + 0x20000));
					unsigned int * map = ((unsigned int *)adr), i = 0;
					unsigned int * dst = (unsigned int *)texMAP;

					for (y = 0; y < (sizeY/4); y ++)
					{
						for (x = 0; x < (sizeX/4); x ++, i++)
						{
							u32 currBlock	= map[i], sy;
							u16 pal1		= slot1[i];
							u16 pal1offset	= (pal1 & 0x3FFF)<<1;
							u8  mode		= pal1>>14;

							for (sy = 0; sy < 4; sy++)
							{
								// Texture offset
								u32 xAbs = (x<<2);
								u32 yAbs = ((y<<2) + sy);
								u32 currentPos = xAbs + yAbs*sizeX;

								// Palette							
								u8  currRow		= (u8)((currBlock >> (sy*8)) & 0xFF);
		#define RGB16TO32(col,alpha) (((alpha)<<24) | ((((col) & 0x7C00)>>7)<<16) | ((((col) & 0x3E0)>>2)<<8) | (((col) & 0x1F)<<3))
		#define RGB32(r,g,b,a) (((a)<<24) | ((r)<<16) | ((g)<<8) | (b))

								switch (mode)
								{
									case 0:
									{
										u16 col0 = pal[pal1offset+((currRow>>0)&3)];
										u16 col1 = pal[pal1offset+((currRow>>2)&3)];
										u16 col2 = pal[pal1offset+((currRow>>4)&3)];
										u16 col3 = pal[pal1offset+((currRow>>6)&3)];

										dst[currentPos+0] = RGB16TO32(col0, 255);
										dst[currentPos+1] = RGB16TO32(col1, 255);
										dst[currentPos+2] = RGB16TO32(col2, 255);
										dst[currentPos+3] = RGB16TO32(col3, 128);

										break;
									}
									case 1:
									{
										u16 col0 = pal[pal1offset+((currRow>>0)&3)];
										u16 col1 = pal[pal1offset+((currRow>>2)&3)];
										u16 col3 = pal[pal1offset+((currRow>>6)&3)];

										u32 col0R = ((col0 & 0x7C00)>>7);
										u32 col0G = ((col0 & 0x3E0 )>>2);
										u32 col0B = ((col0 & 0x1F  )<<3);
										u32 col0A = ((pal1offset+((currRow>>0)&1)) == 0) ? palZeroTransparent : 255;

										u32 col1R = ((col1 & 0x7C00)>>7);
										u32 col1G = ((col1 & 0x3E0 )>>2);
										u32 col1B = ((col1 & 0x1F  )<<3);
										u32	col1A = ((pal1offset+((currRow>>2)&1)) == 0) ? palZeroTransparent : 255;

										dst[currentPos+0] = RGB16TO32(col0, 255);
										dst[currentPos+1] = RGB16TO32(col1, 255);
										dst[currentPos+2] = RGB32(	(col0R+col1R)>>1, 
																	(col0G+col1G)>>1, 
																	(col0B+col1B)>>1, 
																	(col0A+col1A)>>1);
										dst[currentPos+3] = RGB16TO32(col3, 128);
										
										break;
									}
									case 2:
									{
										u16 col0 = pal[pal1offset+((currRow>>0)&3)];
										u16 col1 = pal[pal1offset+((currRow>>2)&3)];
										u16 col2 = pal[pal1offset+((currRow>>4)&3)];
										u16 col3 = pal[pal1offset+((currRow>>6)&3)];

										dst[currentPos+0] = RGB16TO32(col0, 255);
										dst[currentPos+1] = RGB16TO32(col1, 255);
										dst[currentPos+2] = RGB16TO32(col2, 255);
										dst[currentPos+3] = RGB16TO32(col3, 255);

										break;
									}
									case 3:
									{					
										u16 col0 = pal[pal1offset+((currRow>>0)&3)];
										u16 col1 = pal[pal1offset+((currRow>>2)&3)];

										u32 col0R = ((col0 & 0x7C00)>>7);
										u32 col0G = ((col0 & 0x3E0 )>>2);
										u32 col0B = ((col0 & 0x1F  )<<3);
										u32 col0A = ((pal1offset+((currRow>>0)&1)) == 0) ? palZeroTransparent : 255;

										u32 col1R = ((col1 & 0x7C00)>>7);
										u32 col1G = ((col1 & 0x3E0 )>>2);
										u32 col1B = ((col1 & 0x1F  )<<3);
										u32	col1A = ((pal1offset+((currRow>>2)&1)) == 0) ? palZeroTransparent : 255;

										dst[currentPos+0] = RGB32(col0R, col0G, col0B, col0A);
										dst[currentPos+1] = RGB32(col1R, col1G, col1B, col1A);
										dst[currentPos+2] = RGB32(	(col0R*5+col1R*3)>>3, 
																	(col0G*5+col1G*3)>>3, 
																	(col0B*5+col1B*3)>>3, 
																	(col0A*5+col1A*3)>>3);

										dst[currentPos+3] = RGB32(	(col0R*3+col1R*5)>>3, 
																	(col0G*3+col1G*5)>>3, 
																	(col0B*3+col1B*5)>>3, 
																	(col0A*3+col1A*5)>>3);
										break;
									}
								}
							}
						}
					}
					
					break;
				}
				case 6:
				{
					for(x = 0; x < imageSize; x++)
					{
						unsigned short c = pal[adr[x]&7];
						dst[0] = (unsigned char)((c & 0x1F)<<3);
						dst[1] = (unsigned char)((c & 0x3E0)>>2);
						dst[2] = (unsigned char)((c & 0x7C00)>>7);
						dst[3] = (adr[x]&0xF8);
						dst += 4;
					}
					break;
				}
				case 7:
				{
					unsigned short * map = ((unsigned short *)adr);
					for(x = 0; x < imageSize; ++x)
					{
						unsigned short c = map[x];
						dst[0] = ((c & 0x1F)<<3);
						dst[1] = ((c & 0x3E0)>>2);
						dst[2] = ((c & 0x7C00)>>7);
						dst[3] = (c>>15)*255;
						dst += 4;
					}
				}
				break;
			}

			glBindTexture(GL_TEXTURE_2D, oglTextureID);
			glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, sizeX, sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, texMAP);
			//textureCache.SetTexture ( texMAP, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode, palZeroTransparent);

	/*
			switch ((format>>18)&3)
			{
				case 0:
				{
					textureCache.SetTexture ( texMAP, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
					break;
				}

				case 1:
				{
					u32 *src = (u32*)texMAP, *dst = (u32*)texMAP2;

					for (int y = 0; y < sizeY; y++)
					{
						for (int x = 0; x < sizeX; x++)
						{
							dst[y*sizeX*2 + x] = dst[y*sizeX*2 + (sizeX*2-x-1)] = src[y*sizeX + x];
						}
					}

					sizeX <<= 1;
					textureCache.SetTexture ( texMAP2, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
					break;
				}

				case 2:
				{
					u32 *src = (u32*)texMAP;

					for (int y = 0; y < sizeY; y++)
					{
						memcpy (&src[(sizeY*2-y-1)*sizeX], &src[y*sizeX], sizeX*4);
					}

					sizeY <<= 1;
					textureCache.SetTexture ( texMAP, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
					break;
				}

				case 3:
				{
					u32 *src = (u32*)texMAP, *dst = (u32*)texMAP2;

					for (int y = 0; y < sizeY; y++)
					{
						for (int x = 0; x < sizeX; x++)
						{
							dst[y*sizeX*2 + x] = dst[y*sizeX*2 + (sizeX*2-x-1)] = src[y*sizeX + x];
						}
					}

					sizeX <<= 1;

					for (int y = 0; y < sizeY; y++)
					{
						memcpy (&dst[(sizeY*2-y-1)*sizeX], &dst[y*sizeX], sizeX*4);
					}

					sizeY <<= 1;
					textureCache.SetTexture ( texMAP2, sizeX, sizeY, (u8*)pal, paletteSize, adr, imageSize, mode);
					break;
				}
			}
	*/
		}

		invTexWidth  = 1.f/((float)sizeX*(1<<4));//+ 1;
		invTexHeight = 1.f/((float)sizeY*(1<<4));//+ 1;
		
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

		// S Coordinate options
		if (!BIT16(format))
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
		else
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);

		// T Coordinate options
		if (!BIT17(format))
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
		else
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

		//flipS = BIT18(format);
		//flipT = BIT19(format);

		texCoordinateTransform = (format>>30);
	}
}

void NDS_glBegin(unsigned long v)
{
	if (beginCalled)
	{
		glEnd();
	}

	// Light enable/disable
	if (lightMask)// && !disableLighting)
	{
		if (lightMask&1)	glEnable (GL_LIGHT0);
		else				glDisable(GL_LIGHT0);

		if (lightMask&2)	glEnable (GL_LIGHT1);
		else				glDisable(GL_LIGHT1);

		if (lightMask&4)	glEnable (GL_LIGHT2);
		else				glDisable(GL_LIGHT2);

		if (lightMask&8)	glEnable (GL_LIGHT3);
		else				glDisable(GL_LIGHT3);

		glEnable (GL_LIGHTING);
		//glEnable (GL_COLOR_MATERIAL);
	}
	else
	{
		glDisable (GL_LIGHTING);
	}

	// texture environment
	glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, envMode);

	glDepthFunc (depthFuncMode);

	// Cull face
	if (cullingMask != 0xC0)
	{
		glEnable(GL_CULL_FACE);
		glCullFace(map3d_cull[cullingMask>>6]);
	} 
	else
	{
		glDisable(GL_CULL_FACE);
	}

	// Alpha value, actually not well handled, 0 should be wireframe
	if (colorAlpha == 0.0f)
	{
		//glPolygonMode (GL_FRONT, GL_LINE);
		//glPolygonMode (GL_BACK, GL_LINE);
	}
	else
	{
		//glPolygonMode (GL_FRONT, GL_FILL);
		//glPolygonMode (GL_BACK, GL_FILL);
		/*
		colorRGB[0] = 1.f;
		colorRGB[1] = 1.f;
		colorRGB[2] = 1.f;
		*/
		colorRGB[3] = colorAlpha;
		glColor4iv (colorRGB);
	}

	if (textureFormat  != lastTextureFormat ||
		texturePalette != lastTexturePalette)
	{
		SetupTexture (textureFormat, texturePalette);

		lastTextureFormat = textureFormat;
		lastTexturePalette = texturePalette;
	}

	glMatrixMode (GL_TEXTURE);
	glLoadIdentity ();
	glScaled (invTexWidth, invTexHeight, 1.f);

	glMatrixMode (GL_PROJECTION);
	glLoadMatrixf(mtxCurrent[0]);

	// COMPTE, AMPARO, QUE SI POSO AQUI UN 2, es trenca tot
	glMatrixMode (GL_MODELVIEW);
	glLoadMatrixf(mtxCurrent[1]);

	beginCalled = 1;
	vtxFormat = polyType[v&3];
	glBegin(vtxFormat);
}

void NDS_glEnd (void)
{
	if (beginCalled)
	{
		glEnd();
		beginCalled = 0;
	}
}

void NDS_glColor3b(unsigned long v)
{
	colorRGB[0] =  (v&0x1F) << 26;
	colorRGB[1] = ((v>>5)&0x1F) << 26;
	colorRGB[2] = ((v>>10)&0x1F) << 26;
	glColor4iv (colorRGB);
}

#define SetTextureCoordinate(s,t) glTexCoord2i (s,t)

static __inline void  SetVertex()
{
	if (texCoordinateTransform == 3) 
	{ 
		float *textureMatrix = mtxCurrent[3]; 
		int s2 =	(int)((	coord[0]*textureMatrix[0] +  
							coord[1]*textureMatrix[4] +  
							coord[2]*textureMatrix[8]) + s);
		int t2 =	(int)((	coord[0]*textureMatrix[1] +  
							coord[1]*textureMatrix[5] +  
							coord[2]*textureMatrix[9]) + t); 

		SetTextureCoordinate (s2, t2); 
	} 

	glVertex3fv (coord); 
	numVertex++;
}

void NDS_glVertex16b(unsigned int v)
{
	if(coordind==0)
	{
		coord[0]		= float16table[v&0xFFFF];
//		coordFixed[0]	= (signed short) (v&0xFFFF);

		coord[1]		= float16table[v>>16];		
//		coordFixed[1]	= (signed short) (v>>16);
		++coordind;
		return;
	}

	coord[2]	  = float16table[v&0xFFFF];
//	coordFixed[2] = (signed short) (v&0xFFFF);

	coordind = 0;
	SetVertex ();
}

void NDS_glVertex10b(unsigned long v)
{
	coord[0]		= float10Table[v&1023];
//	coordFixed[0]	= (signed short)(( v     &1023)<<6);
	coord[1]		= float10Table[(v>>10)&1023];
//	coordFixed[1]	= (signed short)(((v>>10)&1023)<<6);
	coord[2]		= float10Table[(v>>20)&1023];
//	coordFixed[2]	= (signed short)(((v>>20)&1023)<<6);

	SetVertex ();
}

void NDS_glVertex3_cord(unsigned int one, unsigned int two, unsigned int v)
{
	coord[one]		= float16table[v&0xffff];
//	coordFixed[one] = (signed short) (v&0xFFFF);
	coord[two]		= float16table[v>>16];
//	coordFixed[two] = (signed short) (v>>16);
	
	SetVertex ();
}

void NDS_glVertex_rel(unsigned long v)
{
	coord[0]		+= float10RelTable[v&1023];
//	coordFixed[0]	+= (signed short)((v&511) | ((v&512)<<6));
	coord[1]		+= float10RelTable[(v>>10)&1023];
//	coordFixed[1]	+= (signed short)((((v>>10)&511)) | (((v>>10)&512)<<6));
	coord[2]		+= float10RelTable[(v>>20)&1023];
//	coordFixed[2]	+= (signed short)(((v>>20)&511) | (((v>>20)&512)<<6));

	SetVertex ();
}

// Ignored for now
void NDS_glSwapScreen(unsigned int screen)
{
}


// THIS IS A HACK :D
int NDS_glGetNumPolys (void)
{
	return numVertex/3;
}

int NDS_glGetNumVertex (void)
{
	return numVertex;
}

void NDS_glGetLine (int line, unsigned short * dst)
{
	int i;
	u8 *screen3D = (u8 *)&GPU_screen3D[(192-(line%192))*256*4];

	for(i = 0; i < 256; i++)
	{
		u32	r = screen3D[i*4+0],
			g = screen3D[i*4+1],
			b = screen3D[i*4+2],
			a = screen3D[i*4+3];

		r = (r*a)/255;
		g = (g*a)/255;
		b = (b*a)/255;

		if (r != 0 || g != 0 || b != 0)
		{
			dst[i] = (((r>>3)<<10) | ((g>>3)<<5) | (b>>3));
		}
	}
}

void NDS_glFlush(unsigned long v)
{
	if (beginCalled)
	{
		glEnd();
		beginCalled = 0;
	}

	clCmd = 0;
	clInd = 0;

//	if (!disable3D)
	{
		glFlush();
		glReadPixels(0,0,256,192,GL_BGRA,GL_UNSIGNED_BYTE,GPU_screen3D);
	}

	numVertex = 0;

	/*if (wireframeMode)
	{
		glPolygonMode (GL_BACK,  GL_LINE);
		glPolygonMode (GL_FRONT, GL_LINE);
	}
	else*/
	{
		glPolygonMode (GL_BACK,  GL_FILL);
		glPolygonMode (GL_FRONT, GL_FILL);
	}

	glDepthMask (GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void NDS_glPolygonAttrib (unsigned long val)
{
	u32 polygonID = (val>>24)&63;

	// Light enable/disable
	lightMask = (val&0xF);

	// texture environment
    envMode = texEnv[(val&0x30)>>4];

	// overwrite depth on alpha pass
	alphaDepthWrite = BIT11(val);

	// depth test function
	depthFuncMode = depthFunc[BIT14(val)];

	// back face culling
	cullingMask = (val&0xC0);

	// Alpha value, actually not well handled, 0 should be wireframe
	colorAlpha = ((val>>16)&0x1F)<<26;
}

/*
	0-4   Diffuse Reflection Red
	5-9   Diffuse Reflection Green
	10-14 Diffuse Reflection Blue
	15    Set Vertex Color (0=No, 1=Set Diffuse Reflection Color as Vertex Color)
	16-20 Ambient Reflection Red
	21-25 Ambient Reflection Green
	26-30 Ambient Reflection Blue
*/
void NDS_glMaterial0 (unsigned long val)
{
	int		diffuse[4] = {	(val&0x1F) << 26,
							((val>>5)&0x1F) << 26,
							((val>>10)&0x1F) << 26,
							0xffffffff },
			ambient[4] = {	((val>>16)&0x1F) << 26,
							((val>>21)&0x1F) << 26,
							((val>>26)&0x1F) << 26,
							0xffffffff };

	if (BIT15(val))
	{
		colorRGB[0] = diffuse[0];
		colorRGB[1] = diffuse[1];
		colorRGB[2] = diffuse[2];
	}

//	if (disableLighting)
//		return;

	if (beginCalled)
	{
		glEnd();
	}

	glMaterialiv (GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
	glMaterialiv (GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

void NDS_glMaterial1 (unsigned long val)
{
	int		specular[4] = {	(val&0x1F) << 26,
							((val>>5)&0x1F) << 26,
							((val>>10)&0x1F) << 26,
							0xffffffff },
			emission[4] = {	((val>>16)&0x1F) << 26,
							((val>>21)&0x1F) << 26,
							((val>>26)&0x1F) << 26,
							0xffffffff };

//	if (disableLighting)
//		return;

	if (beginCalled)
	{
		glEnd();
	}

	glMaterialiv (GL_FRONT_AND_BACK, GL_SPECULAR, specular);
	glMaterialiv (GL_FRONT_AND_BACK, GL_EMISSION, emission);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

void NDS_glShininess (unsigned long val)
{
}

void NDS_glTexImage(unsigned long val)
{
	textureFormat = val;
}

void NDS_glTexPalette(unsigned long val)
{
	texturePalette = val;
}

void NDS_glTexCoord(unsigned long val)
{
	t = val>>16;
	s = val&0xFFFF;

	if (texCoordinateTransform == 1)
	{			
		float *textureMatrix = mtxCurrent[3];
		int s2 =(int)(	s*			textureMatrix[0] + t*			textureMatrix[4] + 
						(1.f/16.f)* textureMatrix[8] + (1.f/16.f)*	textureMatrix[12]);
		int t2 =(int)(	s*			textureMatrix[1] + t*			textureMatrix[5] + 
						(1.f/16.f)* textureMatrix[9] + (1.f/16.f)*	textureMatrix[13]);

		SetTextureCoordinate (s2, t2);
	}
	else
	{
		SetTextureCoordinate (s, t);
	}
}

signed long NDS_glGetClipMatrix (unsigned int index)
{
	float val = MatrixGetMultipliedIndex (index, mtxCurrent[0], mtxCurrent[1]);

	val *= (1<<12);

	return (signed long)val;
}

signed long NDS_glGetDirectionalMatrix (unsigned int index)
{
	index += (index/3);

	return (signed long)(mtxCurrent[2][(index)*(1<<12)]);
}


/*
	0-9   Directional Vector's X component (1bit sign + 9bit fractional part)
	10-19 Directional Vector's Y component (1bit sign + 9bit fractional part)
	20-29 Directional Vector's Z component (1bit sign + 9bit fractional part)
	30-31 Light Number                     (0..3)
*/

void NDS_glLightDirection (unsigned long v)
{
	float lightDirection[4] = {0};
	lightDirection[0] = -normalTable[v&1023];
	lightDirection[1] = -normalTable[(v>>10)&1023];
	lightDirection[2] = -normalTable[(v>>20)&1023];

	if (beginCalled)
	{
		glEnd();
	}

	glLightfv		(GL_LIGHT0 + (v>>30), GL_POSITION, lightDirection);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

void NDS_glLightColor (unsigned long v)
{
	int lightColor[4] = {	((v)    &0x1F)<<26, 
							((v>> 5)&0x1F)<<26, 
							((v>>10)&0x1F)<<26, 
							0xffffffff};

	if (beginCalled)
	{
		glEnd();
	}

	glLightiv (GL_LIGHT0 + (v>>30), GL_AMBIENT, lightColor);
	glLightiv (GL_LIGHT0 + (v>>30), GL_DIFFUSE, lightColor);
	glLightiv (GL_LIGHT0 + (v>>30), GL_SPECULAR, lightColor);

	if (beginCalled)
	{
		glBegin (vtxFormat);
	}
}

void NDS_glAlphaFunc(unsigned long v)
{
	glAlphaFunc (GL_GREATER, (v&31)/31.f);
}

void NDS_glControl(unsigned long v)
{
	if(v&1)
	{
		glEnable (GL_TEXTURE_2D);
	}
	else
	{
		glDisable (GL_TEXTURE_2D);
	}

	if(v&(1<<2))
	{
		glEnable(GL_ALPHA_TEST);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}

/*
	// Seems to broke hell a lot of stuff
	if(v&(1<<3))// && !disableBlending)
	{
		/*
		if (alphaDepthWrite)
		{
			glDepthMask (GL_TRUE);
		}
		else
		{
			glDepthMask (GL_FALSE);
			//glDisable	(GL_DEPTH_TEST);
		}
		*-/

		glBlendFunc	(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		//glBlendFunc	(GL_ONE, GL_ONE);
		//glBlendFunc	(GL_SRC_ALPHA, GL_ONE);
		glEnable		(GL_BLEND);
	}
	else
	{
		glDepthMask (GL_TRUE);
		glDisable(GL_BLEND);
	}
*/
	if(v&(1<<7))
	{
		int fog = 1;
	}


	if(v&(1<<4))
	{
		glHint (GL_LINE_SMOOTH_HINT, GL_DONT_CARE);
	}
}

void NDS_glNormal(unsigned long v)
{
	float normal[3] = { normalTable[v&1023],
						normalTable[(v>>10)&1023],
						normalTable[(v>>20)&1023]};

	if (texCoordinateTransform == 2)
	{
		float *textureMatrix = mtxCurrent[3];
		int s2 =(int)(	(normal[0] *textureMatrix[0] + normal[1] *textureMatrix[4] + 
						 normal[2] *textureMatrix[8]) + s);
		int t2 =(int)(	(normal[0] *textureMatrix[1] + normal[1] *textureMatrix[5] + 
						 normal[2] *textureMatrix[9]) + t);

		SetTextureCoordinate (s2, t2);
	}

	glNormal3fv(normal);
}

void NDS_glCallList(unsigned long v)
{
	static unsigned long oldval = 0, shit = 0;

	if(!clInd)
	{
		clInd = 4;
		clCmd = v;
		return;
	}


	for (;;)
	{
		switch ((clCmd&0xFF))
		{
			case 0x0:
			{
				--clInd;

				if (!clInd)
					break;

				continue;
			}

			case 0x11 :
			{
				((unsigned long *)ARM9Mem.ARM9_REG)[0x444>>2] = v;
				NDS_glPushMatrix();
				--clInd;
				clCmd>>=8;
				continue;
			}

			case 0x15:
			{
				((unsigned long *)ARM9Mem.ARM9_REG)[0x454>>2] = v;
				NDS_glLoadIdentity();
				--clInd;
				clCmd>>=8;
				break;
			}

			case 0x41:
			{
				NDS_glEnd();                      
				--clInd;
				clCmd>>=8;
				continue;
			}
		}

		break;
	}


	if(!clInd)
	{
		clInd = 4;
		clCmd = v;
		return;
	}

	switch(clCmd&0xFF)
	{
		case 0x10:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x440>>2] = v;
			NDS_glMatrixMode (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x12:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x448>>2] = v;
			NDS_glPopMatrix(v);
			--clInd;
			clCmd>>=8;
			break; 
		}

		case 0x13:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x44C>>2] = v;
			NDS_glStoreMatrix(v);
			--clInd;
			clCmd>>=8;
			break; 
		}

		case 0x14:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x450>>2] = v;
			NDS_glRestoreMatrix (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x16:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x458>>2] = v;
			NDS_glLoadMatrix4x4(v);
			clInd2++;
			if(clInd2==16)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x17:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x45C>>2] = v;
			NDS_glLoadMatrix4x3(v);
			clInd2++;
			if(clInd2==12)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x18:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x460>>2] = v;
			NDS_glMultMatrix4x4(v);
			clInd2++;
			if(clInd2==16)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x19:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x464>>2] = v;
			NDS_glMultMatrix4x3(v);
			clInd2++;
			if(clInd2==12)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x1A:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x468>>2] = v;
			NDS_glMultMatrix3x3(v);
			clInd2++;
			if(clInd2==9)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x1B:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x46C>>2] = v;
			NDS_glScale (v);
			clInd2++;
			if(clInd2==3)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x1C:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x470>>2] = v;
			NDS_glTranslate (v);
			clInd2++;
			if(clInd2==3)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x20 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x480>>2] = v;
			NDS_glColor3b(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x21 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x484>>2] = v;
			NDS_glNormal(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x22 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x488>>2] = v;
			NDS_glTexCoord(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x23 :
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x48C>>2] = v;
			NDS_glVertex16b(v);
			clInd2++;
			if(clInd2==2)
			{
				--clInd;
				clCmd>>=8; 
				clInd2 = 0;
			}
			break;
		}

		case 0x24:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x490>>2] = v;
			NDS_glVertex10b(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x25:// GFX_VERTEX_XY
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x494>>2] = v;
			NDS_glVertex3_cord(0,1,v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x26:// GFX_VERTEX_XZ
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x498>>2] = v;
			NDS_glVertex3_cord(0,2,v);							
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x27:// GFX_VERTEX_YZ
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x49C>>2] = v;
			NDS_glVertex3_cord(1,2,v);							
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x28: // GFX_VERTEX_DIFF
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A0>>2] = v;
			NDS_glVertex_rel (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x29:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A4>>2] = v;
			NDS_glPolygonAttrib (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x2A:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4A8>>2] = v;
			NDS_glTexImage (v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x2B:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4AC>>2] = v;
			NDS_glTexPalette (v&0x1FFF);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x30: // GFX_DIFFUSE_AMBIENT
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C0>>2] = v;
			NDS_glMaterial0(v);
			--clInd;
			clCmd>>=8;
			break;
		}
		
		case 0x31: // GFX_SPECULAR_EMISSION
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C4>>2] = v;
			NDS_glMaterial1(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x32:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4C8>>2] = v;
			NDS_glLightDirection(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x33:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4CC>>2] = v;
			NDS_glLightColor(v);
			--clInd;
			clCmd>>=8;
			break;
		}

		case 0x34:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x4D0>>2] = v;
			NDS_glShininess (v);
			clInd2++;
			if(clInd2==32)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}
			break;
		}

		case 0x40 : 
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x500>>2] = v;
			NDS_glBegin(v);
			--clInd;
			clCmd>>=8;
			break;
		}
/*
		case 0x50:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x540>>2] = v;
			NDS_glFlush(v);
			--clInd;
			clCmd>>=8;
			break;
		}
*/
		case 0x60:
		{
			((unsigned long *)ARM9Mem.ARM9_REG)[0x580>>2] = v;
			NDS_glViewPort(v);
			--clInd;
			clCmd>>=8;
			break;
		}
/*
		case 0x80:
		{
			clInd2++;
			if(clInd2==7)
			{
				--clInd;
				clCmd>>=8;
				clInd2 = 0;
			}

			break;
		}
*/
		default:
		{
			LOG ("Unknown 3D command %02X", clCmd&0xFF);
			--clInd;
			clCmd>>=8;
			break;
		}
	}
	if((clCmd&0xFF)==0x41)
	{
		glEnd();                      
		--clInd;
		clCmd>>=8;
	}
}


GPU3DInterface gpu3Dgl = {	NDS_glInit,
							NDS_glViewPort,
							NDS_glClearColor,
							NDS_glFogColor,
							NDS_glFogOffset,
							NDS_glClearDepth,
							NDS_glMatrixMode,
							NDS_glLoadIdentity,
							NDS_glLoadMatrix4x4,
							NDS_glLoadMatrix4x3,
							NDS_glStoreMatrix,
							NDS_glRestoreMatrix,
							NDS_glPushMatrix,
							NDS_glPopMatrix,
							NDS_glTranslate,
							NDS_glScale,
							NDS_glMultMatrix3x3,
							NDS_glMultMatrix4x3,
							NDS_glMultMatrix4x4,
							NDS_glBegin,
							NDS_glEnd,
							NDS_glColor3b,
							NDS_glVertex16b,
							NDS_glVertex10b,
							NDS_glVertex3_cord,
							NDS_glVertex_rel,
							NDS_glSwapScreen,
							NDS_glGetNumPolys,
							NDS_glGetNumVertex,
							NDS_glFlush,
							NDS_glPolygonAttrib,
							NDS_glMaterial0,
							NDS_glMaterial1,
							NDS_glShininess,
							NDS_glTexImage,
							NDS_glTexPalette,
							NDS_glTexCoord,
							NDS_glLightDirection,
							NDS_glLightColor,
							NDS_glAlphaFunc,
							NDS_glControl,
							NDS_glNormal,
							NDS_glCallList,
							  
							NDS_glGetClipMatrix,
							NDS_glGetDirectionalMatrix,
							NDS_glGetLine};
							