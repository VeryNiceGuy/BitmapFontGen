#include "BitmapFontGen.h"

int BitmapFontGen::calculateBitmapHeight(unsigned int bitmapWidth, FT_Face face, std::vector<FT_ULong> charcodes)
{
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int bitmapHeight = 1;

	for (size_t i = 0; i < charcodes.size(); ++i)
	{
		FT_Load_Char(face, charcodes[i], FT_LOAD_DEFAULT);
		if (x + face->glyph->bitmap.width > bitmapWidth)
		{
			x = 0;
			y += face->glyph->bitmap.rows;
		}
		x += face->glyph->bitmap.width;
	}

	while(bitmapHeight < y)
		bitmapHeight <<= 1;

	return bitmapHeight;
}

void BitmapFontGen::write(int fontSize, unsigned int bitmapWidth, const char* fontPath, const char* bitmapPath, const char* configPath, Color color) {
	std::ofstream config(configPath);

	FT_Library ft;
	FT_Face face;
	FT_Init_FreeType(&ft);
	FT_New_Face(ft, fontPath, 0, &face);
	FT_Set_Char_Size(face, 0, fontSize << 6, 96, 96);

	auto charcodes = getCharcodes(face);
	auto kerning = getKerningPairs(face, charcodes);
	std::vector<Glyph> glyphs;

	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int maxY = 0;
	unsigned int bitmapHeight = calculateBitmapHeight(bitmapWidth, face, charcodes);

	CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);
	IWICImagingFactory2* imagingFactory;
	CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&imagingFactory));
	
	IWICBitmap* bitmap;
	imagingFactory->CreateBitmap(bitmapWidth, bitmapHeight, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap);

	for (size_t i = 0; i < charcodes.size(); ++i)
	{
		FT_Load_Char(face, charcodes[i], FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT);
		if (face->glyph->bitmap.buffer == nullptr)
			continue;

		if ((x + face->glyph->bitmap.width) > bitmapWidth)
		{
			x = 0;
			y += maxY;
			maxY = 0;
		}

		copyPixels(x, y, bitmapWidth, bitmapHeight, face, bitmap, color);
		glyphs.push_back({charcodes[i], x, y, face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap_left, face->glyph->bitmap_top, face->glyph->advance.x});

		x += face->glyph->bitmap.width;
		maxY = std::max(maxY, face->glyph->bitmap.rows);
	}

	FT_Done_FreeType(ft);

	writeRootBeginning(config);
	writeGlyphs(glyphs, config);

	if(FT_HAS_KERNING(face))
		writeKerningPairs(kerning, config);
	writeRootEnd(config);

	GUID format = GUID_ContainerFormatPng;
	IWICBitmapEncoder* encoder;
	imagingFactory->CreateEncoder(format, nullptr, &encoder);

	IWICStream* stream;
	imagingFactory->CreateStream(&stream);

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> convert;
	stream->InitializeFromFilename(convert.from_bytes(bitmapPath).c_str(), GENERIC_WRITE);
	encoder->Initialize(stream, WICBitmapEncoderNoCache);

	IWICBitmapFrameEncode* encode;
	encoder->CreateNewFrame(&encode, nullptr);
	encode->Initialize(nullptr);

	auto rrr = encode->WriteSource(bitmap, nullptr);
	encode->Commit();
	encoder->Commit();

	encode->Release();
	encoder->Release();
	stream->Release();
	bitmap->Release();
	imagingFactory->Release();
	CoUninitialize();
}

void BitmapFontGen::writeRootBeginning(std::ofstream& stream)
{
	stream << "{" << std::endl;
}

void BitmapFontGen::writeRootEnd(std::ofstream& stream)
{
	stream << std::endl << "}";
}

void BitmapFontGen::copyPixels(unsigned int bitmapX, unsigned int bitmapY, unsigned int bitmapWidth, unsigned int bitmapHeight, FT_Face face, IWICBitmap* bitmap, Color color)
{
	WICRect rect = { 0, 0, bitmapWidth , bitmapHeight };
	IWICBitmapLock* lock = NULL;
	bitmap->Lock(&rect, WICBitmapLockWrite, &lock);

	UINT size = 0;
	BYTE* pointer = NULL;
	lock->GetDataPointer(&size, &pointer);

	for (int y = 0; y < face->glyph->bitmap.rows; ++y)
		for (int x = 0; x < face->glyph->bitmap.width; ++x) {
			Color transformed = color.transform(face->glyph->bitmap.buffer[y * face->glyph->bitmap.pitch + x]);

			if (face->glyph->bitmap.buffer[y * face->glyph->bitmap.pitch + x] == 0) {
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 0] = 0;
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 1] = 0;
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 2] = 0;
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 3] = 0;
			}
			else {
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 0] = transformed.b;
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 1] = transformed.g;
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 2] = transformed.r;
				pointer[(bitmapY + y) * (bitmapWidth * 4) + ((bitmapX + x) * 4) + 3] = transformed.a;
			}
		}
	lock->Release();
}

std::vector<FT_ULong> BitmapFontGen::getCharcodes(FT_Face face)
{
	FT_UInt index;
	std::vector<FT_ULong> charcodes;
	FT_ULong charcode = FT_Get_First_Char(face, &index);
	charcodes.push_back(charcode);
	while (index != 0) {
		charcode = FT_Get_Next_Char(face, charcode, &index);
		charcodes.push_back(charcode);
	}
	return charcodes;
}

std::vector<KerningPair> BitmapFontGen::getKerningPairs(FT_Face face, std::vector<FT_ULong> charcodes)
{
	std::vector<KerningPair> kerningPairs;
	for (FT_UInt i = 0; i < charcodes.size(); ++i)
	{
		FT_UInt left = FT_Get_Char_Index(face, charcodes[i]);
		for (FT_UInt k = 0; k < charcodes.size(); ++k)
		{
			FT_UInt right = FT_Get_Char_Index(face, charcodes[k]);
			FT_Vector kerning;
			FT_Get_Kerning(face, left, right, FT_KERNING_DEFAULT, &kerning);

			if (kerning.x != 0)
				kerningPairs.push_back({left, right, kerning.x});
		}
	}
	return kerningPairs;
}

void BitmapFontGen::writeGlyphs(std::vector<Glyph> glyphs, std::ofstream& stream)
{
	if (glyphs.empty())
		return;

	stream << "    glyphs: [" << std::endl;
	size_t i = 0;
	for (auto &glyph : glyphs)
	{
		stream << "        {" << std::endl;
		stream << "            \"codepoint\":" << glyph.codepoint << "," << std::endl;
		stream << "            \"x\":" << glyph.x << "," << std::endl;
		stream << "            \"y\":" << glyph.y << "," << std::endl;
		stream << "            \"width\":" << glyph.width << "," << std::endl;
		stream << "            \"height\":" << glyph.height << "," << std::endl;
		stream << "            \"leftBearing\":" << glyph.leftBearing << "," << std::endl;
		stream << "            \"topBearing\":" << glyph.topBearing << std::endl;
		stream << "            \"advance\":" << glyph.advance << std::endl;
		stream << "        }";

		if (i != glyphs.size() - 1)
			stream << ",";
		stream << std::endl;
		++i;
	}
	stream << "    ]" << std::endl;
}

void BitmapFontGen::writeKerningPairs(std::vector<KerningPair> pairs, std::ofstream& stream)
{
	if (pairs.empty())
		return;

	stream << "    kerningPairs: [" << std::endl;
	size_t i = 0;
	for (auto pair : pairs) {
		stream << "        {" << std::endl;
		stream << "            \"left\":" << pair.left << "," << std::endl;
		stream << "            \"right\":" << pair.right << "," << std::endl;
		stream << "            \"kerning\":" << pair.kerning << "," << std::endl;
		stream << "        }";

		if (i != pairs.size() - 1)
			stream << ",";
		stream << std::endl;
	}
	stream << std::endl << "    ]";
	++i;
}

int main()
{
	BitmapFontGen::write(22, 1024, "Arial.otf", "bitmap.png", "config.cfg", {0,0,0,0});
	return 0;
}