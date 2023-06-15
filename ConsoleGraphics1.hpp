#if __cplusplus < 201103L
	#error C++ 11 or higher must be used to compile ConsoleGraphics.
#endif

#include <vector>
#include <windows.h>
#include <math.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <utility>
#include <deque>
#include <mutex>

#ifdef CG_DEBUG
	#include <iostream>
#endif

#ifndef int8
	#define int8 int8_t
	#define uint8 uint8_t
	#define int16 int16_t
	#define uint16 uint16_t
	#define int32 int32_t
	#define uint32 uint32_t
	#define int64 int64_t
	#define uint64 uint64_t
#endif

//CGV1
//Focus on performance
//Fix RGB reversal issue
//Make improvements
//Fix endianness issue

/*+===============================================================>
+ Add support for animated images (AniImage) FINISHED
+ Add matrix transformations to image class (shear, rotation, etc) ONLY IN CG-V2
. Make most integers signed CG-V2
. Optimize code PARTIALLY DONE
. Make pseudo-transparency function work propely CG-V2
. Make code look cleaner 
. Fix text class NANI??!!!
+ Add SIMD/MIMD to increase performance
*/

//File extentions
//".CGIF" - Console Graphics Image Format (supports run length encoding)
//".CGFNT" - Console Graphics Font (Stores the font's width and height within the file, doesn't support run length encoding)
//".CGAI" - Console Graphics Animated Image (supports run length encoding)
//".CGVID" - Console Graphics Video (will reduce colour space to 2 bits per colour and have RLE to reduce video size, option to sync video to audio)
//".CGVF" - ^^^

//File formats
//First for bytes = "CG**", * will change depending on file type
//All integers will be saved as big endian <-

//CGIF
//

//When using this console graphics API, to use colour use the windows.h RGB macro
//Maybe get colour struct to work and eventually change all unsigned position ints to signed

#ifndef CG_INCLUDE
#define CG_INCLUDE

//When using BITBLT mode, it expects every colour to be in BGR to display colour correctly
#define BITBLT 1
#define BITBLT_INVERT ~BITBLT
//When using SET_PIXEL mode, it expects every colour to be in RGB to display colour correctly
#define SET_PIXEL 2
#define SET_PIXEL_INVERT ~SET_PIXEL
#define SET_PIXEL_VERTICAL SET_PIXEL
#define SET_PIXEL_VERTICAL_INVERT ~SET_PIXEL
#define SET_PIXEL_HORIZONTAL 3
#define SET_PIXEL_HORIZONTAL_INVERT ~SET_PIXEL_HORIZONTAL

class ConsoleGraphics;

/*
union
{
	__m128 v;
	uint16 i[8];
} C;


uint32 blendPixelSIMD(uint32 *dstRGB, uint32 *srcRGB, uint8 srcA, uint8 size = 8)
{
	static uint16 r, g, b;
	C v;
	v.i[0] = GetBValue(dstRGB);
	v.i[1] = GetGValue(dstRGB);
	v.i[2] = GetRValue(dstRGB);
	
	//Get*Value calls are flipped because the rgb values are.
	r = (GetBValue(dstRGB)*(255-srcA))+(GetBValue(srcRGB)*srcA);
	g = (GetGValue(dstRGB)*(255-srcA))+(GetGValue(srcRGB)*srcA);
	b = (GetRValue(dstRGB)*(255-srcA))+(GetRValue(srcRGB)*srcA);
	r /= 255, g /= 255, b /= 255;
	
	return RGB((uint8)b, (uint8)g, (uint8)r);
}
*/

#undef RGB

namespace cg
{
	uint32 RGB(uint8 r, uint8 g, uint8 b){return r | (g << 8) | (b << 16);
	}
	uint32 BGR(uint8 r, uint8 g, uint8 b){return RGB(b, g, r);
	}
	uint32 RGBA(uint8 r, uint8 g, uint8 b, uint8 a){return RGB(r, g, b) | (a << 24);
	}
	uint32 RGBA(uint32 rgb, uint8 a){return (rgb & 0x00FFFFFF) | (a << 24);
	}
	uint32 BGRA(uint8 r, uint8 g, uint8 b, uint8 a){return RGBA(b, g, r, a);
	}
	uint32 BGRA(uint32 bgr, uint8 a){return (bgr & 0x00FFFFFF) | (a << 24);
	}
	
	//If isRGB == false, then input is assumed to be in the BGRA format
	uint8 GetR(uint32 rgba, bool isRGB = false){return isRGB ? rgba : rgba >> 16;
	}
	uint8 GetG(uint32 rgba){return rgba >> 8;
	}
	uint8 GetB(uint32 rgba, bool isRGB = false){return isRGB ? rgba >> 16 : rgba;
	}
	uint8 GetA(uint32 rgba){return rgba >> 24;
	}
};

using namespace cg;

uint32 blendPixel(uint32 dstRGB, uint32 srcRGB, uint8 srcA)
{
	static uint16 r, g, b;
	
	//Get*Value calls are flipped because the rgb values are.
	r = (GetR(dstRGB, false)*(255-srcA))+(GetR(srcRGB, false)*srcA);
	g = (GetG(dstRGB)*(255-srcA))+(GetG(srcRGB)*srcA);
	b = (GetB(dstRGB, false)*(255-srcA))+(GetB(srcRGB, false)*srcA);
	r /= 255, g /= 255, b /= 255;
	
	return cg::BGR((uint8)r, (uint8)g, (uint8)b);
}

struct BMPHeader
{
	uint8 fileType[2];
	uint32 fileSize;
	int16 unused[2];
	uint32 dataOffset;
	uint32 dibHeaderSize;
	int32 width;
	int32 height;
	uint16 colourPlanes;
	uint16 bitsPerPixel;
	uint32 compression;
	uint32 dataSize;
	uint32 prRes[2];
	uint32 pColours;
	uint32 iColours;
	
	BMPHeader()
	{
		fileType[0] = 'B';
		fileType[1] = 'M';
		fileSize = 54;
		memset(&unused[0], 0, sizeof(int16)*2);
		dataOffset = 54;
		dibHeaderSize = 40;
		colourPlanes = 1;
		bitsPerPixel = 24;
		compression = 0;
		memset(&prRes[0], 2835, sizeof(uint32)*2);
		pColours = 0;
		iColours = 0;
	}
	
	uint8 *getHeaderData(void)
	{
		uint8 *data = new uint8[54];
		uint32 i = 0;
		memcpy(&data[0], &fileType[0], sizeof(fileType));
		i += sizeof(fileType);
		memcpy(&data[i], &fileSize, sizeof(fileSize));
		i += sizeof(fileSize);
		memcpy(&data[i], &unused[0], sizeof(unused));
		i += sizeof(unused);
		memcpy(&data[i], &dataOffset, sizeof(dataOffset));
		i += sizeof(dataOffset);
		memcpy(&data[i], &dibHeaderSize, sizeof(dibHeaderSize));
		i += sizeof(dibHeaderSize);
		memcpy(&data[i], &width, sizeof(width));
		i += sizeof(width);
		memcpy(&data[i], &height, sizeof(height));
		i += sizeof(height);
		memcpy(&data[i], &colourPlanes, sizeof(colourPlanes));
		i += sizeof(colourPlanes);
		memcpy(&data[i], &bitsPerPixel, sizeof(bitsPerPixel));
		i += sizeof(bitsPerPixel);
		memcpy(&data[i], &compression, sizeof(compression));
		i += sizeof(compression);
		memcpy(&data[i], &prRes[0], sizeof(prRes));
		i += sizeof(prRes);
		memcpy(&data[i], &pColours, sizeof(pColours));
		i += sizeof(pColours);
		memcpy(&data[i], &iColours, sizeof(iColours));
		
		return data;
	}
	
	uint32 calcPaddingSize(uint32 width = 0){return (4-((width == 0 ? this->width : width)*3)%4)%4;
	}
};

class _BMPHandler
{
		uint8 *buffer;
		
	protected:
		void buffToData(void)
		{
			memcpy(&buffer[0], &fileType[0], sizeof(fileType));
			memcpy(&buffer[2], &fileSize, sizeof(fileSize));
			memcpy(&buffer[6], &unused[0], sizeof(unused));
			memcpy(&buffer[14], &dataOffset, sizeof(dataOffset));
			memcpy(&buffer[18], &dibHeaderSize, sizeof(dibHeaderSize));
			memcpy(&buffer[22], &width, sizeof(width));
			memcpy(&buffer[26], &height, sizeof(height));
			memcpy(&buffer[30], &colourPlanes, sizeof(colourPlanes));
			memcpy(&buffer[32], &bitsPerPixel, sizeof(bitsPerPixel));
			memcpy(&buffer[34], &compression, sizeof(compression));
			memcpy(&buffer[38], &prRes[0], sizeof(prRes));
			memcpy(&buffer[48], &pColours, sizeof(pColours));
			memcpy(&buffer[50], &iColours, sizeof(iColours));
			return;
		}
		uint8 calcPaddingSize(uint8 w){return (w*3)%4;
		}
		
	public:
		uint8 fileType[2];
		uint32 fileSize;
		int16 unused[2];
		uint32 dataOffset;
		uint32 dibHeaderSize;
		int32 width;
		int32 height;
		uint16 colourPlanes;
		uint16 bitsPerPixel;
		uint32 compression;
		uint32 dataSize;
		uint32 prRes[2];
		uint32 pColours;
		uint32 iColours;
		
		void getHeader(std::ifstream &fileHandle) //Load BMP file from file stream
		{
			if (fileHandle.is_open())
			{
				buffer = new uint8[54];
				fileHandle.read((char*)buffer, sizeof(uint8)*54);
				buffToData();
				delete[] buffer;
			}
			return;
		}
		std::vector<std::pair<uint32, uint8>> getImageData(std::ifstream &fileHandle)
		{
			return std::vector<std::pair<uint32, uint8>>();
		}
		
};

enum class InterpolationMethod{None, NearestNeighbor, Bilinear, AreaAveraging};
enum class ExtrapolationMethod{None, Repeat, Extend};

enum class FilterType{Grayscale, WeightedGrayscale, Invert, Custom = 255};

class Image
{
	//First element in pair is for rgb data, the second is for alpha
	std::vector<std::pair<uint32, uint8>> pixels;
	uint32 width, height, x = 0, y = 0;
	float aspectRatio;
	bool bigEndian = (htonl(123) == 123);
	
	protected:
		std::vector<std::pair<uint32, uint8>> resizeData(std::vector<std::pair<uint32, uint8>> &pixels, uint32 newWidth, uint32 newHeight, InterpolationMethod m)
		{
			std::vector<std::pair<uint32, uint8>> data;
			bool success = true;
			float xScale = (float)width/(float)newWidth, yScale = (float)height/(float)newHeight;
			data.resize(newWidth * newHeight);
			
			switch (m)
			{
				default:
				case InterpolationMethod::NearestNeighbor:
				{
					uint32 srcX, srcY;
					for (uint32 y = 0; y < newHeight; ++y)
					{
						for (uint32 x = 0; x < newWidth; ++x)
						{
							srcX = x*xScale;
							srcY = y*yScale;
							data[(y*newWidth)+x] = pixels[(srcY*width)+srcX];
						}
					}
				}
				break;
				
				case InterpolationMethod::Bilinear:
				{
					float srcX, srcY;
					for (uint32 y = 0; y < newHeight; ++y)
					{
						for (uint32 x = 0; x < newWidth; ++x)
						{
							srcX = x*xScale;
							srcX /= width;
							srcY = y*yScale;
							srcY /= height;
							data[(y*newWidth)+x] = getPixel(srcX, srcY, InterpolationMethod::Bilinear, ExtrapolationMethod::Extend);
						}
					}
				}
				break;
				
				case InterpolationMethod::AreaAveraging:
				{
					if ((success = (xScale > 1.f && yScale > 1.f)))
					{
						uint16 pixW = xScale, pixH = yScale;
						uint16 _w = pixW/2, _h = pixH/2;
						pixW = std::max<uint16>(pixW, 2);
						pixH = std::max<uint16>(pixH, 2);
						uint32 avR, avG, avB, avA;
						float alphaRatio;
						uint32 srcX, srcY;
						uint16 div = pixW*pixH;
						
						for (uint32 y = 0; y < newHeight; ++y)
						{
							for (uint32 x = 0; x < newWidth; ++x)
							{
								srcX = x*xScale;
								srcY = y*yScale;
								avR = 0, avG = 0, avB = 0, avA = 0;
								
								for (uint32 w = 0; w < pixW; ++w)
								{
									for (uint32 h = 0; h < pixH; ++h)
									{
										if (getPixel(srcX+w-_w, srcY+h-_h) != nullptr)
										{
											alphaRatio = pixels[((srcY+h-_h)*width)+(srcX+w-_w)].second/255.f;
											avR += GetBValue(pixels[((srcY+h-_h)*width)+(srcX+w-_w)].first)*alphaRatio;
											avG += GetGValue(pixels[((srcY+h-_h)*width)+(srcX+w-_w)].first)*alphaRatio;
											avB += GetRValue(pixels[((srcY+h-_h)*width)+(srcX+w-_w)].first)*alphaRatio;
											avA += pixels[((srcY+h-_h)*width)+(srcX+w-_w)].second;
										} else {
											alphaRatio = pixels[(srcY*width)+srcX].second/255.f;
											avR += GetBValue(pixels[(srcY*width)+srcX].first)*alphaRatio;
											avG += GetGValue(pixels[(srcY*width)+srcX].first)*alphaRatio;
											avB += GetRValue(pixels[(srcY*width)+srcX].first)*alphaRatio;
											avA += pixels[(srcY*width)+srcX].second;
										}
									}
								}
								
								avR /= div, avG /= div, avB /= div, avA /= div;
								data[(y*newWidth)+x] = std::make_pair(RGB(avB, avG, avR), avA);
							}
						}
						
					} else data = this->pixels;
				}
				break;
			}
			if (success)
			{
				width = newWidth;
				height = newHeight;
			} else {
				#ifdef CG_DEBUG
					std::cerr<<"ConsoleGraphics IMAGE ERROR: Image size greater than original ("<<width<<char(158)<<height<<" -> "<<newWidth<<char(158)<<newHeight<<", InterpolationMethod::AreaAveraging)"<<std::endl;
				#endif
			}
			return data;
		}
		
		void getHWNDSize(const HWND &hwnd, uint32 &width, uint32 &height)
		{
			RECT desktop;
			GetWindowRect(hwnd, &desktop);
			width = desktop.right - desktop.left, height = desktop.bottom - desktop.top;
			return;
		}
		
		//This doesn't work properly (header I think??)
		void saveBMPFile(std::string fileName, uint32 ver)
		{
			std::ofstream writeFile(fileName, std::ios::binary);
			BMPHeader header;
			uint8 p = 0;
			header.width = bigEndian ? _byteswap_ulong(width) : width;
			header.height = bigEndian ? _byteswap_ulong(-height) : -height;
			header.fileSize += 3*(width+header.calcPaddingSize())*height;
			header.dataSize = header.fileSize-54;
			if (bigEndian)
			{
				header.fileSize = _byteswap_ulong(header.fileSize);
				header.dataSize = _byteswap_ulong(header.dataSize);
			}
			uint8 *headerInfo = header.getHeaderData();
			writeFile.write((char*)headerInfo, 54);
			delete[] headerInfo;
			uint32 paddingSize = header.calcPaddingSize();
			
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					writeFile.write((char*)&accessPixel(x, y)->first, sizeof(uint8)*3);
				}
				
				for (uint32 i = 0; i < paddingSize; ++i)
				{
					writeFile.write((char*)&p, sizeof(uint8));
				}
			}
			writeFile.close();
			return;
		}
		
	public:
		Image()
		{
			width = 256;
			height = 256;
			aspectRatio = 1.f;
			pixels.resize(width * height);
			memset(pixels.data(), 0, pixels.size()*sizeof(std::pair<uint32, uint8>));
		}
		Image(uint32 width, uint32 height, uint32 rgb = 0, uint8 a = 255)
		{
			this->width = width;
			this->height = height;
			aspectRatio = (float)width/(float)height;
			pixels.resize(width * height);
			for (uint32 y = 0; y<height; ++y)
			{
				for (uint32 x = 0; x<width; ++x)
				{
					pixels[(y * width)+x] = std::make_pair(rgb, a);
				}
			}
		}
		
		//Load image from memory
		Image(const uint32 *data, uint32 width, uint32 height, bool alpha = false)
		{
			loadImageFromArray(data, width, height, alpha);
		}
		Image(const std::pair<uint32, uint8> *data, uint32 width, uint32 height)
		{
			loadImageFromArray(data, width, height);
		}
		
		//Load image from file
		Image(const std::string fileName)
		{
			loadImage(fileName);
		}
		
		//Copy constructor
		Image(const Image &image)
		{
			width = image.getWidth();
			height = image.getHeight();
			aspectRatio = (float)width/(float)height;
			x = image.getPosX();
			y = image.getPosY();
			
			pixels.resize(width * height);
			memcpy(&pixels[0], image.getPixelData(), width*height*sizeof(std::pair<uint32, uint8>));
		}
		Image& operator=(const Image &image)
		{
			if (&image != this)
			{
				width = image.getWidth();
				height = image.getHeight();
				aspectRatio = (float)width/(float)height;
				x = image.getPosX();
				y = image.getPosY();
				
				pixels.resize(width * height);
				memcpy(&pixels[0], image.getPixelData(), width*height*sizeof(std::pair<uint32, uint8>));
			}
			return *this;
		}
		
		std::pair<uint32, uint8> *operator[](uint32 i)
		{
			if (i < width * height){return &pixels[i];
			} else return nullptr;
		}
		
		int8 loadImage(const std::string fileName)
		{
			std::string _fileName = fileName;
			std::transform(_fileName.begin(), _fileName.end(), _fileName.begin(), [](char c)->char{return toupper(c);});
			if (!pixels.empty()){pixels.clear();
			}
			
			//Currently only supports 24bit bitmaps
			if (_fileName.find(".BMP") != std::string::npos)
			{
				std::ifstream readfile(fileName, std::ios::binary);
				if (!readfile.is_open()){return 1;
				}
				BMPHeader header;
				unsigned char *data;
				readfile.read((char*)&header.fileType[0], sizeof(uint8)*2);
				readfile.read((char*)&header.fileSize, sizeof(uint32));
				readfile.read((char*)&header.unused[0], sizeof(int16)*2);
				readfile.read((char*)&header.dataOffset, sizeof(uint32));
				readfile.read((char*)&header.dibHeaderSize, sizeof(uint32));
				readfile.read((char*)&header.width, sizeof(int32));
				readfile.read((char*)&header.height, sizeof(int32));
				readfile.read((char*)&header.colourPlanes, sizeof(int16));
				readfile.read((char*)&header.bitsPerPixel, sizeof(int16));
				readfile.read((char*)&header.compression, sizeof(uint32));
				readfile.read((char*)&header.dataSize, sizeof(uint32));
				readfile.read((char*)&header.prRes[0], sizeof(uint32)*2);
				readfile.read((char*)&header.pColours, sizeof(uint32));
				readfile.read((char*)&header.iColours, sizeof(uint32));
				
				if (bigEndian)
				{
					std::swap(header.fileType[0], header.fileType[1]);
					header.fileSize = _byteswap_ulong(header.fileSize);
					header.unused[0] = _byteswap_ushort(header.unused[0]);
					header.unused[1] = _byteswap_ushort(header.unused[1]);
					header.dataOffset = _byteswap_ulong(header.dataOffset);
					header.dibHeaderSize = _byteswap_ulong(header.dibHeaderSize);
					header.width = _byteswap_ulong(header.width);
					header.height = _byteswap_ulong(header.height);
					header.colourPlanes = _byteswap_ushort(header.colourPlanes);
					header.bitsPerPixel = _byteswap_ushort(header.bitsPerPixel);
					header.dataSize = _byteswap_ulong(header.dataSize);
					header.prRes[0] = _byteswap_ulong(header.prRes[0]);
					header.prRes[1] = _byteswap_ulong(header.prRes[1]);
					header.pColours = _byteswap_ulong(header.pColours);
					header.iColours = _byteswap_ulong(header.iColours);
				}
				
				//Magic number != "BM"
				if (header.fileType[0] != 'B' || header.fileType[1] != 'M')
				{
					#ifdef CG_DEBUG
						std::cerr<<"Invalid BMP Magic Number"<<std::endl; 
					#endif
					return 1;
				}
				
				const unsigned int w = header.width*3;
				this->width = header.width;
				this->height = abs(header.height);
				aspectRatio = (float)this->width/(float)this->height;
				data = new unsigned char[w];
				unsigned long int x = 0, y = 0;
				pixels.resize(this->width * this->height);
				readfile.read(reinterpret_cast<char*>(data), header.dataOffset - 54);
				while (y<this->height)
				{
					readfile.read(reinterpret_cast<char*>(data), w);
					while (x<w)
					{
						pixels[(y*this->width)+(x/3)] = std::make_pair(cg::BGR(data[x+2], data[x+1], data[x]), 255);
						x += 3;
					}
					readfile.read(reinterpret_cast<char*>(data), header.calcPaddingSize());
					x = 0;
					++y;
				}
				delete[] data;
				y = 0;
				readfile.close();
				if (header.height > 0){flipVertically();
				}
				return 1;
			} else if (_fileName.find(".CGIF") != std::string::npos) //CGIF = Console Graphics Image Format, <- make all integers big endian
			{
				std::ifstream readfile(fileName, std::ios::binary);
				if (!readfile.is_open()){return 1;
				}
				uint32 width, height, version, size;
				
				readfile.read((char*)&width, sizeof(uint32));
//				width = ntohl(width);
				this->width = width;
				readfile.read((char*)&height, sizeof(uint32));
//				height = ntohl(height);
				this->height = height;
				readfile.read((char*)&version, sizeof(uint32));
				aspectRatio = (float)width/(float)height;
				size = width*height;
				
				pixels.resize(width*height);
				
				if (version == 1)
				{
					for (uint32 i = 0; i < size; ++i)
					{
						readfile.read((char*)&pixels[i].first, sizeof(uint32));
//						pixels[i].first = ntohl(pixels[i].first);
						readfile.read((char*)&pixels[i].second, sizeof(uint8));
						for (uint8 i = 0; i < 3; ++i)
						{
							uint8 c;
							readfile.read((char*)&c, sizeof(uint8));
						}
					}
				} else if (version == 2)
				{
					uint32 i = 0;
					uint8 count;
					std::pair<uint32, uint8> pixel;
					
					while (i < (width * height))
					{
						readfile.read(reinterpret_cast<char*>(&count), sizeof(uint8));
						readfile.read(reinterpret_cast<char*>(&pixel.first), sizeof(uint32));
//						pixel.first = ntohl(pixel.first);
						readfile.read(reinterpret_cast<char*>(&pixel.second), sizeof(uint8));
						for (uint8 i = 0; i < 3; ++i)
						{
							uint8 c;
							readfile.read((char*)&c, sizeof(uint8));
						}
						
						while(count != 0)
						{
							pixels[i] = pixel;
							++i;
							--count;
						}
					}
				} else return 1;
				readfile.close();
			} else if (_fileName.find(".GIF") != std::string::npos)
			{
				#ifdef CG_DEBUG
					std::cerr<<"GIF loading function has not been implemented yet."<<std::endl;
				#endif
				return 0;
			} else return 0;
			return 0;
		}
		void loadImageFromArray(const uint32 *arr, uint32 width, uint32 height, bool alpha = false)
		{
			pixels.resize(width * height);
			this->width = width; 
			this->height = height;
			this->aspectRatio = (float)width/(float)height;
			
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					const uint32 &pixel = arr[(y*width)+x];
					uint32 rgb = cg::BGR(cg::GetR(pixel), cg::GetG(pixel), cg::GetB(pixel));
					uint8 a = alpha ? cg::GetA(pixel) : 255;
					pixels[(y*width)+x] = std::make_pair(rgb, a);
				}
			}
			return;
		}
		void loadImageFromArray(const std::pair<uint32, uint8> *arr, uint32 width, uint32 height)
		{
			pixels.resize(width * height);
			this->width = width; 
			this->height = height;
			this->aspectRatio = (float)width/(float)height;
			
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					const uint32 &rgb = arr[(y*width)+x].first;
					const uint8 &a = arr[(y*width)+x].second;
					pixels[(y*width)+x] = std::make_pair(cg::BGR(cg::GetR(rgb), cg::GetG(rgb), cg::GetB(rgb)), a);
				}
			}
			return;
		}
		
		void saveImage(const std::string fileName, uint32 ver)
		{
			std::string _fileName = fileName;
			std::transform(_fileName.begin(), _fileName.end(), _fileName.begin(), [](char c)->char{return toupper(c);});
			
			if (_fileName.find(".BMP") != std::string::npos)
			{
				saveBMPFile(fileName, ver);
			} else if (_fileName.find(".CGIF") != std::string::npos)
			{
				std::ofstream writeFile(fileName, std::ios::binary);
				if (ver == 1)
				{
					writeFile.write(reinterpret_cast<const char*>(&width), sizeof(uint32));
					writeFile.write(reinterpret_cast<const char*>(&height), sizeof(uint32));
					writeFile.write(reinterpret_cast<const char*>(&ver), sizeof(uint32));
					
					writeFile.write(reinterpret_cast<const char*>(pixels.data()), pixels.size()*sizeof(std::pair<uint32, uint8>));
				} else if (ver == 2)
				{
					writeFile.write(reinterpret_cast<const char*>(&width), sizeof(uint32));
					writeFile.write(reinterpret_cast<const char*>(&height), sizeof(uint32));
					writeFile.write(reinterpret_cast<const char*>(&ver), sizeof(uint32));
					
					uint8 count;
					
					for (uint32 i = 0; i<pixels.size(); ++i)
					{
						count = 1;
						while (i<pixels.size() && pixels[i].first == pixels[i+1].first && pixels[i].second == pixels[i+1].second && count < 255)
						{
							++count;
							++i;
						}
						writeFile.write(reinterpret_cast<const char*>(&count), sizeof(uint8));
						writeFile.write(reinterpret_cast<const char*>(&pixels[i]), sizeof(std::pair<uint32, uint8>));
					}
				}
				writeFile.close();
			}
			return;
		}
		
		//0 = Grayscale
		//1 = Standard grayscale
		//2 = Invert Colours
		//255 = Custom (pass in a function pointer, or lamda)(with or without parameters)
		//I will add some macros or constants for this function later
		void filter(FilterType filterType, void (*funcPtr)(std::pair<uint32, uint8>*, void*) = nullptr, void *funcData = nullptr)
		{
			switch (filterType)
			{
				default:
				case FilterType::Grayscale:
				{
					uint8 r, g, b;
					uint16 c;
					for (uint32 i = 0; i < pixels.size(); ++i)
					{
						r = GetBValue(pixels[i].first);
						g = GetGValue(pixels[i].first);
						b = GetRValue(pixels[i].first);
						c = (r+g+b)/3;
						pixels[i].first = RGB(c, c, c);
					}
				}
				break;
				
				case FilterType::WeightedGrayscale:
				{
					uint8 r, g, b, c;
					for (uint32 i = 0; i < pixels.size(); ++i)
					{
						r = GetBValue(pixels[i].first);
						g = GetGValue(pixels[i].first);
						b = GetRValue(pixels[i].first);
						c = (r*0.3f)+(g*0.59f)+(b*0.11f);
						pixels[i].first = RGB(c, c, c);
					}
				}
				break;
				
				case FilterType::Invert:
				{
					for (uint32 i = 0; i < pixels.size(); ++i)
					{
						pixels[i].first = pixels[i].first ^ 0xFFFFFF;
					}
				}
				break;
				
				case FilterType::Custom:
				{
					for (uint32 i = 0; i<pixels.size(); ++i)
					{
						funcPtr(&pixels[i], funcData);
					}
				}
				break;
			}
			return;
		}
		
		uint32 getWidth(void) const {return width;
		}
		uint32 getHeight(void) const {return height;
		}
		
		void setPos(uint32 x, uint32 y)
		{
			this->x = x;
			this->y = y;
			return;
		}
		void move(uint32 x, uint32 y)
		{
			this->x += x;
			this->y += y;
			return;
		}
		
		//Returns X/Y co-ordinates of the image
		uint32 getPosX(void) const {return x;
		}
		uint32 getPosY(void) const {return y;
		}
		
		void flipVertically(void)
		{
			uint32 halfHeight = height/2;
			for (uint32 y = 0; y < halfHeight; ++y)
			{
				for (uint32 x = 0; x < width; ++x)
				{
					std::swap(pixels[(y * width)+x], pixels[((height-y-1)*width)+x]);
				}
			}
			return;
		}
		void flipHorizontally(void)
		{
			uint32 halfWidth = width/2;
			for (uint32 y = 0; y < height; ++y)
			{
				for (uint32 x = 0; x < halfWidth; ++x)
				{
					std::swap(pixels[(y * width)+x], pixels[(y * width)+(width-x-1)]);
				}
			}
			return;
		}
		
		//Return a read only pointer to pixel array
		const std::pair<uint32, uint8> *getPixelData(void) const {return this->pixels.data();
		}
		
		//Resizes image using the nearest neighbor method
		void resize(uint32 newWidth, uint32 newHeight, InterpolationMethod m = InterpolationMethod::NearestNeighbor)
		{
			if (newWidth == 0)
			{
				pixels = resizeData(pixels, newHeight * aspectRatio, newHeight, m);
				width = newHeight * aspectRatio;
				height = newHeight;
			} else if (newHeight == 0)
			{
				pixels = resizeData(pixels, newWidth, newWidth / aspectRatio, m);
				width = newWidth;
				height = newWidth / aspectRatio;
			} else {
				pixels = resizeData(pixels, newWidth, newHeight, m);
				width = newWidth;
				height = newHeight;
				aspectRatio = (float)width/(float)height;
			}
			return;
		}
		void scale(float s, InterpolationMethod m = InterpolationMethod::NearestNeighbor)
		{
			if (s > 0.f)
			{
				resize(width * s, height * s, m);
			}
			return;
		}
		void scale(float x, float y, InterpolationMethod m = InterpolationMethod::NearestNeighbor)
		{
			if (x > 0.f && y > 0.f)
			{
				resize(width * x, height * y, m);
			}
			return;
		}
		
//		//Returns the first element of the array because references don't have a "nullptr" equivilant
//		std::pair<unsigned long int, unsigned char> &getPixel(unsigned long int x, unsigned long int y)
//		{
//			if (x < width && y < height){return pixels[(y * width)+x];
//			} else return pixels[0];
//		}
		std::pair<uint32, uint8> *getPixel(uint32 x, uint32 y)
		{
			if (x < width && y < height){return &pixels[(y * width)+x];
			} else return nullptr;
		}
		std::pair<uint32, uint8> *accessPixel(uint32 x, uint32 y){return &pixels[(y * width)+x];
		}
		
//		std::pair<uint32, uint8> getPixelWInterpolation(float x, float y, InterpolationMethod im = InterpolationMethod::NearestNeighbor, ExtrapolationMethod em = ExtrapolationMethod::Repeat) const
//		{
//			std::pair<uint32, uint8> pixel(0, 0);
//			
//			bool n = false, xNeg = (x < 0.f), yNeg = (y < 0.f);
//			uint32 ix = width-1, iy = height-1;
//			if (x > 1.f || xNeg)
//			{
//				switch (em)
//				{
//					default:
//					case ExtrapolationMethod::None:
//						n = true;
//						break;
//					
//					case ExtrapolationMethod::Repeat:
//						x = fabs(x) - floor(fabs(x));
//						x = xNeg ? 1.f-x :x;
//						ix = floor(x * (width-1));
//						break;
//					
//					case ExtrapolationMethod::Extend:
//						x = std::max(std::min(1.f, x), 0.f);
//						ix = floor(x * (width-1));
//						break;
//				}
//			} else ix = floor(x * (width-1));
//			if (y > 1.f || yNeg)
//			{
//				switch (em)
//				{
//					default:
//					case ExtrapolationMethod::None:
//						n = true;
//						break;
//					
//					case ExtrapolationMethod::Repeat:
//						y = fabs(y) - floor(fabs(y));
//						y = yNeg ? 1.f-y : y;
//						iy = floor(y * (height-1));
//						break;
//					
//					case ExtrapolationMethod::Extend:
//						y = std::max(std::min(1.f, y), 0.f);
//						iy = floor(y * (height-1));
//						break;
//				}
//			} else iy = floor(y * (height-1));
//			
//			if (!n) //Inbounds
//			{
//				
//			}
//			
//			return pixel;
//		}
		
		std::pair<uint32, uint8> getPixel(float x, float y, InterpolationMethod im = InterpolationMethod::NearestNeighbor, ExtrapolationMethod em = ExtrapolationMethod::Repeat) const
		{
			bool n = false, xNeg = (x < 0.f), yNeg = (y < 0.f);
			uint32 ix = width-1, iy = height-1;
			if (x > 1.f || xNeg)
			{
				switch (em)
				{
					default:
					case ExtrapolationMethod::None:
						n = true;
						break;
					
					case ExtrapolationMethod::Repeat:
						x = fabs(x) - floor(fabs(x));
						x = xNeg ? 1.f-x :x;
						ix = floor(x * (width-1));
						break;
					
					case ExtrapolationMethod::Extend:
						x = std::max(std::min(1.f, x), 0.f);
						ix = floor(x * (width-1));
						break;
				}
			} else ix = floor(x * (width-1));
			if (y > 1.f || yNeg)
			{
				switch (em)
				{
					default:
					case ExtrapolationMethod::None:
						n = true;
						break;
					
					case ExtrapolationMethod::Repeat:
						y = fabs(y) - floor(fabs(y));
						y = yNeg ? 1.f-y : y;
						iy = floor(y * (height-1));
						break;
					
					case ExtrapolationMethod::Extend:
						y = std::max(std::min(1.f, y), 0.f);
						iy = floor(y * (height-1));
						break;
				}
			} else iy = floor(y * (height-1));
			
			std::pair<uint32, uint8> pixel;
			
			if (!n)
			{
				float invX = 1.f-x;
				float invY = 1.f-y;
				
				switch (im)
				{
					default:
					case InterpolationMethod::NearestNeighbor:
					case InterpolationMethod::None:
						pixel = pixels[(iy * width) + ix];
						break;
					
					case InterpolationMethod::Bilinear:
					{
						//Convert to intergers and round down
						
						uint8 r[] = {0x00, 0x00, 0x00, 0x00};
						uint8 g[] = {0x00, 0x00, 0x00, 0x00};
						uint8 b[] = {0x00, 0x00, 0x00, 0x00};
						uint8 a[] = {0xFF, 0xFF, 0xFF, 0xFF};
						
						r[0] = (cg::GetR(pixels[(iy * width)+ix].first) * invX);
						g[0] = (cg::GetG(pixels[(iy * width)+ix].first) * invX);
						b[0] = (cg::GetB(pixels[(iy * width)+ix].first) * invX);
						a[0] = pixels[(iy * width)+ix].second * invX;
						
						if (ix + 1 > width-1)
						{
							switch (em)
							{
								default:
								case ExtrapolationMethod::None:
									r[1] = 0;
									g[1] = 0;
									b[1] = 0;
									a[1] = 0;
									break;
								
								case ExtrapolationMethod::Repeat:
									r[1] = cg::GetR(pixels[iy * width].first);
									g[1] = cg::GetG(pixels[iy * width].first);
									b[1] = cg::GetB(pixels[iy * width].first);
									a[1] = pixels[iy * width].second;
									break;
								
								case ExtrapolationMethod::Extend:
									r[1] = cg::GetR(pixels[(iy * width)+width-1].first);
									g[1] = cg::GetG(pixels[(iy * width)+width-1].first);
									b[1] = cg::GetB(pixels[(iy * width)+width-1].first);
									a[1] = pixels[(iy * width)+width-1].second;
									break;
							}
						} else {
							r[1] = cg::GetR(pixels[(iy * width)+(ix+1)].first);
							g[1] = cg::GetG(pixels[(iy * width)+(ix+1)].first);
							b[1] = cg::GetB(pixels[(iy * width)+(ix+1)].first);
							a[1] = pixels[(iy * width)+(ix+1)].second;
						}
						r[1] *= x;
						g[1] *= x;
						b[1] *= x;
						a[1] *= x;
						
						if (iy + 1 > height-1)
						{
							switch (em)
							{
								default:
								case ExtrapolationMethod::None:
									r[2] = 0;
									g[2] = 0;
									b[2] = 0;
									a[2] = 0;
									break;
								
								case ExtrapolationMethod::Repeat:
									r[2] = cg::GetR(pixels[ix].first);
									g[2] = cg::GetG(pixels[ix].first);
									b[2] = cg::GetB(pixels[ix].first);
									a[2] = pixels[ix].second;
									break;
								
								case ExtrapolationMethod::Extend:
									r[2] = cg::GetR(pixels[((height-1) * width)+ix].first);
									g[2] = cg::GetG(pixels[((height-1) * width)+ix].first);
									b[2] = cg::GetB(pixels[((height-1) * width)+ix].first);
									a[2] = pixels[((height-1) * width)+ix].second;
									break;
							}
						} else {
							r[2] = cg::GetR(pixels[(iy * width)+(ix+1)].first);
							g[2] = cg::GetG(pixels[(iy * width)+(ix+1)].first);
							b[2] = cg::GetB(pixels[(iy * width)+(ix+1)].first);
							a[2] = pixels[(iy * width)+(ix+1)].second;
						}
						r[2] *= invX;
						g[2] *= invX;
						b[2] *= invX;
						a[2] *= invX;
						
						uint8 k = (iy + 1 > height-1 ? 0 : 1)+(ix + 1 > width-1 ? 2 : 4);
						
						switch (k)
						{
							case 5: //Both in bounds
								r[3] = cg::GetR(pixels[((iy+1) * width)+(ix+1)].first);
								g[3] = cg::GetG(pixels[((iy+1) * width)+(ix+1)].first);
								b[3] = cg::GetB(pixels[((iy+1) * width)+(ix+1)].first);
								a[3] = pixels[((iy+1) * width)+(ix+1)].second;
								break;
								
							case 3: //x IN, y OUT
								switch (em)
								{
									default:
									case ExtrapolationMethod::None:
										r[3] = 0;
										g[3] = 0;
										b[3] = 0;
										a[3] = 0;
										break;
									
									case ExtrapolationMethod::Repeat:
										r[3] = cg::GetR(pixels[ix].first);
										g[3] = cg::GetG(pixels[ix].first);
										b[3] = cg::GetB(pixels[ix].first);
										a[3] = pixels[ix].second;
										break;
									
									case ExtrapolationMethod::Extend:
										r[3] = cg::GetR(pixels[((height-1) * width)+ix].first);
										g[3] = cg::GetG(pixels[((height-1) * width)+ix].first);
										b[3] = cg::GetB(pixels[((height-1) * width)+ix].first);
										a[3] = pixels[((height-1) * width)+ix].second;
										break;
								}
								break;
								
							case 4: //x OUT, y IN
								switch (em)
								{
									default:
									case ExtrapolationMethod::None:
										r[3] = 0;
										g[3] = 0;
										b[3] = 0;
										a[3] = 0;
										break;
									
									case ExtrapolationMethod::Repeat:
										r[3] = cg::GetR(pixels[iy * width].first);
										g[3] = cg::GetG(pixels[iy * width].first);
										b[3] = cg::GetB(pixels[iy * width].first);
										a[3] = pixels[iy * width].second;
										break;
									
									case ExtrapolationMethod::Extend:
										r[3] = cg::GetR(pixels[(iy * width)+width-1].first);
										g[3] = cg::GetG(pixels[(iy * width)+width-1].first);
										b[3] = cg::GetB(pixels[(iy * width)+width-1].first);
										a[3] = pixels[(iy * width)+width-1].second;
										break;
								}
								break;
								
							case 2: //Both out of bounds
								switch (em)
								{
									default:
									case ExtrapolationMethod::None:
										r[3] = 0;
										g[3] = 0;
										b[3] = 0;
										a[3] = 0;
										break;
									
									case ExtrapolationMethod::Repeat:
										r[3] = cg::GetR(pixels[0].first);
										g[3] = cg::GetG(pixels[0].first);
										b[3] = cg::GetB(pixels[0].first);
										a[3] = pixels[0].second;
										break;
									
									case ExtrapolationMethod::Extend:
										r[3] = cg::GetR(pixels[((height-1) * width)+(width-1)].first);
										g[3] = cg::GetG(pixels[((height-1) * width)+(width-1)].first);
										b[3] = cg::GetB(pixels[((height-1) * width)+(width-1)].first);
										a[3] = pixels[((height-1) * width)+(width-1)].second;
										break;
								}
								break;
						}
						r[3] *= x;
						g[3] *= x;
						b[3] *= x;
						a[3] *= x;
						
						uint8 finalR = ((r[0]+r[1]) * invY) + ((r[2]+r[3]) * y);
						uint8 finalG = ((g[0]+g[1]) * invY) + ((g[2]+g[3]) * y);
						uint8 finalB = ((b[0]+b[1]) * invY) + ((b[2]+b[3]) * y);
						uint8 finalA = ((a[0]+a[1]) * invY) + ((a[2]+a[3]) * y);
						
						pixel = std::make_pair(cg::BGR(finalR, finalG, finalB), finalA);
					}
					break;
				}
			} else pixel = std::make_pair(0, 0);
			
			return pixel;
		}
		
		void setPixel(uint32 x, uint32 y, uint32 rgb, uint8 a = 255)
		{
			if (x < width && y < height)
			{
				pixels[(y * width)+x] = std::make_pair(rgb, a);
			}
			return;
		}
		
		//Sets all alpha values in the image to the "a"
		void setAlpha(uint8 a)
		{
			for (uint32 i = 0; i < pixels.size(); ++i)
			{
				pixels[i].second = a;
			}
			return;
		}
		//Sets the alpha value of a certain pixel colour within the image to "a"
		void setColourToAlpha(uint32 rgb, uint8 a = 0)
		{
			for (uint32 i = 0; i < pixels.size(); ++i)
			{
				if (pixels[i].first == rgb){pixels[i].second = a;
				}
			}
			return;
		}
		
		//Resizes array and updates variables
		void setSize(uint32 newWidth, uint32 newHeight, bool clearData = false)
		{
			auto temp = pixels;
			pixels.clear();
			pixels.resize(newWidth * newHeight);
			if (!clearData)
			{
				for (uint32 y = 0; y < height; y++)
				{
					for (uint32 x = 0; x < width; x++)
					{
						pixels[(y * newWidth)+x] = temp[(y * width)+x];
					}
				}
			}
			width = newWidth;
			height = newHeight;
			aspectRatio = (float)width/(float)height;
			return;
		}
		//This is very slow VVVVV
		void copy(Image &image, uint32 dstX, uint32 dstY, uint32 srcX, uint32 srcY, uint32 width, uint32 height, bool alpha = false)
		{
			for (uint32 iy = 0; iy < height; iy++)
			{
				for (uint32 ix = 0; ix < width; ix++)
				{
					if (getPixel(ix+dstX, iy+dstY) != nullptr && image.getPixel(ix+srcX, iy+srcY) != nullptr)
					{
//						*getPixel(ix+dstX, iy+dstY) = std::make_pair(image.getPixel(ix+srcX, iy+srcY)->first, alpha ? image.getPixel(ix+srcX, iy+srcY)->second : 255);
						pixels[((iy+dstY)*this->width)+(ix+dstX)] = std::make_pair(image.getPixelData()[((iy+srcY)*image.getWidth())+(ix+srcX)].first, alpha ? image.getPixelData()[((iy+srcY)*image.getWidth())+(ix+srcX)].second : 255);
					}
				}
			}
			return;
		}
		void blendImage(Image &image, uint32 dstX, uint32 dstY, uint32 srcX, uint32 srcY, uint32 width, uint32 height, bool keepAlpha = true, bool mask = true)
		{
			uint8 tempA;
			for (uint32 iy = 0; iy < height; iy++)
			{
				for (uint32 ix = 0; ix < width; ix++)
				{
					if (getPixel(ix+dstX, iy+dstY) != nullptr && image.getPixel(ix+srcX, iy+srcY) != nullptr)
					{
						tempA = pixels[((iy+dstY)*this->width)+(ix+dstX)].second;
						std::pair<uint32, uint8> srcRGB = image.getPixelData()[((iy+srcY)*image.getWidth())+(ix+srcX)], &dstRGB = pixels[((iy+dstY)*this->width)+(ix+dstX)];
						dstRGB.first = blendPixel(dstRGB.first, srcRGB.first, srcRGB.second);
						dstRGB.second = keepAlpha ? dstRGB.second : srcRGB.second;
						if (mask && srcRGB.second == 0){dstRGB.second = tempA;
						}
					}
				}
			}
			return;
		}
		
		void getHWNDBitmap(HWND handle)
		{
			HDC hdc = GetDC(handle);
			getHWNDSize(handle, width, height);
			
    		HDC tempDC  = CreateCompatibleDC(hdc);
    		HBITMAP bitmap = CreateCompatibleBitmap(hdc, width, height);
    		HGDIOBJ lastObj = SelectObject(tempDC, bitmap); 
    		BitBlt(tempDC, 0, 0, width, height, hdc, 0, 0, SRCCOPY); 
		
		    SelectObject(tempDC, lastObj); // always select the previously selected object once done
		    DeleteDC(tempDC);
		    
		    BITMAPINFO _bitmapInfo;
		    memset(&_bitmapInfo, 0, sizeof(BITMAPINFO));
    		_bitmapInfo.bmiHeader.biSize = sizeof(_bitmapInfo.bmiHeader); 
		
		    // Get the BITMAPINFO structure from the bitmap
		    if (GetDIBits(hdc, bitmap, 0, 0, NULL, &_bitmapInfo, DIB_RGB_COLORS) == 0) {
		        std::cout << "error" << std::endl;
		        return;
		    }
		    
		    _bitmapInfo.bmiHeader.biCompression = BI_RGB;
		    _bitmapInfo.bmiHeader.biHeight = -_bitmapInfo.bmiHeader.biHeight;
		    _bitmapInfo.bmiHeader.biWidth = width;
		    _bitmapInfo.bmiHeader.biBitCount = 8*4;
			_bitmapInfo.bmiHeader.biPlanes = 1;
			_bitmapInfo.bmiHeader.biCompression = BI_RGB;
//		    _bitmapInfo.bmiHeader.biHeight = BI_RGB;
//		    _bitmapInfo.bmiHeader.biBitCount = 24;
			pixels.resize((_bitmapInfo.bmiHeader.biWidth * -_bitmapInfo.bmiHeader.biHeight) + 1);
			width = _bitmapInfo.bmiHeader.biWidth;
			height = -_bitmapInfo.bmiHeader.biHeight;
		
    		// get the actual bitmap buffer
    		if (GetDIBits(hdc, bitmap, 0, -_bitmapInfo.bmiHeader.biHeight, &pixels[0], &_bitmapInfo, DIB_RGB_COLORS) == 0) {
    		    std::cout << "error2" << std::endl;
    		    return;
    		}
//    		setSize(width/2, height/2);
    		
    		DeleteObject(bitmap);
		}
};

struct Size
{
	uint32 width, height;
	
	Size()
	{
		width = 0, height = 0;
	}
	Size(uint32 width, uint32 height)
	{
		this->width = width;
		this->height = height;
	}
};

struct SubImageData
{
	Size size;
	uint32 srcX, srcY, dstX, dstY;
	bool isVertical, transparency;
	
	SubImageData()
	{
		srcX = 0, srcY = 0;
		dstX = 0, dstY = 0;
		isVertical = false;
		transparency = false;
	}
	SubImageData(uint32 srcX, uint32 srcY, uint32 dstX, uint32 dstY, uint32 width, uint32 height, bool isVertical, bool transparency = false)
	{
		this->srcX = srcX;
		this->srcY = srcY;
		this->dstX = dstX;
		this->dstY = dstY;
		size.width = width;
		size.height = height;
		this->isVertical = isVertical;
		this->transparency = transparency;
	}
};

//Class for animated images
class AniImage
{
	Image image, dstImage;
	bool cycleMethod, dir, finished = false, firstFrame = true, firstFrameDrawn = true;
	uint8 ver, currentCycle = 0, maxCycles = 0;
	uint32 currentFrame = 0, maxFrames;
	uint32 frameTime, currentFrameTime = 0, currentTime = 0;
	std::vector<SubImageData> subImageData;
	
	protected:
		void load(std::string fileName)
		{
			if (fileName.find(".gif") != std::string::npos || fileName.find(".GIF") != std::string::npos)
			{
				#ifdef CG_DEBUG
					std::cerr<<"Cannot open:"<<fileName<<std::endl;
					std::cerr<<"GIF file loading has not been implemented yet."<<std::endl;
				#endif
				return;
			}
			
			std::ifstream readFile(fileName.c_str(), std::ios::binary);
			if (readFile.is_open())
			{
				readFile.read(reinterpret_cast<char*>(&ver), sizeof(uint8)); //Get version
				Size tempSize;
				readFile.read(reinterpret_cast<char*>(&tempSize.width), sizeof(uint32)); //Get main image dimensions
				readFile.read(reinterpret_cast<char*>(&tempSize.height), sizeof(uint32));
				dstImage.setSize(tempSize.width, tempSize.height, true);
				readFile.read(reinterpret_cast<char*>(&tempSize.width), sizeof(uint32)); //Get total image dimensions
				readFile.read(reinterpret_cast<char*>(&tempSize.height), sizeof(uint32));
				image.setSize(tempSize.width, tempSize.height, true);
				
				//Get Image data
				std::pair<uint32, uint8> pixel;
				uint8 c;
				uint32 size = image.getWidth()*image.getHeight();
				switch (ver)
				{
					case 1:
					{
						for (uint32 i = 0; i < size; ++i)
						{
							readFile.read(reinterpret_cast<char*>(&pixel), sizeof(std::pair<uint32, uint8>));
							*image[i] = pixel;
						}
					}
					break;
					
					default:
					case 2:
					{
						uint32 i = 0;
						while (i < size)
						{
							readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
							readFile.read(reinterpret_cast<char*>(&pixel), sizeof(std::pair<uint32, uint8>));
							
							while (c != 0)
							{
								*image[i] = pixel;
								++i;
								--c;
							}
						}
					}
				}
				
				maxFrames = 0;
				if (ver == 1)
				{
					readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
					maxFrames = c;
				} else if (ver >= 2){readFile.read(reinterpret_cast<char*>(&maxFrames), sizeof(uint32));
				}
				readFile.read(reinterpret_cast<char*>(&frameTime), sizeof(uint32));
				readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
				
				switch (c)
				{
					default:
					case 0:
						cycleMethod = false;
						break;
					
					case 1:
						cycleMethod = true;
						break;
				}
				
				if (ver >= 2){readFile.read(reinterpret_cast<char*>(&maxCycles), sizeof(uint8));
				}
				
				uint8 s;
				readFile.read(reinterpret_cast<char*>(&s), sizeof(uint8));
				
				subImageData.clear();
				for (uint8 i = 0; i < s; ++i)
				{
					subImageData.push_back(SubImageData());
					readFile.read(reinterpret_cast<char*>(&subImageData.back().srcX), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().srcY), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().dstX), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().dstY), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().size.width), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&subImageData.back().size.height), sizeof(uint32));
					readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
					subImageData.back().isVertical = (c == 1 ? true : false);
					if (ver >= 2)
					{
						readFile.read(reinterpret_cast<char*>(&c), sizeof(uint8));
						subImageData.back().transparency = (c == 1 ? true : false);
					} else subImageData.back().transparency = false;
				}
				
				readFile.close();
				
				update(0.f);
				reset();
				
				#ifdef CG_DEBUG
					std::clog<<"Filename:\t"<<fileName<<std::endl;
					std::clog<<"Version:\t"<<(int)ver<<std::endl;
					std::clog<<"Main Image dimensions:\t"<<dstImage.getWidth()<<(char)158<<dstImage.getHeight()<<std::endl;
					std::clog<<"Image dimensions:\t"<<image.getWidth()<<(char)158<<image.getHeight()<<std::endl;
					std::clog<<"Expected data size:\t"<<image.getWidth()*image.getHeight()<<std::endl;
//					std::clog<<"Data size:\t"<<d.size()<<std::endl;
					std::clog<<"Number of frames:\t"<<maxFrames<<std::endl;
					std::clog<<"Frame time:\t"<<frameTime<<" ("<<(float)1.f/(frameTime/1000.f)<<" FPS)"<<std::endl;
					if (cycleMethod){std::clog<<"Cycle method:\tTRUE ("<<(int)c<<')'<<std::endl;
					} else std::clog<<"Cycle method:\tFALSE ("<<(int)c<<')'<<std::endl;
					std::clog<<"Cycles:\t"<<(int)maxCycles<<std::endl;
					std::cout<<"Number of sub images:\t"<<(int)s<<std::endl;
				#endif
			}
			return;
		}
		
		void play(void)
		{
			if (firstFrame){firstFrame = false;
			}
			if (cycleMethod)
			{
				if (dir)
				{
					if (currentFrame == maxFrames-1){dir = false;
					} else ++currentFrame;
				} else {
					if (currentFrame == 0){dir = true;
					} else --currentFrame;
				}
				if (!firstFrame && currentFrame == 0 && !dir){++currentCycle;
				}
			} else {
				if ((++currentFrame) == maxFrames-1){++currentCycle;
				}
				currentFrame %= maxFrames;
			}
			finished = (currentTime >= getAniTime() ? true : false);
		}
	public:
		AniImage()
		{
			ver = 0;
			maxFrames = 0;
			frameTime = 0;
			cycleMethod = false;
			dir = false;
		}
		AniImage(std::string fileName)
		{
			ver = 0;
			maxFrames = 0;
			frameTime = 0;
			cycleMethod = false;
			dir = false;
			load(fileName);
		}
		~AniImage()
		{
			
		}
		
		//Filename must end with .cgai
		void loadAniImage(std::string fileName)
		{
			load(fileName);
			return;
		}
		void unload(void)
		{
			
			return;
		}
		void update(float deltaTime) //Seconds
		{
			currentFrameTime += deltaTime*1000.f;
			currentTime += deltaTime*1000.f;
			if (currentFrameTime >= frameTime)
			{
				for (uint32 i = 0; i < (currentFrameTime / frameTime); ++i){play();
				}
				currentFrameTime = 0;
			}
			
			dstImage.copy(image, 0, 0, 0, 0, dstImage.getWidth(), dstImage.getHeight(), true);
			for (auto &data : subImageData)
			{
				if (!data.transparency || ver <= 1)
				{
					dstImage.copy(image, data.dstX, data.dstY, data.srcX + (data.isVertical ? 0 : (currentFrame*data.size.width)), data.srcY + (data.isVertical ? (currentFrame*data.size.height) : 0), data.size.width, data.size.height, true);
				} else dstImage.blendImage(image, data.dstX, data.dstY, data.srcX + (data.isVertical ? 0 : (currentFrame*data.size.width)), data.srcY + (data.isVertical ? (currentFrame*data.size.height) : 0), data.size.width, data.size.height, true);
			}
			return;
		}
		
		void setFrame(uint32 frame)
		{
			currentFrame = frame;
			currentFrame %= maxFrames;
			return;
		}
		uint32 getCurrentFrame(void){return currentFrame;
		}
		uint32 getMaxFrames(void){return maxFrames;
		}
		void advanceFrame(void)
		{
			++currentFrame;
			currentFrame %= maxFrames;
			return;
		}
		void reverseFrame(void)
		{
			--currentFrame;
			currentFrame %= maxFrames;
			return;
		}
		
		void setMaxCycles(uint8 cycles)
		{
			maxCycles = cycles;
			return;
		}
		uint8 getMaxCycles(void){return maxCycles;
		}
		uint8 getCurrentCycle(void){return currentCycle;
		}
		uint32 getCycleTime(void){return frameTime * maxFrames * (cycleMethod ? 2 : 1);
		}
		uint32 getAniTime(void){return frameTime * maxFrames * maxCycles * (cycleMethod ? 2 : 1);
		}
		uint32 getCurrentTime(void){return currentTime;
		}
		
		bool isFinished(void){return finished;
		}
		bool isFirstFrame(void){return firstFrame;
		}
		bool isFirstFrameDrawn(void){return firstFrameDrawn;
		}
		
		void reset(void)
		{
			firstFrame = true;
			firstFrameDrawn = true;
			currentFrame = 0;
			currentCycle = 0;
			finished = false;
			currentFrameTime = 0;
			currentTime = 0;
			dir = false;
			return;
		}
		
		void scale(float n)
		{
			image.scale(n);
			dstImage.scale(n);
			
			for (SubImageData &data : subImageData)
			{
				data.srcX *= n;
				data.srcY *= n;
				data.dstX *= n;
				data.dstY *= n;
				data.size.width *= n;
				data.size.height *= n;
			}
			return;
		}
		
		uint32 getWidth(void){return dstImage.getWidth();
		}
		uint32 getHeight(void){return dstImage.getHeight();
		}
		uint32 getPosX(void){return dstImage.getPosX();
		}
		uint32 getPosY(void){return dstImage.getPosY();
		}
		
		Image *getFinalImage(void){return &dstImage;
		}
		Image *getSourceImage(void){return &image;
		}
		
		void setPos(uint32 x, uint32 y)
		{
			dstImage.setPos(x, y);
			return;
		}
		void draw(ConsoleGraphics *graphics);
};

class Font
{
	uint32 charWidth, charHeight;
};

class Text
{
	Image *font = nullptr;
	std::string text;
	uint32 charWidth, charHeight;
	Image textImage;
	
	protected:
		void getTextSize(const std::string text, uint32 &width, uint32 &height)
		{
			std::vector<uint32> sizes;
			
			for (uint32 i = 0; i < text.size(); ++i)
			{
				if (text[i] == '\n')
				{
					sizes.push_back(width);
					width = 0;
					++height;
				} else ++width;
			}
			
			if (height != 1)
			{
				++sizes.front();
				for (uint32 i = 0; i < sizes.size(); ++i)
				{
					if (width < sizes[i]){width = sizes[i]-1;
					}
				}
			}
			width = std::max<uint32>(width, 1);
		}
	public:
		Text()
		{
			this->charWidth = 0;
			this->charHeight = 0;
		}
		Text(Image *fontImage, uint32 charWidth, uint32 charHeight, const std::string text)
		{
			this->font = fontImage;
			this->charWidth = charWidth;
			this->charHeight = charHeight;
			this->text = text;
			setText(text);
		}
		~Text()
		{
			this->font = nullptr;
		}
		
		//Pass in image pointer with font loaded to save memory
		void setFont(Image *fontImage, uint32 charWidth, uint32 charHeight)
		{
			font = fontImage;
			this->charWidth = charWidth;
			this->charHeight = charHeight;
			return;
		}
		void setCharSize(uint32 charWidth, uint32 charHeight)
		{
			this->charWidth = charWidth;
			this->charHeight = charHeight;
			return;
		}
		void setPos(uint32 x, uint32 y)
		{
			textImage.setPos(x, y);
			return;
		}
		uint32 getPosX(void){return textImage.getPosX();
		}
		uint32 getPosY(void){return textImage.getPosY();
		}
		
		void setText(const std::string text, uint32 compX = 0, uint32 compY = 0)
		{
			this->text = text;
			uint32 textWidth = 0, textHeight = 1;
			getTextSize(text, textWidth, textHeight);
			textImage.setSize((textWidth*charWidth)-(compX*textWidth), (textHeight*charHeight)-(compY*textHeight), true);
			uint32 srcX = 0, srcY = 0, dstX = 0, dstY = 0;
			
			for (uint32 i = 0; i < text.size(); ++i)
			{
				uint8 c = text[i];
				srcX = c*charWidth;
				
				switch (c)
				{
					case '\n':
						dstX = 0;
						dstY++;
						break;
					
					default:
						textImage.copy(*font, (dstX*charWidth)-(compX*dstX), (dstY*charHeight)-(compY*dstY), srcX, 0, charWidth, charHeight, true);
						dstX++;
						break;
				}
			}
			return;
		}
		
		std::string getText(void){return text;
		}
		Image &getTextImage(void){return textImage;
		}
		Image *getFontImage(void){return font;
		}
		
		uint32 getCharWidth(void){return charWidth;
		}
		uint32 getCharHeight(void){return charHeight;
		}
		uint32 getWidth(void){return textImage.getWidth();
		}
		uint32 getHeight(void){return textImage.getHeight();
		}
};

enum class RenderMode{BitBlt, BitBltInv, SetPixel, SetPixelVer, SetPixelInv, SetPixelVerInv};
enum class DrawType{Repeat, Resize};

class ConsoleGraphics
{
	std::vector<uint32> pixels;
	uint32 width, height, startX, startY, consoleWidth, consoleHeight;
	RenderMode renderMode;
	HBITMAP pixelBitmap;
	HDC targetDC, tempDC;
	bool alphaMode;
	uint8 pixelSize;
	bool enableShaders;
	std::vector<void(*)(uint32*, uint32, uint32, uint32, uint32, void*)> shaderList;
	std::vector<void*> shaderDataList;
	std::string title;
	float outputScale = 1.f;
	//"shaderList" function struct
	//Arg 1 = Pointer to current pixel
	//Arg 2 = Current X
	//Arg 3 = Current Y
	//Arg 4 = Number of operations
	//Arg 5 = Extra data
	protected:
		void initialise(void)
		{
			targetDC = GetDC(GetConsoleWindow());
			tempDC = CreateCompatibleDC(targetDC);
			
			renderMode = RenderMode::BitBlt;
			startX = 0;
			startY = 0;
			pixelSize = 1;
			outputScale = 1.f;
			
			alphaMode = false;
			enableShaders = false;
			
			return;
		}
		
		//Improve this
		void removeScrollbars(void)
		{
			HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
  			CONSOLE_SCREEN_BUFFER_INFO csbi;
    		GetConsoleScreenBufferInfo(hstdout, &csbi);
			
    		csbi.dwSize.X -= 2;//= csbi.dwMaximumWindowSize.X;
    		csbi.dwSize.Y = csbi.dwMaximumWindowSize.Y;
    		SetConsoleScreenBufferSize(hstdout, csbi.dwSize);
			
    		HWND x = GetConsoleWindow();
    		ShowScrollBar(x, SB_BOTH, FALSE);
			return;
		}
		
		void getHWNDRes(const HWND &hwnd, uint32 &width, uint32 &height)
		{
			static RECT desktop;
			GetWindowRect(hwnd, &desktop);
			width = desktop.right - desktop.left, height = desktop.bottom - desktop.top;
			return;
		}
		
		void getConsoleRes(void)
		{
			getHWNDRes(GetConsoleWindow(), consoleWidth, consoleHeight);
			return;
		}
		
		void setConsoleRes(const uint32 width, const uint32 height)
		{
			HWND console = GetConsoleWindow();
			RECT window;
			GetWindowRect(console, &window);
			MoveWindow(console, window.left, window.top, width, height, true);
			removeScrollbars();
			return;
		}
		
		std::vector<uint32> resizeDataNearestNeighbor(std::vector<uint32> &pixels, uint32 newWidth, uint32 newHeight)
		{
			std::vector<uint32> data;
			float xScale = (float)width/(float)newWidth, yScale = (float)height/(float)newHeight;
			data.resize(newWidth * newHeight);
			
			uint32 srcX, srcY;
			for (uint32 y = 0; y < newHeight; ++y)
			{
				for (uint32 x = 0; x < newWidth; ++x)
				{
					srcX = x*xScale;
					srcY = y*yScale;
					data[(y*newWidth)+x] = pixels[(srcY*width)+srcX];
				}
			}
			
			width = newWidth;
			height = newHeight;
			return data;
		}
		
		uint32 &accessBuffer(uint32 x, uint32 y){return pixels[(y * width)+x];
		}
		uint32 &accessBuffer(uint32 index){return pixels[index];
		}
		
	public:
		ConsoleGraphics()
		{
			initialise();
			getConsoleRes();
			removeScrollbars();
			width = consoleWidth;
			height = consoleHeight;
			pixels.resize(width * height);
		}
		
		ConsoleGraphics(uint32 width, uint32 height, bool setSize = false, uint8 pixelSize = 1, bool pixelMode = false)
		{
			initialise();
			this->pixelSize = std::max<uint8>(pixelSize, 1);
			getConsoleRes();
			this->width = width / (!pixelMode ? 1 : pixelSize);
			this->height = height / (!pixelMode ? 1 : pixelSize);
			
			if (setSize || pixelMode)
			{
				OSVERSIONINFO windowsVersion;
    			memset(&windowsVersion, 0, sizeof(OSVERSIONINFO));
    			windowsVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			    GetVersionEx(&windowsVersion);
			    
			    unsigned char windowWidthOffset = 0, windowHeightOffset = 0;
			    float _windowsVersion = windowsVersion.dwMajorVersion + ((float)windowsVersion.dwMinorVersion/10);
			    
				consoleWidth = (!pixelMode ? width : this->width) * pixelSize;
				consoleHeight = (!pixelMode ? height : this-> height) * pixelSize;
			    if (_windowsVersion >= 6.2f)
			    {
			    	windowWidthOffset = 15; //15
			    	windowHeightOffset = 39; //39
				} else if (_windowsVersion <= 6.1f)
				{
					float aspectRatio = (float)consoleWidth / (float)consoleHeight;
					if (consoleWidth > 800)
					{
						consoleWidth = 800;
						this->width = consoleWidth / pixelSize;
						
						consoleHeight = consoleWidth / aspectRatio;
						this->height = consoleHeight / pixelSize;
					}
				}
				
				setConsoleRes(consoleWidth+windowWidthOffset, consoleHeight+windowHeightOffset);
			}
			
			if (!pixelMode){pixels.resize(width * height);
			} else pixels.resize(this->width * this->height);
		}
		
		std::pair<uint32, uint32> display(void)
		{
			std::pair<uint32, uint32> returnValue;
			
			if (enableShaders)
			{
				for (uint8 i = 0; i < shaderList.size(); ++i)
				{
					for (uint32 y = 0; y < height; ++y)
					{
						for (uint32 x = 0; x < width; ++x)
						{
							shaderList[i](&pixels[(y * width)+x], width, height, x, y, shaderDataList[i]);
						}
					}
				}
			}
			
			switch (renderMode)
			{
				default:
				case RenderMode::BitBltInv:
				case RenderMode::BitBlt:
					pixelBitmap = CreateBitmap(width, height, 1, 8*4, &pixels[0]);
					switch (reinterpret_cast<uint32>(pixelBitmap))
					{
						default:
							returnValue.first = true;
							break;
						
						case NULL:
							returnValue.first = NULL;
							break;
					}
					SelectObject(tempDC, pixelBitmap);
					returnValue.second = StretchBlt(targetDC, startX, startY, consoleWidth*outputScale, consoleHeight*outputScale, tempDC, 0, 0, width, height, renderMode == RenderMode::BitBlt ? SRCCOPY : NOTSRCCOPY);
					DeleteObject(pixelBitmap);
					break;
					
				case RenderMode::SetPixelInv:
				case RenderMode::SetPixel:
					for (uint32 y = 0; y < consoleHeight; ++y)
					{
						for (uint32 x = 0; x < consoleWidth; ++x)
						{
							uint32 &p = pixels[((y/pixelSize) * width)+(x/pixelSize)];
							SetPixelV(targetDC, x, y, renderMode == RenderMode::SetPixel ? RGB(GetBValue(p), GetGValue(p), GetRValue(p)) : RGB(GetBValue(p), GetGValue(p), GetRValue(p)) ^ 0x00ffffff); //^ 0x00ffffff << inverts colours
						}
					}
					break;
					
				case RenderMode::SetPixelVer:
				case RenderMode::SetPixelVerInv:
					for (uint32 x = 0; x < consoleWidth; ++x)
					{
						for (uint32 y = 0; y < consoleHeight; ++y)
						{
							uint32 &p = pixels[((y/pixelSize) * width)+(x/pixelSize)];
							SetPixelV(targetDC, x, y, renderMode == RenderMode::SetPixelVer ? RGB(GetBValue(p), GetGValue(p), GetRValue(p)) : RGB(GetBValue(p), GetGValue(p), GetRValue(p)) ^ 0x00ffffff);
						}
					}
					break;
			}
			return returnValue;
		}
		
		void setRenderMode(RenderMode mode)
		{
			renderMode = mode;
			return;
		}
		void enableAlpha(void)
		{
			this->alphaMode = true;
			return;
		}
		void disableAlpha(void)
		{
			this->alphaMode = false;
			return;
		}
		
		void clear(void)
		{
			memset(pixels.data(), 0, pixels.size()*sizeof(uint32));
			return;
		}
		
		void setPixel(uint32 x, uint32 y, uint32 rgb)
		{
			if (x < width && y < height){pixels[(y * width)+x] = rgb;
			}
			return;
		}
		void drawPixel(uint32 x, uint32 y, uint32 rgb, uint8 a)
		{
			if (x < width && y < height && alphaMode)
			{
				pixels[(y * width)+x] = blendPixel(pixels[(y * width)+x], rgb, a);
			}
			return;
		}
		uint32 *getPixel(uint32 x, uint32 y)
		{
			if (x < width && y < height){return &pixels[(y * width)+x];
			} else return nullptr;
		}
		uint32 *accessPixel(uint32 x, uint32 y){return &pixels[(y * width)+x];
		}
		const uint32 *getPixelData(void){return &pixels[0];
		}
		
		uint32 getWidth(void){return width;
		}
		uint32 getHeight(void){return height;
		}
		
		uint32 getConsoleWidth(void){return consoleWidth;
		}
		uint32 getConsoleHeight(void){return consoleHeight;
		}
		
		uint8 getPixelSize(void){return pixelSize;
		}
		
		//Pixelizes the output already drawn to the screen (this is a very costly function, only use when nessesary)
		void pixelize(const float ratio)
		{
			if (!(ratio <= 1.f) && ratio < width && ratio < height)
			{
				pixels = resizeDataNearestNeighbor(pixels, width / ratio, height / ratio);
				pixels = resizeDataNearestNeighbor(pixels, consoleWidth / pixelSize, consoleHeight / pixelSize);
			}
			return;
		}
		
		void lineHorizontal(uint32 x, uint32 y, uint32 iterations, uint32 rgb, bool forward = true)
		{
			if (x < width && y < height && ((forward && x+iterations < width) || (!forward && x-iterations < width)))
			{
				for (uint32 i = 0; i < iterations; ++i)
				{
					accessBuffer(forward ? x+i : x-i, y) = rgb;
				}
			} else {
				for (uint32 i = 0; i < iterations; ++i)
				{
					if (x+i < width){accessBuffer(forward ? x+i : x-i, y) = rgb;
					} else break;
				}
			}
			return;
		}
		
		void lineVertical(uint32 x, uint32 y, uint32 iterations, uint32 rgb, bool forward = true)
		{
			if (x < width && y < height && ((forward && y+iterations < height) || (!forward && y-iterations < height)))
			{
				for (uint32 i = 0; i < iterations; ++i)
				{
					accessBuffer(x, forward ? y+i : y-i) = rgb;
				}
			} else {
				for (uint32 i = 0; i < iterations; ++i)
				{
					if (y+i < height){accessBuffer(x, forward ? y+i : y-i) = rgb;
					} else break;
				}
			}
			return;
		}
		
		void drawLine(uint32 x0, uint32 y0, uint32 x1, uint32 y1, uint32 rgb, uint8 alpha = 255)
		{
			int32 w, h, l;
			float dx, dy;
			uint32 x = x0, y = y0;
			
			w = x1-x0;
			h = y1-y0;
			l = ceil(sqrtf((w*w)+(h*h)));
			dx = (float)w/(float)l;
			dy = (float)h/(float)l;
			
			for (uint32 i = 0; i < l; ++i)
			{
				x = x0+(dx*i);
				y = y0+(dy*i);
				if (x < width && y < height)
				{
					switch (alpha)
					{
						case 255:
							accessBuffer(x, y) = rgb;
							break;
							
						default:
							accessBuffer(x, y) = blendPixel(accessBuffer(x, y), rgb, alpha);
							break;
					}
				} else break;
				
//				x += dx;
//				y += dy;
			}
			return;
		}
		
		void drawRect(uint32 x, uint32 y, uint32 width, uint32 height, uint32 rgb, bool fill = true)
		{
			if (fill)
			{
				for (uint32 dy = y; dy < y+height; ++dy)
				{
					for (uint32 dx = x; dx < x+width; ++dx)
					{
						if (dx < this->width && dy < this->height)
						{
							pixels[(dy * this->width)+dx] = rgb;
						}
					}
				}
			} else {
				lineHorizontal(x, y, width, rgb);
				lineVertical(x+width, y, height, rgb);
				lineHorizontal(x+width, y+height, width, rgb, false);
				lineVertical(x, y+height, height, rgb, false);
			}
			return;
		}
		void drawRectA(uint32 x, uint32 y, uint32 width, uint32 height, uint32 rgb, uint8 a)
		{
			for (uint32 dy = y; dy < y+height; ++dy)
			{
				for (uint32 dx = x; dx < x+width; ++dx)
				{
					if (dx < this->width && dy < this->height)
					{
						pixels[(dy * this->width)+dx] = blendPixel(pixels[(dy * this->width)+dx], rgb, a);
					}
				}
			}
			return;
		}
		
		//PP = Post-Processing
		void enablePPShaders(void)
		{
			enableShaders = true;
			return;
		}
		void disablePPShaders(void)
		{
			enableShaders = false;
			return;
		}
		void loadPPShader(void(*funcPtr)(uint32*, uint32, uint32, uint32, uint32, void*), void* shaderData = nullptr)
		{
			shaderList.push_back(funcPtr);
			shaderDataList.push_back(shaderData);
			return;
		}
		void clearPPShaders(void)
		{
			shaderList.clear();
			shaderDataList.clear();
			return;
		}
		
		void draw(Image &image)
		{
			for (uint32 y = 0; y < image.getHeight(); ++y)
			{
				for (uint32 x = 0; x < image.getWidth(); ++x)
				{
					uint32 dstX = x+image.getPosX(), dstY = y+image.getPosY();
					if (dstX < width && dstY < height) //Within bounds
					{
						if (image.getPixel(x, y)->second == 255 || !alphaMode) //Can't do alpha or alpha isn't enabled
						{
							pixels[(dstY * width)+dstX] = image.accessPixel(x, y)->first;
						} else if (image.getPixel(x, y)->second != 0){ //Alpha is enabled
							pixels[(dstY * width)+dstX] = blendPixel(pixels[(dstY * width)+dstX], image.accessPixel(x, y)->first, image.accessPixel(x, y)->second);
						}
					} else if (dstY > height-1){return;
					} else if (dstX > width-1){break;
					}
				}
			}
			return;
		}
		void draw(Text &text)
		{
			this->draw(text.getTextImage());
			return;
		}
		//drawType = 0 - if width > image.width() or height > image.height(), the image will be tiled
		//drawType = 1 - if width > image.width() or height > image.height(), the image will be resized using nearest neighbor interpolation
		void drawEX(Image &image, uint32 srcX, uint32 srcY, uint32 dstX, uint32 dstY, uint32 width, uint32 height, DrawType drawType = DrawType::Repeat, void(*funcPtr)(std::pair<uint32, uint8>*, void*) = nullptr, void *funcData = nullptr)
		{
			uint32 x = srcX, y = srcY, dx = dstX, dy = dstY;
			
			if (drawType == DrawType::Repeat)
			{
				while (dy<dstY+height && dy<this->height)
				{
					while (dx<dstX+width && dx<this->width)
					{
						if (dx < this->width && dy < this->height) //Within bounds
						{
							auto pixel = image.getPixelData()[(y*image.getWidth())+x];
							if (funcPtr != nullptr){funcPtr(&pixel, funcData);
							}
							if (pixel.second == 255 || !alphaMode) //Can't do alpha or alpha isn't enabled
							{
								pixels[(dy * this->width)+dx] = pixel.first;//image.getPixel(x, y)->first;
							} else if (pixel.second != 0){ //Alpha is enabled
								pixels[(dy * this->width)+dx] = blendPixel(pixels[(dy * this->width)+dx], pixel.first, pixel.second);
							}
						} else if (dy > this->height-1){return;
						} else if (dx > this->width-1){break;
						}
						if (x == image.getWidth()-1){x = 0;
						} else ++x;
						++dx;
					}
					x = srcX;
					dx = dstX;
					if (y == image.getHeight()-1){y = 0;
					} else ++y;
					++dy;
				}
			} else {
				if (srcX >= image.getWidth()){srcX %= image.getWidth();
				}
				if (srcY >= image.getHeight()){srcY %= image.getHeight();
				}
				float scaleX = (float)(image.getWidth()-srcX)/(float)width, scaleY = (float)(image.getHeight()-srcY)/(float)height;
				
				while (dy<dstY+height && dy<this->height)
				{
					while (dx<dstX+width && dx<this->width)
					{
						if (dx < this->width && dy < this->height) //Within bounds
						{
							x = srcX+((dx-dstX)*scaleX);
							x = std::max<int>(std::min<int>(x, image.getWidth()), 0);
							y = srcY+((dy-dstY)*scaleY);
							y = std::max<int>(std::min<int>(y, image.getHeight()), 0);
							auto pixel = image.getPixelData()[(y*image.getWidth())+x];
							if (funcPtr != nullptr){funcPtr(&pixel, funcData);
							}
							if (pixel.second == 255 || !alphaMode) //Can't do alpha or alpha isn't enabled
							{
								pixels[(dy * this->width)+dx] = pixel.first;//image.getPixel(x, y)->first;
							} else if (pixel.second != 0){ //Alpha is enabled
								pixels[(dy * this->width)+dx] = blendPixel(pixels[(dy * this->width)+dx], pixel.first, pixel.second);
							}
						} else if (dy > this->height-1){return;
						} else if (dx > this->width-1){break;
						}
						++dx;
					}
					dx = dstX;
					++dy;
				}
			}
		}
		
		void setTitle(const std::string title)
		{
			this->title = title;
			SetConsoleTitle(this->title.c_str());
			return;
		}
		std::string getTitle(void){return title;
		}
		
		void setOutputScale(float s)
		{
			outputScale = s;
			return;
		}
		float getOutputScale(void){return outputScale;
		}
		
		void setOutputPos(uint32 x, uint32 y)
		{
			startX = x;
			startY = y;
			return;
		}
		uint32 getOutputPosX(void){return startX;
		}
		uint32 getOutputPosY(void){return startY;
		}
		
		void setRenderTarget(HWND hwnd)
		{
			targetDC = GetDC(hwnd);
			tempDC = CreateCompatibleDC(targetDC);
			return;
		}
		void setRenderTarget(HDC hdc)
		{
			targetDC = hdc;
			tempDC = CreateCompatibleDC(targetDC);
			return;
		}
		HWND getRenderTarget(void){return WindowFromDC(targetDC);
		}
};

void AniImage::draw(ConsoleGraphics *graphics)
{
	if (firstFrameDrawn){firstFrameDrawn = false;
	}
	if (graphics != nullptr){graphics->draw(dstImage);
	}
	return;
}

#endif
