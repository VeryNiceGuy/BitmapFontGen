#pragma once

#define NOMINMAX
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <string>
#include <fstream>
#include <codecvt>
#include <vector>
#include <wincodec.h>
#include <algorithm>

#pragma comment(lib, "Windowscodecs.lib")

struct Glyph
{
	unsigned long codepoint;
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	int leftBearing;
	int topBearing;
	long advance;
};

struct Whitespace
{
	unsigned long codepoint;
	long advance;
};

struct KerningPair
{
	unsigned long left;
	unsigned long right;
	long kerning;
};

struct Color
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;

	Color transform(unsigned char intensity)
	{
		double alpha = (a / 255.0) * (intensity / 255.0);
		return {
			static_cast<unsigned char>((r / 255.0) * alpha * 255),
			static_cast<unsigned char>((g / 255.0) * alpha * 255),
			static_cast<unsigned char>((b / 255.0) * alpha * 255),
			static_cast<unsigned char>(alpha * 255)
		};
	}
};

class BitmapFontGen
{
public:
	static int calculateBitmapHeight(unsigned int bitmapWidth, FT_Face face, std::vector<FT_ULong> charcodes);
	static void write(unsigned int fontSize, unsigned int bitmapWidth, const char* fontPath, const char* bitmapPath, const char* configPath, Color color);
	static std::vector<FT_ULong> getCharcodes(FT_Face face);
	static std::vector<KerningPair> getKerningPairs(FT_Face face, std::vector<FT_ULong> charcodes);
	static void writeRootBeginning(std::ofstream& stream);
	static void writeRootEnd(std::ofstream& stream);
	static void writeLineHeight(long lineHeight, std::ofstream& stream);
	static void writeGlyphs(std::vector<Glyph> glyphs, std::ofstream& stream);
	static void writeWhitespaces(std::vector<Whitespace> whitespaces, std::ofstream& stream);
	static void writeKerningPairs(std::vector<KerningPair> pairs, std::ofstream& stream);
	static void copyPixels(unsigned int bitmapX, unsigned int bitmapY, unsigned int bitmapWidth, unsigned int bitmapHeight, FT_Face face, IWICBitmap* bitmap, Color color);
};