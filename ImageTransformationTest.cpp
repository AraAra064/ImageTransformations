#include <iostream>
#include <math.h>

#include "ConsoleGraphics1.hpp"
#include "ConsoleUtilities.h"

int main()
{
	ConsoleGraphics *graphics = new ConsoleGraphics(400, 400, true, 2);
	graphics->enableAlpha();
	graphics->enablePPShaders();
	graphics->setTitle("Image Transformation Test");
	Image image("wall.bmp"), transformedImage, font("DefaultFont.cgif");
	image.resize(graphics->getWidth(), graphics->getHeight());
	transformedImage.resize(image.getWidth(), image.getHeight());
	float deltaTime, theta = 0.f, s = 30.f, t;
	Clock frameTime;
	Text text;
	text.setFont(&font, 8, 13);
	
	bool bilinear = true;
	ExtrapolationMethod extra = ExtrapolationMethod::Repeat;
	
	FPMatrix3x3 mat;
	FPVector3D vec;
	float widthRec = 1.f/transformedImage.getWidth(), heightRec = 1.f/transformedImage.getHeight();
	
	float n = 0.1f;
//	graphics->loadPPShader([](uint32 *pixel, uint32 width, uint32 height, uint32 x, uint32 y, void *data)->void
//	{
//		float *n = reinterpret_cast<float*>(data);
//		
//		if (fabs((*n)) > 0.05f)
//		{
//			static int r, g, b;
//			r = GetBValue(*pixel);
//			r += (*n)*(rand()%256)*((rand()%2) ? -1 : 1);
////			r += (*n)*rand()*map<float>(rand(), 0, RAND_MAX, -1.f, 1.f);
//			r = std::max(0, r), r = std::min(r, 255);
//			
//			g = GetGValue(*pixel);
//			g += (*n)*(rand()%256)*((rand()%2) ? -1 : 1);
////			g += (*n)*rand()*map<float>(rand(), 0, RAND_MAX, -1.f, 1.f);
//			g = std::max(0, g), g = std::min(g, 255);
//			
//			b = GetRValue(*pixel);
//			b += (*n)*(rand()%256)*((rand()%2) ? -1 : 1);
////			b += (*n)*rand()*map<float>(rand(), 0, RAND_MAX, -1.f, 1.f);
//			b = std::max(0, b), b = std::min(b, 255);
//			
//			*pixel = RGB((unsigned char)b, (unsigned char)g, (unsigned char)r);
//		}
//	}, &n);
//	graphics->loadPPShader([](unsigned long int *pixel, unsigned long int width, unsigned long int height, unsigned long int x, unsigned long int y, unsigned long long int ops, void *data)->void
//	{
//		unsigned char r = GetBValue(*pixel), g = GetGValue(*pixel), b = GetRValue(*pixel);
//		unsigned int c = r+g+b;
//		c /= 3;
//		*pixel = RGB(c, c, c);
//		return;
//	});
	
	while (!GetAsyncKeyState(VK_ESCAPE))
	{
		deltaTime = frameTime.restart()/1000.f;
		theta += s * deltaTime;
		if (theta > 360.f){theta -= 360.f;
		}
		t = sinf(theta / 180.f * 3.14159265f)*50.f;
		mat.identity();
		mat.rotateDeg(theta);
		mat.translate(image.getWidth()/2, image.getHeight()/2);
		mat.multiply(0.75f);
		mat.inv();
		mat.translate(image.getWidth()/2, image.getHeight()/2);
		
		for (int y = 0; y < graphics->getHeight(); ++y)
		{
			for (int x = 0; x < graphics->getWidth(); ++x)
			{
				vec = mat.multiply(FPVector3D(x, y, 1.f));
				vec.multiply(FPVector3D(widthRec, heightRec, 1.f));
				*graphics->accessPixel(x, y) = image.getPixel(vec.x, vec.y, bilinear ? InterpolationMethod::Bilinear : InterpolationMethod::NearestNeighbor, extra).first;
			}
		}
		
		graphics->drawRectA(0, 0, graphics->getWidth(), 26, cg::BGR(175, 175, 175), 127);
		text.setPos(0, 0);
		text.setText(std::string("InterpolationMethod=")+(bilinear ? "Bilinear" : "NearestNeighbor"));
		graphics->draw(text);
		text.setPos(0, 13);
		text.setText(std::string("ExtrapolationMethod=")+(extra == ExtrapolationMethod::None ? "None" : (extra == ExtrapolationMethod::Repeat ? "Repeat" : "Extend")));
		graphics->draw(text);
		
		if (GetAsyncKeyState(VK_RETURN)){bilinear = !bilinear;
		}
		if (GetAsyncKeyState(VK_UP))
		{
			switch (extra)
			{
				case ExtrapolationMethod::None:
					extra = ExtrapolationMethod::Repeat;
					break;
				
				case ExtrapolationMethod::Repeat:
					extra = ExtrapolationMethod::Extend;
					break;
				
				case ExtrapolationMethod::Extend:
					extra = ExtrapolationMethod::None;
					break;
			}
		}
		
//		for (int y = 0; y < transformedImage.getHeight(); ++y)
//		{
//			for (int x = 0; x < transformedImage.getWidth(); ++x)
//			{
//				vec = mat.multiply(FPVector3D(x, y, 1.f));
//				transformedImage.setPixel(x, y, image.getPixel(vec.x, vec.y) != nullptr ? image.getPixel(vec.x, vec.y)->first : 0);
//			}
//		}
		
//		graphics->clear();
//		graphics->draw(transformedImage);
		graphics->display();
	}
	
	delete graphics;
}
